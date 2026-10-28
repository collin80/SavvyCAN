#include "githubbrowserdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QNetworkReply>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSet>
#include <QSettings>
#include <QStyle>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QShortcut>
#include "githubaccountdialog.h"
#include "githubhelp.h"

//GitHub's API sends and takes files up to 100 MB; bigger ones need Git LFS, which isn't supported yet.
//It warns about files over 50 MB.
static const qint64 maxFileSize = 100 * 1024 * 1024;
static const qint64 largeFileSize = 50 * 1024 * 1024;

//runs an asynchronous call and waits for it, the event loop still running (start calls quit() when it's done)
static void waitFor(const std::function<void(std::function<void()> quit)> &start)
{
    QEventLoop loop;
    bool done = false;
    start([&loop, &done]()
    {
        done = true;
        loop.quit();
    });
    if (!done) loop.exec();
}

GitHubBrowserDialog::GitHubBrowserDialog(Kind kind, Mode mode, const QStringList &fileTypes, QWidget *parent)
    : QDialog(parent),
      kind(kind),
      mode(mode),
      client(new GitHubClient(this)),
      generation(0)
{
    if (mode == Open) setWindowTitle(kind == Logs ? tr("Load Log File from GitHub") : tr("Load DBC File from GitHub"));
    else setWindowTitle(kind == Logs ? tr("Save Log File to GitHub") : tr("Save DBC File to GitHub"));
    client->setApiUrl(GitHubSettings::apiUrl());
    client->setToken(GitHubSettings::token());
    ask = [this](QMessageBox::Icon icon, const QString &text, QMessageBox::StandardButtons buttons,
                 const QMap<QMessageBox::StandardButton, QString> &labels)
    {
        QMessageBox box(icon, windowTitle(), text, buttons, this);
        for (auto it = labels.constBegin(); it != labels.constEnd(); ++it)
            if (box.button(it.key())) box.button(it.key())->setText(it.value());
        if (text.contains("<a href")) box.setTextInteractionFlags(Qt::TextBrowserInteraction);
        return QMessageBox::StandardButton(box.exec());
    };

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *accountRow = new QHBoxLayout;
    lblAccount = new QLabel;
    btnAccount = new QPushButton;
    accountRow->addWidget(lblAccount, 1);
    accountRow->addWidget(btnAccount);
    layout->addLayout(accountRow);

    QFormLayout *form = new QFormLayout;
    QHBoxLayout *repoRow = new QHBoxLayout;
    comboRepo = new QComboBox;
    comboRepo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    btnAddRepo = new QPushButton(tr("Add..."));
    btnNewRepo = new QPushButton(tr("New Repo..."));
    btnNewRepo->setVisible(mode == Save);
    btnRemoveRepo = new QPushButton(tr("Remove"));
    repoRow->addWidget(comboRepo, 1);
    repoRow->addWidget(btnAddRepo);
    repoRow->addWidget(btnNewRepo);
    repoRow->addWidget(btnRemoveRepo);
    form->addRow(tr("Repo:"), repoRow);
    comboBranch = new QComboBox;
    form->addRow(tr("Branch:"), comboBranch);
    comboType = new QComboBox;
    comboType->addItems(fileTypes);
    form->addRow(tr("File type:"), comboType);
    editSearch = new QLineEdit;
    editSearch->setPlaceholderText(tr("Part of a file or folder name"));
    editSearch->setClearButtonEnabled(true);
    form->addRow(tr("Search:"), editSearch);
    layout->addLayout(form);

    tree = new QTreeWidget;
    tree->setHeaderLabels({ tr("Name"), tr("Size") });
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setStretchLastSection(false);
    tree->setSortingEnabled(true);
    tree->sortByColumn(0, Qt::AscendingOrder);
    layout->addWidget(tree, 1);

    //Save: where in the repo, and the commit
    QFormLayout *saveForm = new QFormLayout;
    editFolder = new QLineEdit;
    editFolder->setPlaceholderText(tr("(the top of the repo)"));
    saveForm->addRow(tr("Folder:"), editFolder);
    editName = new QLineEdit;
    saveForm->addRow(tr("File name:"), editName);
    editMessage = new QLineEdit;
    editMessage->setPlaceholderText(tr("\"Add <file> from SavvyCAN\" or \"Update <file> from SavvyCAN\""));
    saveForm->addRow(tr("Commit message:"), editMessage);
    cbNewBranch = new QCheckBox(tr("Save to a new branch and open a pull request"));
    saveForm->addRow(QString(), cbNewBranch);
    newBranchBox = new QWidget;
    QFormLayout *newBranchForm = new QFormLayout(newBranchBox);
    newBranchForm->setContentsMargins(20, 0, 0, 0);
    editBranchName = new QLineEdit;
    newBranchForm->addRow(tr("New branch:"), editBranchName);
    editPrTitle = new QLineEdit;
    editPrTitle->setPlaceholderText(tr("The commit message"));
    newBranchForm->addRow(tr("Pull request title:"), editPrTitle);
    newBranchBox->setVisible(false);
    saveForm->addRow(newBranchBox);
    QWidget *saveBox = new QWidget;
    saveForm->setContentsMargins(0, 0, 0, 0);
    saveBox->setLayout(saveForm);
    saveBox->setVisible(mode == Save);
    layout->addWidget(saveBox);

    //Save: a warning when the GitHub App signed in with isn't installed on the repo picked
    lblAppHint = new QLabel;
    lblAppHint->setWordWrap(true);
    lblAppHint->setTextFormat(Qt::RichText);
    lblAppHint->setVisible(false);
    layout->addWidget(lblAppHint);
    connect(lblAppHint, &QLabel::linkActivated, this, &GitHubBrowserDialog::openInstallPage);

    lblStatus = new QLabel;
    lblStatus->setOpenExternalLinks(true); //the link to install the SavvyCAN app (showError)
    lblStatus->setWordWrap(true);
    layout->addWidget(lblStatus);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Help);
    btnOpen = buttons->addButton(mode == Open ? tr("Open") : tr("Save"), QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);

    QSettings settings;
    const int lastType = settings.value(settingsKey() + "Type", 0).toInt();
    if (lastType >= 0 && lastType < comboType->count()) comboType->setCurrentIndex(lastType);

    connect(btnAccount, &QPushButton::clicked, this, &GitHubBrowserDialog::showAccount);
    connect(btnAddRepo, &QPushButton::clicked, this, &GitHubBrowserDialog::addRepo);
    connect(btnNewRepo, &QPushButton::clicked, this, &GitHubBrowserDialog::newRepo);
    connect(btnRemoveRepo, &QPushButton::clicked, this, &GitHubBrowserDialog::removeRepo);
    connect(comboRepo, &QComboBox::currentIndexChanged, this, &GitHubBrowserDialog::repoChanged);
    connect(comboBranch, &QComboBox::currentIndexChanged, this, &GitHubBrowserDialog::branchChanged);
    connect(comboType, &QComboBox::currentIndexChanged, this, &GitHubBrowserDialog::fillTree);
    connect(editSearch, &QLineEdit::textChanged, this, &GitHubBrowserDialog::fillTree);
    connect(editName, &QLineEdit::textChanged, this, &GitHubBrowserDialog::updateOpenButton);
    //Save: a name keeps up with the file type picked ("run.csv" becomes "run.asc")
    connect(comboType, &QComboBox::currentIndexChanged, this, [this]()
    {
        const QString name = editName->text().trimmed();
        const QString ext = extensionFor(comboType->currentText());
        if (this->mode != Save || name.isEmpty() || ext.isEmpty() || matchesFileType(name, comboType->currentText()))
            return;
        const QFileInfo info(name);
        editName->setText((info.suffix().isEmpty() ? name : name.left(name.length() - info.suffix().length() - 1)) + ext);
    });
    connect(cbNewBranch, &QCheckBox::toggled, this, [this](bool on)
    {
        newBranchBox->setVisible(on);
        if (on && editBranchName->text().isEmpty())
        {
            //savvycan/<file name>-<when>: unique enough, and says where it came from
            QString base = QFileInfo(editName->text().trimmed()).completeBaseName();
            base.replace(QRegularExpression("[^A-Za-z0-9._-]+"), "-");
            if (base.isEmpty()) base = this->kind == Logs ? "log" : "dbc";
            editBranchName->setText("savvycan/" + base + "-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmm"));
        }
    });
    connect(tree, &QTreeWidget::itemSelectionChanged, this, &GitHubBrowserDialog::updateOpenButton);
    connect(tree, &QTreeWidget::itemClicked, this, &GitHubBrowserDialog::itemClicked);
    connect(tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item)
    {
        if (this->mode == Open && item->data(0, Qt::UserRole).toInt() >= 0) openSelected();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, [this]()
    {
        if (this->mode == Open) openSelected();
        else saveSelected();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::helpRequested, this, &showGitHubHelp);
    new QShortcut(QKeySequence::HelpContents, this, &showGitHubHelp);

    comboRepo->setObjectName("comboRepo");
    comboBranch->setObjectName("comboBranch");
    comboType->setObjectName("comboType");
    editSearch->setObjectName("editSearch");
    tree->setObjectName("tree");
    editFolder->setObjectName("editFolder");
    editName->setObjectName("editName");
    editMessage->setObjectName("editMessage");
    cbNewBranch->setObjectName("cbNewBranch");
    editBranchName->setObjectName("editBranchName");
    editPrTitle->setObjectName("editPrTitle");
    lblStatus->setObjectName("lblStatus");
    btnOpen->setObjectName("btnOpen");
    btnAccount->setObjectName("btnAccount");
    btnNewRepo->setObjectName("btnNewRepo");
    lblAppHint->setObjectName("lblAppHint");

    resize(640, mode == Open ? 560 : 680);
    updateOpenButton();
    refreshAccount();
    fillRepos(settings.value(settingsKey() + "Repo").toString());
}

GitHubBrowserDialog::~GitHubBrowserDialog()
{
    //before this dialog's members go: the client's callbacks use them
    delete client;
}

int GitHubBrowserDialog::fileTypeIndex() const
{
    return comboType->currentIndex();
}

QString GitHubBrowserDialog::settingsKey() const
{
    if (mode == Save) return kind == Logs ? "GitHub/LastLogsSave" : "GitHub/LastDBCsSave";
    return kind == Logs ? "GitHub/LastLogs" : "GitHub/LastDBCs";
}

QMessageBox::StandardButton GitHubBrowserDialog::askUser(QMessageBox::Icon icon, const QString &text,
                                                         QMessageBox::StandardButtons buttons,
                                                         const QMap<QMessageBox::StandardButton, QString> &labels)
{
    return ask(icon, text, buttons, labels);
}

//the patterns between a name filter's brackets; a missing ')' is tolerated
static QStringList filterPatterns(const QString &nameFilter)
{
    const int open = int(nameFilter.indexOf('('));
    if (open < 0) return QStringList();
    int close = int(nameFilter.indexOf(')', open));
    if (close < 0) close = int(nameFilter.length());
    return nameFilter.mid(open + 1, close - open - 1).split(' ', Qt::SkipEmptyParts);
}

bool GitHubBrowserDialog::matchesFileType(const QString &fileName, const QString &nameFilter)
{
    const QStringList patterns = filterPatterns(nameFilter);
    if (patterns.isEmpty()) return true;

    for (const QString &pattern : patterns)
    {
        if (pattern == "*" || pattern == "*.*") return true;
        const QRegularExpression rx(QRegularExpression::wildcardToRegularExpression(pattern),
                                    QRegularExpression::CaseInsensitiveOption);
        if (rx.match(fileName).hasMatch()) return true;
    }
    return false;
}

QString GitHubBrowserDialog::extensionFor(const QString &nameFilter)
{
    const QStringList patterns = filterPatterns(nameFilter);
    if (patterns.isEmpty() || !patterns[0].startsWith("*.") || patterns[0] == "*.*") return QString();
    return patterns[0].mid(1);
}

void GitHubBrowserDialog::refreshAccount()
{
    client->setToken(GitHubSettings::token());
    accountLogin.clear();
    btnNewRepo->setEnabled(false);
    if (GitHubSettings::token().isEmpty())
    {
        lblAccount->setText(mode == Open ? tr("Not signed in: public repos only.")
                                         : tr("Not signed in: sign in to save to GitHub."));
        btnAccount->setText(tr("Sign In..."));
        return;
    }
    btnAccount->setText(tr("GitHub Account..."));
    lblAccount->setText(tr("Checking the sign-in..."));
    client->getUser([this](const QString &login, const QString &error)
    {
        lblAccount->setText(error.isEmpty() ? tr("Signed in as %1.").arg(login) : error);
        accountLogin = login;
        btnNewRepo->setEnabled(!login.isEmpty());
    });
}

//an error in the status line; a repo GitHub won't show may only need signing in, or the SavvyCAN app installing
void GitHubBrowserDialog::showError(const QString &error)
{
    QString text = error.toHtmlEscaped();
    if (error.endsWith(GitHubClient::notFoundText()))
    {
        if (GitHubSettings::token().isEmpty())
            text += " " + tr("If it's a private repo, sign in first.").toHtmlEscaped();
        else if (GitHubSettings::tokenFromDeviceFlow() && !GitHubSettings::appInstallUrl().isEmpty())
            text += " " + tr("If it's a private repo, <a href=\"%1\">install the SavvyCAN app on it</a>.")
                    .arg(GitHubSettings::appInstallUrl());
    }
    lblStatus->setTextFormat(Qt::RichText);
    lblStatus->setText(text);
}

void GitHubBrowserDialog::setStatus(const QString &text)
{
    lblStatus->setTextFormat(Qt::PlainText);
    lblStatus->setText(text);
}

void GitHubBrowserDialog::fillRepos(const QString &select)
{
    savedRepos = GitHubSettings::repos();

    QSignalBlocker block(comboRepo);
    comboRepo->clear();
    int selected = 0;
    for (int i = 0; i < savedRepos.count(); i++)
    {
        comboRepo->addItem(savedRepos[i].fullName());
        if (savedRepos[i].fullName().compare(select, Qt::CaseInsensitive) == 0) selected = i;
    }
    comboRepo->setCurrentIndex(savedRepos.isEmpty() ? -1 : selected);
    btnRemoveRepo->setEnabled(!savedRepos.isEmpty());
    repoChanged();
}

void GitHubBrowserDialog::repoChanged()
{
    const int gen = ++generation;
    branches.clear();
    entries.clear();
    {
        QSignalBlocker block(comboBranch);
        comboBranch->clear();
    }
    fillTree();

    const int idx = comboRepo->currentIndex();
    if (idx < 0 || idx >= savedRepos.count())
    {
        lblAppHint->setVisible(false);
        setStatus(tr("Add a repo with Add... (\"owner/repo\" or a github.com link)."));
        return;
    }
    const GitHubRepo repo = savedRepos[idx];
    QSettings().setValue(settingsKey() + "Repo", repo.fullName());
    if (mode == Save) QTimer::singleShot(0, this, &GitHubBrowserDialog::updateAppHint);

    setStatus(tr("Reading the branches of %1...").arg(repo.fullName()));
    client->listBranches(repo.owner, repo.name, [this, gen, repo](const QList<GitHubBranch> &list, const QString &error)
    {
        if (gen != generation) return;
        if (!error.isEmpty())
        {
            showError(error);
            return;
        }
        if (list.isEmpty())
        {
            setStatus(tr("%1 has no branches yet.").arg(repo.fullName()));
            return;
        }
        branches = list;
        const QString wanted = pendingBranch.isEmpty() ? repo.branch : pendingBranch;
        pendingBranch.clear();
        QSignalBlocker block(comboBranch);
        int selected = 0;
        for (int i = 0; i < branches.count(); i++)
        {
            comboBranch->addItem(branches[i].name);
            if (branches[i].name == wanted) selected = i;
        }
        comboBranch->setCurrentIndex(selected);
        block.unblock();
        branchChanged();
    });
}

void GitHubBrowserDialog::branchChanged()
{
    const int gen = ++generation;
    entries.clear();
    fillTree();

    const int repoIdx = comboRepo->currentIndex();
    const int branchIdx = comboBranch->currentIndex();
    if (repoIdx < 0 || repoIdx >= savedRepos.count() || branchIdx < 0 || branchIdx >= branches.count()) return;
    const GitHubRepo repo = savedRepos[repoIdx];
    const GitHubBranch branch = branches[branchIdx];
    rememberBranch(branch.name);

    setStatus(tr("Reading the files in %1...").arg(branch.name));
    client->getTree(repo.owner, repo.name, branch.commitSha,
                    [this, gen](const QList<GitHubEntry> &list, bool truncated, const QString &error)
    {
        if (gen != generation) return;
        if (!error.isEmpty())
        {
            showError(error);
            return;
        }
        entries = list;
        fillTree();
        if (entries.isEmpty()) setStatus(tr("This branch has no files."));
        if (truncated)
            setStatus(lblStatus->text() + " " + tr("GitHub sent only part of this repo's file list: "
                                                            "it's too large to list completely."));
    });
}

void GitHubBrowserDialog::rememberBranch(const QString &branch)
{
    const int idx = comboRepo->currentIndex();
    if (idx < 0 || idx >= savedRepos.count() || savedRepos[idx].branch == branch) return;
    savedRepos[idx].branch = branch;

    QList<GitHubRepo> all = GitHubSettings::repos();
    for (GitHubRepo &repo : all)
        if (repo.fullName().compare(savedRepos[idx].fullName(), Qt::CaseInsensitive) == 0) repo.branch = branch;
    GitHubSettings::setRepos(all);
}

void GitHubBrowserDialog::setCompanionSuffixes(const QStringList &suffixes)
{
    companions = suffixes;
    fillTree();
}

int GitHubBrowserDialog::entryIndexOf(const QString &path) const
{
    for (int i = 0; i < entries.count(); i++)
        if (!entries[i].isFolder && entries[i].path == path) return i;
    return -1;
}

void GitHubBrowserDialog::fillTree()
{
    QSettings().setValue(settingsKey() + "Type", comboType->currentIndex());

    tree->setSortingEnabled(false);
    tree->clear();
    const QString type = comboType->currentText();
    const QString search = editSearch->text().trimmed();

    //each item keeps its path (UserRole + 1); files also their index in entries (UserRole), folders -1
    QHash<QString, QTreeWidgetItem *> folders;
    std::function<QTreeWidgetItem *(const QString &)> folderItem = [&](const QString &path) -> QTreeWidgetItem *
    {
        if (path.isEmpty()) return tree->invisibleRootItem();
        if (folders.contains(path)) return folders[path];
        const int slash = int(path.lastIndexOf('/'));
        QTreeWidgetItem *parent = folderItem(slash < 0 ? QString() : path.left(slash));
        QTreeWidgetItem *item = new QTreeWidgetItem(parent, { path.mid(slash + 1) });
        item->setData(0, Qt::UserRole, -1);
        item->setData(0, Qt::UserRole + 1, path);
        item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        folders[path] = item;
        return item;
    };

    QSet<QString> files;
    for (const GitHubEntry &entry : std::as_const(entries))
        if (!entry.isFolder) files.insert(entry.path);

    int shown = 0;
    for (int i = 0; i < entries.count(); i++)
    {
        const GitHubEntry &entry = entries[i];
        if (!search.isEmpty() && !entry.path.contains(search, Qt::CaseInsensitive)) continue;
        //saving can go into any folder, even one without files of this type
        if (entry.isFolder)
        {
            if (mode == Save) folderItem(entry.path);
            continue;
        }
        //a companion file goes with its file, so it isn't listed on its own (unless that file isn't there)
        bool isCompanion = false;
        QStringList withIt;
        for (const QString &suffix : std::as_const(companions))
        {
            if (entry.path.endsWith(suffix) && files.contains(entry.path.chopped(suffix.length()))) isCompanion = true;
            if (files.contains(entry.path + suffix)) withIt.append(QFileInfo(entry.path + suffix).fileName());
        }
        if (isCompanion) continue;
        const int slash = int(entry.path.lastIndexOf('/'));
        const QString name = entry.path.mid(slash + 1);
        if (!matchesFileType(name, type)) continue;

        QTreeWidgetItem *item = new QTreeWidgetItem(folderItem(slash < 0 ? QString() : entry.path.left(slash)),
                                                    { name, QLocale().formattedDataSize(entry.size) });
        item->setData(0, Qt::UserRole, i);
        item->setData(0, Qt::UserRole + 1, entry.path);
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        if (withIt.isEmpty()) item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        else
        {
            item->setIcon(0, style()->standardIcon(QStyle::SP_FileDialogDetailedView));
            item->setToolTip(0, tr("With %1").arg(withIt.join(", ")));
            item->setData(0, Qt::UserRole + 2, withIt);
        }
        shown++;
    }

    tree->setSortingEnabled(true);
    if (!search.isEmpty() || shown < 200) tree->expandAll();
    tree->resizeColumnToContents(1);

    if (comboBranch->count() > 0 && !entries.isEmpty())
    {
        if (shown == 0) setStatus(tr("No matching files."));
        else if (shown == 1) setStatus(tr("1 matching file."));
        else setStatus(tr("%1 matching files.").arg(shown));
    }
    updateOpenButton();
}

void GitHubBrowserDialog::updateOpenButton()
{
    if (mode == Save)
    {
        btnOpen->setEnabled(writer && !editName->text().trimmed().isEmpty() && comboRepo->currentIndex() >= 0 &&
                            comboBranch->currentIndex() >= 0 && !GitHubSettings::token().isEmpty());
        return;
    }
    const QList<QTreeWidgetItem *> selected = tree->selectedItems();
    btnOpen->setEnabled(selected.count() == 1 && selected[0]->data(0, Qt::UserRole).toInt() >= 0);
}

//Save: a folder picks where to save, a file also its name (to replace it)
void GitHubBrowserDialog::itemClicked(QTreeWidgetItem *item)
{
    if (mode != Save || !item) return;
    const QString path = item->data(0, Qt::UserRole + 1).toString();
    if (item->data(0, Qt::UserRole).toInt() < 0)
    {
        editFolder->setText(path);
        return;
    }
    const int slash = int(path.lastIndexOf('/'));
    editFolder->setText(slash < 0 ? QString() : path.left(slash));
    editName->setText(path.mid(slash + 1));
}

//a file type that fits the name, keeping the one picked last if it does; if none does, the name gets the extension
//of the type picked last (the type change handler does that)
void GitHubBrowserDialog::selectTypeFor(const QString &fileName)
{
    if (matchesFileType(fileName, comboType->currentText()) && !extensionFor(comboType->currentText()).isEmpty())
        return;
    for (int i = 0; i < comboType->count(); i++)
    {
        if (!extensionFor(comboType->itemText(i)).isEmpty() && matchesFileType(fileName, comboType->itemText(i)))
        {
            comboType->setCurrentIndex(i);
            return;
        }
    }
    emit comboType->currentIndexChanged(comboType->currentIndex());
}

void GitHubBrowserDialog::setSuggestion(const GitHubOrigin &from, const QString &fileName)
{
    suggestion = from;
    if (from.isValid())
    {
        //its repo, added to the list if it isn't there, and its branch
        QList<GitHubRepo> all = GitHubSettings::repos();
        bool found = false;
        for (const GitHubRepo &repo : std::as_const(all))
            if (repo.fullName().compare(from.owner + "/" + from.repo, Qt::CaseInsensitive) == 0) found = true;
        if (!found)
        {
            GitHubRepo repo;
            repo.owner = from.owner;
            repo.name = from.repo;
            repo.branch = from.branch;
            all.append(repo);
            GitHubSettings::setRepos(all);
        }
        pendingBranch = from.branch;
        const int slash = int(from.path.lastIndexOf('/'));
        editFolder->setText(slash < 0 ? QString() : from.path.left(slash));
        editName->setText(from.path.mid(slash + 1));
        fillRepos(from.owner + "/" + from.repo);
    }
    else editName->setText(fileName);
    selectTypeFor(editName->text());
}

void GitHubBrowserDialog::addRepo()
{
    bool ok = false;
    const QString text = QInputDialog::getText(this, tr("Add GitHub Repo"),
                                               tr("Repo (\"owner/repo\" or a github.com link):"),
                                               QLineEdit::Normal, QString(), &ok);
    if (!ok || text.trimmed().isEmpty()) return;

    QString owner, name;
    if (!GitHubSettings::parseRepo(text, owner, name))
    {
        askUser(QMessageBox::Warning, tr("\"%1\" isn't a GitHub repo name or link.").arg(text), QMessageBox::Ok);
        return;
    }

    btnAddRepo->setEnabled(false);
    setStatus(tr("Checking %1/%2...").arg(owner, name));
    client->getRepo(owner, name, [this, owner, name](const QString &defaultBranch, const QString &error)
    {
        btnAddRepo->setEnabled(true);
        if (!error.isEmpty())
        {
            showError(owner + "/" + name + ": " + error);
            return;
        }

        QList<GitHubRepo> all = GitHubSettings::repos();
        bool found = false;
        for (const GitHubRepo &repo : std::as_const(all))
            if (repo.fullName().compare(owner + "/" + name, Qt::CaseInsensitive) == 0) found = true;
        if (!found)
        {
            GitHubRepo repo;
            repo.owner = owner;
            repo.name = name;
            repo.branch = defaultBranch;
            all.append(repo);
            GitHubSettings::setRepos(all);
        }
        fillRepos(owner + "/" + name);
    });
}

//only from the list (for logs and DBC files alike): nothing changes on GitHub
void GitHubBrowserDialog::removeRepo()
{
    const int idx = comboRepo->currentIndex();
    if (idx < 0 || idx >= savedRepos.count()) return;
    const QString fullName = savedRepos[idx].fullName();

    if (askUser(QMessageBox::Question,
                tr("Take %1 off the list of repos, for logs and DBC files? Nothing changes on GitHub.").arg(fullName),
                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;

    QList<GitHubRepo> all = GitHubSettings::repos();
    for (int i = int(all.count()) - 1; i >= 0; i--)
        if (all[i].fullName().compare(fullName, Qt::CaseInsensitive) == 0) all.removeAt(i);
    GitHubSettings::setRepos(all);
    fillRepos(QString());
}

void GitHubBrowserDialog::newRepo()
{
    if (accountLogin.isEmpty()) return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("New GitHub Repo"));
    QFormLayout *form = new QFormLayout(&dialog);
    QComboBox *comboOwner = new QComboBox;
    comboOwner->addItem(accountLogin);
    QLineEdit *editRepoName = new QLineEdit;
    editRepoName->setValidator(new QRegularExpressionValidator(QRegularExpression("[A-Za-z0-9._-]{1,100}"), editRepoName));
    QLineEdit *editDescription = new QLineEdit;
    QCheckBox *cbPrivate = new QCheckBox(tr("Private: only you, and the people you choose, can see it"));
    cbPrivate->setChecked(true);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Help);
    QPushButton *btnCreate = buttons->addButton(tr("Create"), QDialogButtonBox::AcceptRole);
    btnCreate->setEnabled(false);
    form->addRow(tr("Owner:"), comboOwner);
    form->addRow(tr("Name:"), editRepoName);
    form->addRow(tr("Description:"), editDescription);
    form->addRow(QString(), cbPrivate);
    form->addRow(buttons);
    connect(editRepoName, &QLineEdit::textChanged, &dialog, [btnCreate](const QString &text)
    {
        btnCreate->setEnabled(!text.isEmpty() && text != "." && text != "..");
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::helpRequested, &dialog, &showGitHubHelp);
    //the account's organisations, when GitHub says (a GitHub App only sees those it's installed on)
    QPointer<QComboBox> owners(comboOwner);
    client->listOrgs([owners](const QStringList &orgs, const QString &)
    {
        if (owners) owners->addItems(orgs);
    });
    dialog.resize(480, dialog.sizeHint().height());

    if (dialog.exec() != QDialog::Accepted) return;
    createRepoNow(comboOwner->currentText(), editRepoName->text(), editDescription->text().trimmed(),
                  cbPrivate->isChecked());
}

bool GitHubBrowserDialog::createRepoNow(const QString &owner, const QString &name, const QString &description,
                                        bool isPrivate)
{
    //signed in through a GitHub App, it creates repos where it's installed on all the owner's repos
    if (checksAppInstall())
    {
        forgetAppAccess();
        if (readInstallations())
        {
            bool allRepos = false;
            for (const GitHubInstallation &installation : std::as_const(installations))
                if (installation.account.compare(owner, Qt::CaseInsensitive) == 0 && installation.allRepos) allRepos = true;
            if (!allRepos)
            {
                const bool canInstall = !GitHubSettings::appInstallUrl().isEmpty();
                const QMessageBox::StandardButton choice = askUser(QMessageBox::Warning,
                    tr("The %1 isn't installed on all of %2's repos, so GitHub may refuse to create the repo, or "
                       "SavvyCAN may not be able to save into it.\n\nChoose Repos... opens GitHub's page, where "
                       "\"All repositories\" lets it create repos.")
                    .arg(canInstall ? tr("SavvyCAN app") : tr("app you signed in with"), owner),
                    canInstall ? QMessageBox::Yes | QMessageBox::Ignore | QMessageBox::Cancel
                               : QMessageBox::Ignore | QMessageBox::Cancel,
                    { { QMessageBox::Yes, tr("Choose Repos...") }, { QMessageBox::Ignore, tr("Create Anyway") } });
                if (choice == QMessageBox::Yes) openInstallPage();
                if (choice != QMessageBox::Ignore) return false;
            }
        }
    }

    QString createdOwner, defaultBranch, error;
    int status = 0;
    setStatus(tr("Creating %1/%2...").arg(owner, name));
    waitFor([&](std::function<void()> quit)
    {
        client->createRepo(owner.compare(accountLogin, Qt::CaseInsensitive) == 0 ? QString() : owner, name,
                           description, isPrivate,
                           [&, quit](const QString &o, const QString &b, const QString &e, int s)
        {
            createdOwner = o;
            defaultBranch = b;
            error = e;
            status = s;
            quit();
        });
    });

    if (!error.isEmpty())
    {
        QString text = tr("%1/%2 couldn't be created: %3").arg(owner, name, error);
        if (status == 403 || status == 404)
            text += "\n\n" + tr("This sign-in may create no repos. Signed in with GitHub, the SavvyCAN app needs "
                                "Administration: read and write, accepted for this account (GitHub asks the "
                                "account's owner to accept new app permissions). A token needs Administration: "
                                "read and write, or the repo scope. Or create the repo on github.com and add it "
                                "here with Add...");
        setStatus(QString());
        askUser(QMessageBox::Warning, text, QMessageBox::Ok);
        return false;
    }

    GitHubRepo repo;
    repo.owner = createdOwner.isEmpty() ? owner : createdOwner;
    repo.name = name;
    repo.branch = defaultBranch;
    QList<GitHubRepo> all = GitHubSettings::repos();
    all.append(repo);
    GitHubSettings::setRepos(all);
    fillRepos(repo.fullName());
    return true;
}

void GitHubBrowserDialog::showAccount()
{
    GitHubAccountDialog dialog(this);
    dialog.exec();
    forgetAppAccess(); //another sign-in, or the app installed meanwhile
    refreshAccount();
    repoChanged(); //a different account can see different repos and branches
    updateOpenButton();
}

void GitHubBrowserDialog::openSelected()
{
    const QList<QTreeWidgetItem *> selected = tree->selectedItems();
    if (selected.count() != 1) return;
    const int idx = selected[0]->data(0, Qt::UserRole).toInt();
    const int repoIdx = comboRepo->currentIndex();
    const int branchIdx = comboBranch->currentIndex();
    if (idx < 0 || idx >= entries.count() || repoIdx < 0 || branchIdx < 0 || branchIdx >= branches.count()) return;

    const GitHubEntry entry = entries[idx];
    const GitHubRepo repo = savedRepos[repoIdx];
    const GitHubBranch branch = branches[branchIdx];
    const QString name = QFileInfo(entry.path).fileName();

    if (entry.size > maxFileSize)
    {
        askUser(QMessageBox::Warning, tr("%1 is %2: GitHub's API only sends files up to 100 MB.")
                .arg(name, QLocale().formattedDataSize(entry.size)), QMessageBox::Ok);
        return;
    }

    QProgressDialog progress(tr("Downloading %1...").arg(name), tr("Cancel"), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    QByteArray data;
    QString error;
    QEventLoop loop;
    //from the branch's commit, so it's the same version the file list showed
    QNetworkReply *reply = client->download(repo.owner, repo.name, branch.commitSha, entry.path,
                                            [&](const QByteArray &body, const QString &err)
    {
        data = body;
        error = err;
        loop.quit();
    });
    connect(reply, &QNetworkReply::downloadProgress, &progress, [&progress](qint64 received, qint64 total)
    {
        if (total > 0)
        {
            progress.setMaximum(int(total / 1024));
            progress.setValue(int(received / 1024));
        }
    });
    bool cancelled = false;
    connect(&progress, &QProgressDialog::canceled, reply, [&cancelled, reply]()
    {
        cancelled = true;
        reply->abort();
    });
    loop.exec();
    progress.reset();

    if (error.isEmpty() && data.startsWith("version https://git-lfs.github.com/spec/"))
        error = tr("%1 is stored with Git LFS, which isn't supported yet.").arg(name);
    if (!error.isEmpty())
    {
        if (!cancelled) askUser(QMessageBox::Warning, error, QMessageBox::Ok);
        return;
    }

    GitHubOrigin origin;
    origin.owner = repo.owner;
    origin.repo = repo.name;
    origin.branch = branch.name;
    origin.path = entry.path;
    origin.sha = entry.sha;

    QString localFile;
    if (!GitHubCache::store(origin, data, localFile, error))
    {
        askUser(QMessageBox::Warning, error, QMessageBox::Ok);
        return;
    }

    //the files that go with it, from the same commit; a cached one it doesn't have any more would be taken for its
    QStringList companionErrors;
    for (const QString &suffix : std::as_const(companions))
    {
        const int companionIdx = entryIndexOf(entry.path + suffix);
        if (companionIdx < 0)
        {
            GitHubCache::remove(localFile + suffix);
            continue;
        }
        GitHubOrigin companionOrigin = origin;
        companionOrigin.path = entries[companionIdx].path;
        companionOrigin.sha = entries[companionIdx].sha;
        QByteArray companionData;
        QString companionError;
        waitFor([&](std::function<void()> quit)
        {
            client->download(repo.owner, repo.name, branch.commitSha, companionOrigin.path,
                             [&, quit](const QByteArray &body, const QString &e)
            {
                companionData = body;
                companionError = e;
                quit();
            });
        });
        QString companionFile;
        if (companionError.isEmpty()) GitHubCache::store(companionOrigin, companionData, companionFile, companionError);
        if (!companionError.isEmpty())
        {
            GitHubCache::remove(localFile + suffix);
            companionErrors.append(QString("%1: %2").arg(QFileInfo(companionOrigin.path).fileName(), companionError));
        }
    }
    if (!companionErrors.isEmpty())
        askUser(QMessageBox::Warning, tr("%1 was downloaded, but not everything that goes with it:\n%2")
                .arg(name, companionErrors.join('\n')), QMessageBox::Ok);

    resultFile = localFile;
    resultOrigin = origin;
    accept();
}

//a name in that folder that the branch doesn't have yet: "name (2).ext", "name (3).ext"...
QString GitHubBrowserDialog::freeName(const QString &folder, const QString &name) const
{
    const QFileInfo info(name);
    const QString base = info.completeBaseName();
    const QString ext = info.suffix().isEmpty() ? QString() : "." + info.suffix();
    for (int n = 2; ; n++)
    {
        const QString candidate = QString("%1 (%2)%3").arg(base).arg(n).arg(ext);
        const QString path = folder.isEmpty() ? candidate : folder + "/" + candidate;
        bool taken = false;
        for (const GitHubEntry &entry : entries)
            if (entry.path == path) taken = true;
        if (!taken) return candidate;
    }
}

void GitHubBrowserDialog::saveSelected()
{
    const int repoIdx = comboRepo->currentIndex();
    const int branchIdx = comboBranch->currentIndex();
    if (!writer || repoIdx < 0 || repoIdx >= savedRepos.count() || branchIdx < 0 || branchIdx >= branches.count())
        return;
    const GitHubRepo repo = savedRepos[repoIdx];
    const GitHubBranch base = branches[branchIdx];

    QString name = editName->text().trimmed();
    if (name.isEmpty()) return;
    if (!name.contains('.')) name += extensionFor(comboType->currentText());
    QString folder = editFolder->text().trimmed();
    while (folder.startsWith('/')) folder.remove(0, 1);
    while (folder.endsWith('/')) folder.chop(1);
    const QString path = folder.isEmpty() ? name : folder + "/" + name;

    GitHubOrigin target;
    target.owner = repo.owner;
    target.repo = repo.name;
    target.branch = base.name;
    target.path = path;
    if (name.contains('/') || name.contains('\\') || GitHubCache::localPath(target).isEmpty())
    {
        askUser(QMessageBox::Warning, tr("\"%1\" can't be used as a file's path in the repo.").arg(path),
                QMessageBox::Ok);
        return;
    }

    const bool newBranch = cbNewBranch->isChecked();
    QString targetBranch = base.name;
    if (newBranch)
    {
        targetBranch = editBranchName->text().trimmed();
        bool exists = false;
        for (const GitHubBranch &branch : std::as_const(branches))
            if (branch.name == targetBranch) exists = true;
        if (targetBranch.isEmpty() || exists)
        {
            askUser(QMessageBox::Warning, targetBranch.isEmpty() ? tr("Enter a name for the new branch.")
                                                                 : tr("%1 already has a branch called %2.")
                                                                   .arg(repo.fullName(), targetBranch),
                    QMessageBox::Ok);
            return;
        }
    }

    //signed in through a GitHub App, saving needs it installed on the repo: asked afresh, it may have just been installed
    forgetAppAccess();
    if (appAccessTo(repo.owner, repo.name) == AppAccess::NotInstalled)
    {
        const bool canInstall = !GitHubSettings::appInstallUrl().isEmpty();
        const QMessageBox::StandardButton choice = askUser(QMessageBox::Warning,
            canInstall ? tr("The SavvyCAN app isn't installed on %1, so GitHub won't let SavvyCAN save there.\n\n"
                            "Choose Repos... opens GitHub's page to install it on that repo (or on all your repos); "
                            "then save again.").arg(repo.fullName())
                       : tr("The app you signed in with isn't installed on %1, so GitHub won't let SavvyCAN save "
                            "there. Install it on that repo on GitHub, then save again.").arg(repo.fullName()),
            canInstall ? QMessageBox::Yes | QMessageBox::Ignore | QMessageBox::Cancel
                       : QMessageBox::Ignore | QMessageBox::Cancel,
            { { QMessageBox::Yes, tr("Choose Repos...") }, { QMessageBox::Ignore, tr("Save Anyway") } });
        if (choice == QMessageBox::Yes) openInstallPage();
        if (choice != QMessageBox::Ignore) return;
    }

    //the version being replaced: the one loaded, or else the one in the file list (after asking)
    //(copied: the list can change while this waits for GitHub)
    bool existing = false;
    QString existingSha;
    for (const GitHubEntry &entry : std::as_const(entries))
    {
        if (entry.isFolder || entry.path != path) continue;
        existing = true;
        existingSha = entry.sha;
    }
    const bool sameAsLoaded = suggestion.isValid() &&
            suggestion.owner.compare(repo.owner, Qt::CaseInsensitive) == 0 &&
            suggestion.repo.compare(repo.name, Qt::CaseInsensitive) == 0 &&
            suggestion.branch == base.name && suggestion.path == path;
    QString expectedSha;
    if (sameAsLoaded) expectedSha = suggestion.sha;
    else if (existing)
    {
        if (askUser(QMessageBox::Question, tr("%1 is already in %2 on %3. Replace it?")
                    .arg(path, repo.fullName(), base.name), QMessageBox::Yes | QMessageBox::Cancel)
                != QMessageBox::Yes) return;
        expectedSha = existingSha;
    }

    //SavvyCAN's own saver writes it
    QTemporaryDir tmp;
    QString error;
    const QString localFile = tmp.filePath(name);
    if (!tmp.isValid())
    {
        askUser(QMessageBox::Warning, tr("%1 couldn't be written.").arg(name), QMessageBox::Ok);
        return;
    }
    if (!writer(localFile, comboType->currentIndex(), error))
    {
        if (!error.isEmpty()) askUser(QMessageBox::Warning, error, QMessageBox::Ok); //no error: the user cancelled
        return;
    }
    QFile file(localFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        askUser(QMessageBox::Warning, tr("%1 couldn't be read back: %2").arg(name, file.errorString()),
                QMessageBox::Ok);
        return;
    }
    const QByteArray data = file.readAll();
    file.close();

    if (data.size() > maxFileSize)
    {
        askUser(QMessageBox::Warning, tr("%1 is %2: GitHub refuses files over 100 MB.")
                .arg(name, QLocale().formattedDataSize(data.size())), QMessageBox::Ok);
        return;
    }
    if (data.size() > largeFileSize &&
        askUser(QMessageBox::Warning, tr("%1 is %2. GitHub warns about files over 50 MB and refuses them over 100 MB; "
                                         "a more compact file type makes it smaller. Save it anyway?")
                .arg(name, QLocale().formattedDataSize(data.size())), QMessageBox::Yes | QMessageBox::Cancel)
            != QMessageBox::Yes) return;

    QString message = editMessage->text().trimmed();
    if (message.isEmpty())
        message = (existing || sameAsLoaded ? tr("Update %1 from SavvyCAN") : tr("Add %1 from SavvyCAN")).arg(name);

    QProgressDialog progress(tr("Saving %1 to GitHub...").arg(name), QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.show();

    //a branch this save made goes again if the save doesn't happen
    auto dropNewBranch = [&]()
    {
        if (!newBranch) return;
        waitFor([&](std::function<void()> quit)
        {
            client->deleteBranch(repo.owner, repo.name, targetBranch, [quit](const QString &) { quit(); });
        });
    };

    if (newBranch)
    {
        waitFor([&](std::function<void()> quit)
        {
            client->createBranch(repo.owner, repo.name, targetBranch, base.commitSha,
                                 [&, quit](const QString &e) { error = e; quit(); });
        });
        if (!error.isEmpty())
        {
            progress.reset();
            askUser(QMessageBox::Warning, tr("The branch %1 couldn't be made: %2").arg(targetBranch, error),
                    QMessageBox::Ok);
            return;
        }
    }

    GitHubSaveResult result;
    for (;;)
    {
        bool conflict = false;
        waitFor([&](std::function<void()> quit)
        {
            QNetworkReply *reply = client->putFile(repo.owner, repo.name, targetBranch, path, data, message,
                                                   expectedSha,
                                                   [&, quit](const GitHubSaveResult &r, bool c, const QString &e)
            {
                result = r;
                conflict = c;
                error = e;
                quit();
            });
            connect(reply, &QNetworkReply::uploadProgress, &progress, [&progress](qint64 sent, qint64 total)
            {
                if (total > 0)
                {
                    progress.setMaximum(int(total / 1024));
                    progress.setValue(int(sent / 1024));
                }
            });
        });
        if (!conflict) break;

        progress.hide();
        const QMessageBox::StandardButton choice = askUser(QMessageBox::Warning,
            tr("%1 on GitHub isn't the version this one replaces: it changed there after it was loaded, or someone "
               "saved a file of that name meanwhile.\n\nOverwrite it with this file, or save this one under a new "
               "name?").arg(path),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            { { QMessageBox::Yes, tr("Overwrite") }, { QMessageBox::No, tr("Save Under a New Name") } });
        if (choice != QMessageBox::Yes)
        {
            dropNewBranch();
            if (choice == QMessageBox::No)
            {
                editName->setText(freeName(folder, name));
                setStatus(tr("Pick a name, then save again."));
            }
            return;
        }
        //overwrite: replace whatever is there now
        QString currentSha;
        waitFor([&](std::function<void()> quit)
        {
            client->getFileSha(repo.owner, repo.name, targetBranch, path,
                               [&, quit](const QString &sha, const QString &e) { currentSha = sha; error = e; quit(); });
        });
        if (!error.isEmpty()) break;
        expectedSha = currentSha;
        progress.show();
    }
    if (!error.isEmpty())
    {
        progress.reset();
        dropNewBranch();
        askUser(QMessageBox::Warning, tr("%1 couldn't be saved: %2").arg(path, error), QMessageBox::Ok);
        return;
    }

    //the files that go with it, if the writer wrote them: each replaces whatever is there now, as the file did
    struct SavedCompanion { GitHubOrigin origin; QByteArray data; };
    QList<SavedCompanion> savedCompanions;
    QStringList companionErrors, oldCompanions;
    for (const QString &suffix : std::as_const(companions))
    {
        const QString companionPath = path + suffix;
        const QString companionName = QFileInfo(companionPath).fileName();
        QString currentSha, shaError;
        waitFor([&](std::function<void()> quit)
        {
            client->getFileSha(repo.owner, repo.name, targetBranch, companionPath,
                               [&, quit](const QString &sha, const QString &e) { currentSha = sha; shaError = e; quit(); });
        });
        QFile companionFile(localFile + suffix);
        if (!companionFile.exists())
        {
            //left as it is: it may be from an earlier version of the file
            if (!currentSha.isEmpty()) oldCompanions.append(companionName);
            continue;
        }
        if (!companionFile.open(QIODevice::ReadOnly))
        {
            companionErrors.append(QString("%1: %2").arg(companionName, companionFile.errorString()));
            continue;
        }
        const QByteArray companionData = companionFile.readAll();
        GitHubSaveResult companionResult;
        QString companionError = shaError;
        for (int attempt = 0; attempt < 2 && companionError.isEmpty(); attempt++)
        {
            bool conflict = false;
            waitFor([&](std::function<void()> quit)
            {
                client->putFile(repo.owner, repo.name, targetBranch, companionPath, companionData,
                                tr("%1 (%2)").arg(message, companionName), currentSha,
                                [&, quit](const GitHubSaveResult &r, bool c, const QString &e)
                {
                    companionResult = r;
                    conflict = c;
                    companionError = e;
                    quit();
                });
            });
            if (!conflict) break;
            //changed on GitHub just now: replace that version too
            companionError.clear();
            waitFor([&](std::function<void()> quit)
            {
                client->getFileSha(repo.owner, repo.name, targetBranch, companionPath,
                                   [&, quit](const QString &sha, const QString &e) { currentSha = sha; companionError = e; quit(); });
            });
            if (attempt == 1 && companionError.isEmpty()) companionError = tr("it keeps changing on GitHub");
        }
        if (!companionError.isEmpty())
        {
            companionErrors.append(QString("%1: %2").arg(companionName, companionError));
            continue;
        }
        SavedCompanion saved;
        saved.origin = target;
        saved.origin.branch = targetBranch;
        saved.origin.path = companionPath;
        saved.origin.sha = companionResult.sha;
        saved.data = companionData;
        savedCompanions.append(saved);
    }

    QString prUrl, prError;
    if (newBranch)
    {
        const QString title = editPrTitle->text().trimmed().isEmpty() ? message : editPrTitle->text().trimmed();
        waitFor([&](std::function<void()> quit)
        {
            client->createPullRequest(repo.owner, repo.name, targetBranch, base.name, title,
                                      tr("Saved from SavvyCAN."),
                                      [&, quit](const QString &url, const QString &e) { prUrl = url; prError = e; quit(); });
        });
    }
    progress.reset();

    //the cache now holds what's on GitHub
    GitHubOrigin saved = target;
    saved.branch = targetBranch;
    saved.sha = result.sha;
    QString cached;
    if (!GitHubCache::store(saved, data, cached, error)) cached.clear();
    for (const QString &suffix : std::as_const(companions))
        if (!cached.isEmpty()) GitHubCache::remove(cached + suffix);
    for (const SavedCompanion &companion : std::as_const(savedCompanions))
    {
        QString companionCached, companionError;
        GitHubCache::store(companion.origin, companion.data, companionCached, companionError);
    }

    resultFile = cached;
    resultOrigin = saved;
    resultLink = prUrl.isEmpty() ? result.commitUrl : prUrl;

    QString text;
    if (!newBranch) text = tr("Saved %1 to %2 on %3.").arg(path, repo.fullName(), base.name).toHtmlEscaped();
    else if (prError.isEmpty())
        text = tr("Saved %1 to the new branch %2 and opened a pull request into %3.")
               .arg(path, targetBranch, base.name).toHtmlEscaped();
    else
        text = tr("Saved %1 to the new branch %2, but the pull request couldn't be opened: %3")
               .arg(path, targetBranch, prError).toHtmlEscaped();
    QStringList savedNames;
    for (const SavedCompanion &companion : std::as_const(savedCompanions))
        savedNames.append(QFileInfo(companion.origin.path).fileName());
    if (!savedNames.isEmpty())
        text += "<br>" + tr("With %1.").arg(savedNames.join(", ")).toHtmlEscaped();
    if (!companionErrors.isEmpty())
        text += "<br><br>" + tr("Not saved with it:").toHtmlEscaped() + "<br>" + companionErrors.join("\n").toHtmlEscaped()
                                                                                  .replace("\n", "<br>");
    if (!oldCompanions.isEmpty())
        text += "<br><br>" + tr("%1 on GitHub is from an earlier save and wasn't changed; delete it there if it doesn't "
                                "go with this file any more.").arg(oldCompanions.join(", ")).toHtmlEscaped();
    if (!resultLink.isEmpty())
        text += QString("<br><br><a href=\"%1\">%2</a>").arg(resultLink.toHtmlEscaped(),
                                                             prUrl.isEmpty() ? tr("See the commit on GitHub")
                                                                             : tr("See the pull request on GitHub"));
    askUser(QMessageBox::Information, text, QMessageBox::Ok);
    accept();
}

//Signed in through a GitHub App, GitHub says where it's installed. A token, or an OAuth App, reaches what it reaches.
bool GitHubBrowserDialog::checksAppInstall() const
{
    return !GitHubSettings::token().isEmpty() && GitHubSettings::tokenFromDeviceFlow() &&
           GitHubSettings::isGitHubAppClientId(GitHubSettings::clientId());
}

bool GitHubBrowserDialog::readInstallations()
{
    if (installationsRead) return true;
    QString error;
    waitFor([&](std::function<void()> quit)
    {
        client->listInstallations([&, quit](const QList<GitHubInstallation> &list, const QString &e)
        {
            installations = list;
            error = e;
            quit();
        });
    });
    installationsRead = error.isEmpty();
    return installationsRead;
}

//Unknown when it can't be said (another kind of sign-in, or GitHub didn't answer): saving then just goes ahead
GitHubBrowserDialog::AppAccess GitHubBrowserDialog::appAccessTo(const QString &owner, const QString &name)
{
    const QString key = (owner + "/" + name).toLower();
    if (appAccess.contains(key)) return appAccess[key];
    if (!checksAppInstall() || !readInstallations()) return AppAccess::Unknown;

    AppAccess access = AppAccess::NotInstalled;
    for (const GitHubInstallation &installation : std::as_const(installations))
    {
        if (installation.account.compare(owner, Qt::CaseInsensitive) != 0) continue;
        if (installation.allRepos)
        {
            access = AppAccess::Installed;
            break;
        }
        QStringList repos;
        QString error;
        waitFor([&](std::function<void()> quit)
        {
            client->listInstallationRepos(installation.id, [&, quit](const QStringList &list, const QString &e)
            {
                repos = list;
                error = e;
                quit();
            });
        });
        if (!error.isEmpty()) return AppAccess::Unknown;
        for (const QString &repo : std::as_const(repos))
            if (repo.compare(owner + "/" + name, Qt::CaseInsensitive) == 0) access = AppAccess::Installed;
    }
    appAccess[key] = access;
    return access;
}

void GitHubBrowserDialog::forgetAppAccess()
{
    appAccess.clear();
    installations.clear();
    installationsRead = false;
}

//Save: says so as soon as a repo is picked that the app isn't installed on
void GitHubBrowserDialog::updateAppHint()
{
    const int idx = comboRepo->currentIndex();
    if (mode != Save || idx < 0 || idx >= savedRepos.count() || !checksAppInstall())
    {
        lblAppHint->setVisible(false);
        return;
    }
    const QString fullName = savedRepos[idx].fullName();
    const AppAccess access = appAccessTo(savedRepos[idx].owner, savedRepos[idx].name);
    if (comboRepo->currentIndex() != idx) return; //another repo was picked meanwhile: its own check follows
    if (access != AppAccess::NotInstalled)
    {
        lblAppHint->setVisible(false);
        return;
    }
    if (GitHubSettings::appInstallUrl().isEmpty())
        lblAppHint->setText(tr("The app you signed in with isn't installed on %1, so saving there will fail: install "
                               "it on that repo on GitHub.").arg(fullName).toHtmlEscaped());
    else
        lblAppHint->setText(tr("The SavvyCAN app isn't installed on %1, so saving there will fail.")
                            .arg(fullName).toHtmlEscaped() + " <a href=\"install\">" + tr("Choose Repos...") + "</a>");
    lblAppHint->setVisible(true);
}

void GitHubBrowserDialog::openInstallPage()
{
    if (!GitHubSettings::appInstallUrl().isEmpty()) QDesktopServices::openUrl(QUrl(GitHubSettings::appInstallUrl()));
    forgetAppAccess(); //asked again next time: it's probably being installed now
}
