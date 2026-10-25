#ifndef GITHUBBROWSERDIALOG_H
#define GITHUBBROWSERDIALOG_H

#include <QDialog>
#include "githubcache.h"
#include "githubclient.h"
#include "githubsettings.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

//Picks a file in one of the user's GitHub repos and downloads it into the GitHub cache folder, for the existing
//loaders to load from there.
class GitHubBrowserDialog : public QDialog
{
    Q_OBJECT

public:
    enum Kind { Logs, DBCs }; //the window title, and the last repo and file type remembered for each

    //fileTypes are file dialog name filters ("GVRET Logs (*.csv *.CSV)"); the one picked is fileTypeIndex()
    GitHubBrowserDialog(Kind kind, const QStringList &fileTypes, QWidget *parent = nullptr);
    ~GitHubBrowserDialog();

    QString localFile() const { return downloadedFile; }
    GitHubOrigin origin() const { return downloadedOrigin; }
    int fileTypeIndex() const;

    //whether a file name matches a name filter's patterns ("*.*", or no patterns, matches everything)
    static bool matchesFileType(const QString &fileName, const QString &nameFilter);

private:
    Kind kind;
    GitHubClient *client;
    QList<GitHubRepo> savedRepos;   //the repos in comboRepo, in its order (shared by logs and DBC files)
    QList<GitHubBranch> branches;   //the branches in comboBranch, in its order
    QList<GitHubEntry> entries;     //the files and folders of the branch shown
    int generation;                 //bumped on every repo/branch change, so late answers are ignored
    QString downloadedFile;
    GitHubOrigin downloadedOrigin;

    QLabel *lblAccount;
    QComboBox *comboRepo;
    QPushButton *btnAddRepo;
    QPushButton *btnRemoveRepo;
    QComboBox *comboBranch;
    QComboBox *comboType;
    QLineEdit *editSearch;
    QTreeWidget *tree;
    QLabel *lblStatus;
    QPushButton *btnOpen;

    QString settingsKey() const;
    void refreshAccount();
    void fillRepos(const QString &select);
    void repoChanged();
    void branchChanged();
    void fillTree();
    void addRepo();
    void removeRepo();
    void showAccount();
    void updateOpenButton();
    void openSelected();
    void rememberBranch(const QString &branch);
};

#endif // GITHUBBROWSERDIALOG_H
