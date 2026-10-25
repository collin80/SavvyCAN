#include "githubclient.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

static QString encoded(const QString &segment)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(segment));
}

//a path in a repo, each part percent-encoded but the '/' between them kept
static QString encodedPath(const QString &path)
{
    QStringList parts = path.split('/', Qt::SkipEmptyParts);
    for (QString &part : parts) part = encoded(part);
    return parts.join('/');
}

static QString repoPath(const QString &owner, const QString &repo)
{
    return "/repos/" + encoded(owner) + "/" + encoded(repo);
}

GitHubClient::GitHubClient(QObject *parent)
    : QObject(parent),
      manager(new QNetworkAccessManager(this)),
      apiUrl("https://api.github.com")
{
}

GitHubClient::~GitHubClient()
{
    //replies still running would otherwise finish into callbacks whose owners are being deleted
    const QList<QNetworkReply *> replies = manager->findChildren<QNetworkReply *>();
    for (QNetworkReply *reply : replies)
    {
        reply->disconnect();
        reply->abort();
    }
}

void GitHubClient::setApiUrl(const QString &url)
{
    apiUrl = url;
    while (apiUrl.endsWith('/')) apiUrl.chop(1);
}

void GitHubClient::setToken(const QString &newToken)
{
    token = newToken.trimmed();
}

QNetworkRequest GitHubClient::request(const QString &apiPath) const
{
    QNetworkRequest req(QUrl::fromEncoded((apiUrl + apiPath).toUtf8(), QUrl::StrictMode));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    req.setRawHeader("User-Agent", "SavvyCAN");
    if (!token.isEmpty()) req.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    return req;
}

QNetworkReply *GitHubClient::get(const QString &apiPath, ReplyCallback done, bool raw)
{
    QNetworkRequest req = request(apiPath);
    if (raw) req.setRawHeader("Accept", "application/vnd.github.raw");

    QNetworkReply *reply = manager->get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, done]()
    {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) done(reply, QByteArray(), errorText(reply, body));
        else done(reply, body, QString());
    });
    return reply;
}

QString GitHubClient::notFoundText()
{
    return tr("Not found on GitHub, or this account has no access to it.");
}

QString GitHubClient::errorText(QNetworkReply *reply, const QByteArray &body)
{
    if (reply->error() == QNetworkReply::OperationCanceledError) return tr("Cancelled.");

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 0) return tr("Couldn't reach GitHub: %1").arg(reply->errorString());

    const QString message = QJsonDocument::fromJson(body).object().value("message").toString();

    if ((status == 403 || status == 429) && reply->rawHeader("X-RateLimit-Remaining") == "0")
    {
        const qint64 reset = reply->rawHeader("X-RateLimit-Reset").toLongLong();
        const QString until = reset ? QDateTime::fromSecsSinceEpoch(reset).toString("HH:mm") : tr("later");
        return tr("GitHub's request limit is used up until %1. Signed-in accounts get a much higher limit.").arg(until);
    }
    if (status == 401)
        return tr("GitHub didn't accept the sign-in (it may have expired or been withdrawn): sign in again under "
                  "GitHub Account.");
    if (status == 404) return notFoundText();
    if (message.isEmpty()) return tr("GitHub answered %1 %2.").arg(status)
            .arg(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
    return tr("GitHub answered %1: %2").arg(status).arg(message);
}

void GitHubClient::getUser(std::function<void(const QString &login, const QString &error)> done)
{
    get("/user", [done](QNetworkReply *, const QByteArray &body, const QString &error)
    {
        if (!error.isEmpty()) return done(QString(), error);
        const QString login = QJsonDocument::fromJson(body).object().value("login").toString();
        if (login.isEmpty()) return done(QString(), tr("GitHub's answer had no account name in it."));
        done(login, QString());
    });
}

void GitHubClient::getRepo(const QString &owner, const QString &repo,
                           std::function<void(const QString &defaultBranch, const QString &error)> done)
{
    get(repoPath(owner, repo), [done](QNetworkReply *, const QByteArray &body, const QString &error)
    {
        if (!error.isEmpty()) return done(QString(), error);
        const QString branch = QJsonDocument::fromJson(body).object().value("default_branch").toString();
        if (branch.isEmpty()) return done(QString(), tr("The repo has no branches yet."));
        done(branch, QString());
    });
}

void GitHubClient::listBranches(const QString &owner, const QString &repo,
                                std::function<void(const QList<GitHubBranch> &branches, const QString &error)> done)
{
    listBranchesFrom(owner, repo, 1, QList<GitHubBranch>(), done);
}

//100 a page; GitHub's Link header names the next page when there are more
void GitHubClient::listBranchesFrom(const QString &owner, const QString &repo, int page, QList<GitHubBranch> branches,
                                    std::function<void(const QList<GitHubBranch> &branches, const QString &error)> done)
{
    get(repoPath(owner, repo) + "/branches?per_page=100&page=" + QString::number(page),
        [this, owner, repo, page, branches, done](QNetworkReply *reply, const QByteArray &body, const QString &error) mutable
    {
        if (!error.isEmpty()) return done(QList<GitHubBranch>(), error);

        const QJsonArray array = QJsonDocument::fromJson(body).array();
        for (const QJsonValue &value : array)
        {
            const QJsonObject obj = value.toObject();
            branches.append({ obj.value("name").toString(), obj.value("commit").toObject().value("sha").toString() });
        }
        if (reply->rawHeader("Link").contains("rel=\"next\"") && page < 50)
            listBranchesFrom(owner, repo, page + 1, branches, done);
        else done(branches, QString());
    });
}

void GitHubClient::getTree(const QString &owner, const QString &repo, const QString &commitSha,
                           std::function<void(const QList<GitHubEntry> &entries, bool truncated, const QString &error)> done)
{
    get(repoPath(owner, repo) + "/git/trees/" + encoded(commitSha) + "?recursive=1",
        [done](QNetworkReply *, const QByteArray &body, const QString &error)
    {
        if (!error.isEmpty()) return done(QList<GitHubEntry>(), false, error);

        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        QList<GitHubEntry> entries;
        const QJsonArray tree = obj.value("tree").toArray();
        for (const QJsonValue &value : tree)
        {
            const QJsonObject item = value.toObject();
            const QString type = item.value("type").toString();
            if (type != "blob" && type != "tree") continue; //submodules ("commit") have nothing to load
            GitHubEntry entry;
            entry.path = item.value("path").toString();
            entry.sha = item.value("sha").toString();
            entry.size = qint64(item.value("size").toDouble());
            entry.isFolder = (type == "tree");
            entries.append(entry);
        }
        done(entries, obj.value("truncated").toBool(), QString());
    });
}

QNetworkReply *GitHubClient::download(const QString &owner, const QString &repo, const QString &ref,
                                      const QString &path,
                                      std::function<void(const QByteArray &data, const QString &error)> done)
{
    return get(repoPath(owner, repo) + "/contents/" + encodedPath(path) + "?ref=" + encoded(ref),
               [done](QNetworkReply *, const QByteArray &body, const QString &error) { done(body, error); }, true);
}
