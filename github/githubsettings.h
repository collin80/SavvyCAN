#ifndef GITHUBSETTINGS_H
#define GITHUBSETTINGS_H

#include <QString>
#include <QList>

//A GitHub repo the user added. One list for logs and DBC files: most repos hold both.
struct GitHubRepo
{
    QString owner;
    QString name;
    QString branch;     //the branch picked last (the repo's default branch when it was added)

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

    //"Sign in with GitHub" uses the SavvyCAN GitHub App unless the user names another app (an OAuth App or a
    //GitHub App with Device Flow turned on, for forks or organisations that want their own)
    static QString clientId();      //the app in use
    static QString ownClientId();   //the user's own app, empty for the SavvyCAN app
    static void setOwnClientId(const QString &id); //empty: back to the SavvyCAN app
    static bool isGitHubAppClientId(const QString &id);
    //where the SavvyCAN app is installed on repos; empty when the user named another app
    static QString appInstallUrl();
    static QString apiUrl();        //https://api.github.com
    static QString webUrl();        //https://github.com, for the device flow

    static QList<GitHubRepo> repos(); //Tom-evnut/EV-CANlogs until the list is first saved
    static void setRepos(const QList<GitHubRepo> &repos);

    //"owner/repo", or a github.com link to the repo or anything in it; false if it's neither
    static bool parseRepo(const QString &text, QString &owner, QString &name);
};

#endif // GITHUBSETTINGS_H
