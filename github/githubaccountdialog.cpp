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
    QLabel *deviceHelp = new QLabel(tr("SavvyCAN shows a code; enter it on GitHub's page to let SavvyCAN read and "
                                       "write your repos. This needs the client ID of an OAuth App with Device "
                                       "Flow turned on (GitHub > Settings > Developer settings > OAuth Apps)."));
    deviceHelp->setWordWrap(true);
    deviceLayout->addWidget(deviceHelp);
    QFormLayout *deviceForm = new QFormLayout;
    editClientId = new QLineEdit(GitHubSettings::clientId());
    deviceForm->addRow(tr("OAuth App client ID:"), editClientId);
    deviceLayout->addLayout(deviceForm);
    cbPrivateRepos = new QCheckBox(tr("Include private repos (this gives access to all of them)"));
    cbPrivateRepos->setChecked(true);
    deviceLayout->addWidget(cbPrivateRepos);
    QHBoxLayout *deviceRow = new QHBoxLayout;
    btnDeviceSignIn = new QPushButton(tr("Sign in with GitHub"));
    lblCode = new QLabel;
    QFont codeFont = lblCode->font();
    codeFont.setPointSize(codeFont.pointSize() * 2);
    codeFont.setBold(true);
    lblCode->setFont(codeFont);
    lblCode->setTextInteractionFlags(Qt::TextSelectableByMouse);
    btnOpenPage = new QPushButton(tr("Copy Code and Open GitHub"));
    btnOpenPage->setVisible(false);
    deviceRow->addWidget(btnDeviceSignIn);
    deviceRow->addWidget(lblCode);
    deviceRow->addWidget(btnOpenPage);
    deviceRow->addStretch();
    deviceLayout->addLayout(deviceRow);
    lblDeviceStatus = new QLabel;
    lblDeviceStatus->setWordWrap(true);
    deviceLayout->addWidget(lblDeviceStatus);
    layout->addWidget(deviceBox);

    //Personal access token
    QGroupBox *tokenBox = new QGroupBox(tr("Personal access token"));
    QVBoxLayout *tokenLayout = new QVBoxLayout(tokenBox);
    QLabel *tokenHelp = new QLabel(tr("To give access to chosen repos only, make a fine-grained token on GitHub "
                                      "(Settings > Developer settings > Personal access tokens) with Contents: "
                                      "read and write, and paste it here."));
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

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    QPushButton *btnClearCache = buttons->addButton(tr("Clear Downloaded Files"), QDialogButtonBox::ResetRole);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(btnClearCache, &QPushButton::clicked, this, &GitHubAccountDialog::clearCache);
    connect(btnSignOut, &QPushButton::clicked, this, &GitHubAccountDialog::signOut);
    connect(editClientId, &QLineEdit::editingFinished, this, [this]() { GitHubSettings::setClientId(editClientId->text()); });
    connect(btnDeviceSignIn, &QPushButton::clicked, this, &GitHubAccountDialog::startDeviceSignIn);
    connect(btnUseToken, &QPushButton::clicked, this, [this]() { useToken(editToken->text(), false); });
    connect(btnOpenPage, &QPushButton::clicked, this, [this]()
    {
        QApplication::clipboard()->setText(lblCode->text());
        QDesktopServices::openUrl(QUrl(verificationUrl));
    });

    connect(deviceFlow, &GitHubDeviceFlow::codeReady, this, [this](const QString &code, const QString &url)
    {
        verificationUrl = url;
        lblCode->setText(code);
        btnOpenPage->setVisible(true);
        lblDeviceStatus->setText(tr("Enter this code at %1, then approve SavvyCAN. Waiting for GitHub...").arg(url));
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

    lblStatus->setObjectName("lblStatus");

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

void GitHubAccountDialog::startDeviceSignIn()
{
    GitHubSettings::setClientId(editClientId->text());
    const QString clientId = GitHubSettings::clientId();
    if (clientId.isEmpty())
    {
        lblDeviceStatus->setText(tr("Enter the OAuth App's client ID first."));
        return;
    }
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
