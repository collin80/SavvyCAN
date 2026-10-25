#ifndef GITHUBCLIENT_H
#define GITHUBCLIENT_H

#include <QObject>
#include <QList>
#include <QStringList>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

struct GitHubBranch
{
    QString name;
    QString commitSha;
};

//one file or folder in a repo's tree
struct GitHubEntry
{
    QString path;   //'/' separated
    QString sha;    //blob SHA for files
    qint64 size = 0;
    bool isFolder = false;
};

//GitHub's REST API (https://docs.github.com/rest) through QNetworkAccessManager. Every call is asynchronous and
//ends in its callback, with an empty error on success. Callbacks are never called once the client is deleted.
class GitHubClient : public QObject
{
    Q_OBJECT

public:
    explicit GitHubClient(QObject *parent = nullptr);
    ~GitHubClient();

    void setApiUrl(const QString &url);
    void setToken(const QString &token);    //empty: not signed in (public repos only, 60 requests an hour)

    //the signed-in account
    void getUser(std::function<void(const QString &login, const QString &error)> done);
    //the repo's default branch (also checks the repo is there and readable)
    void getRepo(const QString &owner, const QString &repo,
                 std::function<void(const QString &defaultBranch, const QString &error)> done);
    void listBranches(const QString &owner, const QString &repo,
                      std::function<void(const QList<GitHubBranch> &branches, const QString &error)> done);
    //every file and folder at that commit; truncated when GitHub cut the list short (very large repos)
    void getTree(const QString &owner, const QString &repo, const QString &commitSha,
                 std::function<void(const QList<GitHubEntry> &entries, bool truncated, const QString &error)> done);
    //a file's contents at that commit (or branch). The reply is returned for progress and aborting.
    QNetworkReply *download(const QString &owner, const QString &repo, const QString &ref, const QString &path,
                            std::function<void(const QByteArray &data, const QString &error)> done);

    //A message for a failed reply, in plain words
    static QString errorText(QNetworkReply *reply, const QByteArray &body);

private:
    QNetworkAccessManager *manager;
    QString apiUrl;
    QString token;

    using ReplyCallback = std::function<void(QNetworkReply *reply, const QByteArray &body, const QString &error)>;

    QNetworkRequest request(const QString &apiPath) const;
    QNetworkReply *get(const QString &apiPath, ReplyCallback done, bool raw = false);
    void listBranchesFrom(const QString &owner, const QString &repo, int page, QList<GitHubBranch> branches,
                          std::function<void(const QList<GitHubBranch> &branches, const QString &error)> done);
};

#endif // GITHUBCLIENT_H
