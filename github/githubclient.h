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

//what saving a file left on GitHub
struct GitHubSaveResult
{
    QString sha;        //the new blob SHA, for the next save's conflict check
    QString commitUrl;  //the commit on github.com
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

    //the blob SHA of a file at that branch or commit; empty with no error when there's no such file
    void getFileSha(const QString &owner, const QString &repo, const QString &ref, const QString &path,
                    std::function<void(const QString &sha, const QString &error)> done);
    //creates or replaces a file with one commit. sha: the version being replaced (empty for a new file); when the
    //file on GitHub isn't that version (or exists although sha is empty), conflict is true and nothing is saved.
    //The reply is returned for progress.
    QNetworkReply *putFile(const QString &owner, const QString &repo, const QString &branch, const QString &path,
                           const QByteArray &content, const QString &message, const QString &sha,
                           std::function<void(const GitHubSaveResult &result, bool conflict, const QString &error)> done);
    void createBranch(const QString &owner, const QString &repo, const QString &name, const QString &fromSha,
                      std::function<void(const QString &error)> done);
    void deleteBranch(const QString &owner, const QString &repo, const QString &name,
                      std::function<void(const QString &error)> done);
    //a pull request from head into base; done gets its page on github.com
    void createPullRequest(const QString &owner, const QString &repo, const QString &head, const QString &base,
                           const QString &title, const QString &body,
                           std::function<void(const QString &url, const QString &error)> done);
    //the signed-in account's organisations
    void listOrgs(std::function<void(const QStringList &orgs, const QString &error)> done);
    //a new repo with a README (so it has a branch to commit to), under the account (org empty) or an organisation.
    //status is GitHub's HTTP status on failure (403 or 404: the sign-in may create no repos).
    void createRepo(const QString &org, const QString &name, const QString &description, bool isPrivate,
                    std::function<void(const QString &owner, const QString &defaultBranch, const QString &error,
                                       int status)> done);

    //A message for a failed reply, in plain words
    static QString errorText(QNetworkReply *reply, const QByteArray &body);
    static QString notFoundText(); //what errorText gives for a 404

private:
    QNetworkAccessManager *manager;
    QString apiUrl;
    QString token;

    using ReplyCallback = std::function<void(QNetworkReply *reply, const QByteArray &body, const QString &error)>;

    QNetworkRequest request(const QString &apiPath) const;
    QNetworkReply *get(const QString &apiPath, ReplyCallback done, bool raw = false);
    QNetworkReply *send(const QByteArray &verb, const QString &apiPath, const QByteArray &json, ReplyCallback done);
    void listBranchesFrom(const QString &owner, const QString &repo, int page, QList<GitHubBranch> branches,
                          std::function<void(const QList<GitHubBranch> &branches, const QString &error)> done);
};

#endif // GITHUBCLIENT_H
