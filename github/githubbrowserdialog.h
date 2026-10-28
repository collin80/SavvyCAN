#ifndef GITHUBBROWSERDIALOG_H
#define GITHUBBROWSERDIALOG_H

#include <QDialog>
#include <QHash>
#include <QMap>
#include <QMessageBox>
#include <functional>
#include "githubcache.h"
#include "githubclient.h"
#include "githubsettings.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

//Picks a file in one of the user's GitHub repos.
//Open: downloads it into the GitHub cache folder, for the existing loaders to load from there.
//Save: picks a repo, branch, folder and name, has the existing saver write the file (setSaveWriter), and commits it,
//straight to the branch or to a new branch with a pull request. The saved file is then in the cache too.
class GitHubBrowserDialog : public QDialog
{
    Q_OBJECT

public:
    enum Kind { Logs, DBCs }; //the window title, and the last repo and file type remembered for each
    enum Mode { Open, Save };

    //fileTypes are file dialog name filters ("GVRET Logs (*.csv *.CSV)"); the one picked is fileTypeIndex()
    GitHubBrowserDialog(Kind kind, Mode mode, const QStringList &fileTypes, QWidget *parent = nullptr);
    ~GitHubBrowserDialog();

    //the file in the cache folder, and where it is on GitHub: downloaded (Open) or saved (Save)
    QString localFile() const { return resultFile; }
    GitHubOrigin origin() const { return resultOrigin; }
    int fileTypeIndex() const;

    //Save: writes the file to save as that file type; false with error set if it couldn't, or false with no error
    //if the user cancelled (nothing is saved and nothing more is said)
    using SaveWriter = std::function<bool(const QString &localFile, int fileType, QString &error)>;
    void setSaveWriter(SaveWriter saveWriter) { writer = saveWriter; updateOpenButton(); }
    //Save: where the file came from (repo, branch, path, and the version loaded, for the conflict check), or only a
    //file name for a file that didn't come from GitHub
    void setSuggestion(const GitHubOrigin &from, const QString &fileName);
    //Save: the commit, or the pull request, on github.com
    QString resultUrl() const { return resultLink; }

    //Files that go with the file picked, named <file><suffix> (".notes.md" for log notes). Open downloads them into
    //the cache next to it (and drops an old cached one it no longer has). Save uploads those the save writer wrote
    //next to its file, after the file itself and before any pull request. The file list doesn't show them on their
    //own; the file they go with gets a mark instead.
    void setCompanionSuffixes(const QStringList &suffixes);

    //creates a repo under the signed-in account or one of its organisations and selects it; false if GitHub refused
    bool createRepoNow(const QString &owner, const QString &name, const QString &description, bool isPrivate);

    //how the dialog asks the user things (a QMessageBox); the unit tests answer instead. labels rename buttons.
    std::function<QMessageBox::StandardButton(QMessageBox::Icon icon, const QString &text,
                                              QMessageBox::StandardButtons buttons,
                                              const QMap<QMessageBox::StandardButton, QString> &labels)> ask;

    //whether a file name matches a name filter's patterns ("*.*", or no patterns, matches everything)
    static bool matchesFileType(const QString &fileName, const QString &nameFilter);
    //the extension a name filter's first pattern gives ("*.csv" gives ".csv"), empty for "*.*"
    static QString extensionFor(const QString &nameFilter);

private:
    Kind kind;
    Mode mode;
    GitHubClient *client;
    QList<GitHubRepo> savedRepos;   //the repos in comboRepo, in its order (shared by logs and DBC files)
    QList<GitHubBranch> branches;   //the branches in comboBranch, in its order
    QList<GitHubEntry> entries;     //the files and folders of the branch shown
    int generation;                 //bumped on every repo/branch change, so late answers are ignored
    QString accountLogin;           //the signed-in account's name, once GitHub said it
    QString pendingBranch;          //the branch to pick once the branches are read (a suggestion's)
    QString resultFile;
    GitHubOrigin resultOrigin;
    QString resultLink;
    SaveWriter writer;
    GitHubOrigin suggestion;
    QStringList companions; //see setCompanionSuffixes

    int entryIndexOf(const QString &path) const; //-1 if the branch shown has no file at that path

    //Save, signed in through a GitHub App: whether it's installed where the file goes (saving fails otherwise).
    //Answers are kept for the dialog's life, and asked again before each save.
    enum class AppAccess { Unknown, Installed, NotInstalled };
    QHash<QString, AppAccess> appAccess;    //by "owner/name", lower case
    QList<GitHubInstallation> installations;
    bool installationsRead = false;

    QLabel *lblAccount;
    QPushButton *btnAccount;
    QComboBox *comboRepo;
    QPushButton *btnAddRepo;
    QPushButton *btnNewRepo;
    QPushButton *btnRemoveRepo;
    QComboBox *comboBranch;
    QComboBox *comboType;
    QLineEdit *editSearch;
    QTreeWidget *tree;
    QLineEdit *editFolder;
    QLineEdit *editName;
    QLineEdit *editMessage;
    QCheckBox *cbNewBranch;
    QWidget *newBranchBox;
    QLineEdit *editBranchName;
    QLineEdit *editPrTitle;
    QLabel *lblAppHint;
    QLabel *lblStatus;
    QPushButton *btnOpen;

    QString settingsKey() const;
    void refreshAccount();
    void showError(const QString &error);
    void setStatus(const QString &text);
    void fillRepos(const QString &select);
    void repoChanged();
    void branchChanged();
    void fillTree();
    void addRepo();
    void removeRepo();
    void newRepo();
    void showAccount();
    void updateOpenButton();
    void openSelected();
    void saveSelected();
    void itemClicked(QTreeWidgetItem *item);
    void selectTypeFor(const QString &fileName);
    void rememberBranch(const QString &branch);
    QString freeName(const QString &folder, const QString &name) const;
    bool checksAppInstall() const;
    bool readInstallations();
    AppAccess appAccessTo(const QString &owner, const QString &name);
    void forgetAppAccess();
    void updateAppHint();
    void openInstallPage();
    QMessageBox::StandardButton askUser(QMessageBox::Icon icon, const QString &text,
                                        QMessageBox::StandardButtons buttons,
                                        const QMap<QMessageBox::StandardButton, QString> &labels = {});
};

#endif // GITHUBBROWSERDIALOG_H
