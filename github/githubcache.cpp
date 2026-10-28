#include "githubcache.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

static QString cacheRootOverride;

QString GitHubCache::rootDir()
{
    if (!cacheRootOverride.isEmpty()) return cacheRootOverride;
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/github";
}

void GitHubCache::setRootDir(const QString &dir)
{
    cacheRootOverride = dir;
}

//one folder name that can't be "." or ".." or hold a separator
static QString safeSegment(const QString &segment)
{
    if (segment.isEmpty() || segment == "." || segment == "..") return QString();
    if (segment.contains('/') || segment.contains('\\') || segment.contains(':')) return QString();
    return segment;
}

QString GitHubCache::localPath(const GitHubOrigin &origin)
{
    if (!origin.isValid()) return QString();

    //branch names can hold '/', which would make different branch/path pairs the same folder
    const QString branch = QString::fromLatin1(QUrl::toPercentEncoding(origin.branch));
    QStringList parts { safeSegment(origin.owner), safeSegment(origin.repo), safeSegment(branch) };
    const QStringList pathParts = origin.path.split('/');
    for (const QString &part : pathParts) parts.append(safeSegment(part));
    for (const QString &part : std::as_const(parts)) if (part.isEmpty()) return QString();
    if (parts.last().endsWith(".origin.json")) return QString(); //that name is ours

    return rootDir() + "/" + parts.join('/');
}

bool GitHubCache::store(const GitHubOrigin &origin, const QByteArray &data, QString &localFile, QString &error)
{
    localFile = localPath(origin);
    if (localFile.isEmpty())
    {
        error = QObject::tr("The file's path in the repo can't be stored: %1").arg(origin.path);
        return false;
    }
    if (!QDir().mkpath(QFileInfo(localFile).path()))
    {
        error = QObject::tr("Couldn't make the folder %1").arg(QFileInfo(localFile).path());
        return false;
    }

    QSaveFile file(localFile);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
    {
        error = QObject::tr("Couldn't write %1: %2").arg(localFile, file.errorString());
        return false;
    }

    QJsonObject json;
    json["owner"] = origin.owner;
    json["repo"] = origin.repo;
    json["branch"] = origin.branch;
    json["path"] = origin.path;
    json["sha"] = origin.sha;
    QSaveFile sidecar(localFile + ".origin.json");
    const QByteArray jsonData = QJsonDocument(json).toJson();
    if (!sidecar.open(QIODevice::WriteOnly) || sidecar.write(jsonData) != jsonData.size() || !sidecar.commit())
    {
        error = QObject::tr("Couldn't write %1: %2").arg(sidecar.fileName(), sidecar.errorString());
        return false;
    }
    return true;
}

GitHubOrigin GitHubCache::originOf(const QString &localFile)
{
    GitHubOrigin origin;
    QFile sidecar(localFile + ".origin.json");
    if (!sidecar.open(QIODevice::ReadOnly)) return origin;

    const QJsonObject json = QJsonDocument::fromJson(sidecar.readAll()).object();
    origin.owner = json["owner"].toString();
    origin.repo = json["repo"].toString();
    origin.branch = json["branch"].toString();
    origin.path = json["path"].toString();
    origin.sha = json["sha"].toString();

    //only if it's still where that origin puts it (not a copy somewhere else)
    if (QFileInfo(localPath(origin)) != QFileInfo(localFile)) return GitHubOrigin();
    return origin;
}

void GitHubCache::remove(const QString &localFile)
{
    QFile::remove(localFile);
    QFile::remove(localFile + ".origin.json");
}

bool GitHubCache::clear()
{
    QDir dir(rootDir());
    if (!dir.exists()) return true;
    return dir.removeRecursively();
}
