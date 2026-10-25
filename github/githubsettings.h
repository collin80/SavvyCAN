#ifndef GITHUBSETTINGS_H
#define GITHUBSETTINGS_H

#include <QString>
#include <QList>

//A GitHub repo the user added, for logs, DBC files or both
struct GitHubRepo
{
    QString owner;
    QString name;
    QString branch;     //the branch picked last (the repo's default branch when it was added)
    bool logs = true;
    bool dbcs = true;

    QString fullName() const { return owner + "/" + name; }
};

//GitHub settings kept in QSettings under GitHub/: the sign-in token, the OAuth App used for "Sign in with GitHub",
//the URLs (github.com unless changed, which the unit tests do) and the saved repos.
class GitHubSettings
{
public:
    //The token is stored lightly scrambled (SimpleCrypt, as the remote password is): that only hides it from a
    //casual look at the settings.
    static QString token();
    static bool tokenFromDeviceFlow(); //true: "Sign in with GitHub", false: a pasted token
    static void setToken(const QString &token, bool fromDeviceFlow);
    static void clearToken();

    static QString clientId();      //the OAuth App's client ID ("Sign in with GitHub" is off without one)
    static void setClientId(const QString &id);
    static QString apiUrl();        //https://api.github.com
    static QString webUrl();        //https://github.com, for the device flow

    static QList<GitHubRepo> repos();
    static void setRepos(const QList<GitHubRepo> &repos);

    //"owner/repo", or a github.com link to the repo or anything in it; false if it's neither
    static bool parseRepo(const QString &text, QString &owner, QString &name);
};

#endif // GITHUBSETTINGS_H
