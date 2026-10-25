#include "githubbrowserdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFileInfo>
#include <QFormLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>
#include "githubaccountdialog.h"

//GitHub's API sends files up to 100 MB; bigger ones need Git LFS, which isn't supported yet
static const qint64 maxDownloadSize = 100 * 1024 * 1024;

GitHubBrowserDialog::GitHubBrowserDialog(Kind kind, const QStringList &fileTypes, QWidget *parent)
    : QDialog(parent),
      kind(kind),
      client(new GitHubClient(this)),
      generation(0)
{
    setWindowTitle(kind == Logs ? tr("Load Log File from GitHub") : tr("Load DBC File from GitHub"));
    client->setApiUrl(GitHubSettings::apiUrl());
    client->setToken(GitHubSettings::token());

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *accountRow = new QHBoxLayout;
    lblAccount = new QLabel;
    QPushButton *btnAccount = new QPushButton(tr("GitHub Account..."));
    accountRow->addWidget(lblAccount, 1);
    accountRow->addWidget(btnAccount);
    layout->addLayout(accountRow);

    QFormLayout *form = new QFormLayout;
    QHBoxLayout *repoRow = new QHBoxLayout;
    comboRepo = new QComboBox;
    comboRepo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    btnAddRepo = new QPushButton(tr("Add..."));
    btnRemoveRepo = new QPushButton(tr("Remove"));
    repoRow->addWidget(comboRepo, 1);
    repoRow->addWidget(btnAddRepo);
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

    lblStatus = new QLabel;
    lblStatus->setWordWrap(true);
    layout->addWidget(lblStatus);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    btnOpen = buttons->addButton(tr("Open"), QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);

    QSettings settings;
    const int lastType = settings.value(settingsKey() + "Type", 0).toInt();
    if (lastType >= 0 && lastType < comboType->count()) comboType->setCurrentIndex(lastType);

    connect(btnAccount, &QPushButton::clicked, this, &GitHubBrowserDialog::showAccount);
    connect(btnAddRepo, &QPushButton::clicked, this, &GitHubBrowserDialog::addRepo);
    connect(btnRemoveRepo, &QPushButton::clicked, this, &GitHubBrowserDialog::removeRepo);
    connect(comboRepo, &QComboBox::currentIndexChanged, this, &GitHubBrowserDialog::repoChanged);
    connect(comboBranch, &QComboBox::currentIndexChanged, this, &GitHubBrowserDialog::branchChanged);
    connect(comboType, &QComboBox::currentIndexChanged, this, &GitHubBrowserDialog::fillTree);
    connect(editSearch, &QLineEdit::textChanged, this, &GitHubBrowserDialog::fillTree);
    connect(tree, &QTreeWidget::itemSelectionChanged, this, &GitHubBrowserDialog::updateOpenButton);
    connect(tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item)
    {
        if (item->data(0, Qt::UserRole).toInt() >= 0) openSelected();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &GitHubBrowserDialog::openSelected);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    comboRepo->setObjectName("comboRepo");
    comboBranch->setObjectName("comboBranch");
    comboType->setObjectName("comboType");
    editSearch->setObjectName("editSearch");
    tree->setObjectName("tree");
    lblStatus->setObjectName("lblStatus");
    btnOpen->setObjectName("btnOpen");

    resize(640, 560);
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
    return kind == Logs ? "GitHub/LastLogs" : "GitHub/LastDBCs";
}

bool GitHubBrowserDialog::matchesFileType(const QString &fileName, const QString &nameFilter)
{
    //the patterns are between the brackets; a missing ')' is tolerated
    const int open = nameFilter.indexOf('(');
    if (open < 0) return true;
    int close = nameFilter.indexOf(')', open);
    if (close < 0) close = int(nameFilter.length());
    const QStringList patterns = nameFilter.mid(open + 1, close - open - 1).split(' ', Qt::SkipEmptyParts);
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

void GitHubBrowserDialog::refreshAccount()
{
    client->setToken(GitHubSettings::token());
    if (GitHubSettings::token().isEmpty())
    {
        lblAccount->setText(tr("Not signed in: public repos only."));
        return;
    }
    lblAccount->setText(tr("Checking the sign-in..."));
    client->getUser([this](const QString &login, const QString &error)
    {
        lblAccount->setText(error.isEmpty() ? tr("Signed in as %1.").arg(login) : error);
    });
}

void GitHubBrowserDialog::fillRepos(const QString &select)
{
    shownRepos.clear();
    const QList<GitHubRepo> all = GitHubSettings::repos();
    for (const GitHubRepo &repo : all)
        if ((kind == Logs && repo.logs) || (kind == DBCs && repo.dbcs)) shownRepos.append(repo);

    QSignalBlocker block(comboRepo);
    comboRepo->clear();
    int selected = 0;
    for (int i = 0; i < shownRepos.count(); i++)
    {
        comboRepo->addItem(shownRepos[i].fullName());
        if (shownRepos[i].fullName().compare(select, Qt::CaseInsensitive) == 0) selected = i;
    }
    comboRepo->setCurrentIndex(shownRepos.isEmpty() ? -1 : selected);
    btnRemoveRepo->setEnabled(!shownRepos.isEmpty());
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
    if (idx < 0 || idx >= shownRepos.count())
    {
        lblStatus->setText(tr("Add a repo with Add... (\"owner/repo\" or a github.com link)."));
        return;
    }
    const GitHubRepo repo = shownRepos[idx];
    QSettings().setValue(settingsKey() + "Repo", repo.fullName());

    lblStatus->setText(tr("Reading the branches of %1...").arg(repo.fullName()));
    client->listBranches(repo.owner, repo.name, [this, gen, repo](const QList<GitHubBranch> &list, const QString &error)
    {
        if (gen != generation) return;
        if (!error.isEmpty())
        {
            lblStatus->setText(error);
            return;
        }
        if (list.isEmpty())
        {
            lblStatus->setText(tr("%1 has no branches yet.").arg(repo.fullName()));
            return;
        }
        branches = list;
        QSignalBlocker block(comboBranch);
        int selected = 0;
        for (int i = 0; i < branches.count(); i++)
        {
            comboBranch->addItem(branches[i].name);
            if (branches[i].name == repo.branch) selected = i;
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
    if (repoIdx < 0 || repoIdx >= shownRepos.count() || branchIdx < 0 || branchIdx >= branches.count()) return;
    const GitHubRepo repo = shownRepos[repoIdx];
    const GitHubBranch branch = branches[branchIdx];
    rememberBranch(branch.name);

    lblStatus->setText(tr("Reading the files in %1...").arg(branch.name));
    client->getTree(repo.owner, repo.name, branch.commitSha,
                    [this, gen](const QList<GitHubEntry> &list, bool truncated, const QString &error)
    {
        if (gen != generation) return;
        if (!error.isEmpty())
        {
            lblStatus->setText(error);
            return;
        }
        entries = list;
        fillTree();
        if (entries.isEmpty()) lblStatus->setText(tr("This branch has no files."));
        if (truncated)
            lblStatus->setText(lblStatus->text() + " " + tr("GitHub sent only part of this repo's file list: "
                                                            "it's too large to list completely."));
    });
}

void GitHubBrowserDialog::rememberBranch(const QString &branch)
{
    const int idx = comboRepo->currentIndex();
    if (idx < 0 || idx >= shownRepos.count() || shownRepos[idx].branch == branch) return;
    shownRepos[idx].branch = branch;

    QList<GitHubRepo> all = GitHubSettings::repos();
    for (GitHubRepo &repo : all)
        if (repo.fullName().compare(shownRepos[idx].fullName(), Qt::CaseInsensitive) == 0) repo.branch = branch;
    GitHubSettings::setRepos(all);
}

void GitHubBrowserDialog::fillTree()
{
    QSettings().setValue(settingsKey() + "Type", comboType->currentIndex());

    tree->setSortingEnabled(false);
    tree->clear();
    const QString type = comboType->currentText();
    const QString search = editSearch->text().trimmed();

    QHash<QString, QTreeWidgetItem *> folders;
    std::function<QTreeWidgetItem *(const QString &)> folderItem = [&](const QString &path) -> QTreeWidgetItem *
    {
        if (path.isEmpty()) return tree->invisibleRootItem();
        if (folders.contains(path)) return folders[path];
        const int slash = int(path.lastIndexOf('/'));
        QTreeWidgetItem *parent = folderItem(slash < 0 ? QString() : path.left(slash));
        QTreeWidgetItem *item = new QTreeWidgetItem(parent, { path.mid(slash + 1) });
        item->setData(0, Qt::UserRole, -1);
        item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        folders[path] = item;
        return item;
    };

    int shown = 0;
    for (int i = 0; i < entries.count(); i++)
    {
        const GitHubEntry &entry = entries[i];
        if (entry.isFolder) continue;
        const int slash = int(entry.path.lastIndexOf('/'));
        const QString name = entry.path.mid(slash + 1);
        if (!matchesFileType(name, type)) continue;
        if (!search.isEmpty() && !entry.path.contains(search, Qt::CaseInsensitive)) continue;

        QTreeWidgetItem *item = new QTreeWidgetItem(folderItem(slash < 0 ? QString() : entry.path.left(slash)),
                                                    { name, QLocale().formattedDataSize(entry.size) });
        item->setData(0, Qt::UserRole, i);
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        shown++;
    }

    tree->setSortingEnabled(true);
    if (!search.isEmpty() || shown < 200) tree->expandAll();
    tree->resizeColumnToContents(1);

    if (comboBranch->count() > 0 && !entries.isEmpty())
    {
        if (shown == 0) lblStatus->setText(tr("No matching files."));
        else if (shown == 1) lblStatus->setText(tr("1 matching file."));
        else lblStatus->setText(tr("%1 matching files.").arg(shown));
    }
    updateOpenButton();
}

void GitHubBrowserDialog::updateOpenButton()
{
    const QList<QTreeWidgetItem *> selected = tree->selectedItems();
    btnOpen->setEnabled(selected.count() == 1 && selected[0]->data(0, Qt::UserRole).toInt() >= 0);
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
        QMessageBox::warning(this, tr("Add GitHub Repo"), tr("\"%1\" isn't a GitHub repo name or link.").arg(text));
        return;
    }

    btnAddRepo->setEnabled(false);
    lblStatus->setText(tr("Checking %1/%2...").arg(owner, name));
    client->getRepo(owner, name, [this, owner, name](const QString &defaultBranch, const QString &error)
    {
        btnAddRepo->setEnabled(true);
        if (!error.isEmpty())
        {
            lblStatus->setText(tr("%1/%2: %3").arg(owner, name, error));
            return;
        }

        QList<GitHubRepo> all = GitHubSettings::repos();
        bool found = false;
        for (GitHubRepo &repo : all)
        {
            if (repo.fullName().compare(owner + "/" + name, Qt::CaseInsensitive) != 0) continue;
            if (kind == Logs) repo.logs = true;
            else repo.dbcs = true;
            found = true;
        }
        if (!found)
        {
            GitHubRepo repo;
            repo.owner = owner;
            repo.name = name;
            repo.branch = defaultBranch;
            repo.logs = (kind == Logs);
            repo.dbcs = (kind == DBCs);
            all.append(repo);
        }
        GitHubSettings::setRepos(all);
        fillRepos(owner + "/" + name);
    });
}

//only from this list (logs or DBCs): nothing changes on GitHub
void GitHubBrowserDialog::removeRepo()
{
    const int idx = comboRepo->currentIndex();
    if (idx < 0 || idx >= shownRepos.count()) return;
    const QString fullName = shownRepos[idx].fullName();

    QList<GitHubRepo> all = GitHubSettings::repos();
    for (int i = int(all.count()) - 1; i >= 0; i--)
    {
        if (all[i].fullName().compare(fullName, Qt::CaseInsensitive) != 0) continue;
        if (kind == Logs) all[i].logs = false;
        else all[i].dbcs = false;
        if (!all[i].logs && !all[i].dbcs) all.removeAt(i);
    }
    GitHubSettings::setRepos(all);
    fillRepos(QString());
}

void GitHubBrowserDialog::showAccount()
{
    GitHubAccountDialog dialog(this);
    dialog.exec();
    refreshAccount();
    repoChanged(); //a different account can see different repos and branches
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
    const GitHubRepo repo = shownRepos[repoIdx];
    const GitHubBranch branch = branches[branchIdx];
    const QString name = QFileInfo(entry.path).fileName();

    if (entry.size > maxDownloadSize)
    {
        QMessageBox::warning(this, windowTitle(), tr("%1 is %2: GitHub's API only sends files up to 100 MB.")
                             .arg(name, QLocale().formattedDataSize(entry.size)));
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
        if (!cancelled) QMessageBox::warning(this, windowTitle(), error);
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
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }

    downloadedFile = localFile;
    downloadedOrigin = origin;
    accept();
}
