#ifndef GITHUBACCOUNTDIALOG_H
#define GITHUBACCOUNTDIALOG_H

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class GitHubClient;
class GitHubDeviceFlow;

//Signing in to GitHub, either way: "Sign in with GitHub" (device flow) or a pasted personal access token.
//Also signing out and clearing the downloaded files.
class GitHubAccountDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitHubAccountDialog(QWidget *parent = nullptr);
    ~GitHubAccountDialog();

private:
    GitHubClient *client;
    GitHubDeviceFlow *deviceFlow;
    QLabel *lblStatus;
    QPushButton *btnSignOut;
    QLabel *lblDeviceHelp;
    QWidget *installRow;
    QCheckBox *cbOwnApp;
    QWidget *ownAppBox;
    QLineEdit *editClientId;
    QCheckBox *cbPrivateRepos;
    QPushButton *btnDeviceSignIn;
    QLabel *lblCode;
    QPushButton *btnOpenPage;
    QLabel *lblDeviceStatus;
    QLineEdit *editToken;
    QPushButton *btnUseToken;
    QString verificationUrl;

    void refreshStatus();
    void updateAppWidgets();
    void startDeviceSignIn();
    void useToken(const QString &token, bool fromDeviceFlow);
    void signOut();
    void clearCache();
};

#endif // GITHUBACCOUNTDIALOG_H
