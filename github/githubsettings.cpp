#include "githubsettings.h"

#include <QSettings>
#include <QRegularExpression>
#include <QUrl>
#include "simplecrypt.h"

//"Sign in with GitHub" uses the SavvyCAN GitHub App (https://github.com/apps/savvycan, Tom-evnut's) unless the
//GitHub Account dialog names another app. A client ID isn't secret: no client secret is ever needed.
#ifndef GITHUB_APP_CLIENT_ID
#define GITHUB_APP_CLIENT_ID "Iv23lihZx7wcgpoRCn04"
#endif
#ifndef GITHUB_APP_SLUG
#define GITHUB_APP_SLUG "savvycan"
#endif

//same key as the remote password (MainSettingsDialog): this only keeps the token from being plain text
static SimpleCrypt tokenCrypto(Q_UINT64_C(0xdeadbeefface6285));

QString GitHubSettings::token()
{
    QSettings settings;
    const QByteArray enc = settings.value("GitHub/Token").toByteArray();
    if (enc.isEmpty()) return QString();
    return tokenCrypto.decryptToString(enc);
}

bool GitHubSettings::tokenFromDeviceFlow()
{
    QSettings settings;
    return settings.value("GitHub/TokenFromDeviceFlow", false).toBool();
}

void GitHubSettings::setToken(const QString &token, bool fromDeviceFlow)
{
    QSettings settings;
    settings.setValue("GitHub/Token", tokenCrypto.encryptToByteArray(token));
    settings.setValue("GitHub/TokenFromDeviceFlow", fromDeviceFlow);
}

void GitHubSettings::clearToken()
{
    QSettings settings;
    settings.remove("GitHub/Token");
    settings.remove("GitHub/TokenFromDeviceFlow");
}

QString GitHubSettings::clientId()
{
    const QString own = ownClientId();
    return own.isEmpty() ? QString(GITHUB_APP_CLIENT_ID) : own;
}

QString GitHubSettings::ownClientId()
{
    QSettings settings;
    const QString id = settings.value("GitHub/ClientId").toString().trimmed();
    return id == GITHUB_APP_CLIENT_ID ? QString() : id;
}

void GitHubSettings::setOwnClientId(const QString &id)
{
    QSettings settings;
    if (id.trimmed().isEmpty() || id.trimmed() == GITHUB_APP_CLIENT_ID) settings.remove("GitHub/ClientId");
    else settings.setValue("GitHub/ClientId", id.trimmed());
}

bool GitHubSettings::isGitHubAppClientId(const QString &id)
{
    return id.startsWith("Iv"); //GitHub Apps' client IDs start "Iv1." or "Iv23", OAuth Apps' don't
}

QString GitHubSettings::appInstallUrl()
{
    if (!ownClientId().isEmpty()) return QString(); //another app: its page isn't known
    return webUrl() + "/apps/" + GITHUB_APP_SLUG + "/installations/new";
}

QString GitHubSettings::apiUrl()
{
    QSettings settings;
    return settings.value("GitHub/ApiUrl", "https://api.github.com").toString();
}

QString GitHubSettings::webUrl()
{
    QSettings settings;
    return settings.value("GitHub/WebUrl", "https://github.com").toString();
}

QList<GitHubRepo> GitHubSettings::repos()
{
    QSettings settings;
    QList<GitHubRepo> list;

    //until the list is first saved, it holds Tom-evnut's public collection of EV CAN logs and DBC files
    if (!settings.contains("GitHub/Repos/size") && !settings.value("GitHub/ReposSaved", false).toBool())
    {
        GitHubRepo repo;
        repo.owner = "Tom-evnut";
        repo.name = "EV-CANlogs";
        repo.branch = "main";
        list.append(repo);
        return list;
    }

    const int count = settings.beginReadArray("GitHub/Repos");
    for (int i = 0; i < count; i++)
    {
        settings.setArrayIndex(i);
        GitHubRepo repo;
        repo.owner = settings.value("Owner").toString();
        repo.name = settings.value("Name").toString();
        repo.branch = settings.value("Branch").toString();
        if (!repo.owner.isEmpty() && !repo.name.isEmpty()) list.append(repo);
    }
    settings.endArray();
    return list;
}

void GitHubSettings::setRepos(const QList<GitHubRepo> &repos)
{
    QSettings settings;
    settings.remove("GitHub/Repos");
    settings.setValue("GitHub/ReposSaved", true); //an empty list stays empty
    settings.beginWriteArray("GitHub/Repos", int(repos.count()));
    for (int i = 0; i < repos.count(); i++)
    {
        settings.setArrayIndex(i);
        settings.setValue("Owner", repos[i].owner);
        settings.setValue("Name", repos[i].name);
        settings.setValue("Branch", repos[i].branch);
    }
    settings.endArray();
}

bool GitHubSettings::parseRepo(const QString &text, QString &owner, QString &name)
{
    QString path = text.trimmed();
    bool isLink = true; //a link can go on past the repo name (a branch, a file); a plain "owner/repo" can't

    if (path.startsWith("http://", Qt::CaseInsensitive) || path.startsWith("https://", Qt::CaseInsensitive))
    {
        const QUrl url(path);
        if (url.host().compare("github.com", Qt::CaseInsensitive) != 0 &&
            url.host().compare("www.github.com", Qt::CaseInsensitive) != 0) return false;
        path = url.path();
    }
    else if (path.startsWith("github.com/", Qt::CaseInsensitive)) path = path.mid(11);
    else isLink = false;

    const QStringList parts = path.split('/', Qt::SkipEmptyParts);
    if (parts.count() < 2 || (!isLink && parts.count() != 2)) return false;

    QString repoName = parts[1];
    if (repoName.endsWith(".git", Qt::CaseInsensitive)) repoName.chop(4);

    //GitHub's rules: owners are letters, digits and single hyphens; repo names letters, digits, '.', '-' and '_'
    static const QRegularExpression ownerRx("^[A-Za-z0-9](?:[A-Za-z0-9-]{0,38})$");
    static const QRegularExpression nameRx("^[A-Za-z0-9._-]{1,100}$");
    if (!ownerRx.match(parts[0]).hasMatch() || !nameRx.match(repoName).hasMatch()) return false;
    if (repoName == "." || repoName == "..") return false;

    owner = parts[0];
    name = repoName;
    return true;
}
