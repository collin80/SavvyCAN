#ifndef GITHUBCACHE_H
#define GITHUBCACHE_H

#include <QString>
#include <QByteArray>

//Where a file downloaded from GitHub came from
struct GitHubOrigin
{
    QString owner;
    QString repo;
    QString branch;
    QString path;   //in the repo, '/' separated
    QString sha;    //the file's blob SHA, which saving it back needs

    bool isValid() const { return !owner.isEmpty() && !repo.isEmpty() && !branch.isEmpty() && !path.isEmpty(); }
    QString displayName() const { return "github:" + owner + "/" + repo + "/" + path; }
};

//Files downloaded from GitHub are kept in the cache folder as <owner>/<repo>/<branch>/<path>, with the real file
//name so the existing loaders (and autodetect) work on them, and a <file>.origin.json beside each one saying where
//it came from.
class GitHubCache
{
public:
    static QString rootDir();
    static void setRootDir(const QString &dir); //for the unit tests

    //empty if the origin's path could reach outside the cache folder ("..", absolute paths)
    static QString localPath(const GitHubOrigin &origin);
    static bool store(const GitHubOrigin &origin, const QByteArray &data, QString &localFile, QString &error);
    //an invalid origin for a file that didn't come from GitHub
    static GitHubOrigin originOf(const QString &localFile);
    //a cached file and what it says about where it came from
    static void remove(const QString &localFile);
    static bool clear();
};

#endif // GITHUBCACHE_H
