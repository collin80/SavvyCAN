#include "githubaccountdialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include "githubcache.h"
#include "githubclient.h"
#include "githubdeviceflow.h"
#include "githubsettings.h"
#include "githubhelp.h"
#include <QShortcut>

GitHubAccountDialog::GitHubAccountDialog(QWidget *parent)
    : QDialog(parent),
      client(new GitHubClient(this)),
      deviceFlow(new GitHubDeviceFlow(this))
{
    setWindowTitle(tr("GitHub Account"));
    client->setApiUrl(GitHubSettings::apiUrl());
    deviceFlow->setWebUrl(GitHubSettings::webUrl());

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *statusRow = new QHBoxLayout;
    lblStatus = new QLabel;
    lblStatus->setWordWrap(true);
    btnSignOut = new QPushButton(tr("Sign Out"));
    statusRow->addWidget(lblStatus, 1);
    statusRow->addWidget(btnSignOut);
    layout->addLayout(statusRow);

    //Sign in with GitHub
    QGroupBox *deviceBox = new QGroupBox(tr("Sign in with GitHub"));
    QVBoxLayout *deviceLayout = new QVBoxLayout(deviceBox);
    lblDeviceHelp = new QLabel;
    lblDeviceHelp->setWordWrap(true);
    deviceLayout->addWidget(lblDeviceHelp);
    QHBoxLayout *deviceRow = new QHBoxLayout;
    btnDeviceSignIn = new QPushButton(tr("Sign in with GitHub"));
    btnDeviceSignIn->setDefault(true);
    lblCode = new QLabel;
    QFont codeFont = lblCode->font();
    codeFont.setPointSize(codeFont.pointSize() * 2);
    codeFont.setBold(true);
    lblCode->setFont(codeFont);
    lblCode->setTextInteractionFlags(Qt::TextSelectableByMouse);
    btnOpenPage = new QPushButton(tr("Open GitHub Again"));
    btnOpenPage->setVisible(false);
    deviceRow->addWidget(btnDeviceSignIn);
    deviceRow->addWidget(lblCode);
    deviceRow->addWidget(btnOpenPage);
    deviceRow->addStretch();
    deviceLayout->addLayout(deviceRow);
    lblDeviceStatus = new QLabel;
    lblDeviceStatus->setWordWrap(true);
    deviceLayout->addWidget(lblDeviceStatus);

    //private repos: the SavvyCAN app only reaches the repos it's installed on
    installRow = new QWidget;
    QHBoxLayout *installLayout = new QHBoxLayout(installRow);
    installLayout->setContentsMargins(0, 0, 0, 0);
    QPushButton *btnInstall = new QPushButton(tr("Choose Repos..."));
    QLabel *installHelp = new QLabel(tr("Private repos, yours or your organisations', need the SavvyCAN app "
                                        "installed on them: choose which on GitHub."));
    installHelp->setWordWrap(true);
    installLayout->addWidget(btnInstall);
    installLayout->addWidget(installHelp, 1);
    deviceLayout->addWidget(installRow);

    //another app, for forks or organisations that register their own
    cbOwnApp = new QCheckBox(tr("Use my own GitHub App or OAuth App"));
    cbOwnApp->setChecked(!GitHubSettings::ownClientId().isEmpty());
    deviceLayout->addWidget(cbOwnApp);
    ownAppBox = new QWidget;
    QVBoxLayout *ownAppLayout = new QVBoxLayout(ownAppBox);
    ownAppLayout->setContentsMargins(20, 0, 0, 0);
    QFormLayout *ownAppForm = new QFormLayout;
    editClientId = new QLineEdit(GitHubSettings::ownClientId());
    editClientId->setPlaceholderText(tr("The app's client ID (its Device Flow must be turned on)"));
    ownAppForm->addRow(tr("Client ID:"), editClientId);
    ownAppLayout->addLayout(ownAppForm);
    cbPrivateRepos = new QCheckBox(tr("Include private repos (an OAuth App then reaches all of them)"));
    cbPrivateRepos->setChecked(true);
    ownAppLayout->addWidget(cbPrivateRepos);
    deviceLayout->addWidget(ownAppBox);
    layout->addWidget(deviceBox);

    //Personal access token
    QGroupBox *tokenBox = new QGroupBox(tr("Or use a personal access token"));
    QVBoxLayout *tokenLayout = new QVBoxLayout(tokenBox);
    QLabel *tokenHelp = new QLabel(tr("Make a fine-grained token on GitHub (Settings > Developer settings > Personal "
                                      "access tokens) for the repos you choose, with Contents: read and write, and "
                                      "paste it here."));
    tokenHelp->setWordWrap(true);
    tokenLayout->addWidget(tokenHelp);
    QHBoxLayout *tokenRow = new QHBoxLayout;
    editToken = new QLineEdit;
    editToken->setEchoMode(QLineEdit::Password);
    editToken->setPlaceholderText("github_pat_...");
    btnUseToken = new QPushButton(tr("Use Token"));
    tokenRow->addWidget(editToken, 1);
    tokenRow->addWidget(btnUseToken);
    tokenLayout->addLayout(tokenRow);
    layout->addWidget(tokenBox);

    QLabel *storageNote = new QLabel(tr("The sign-in is kept in SavvyCAN's settings, scrambled but not securely "
                                        "encrypted: anyone with access to this computer account could recover it."));
    storageNote->setWordWrap(true);
    layout->addWidget(storageNote);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::Help);
    QPushButton *btnClearCache = buttons->addButton(tr("Clear Downloaded Files"), QDialogButtonBox::ResetRole);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::helpRequested, this, &showGitHubHelp);
    new QShortcut(QKeySequence::HelpContents, this, &showGitHubHelp);
    connect(btnClearCache, &QPushButton::clicked, this, &GitHubAccountDialog::clearCache);
    connect(btnSignOut, &QPushButton::clicked, this, &GitHubAccountDialog::signOut);
    connect(btnInstall, &QPushButton::clicked, this, []()
    {
        QDesktopServices::openUrl(QUrl(GitHubSettings::appInstallUrl()));
    });
    connect(cbOwnApp, &QCheckBox::toggled, this, [this](bool own)
    {
        GitHubSettings::setOwnClientId(own ? editClientId->text() : QString());
        updateAppWidgets();
    });
    connect(editClientId, &QLineEdit::textChanged, this, [this]()
    {
        GitHubSettings::setOwnClientId(editClientId->text());
        updateAppWidgets();
    });
    connect(btnDeviceSignIn, &QPushButton::clicked, this, &GitHubAccountDialog::startDeviceSignIn);
    connect(btnUseToken, &QPushButton::clicked, this, [this]() { useToken(editToken->text(), false); });
    connect(btnOpenPage, &QPushButton::clicked, this, [this]()
    {
        QApplication::clipboard()->setText(lblCode->text());
        QDesktopServices::openUrl(QUrl(verificationUrl));
    });

    connect(deviceFlow, &GitHubDeviceFlow::codeReady, this, [this](const QString &code, const QString &url)
    {
        //the code goes on the clipboard and GitHub's page opens: the user only pastes and approves
        verificationUrl = url;
        lblCode->setText(code);
        btnOpenPage->setVisible(true);
        QApplication::clipboard()->setText(code);
        QDesktopServices::openUrl(QUrl(url));
        lblDeviceStatus->setText(tr("The code is copied: paste it on the GitHub page that opened (%1) and approve "
                                    "SavvyCAN. Waiting for GitHub...").arg(url));
    });
    connect(deviceFlow, &GitHubDeviceFlow::signedIn, this, [this](const QString &token)
    {
        lblCode->clear();
        btnOpenPage->setVisible(false);
        btnDeviceSignIn->setEnabled(true);
        lblDeviceStatus->clear();
        useToken(token, true);
    });
    connect(deviceFlow, &GitHubDeviceFlow::failed, this, [this](const QString &error)
    {
        lblCode->clear();
        btnOpenPage->setVisible(false);
        btnDeviceSignIn->setEnabled(true);
        lblDeviceStatus->setText(error);
    });

    updateAppWidgets();
    lblStatus->setObjectName("lblStatus");
    installRow->setObjectName("installRow");
    cbOwnApp->setObjectName("cbOwnApp");
    ownAppBox->setObjectName("ownAppBox");
    editClientId->setObjectName("editClientId");
    cbPrivateRepos->setObjectName("cbPrivateRepos");

    resize(560, sizeHint().height());
    refreshStatus();
}

GitHubAccountDialog::~GitHubAccountDialog()
{
    //before this dialog's members go: their callbacks use them
    delete deviceFlow;
    delete client;
}

void GitHubAccountDialog::refreshStatus()
{
    const QString token = GitHubSettings::token();
    btnSignOut->setEnabled(!token.isEmpty());
    if (token.isEmpty())
    {
        lblStatus->setText(tr("Not signed in: public repos only, and GitHub allows 60 requests an hour."));
        return;
    }

    lblStatus->setText(tr("Checking the sign-in..."));
    client->setToken(token);
    const bool fromDevice = GitHubSettings::tokenFromDeviceFlow();
    client->getUser([this, fromDevice](const QString &login, const QString &error)
    {
        if (!error.isEmpty()) lblStatus->setText(error);
        else if (fromDevice) lblStatus->setText(tr("Signed in as %1 (Sign in with GitHub).").arg(login));
        else lblStatus->setText(tr("Signed in as %1 (personal access token).").arg(login));
    });
}

//what the sign-in reaches depends on the app: the SavvyCAN app, or the user's own
void GitHubAccountDialog::updateAppWidgets()
{
    const bool own = cbOwnApp->isChecked();
    ownAppBox->setVisible(own);
    //GitHub Apps reach the repos they're installed on and ignore scopes; OAuth Apps go by scope
    cbPrivateRepos->setVisible(own && !GitHubSettings::isGitHubAppClientId(editClientId->text().trimmed()));
    installRow->setVisible(!GitHubSettings::appInstallUrl().isEmpty());

    if (GitHubSettings::ownClientId().isEmpty())
        lblDeviceHelp->setText(tr("Click Sign in with GitHub: SavvyCAN copies a code and opens GitHub, where you paste "
                                  "it and approve. SavvyCAN can then use public repos and the repos the SavvyCAN "
                                  "app is installed on."));
    else
        lblDeviceHelp->setText(tr("Click Sign in with GitHub: SavvyCAN copies a code and opens GitHub, where you paste "
                                  "it and approve. SavvyCAN can then use the repos your app allows."));
}

void GitHubAccountDialog::startDeviceSignIn()
{
    if (cbOwnApp->isChecked() && editClientId->text().trimmed().isEmpty())
    {
        lblDeviceStatus->setText(tr("Enter your app's client ID, or untick \"Use my own GitHub App or OAuth App\"."));
        return;
    }
    const QString clientId = GitHubSettings::clientId();
    btnDeviceSignIn->setEnabled(false);
    lblDeviceStatus->setText(tr("Asking GitHub for a code..."));
    deviceFlow->start(clientId, cbPrivateRepos->isChecked() ? "repo" : "public_repo");
}

//keep a token only once GitHub has accepted it
void GitHubAccountDialog::useToken(const QString &token, bool fromDeviceFlow)
{
    if (token.trimmed().isEmpty()) return;
    lblStatus->setText(tr("Checking the sign-in..."));
    btnUseToken->setEnabled(false);
    client->setToken(token);
    client->getUser([this, token, fromDeviceFlow](const QString &, const QString &error)
    {
        btnUseToken->setEnabled(true);
        if (!error.isEmpty())
        {
            lblStatus->setText(tr("Not signed in: %1").arg(error));
            return;
        }
        GitHubSettings::setToken(token.trimmed(), fromDeviceFlow);
        editToken->clear();
        refreshStatus();
    });
}

void GitHubAccountDialog::signOut()
{
    deviceFlow->cancel();
    GitHubSettings::clearToken();
    client->setToken(QString());
    refreshStatus();
}

void GitHubAccountDialog::clearCache()
{
    if (QMessageBox::question(this, tr("Clear Downloaded Files"),
                              tr("Delete the copies of files downloaded from GitHub? The files on GitHub stay as "
                                 "they are; DBC files loaded from GitHub won't load at the next start until "
                                 "they're loaded again.")) != QMessageBox::Yes) return;
    if (!GitHubCache::clear())
        QMessageBox::warning(this, tr("Clear Downloaded Files"), tr("Some files couldn't be deleted (are they open?)."));
}
