#ifndef DBCMAINEDITOR_H
#define DBCMAINEDITOR_H

#include <QDialog>
#include <QDebug>
#include "dbchandler.h"
#include "dbcsignaleditor.h"

namespace Ui {
class DBCMainEditor;
}

class DBCMainEditor : public QDialog
{
    Q_OBJECT

public:
    explicit DBCMainEditor(DBCHandler *handler, QWidget *parent = 0);
    ~DBCMainEditor();

private slots:
    void onCellChangedNode(int,int);
    void onCellClickedNode(int, int);
    void onCellClickedMessage(int, int);
    void onCellChangedMessage(int,int);
    void onCustomMenuNode(QPoint);
    void onCustomMenuMessage(QPoint);
    void deleteCurrentNode();
    void deleteCurrentMessage();

private:
    Ui::DBCMainEditor *ui;
    DBCHandler *dbcHandler;
    DBCSignalEditor *sigEditor;
    int currRow;
    bool inhibitCellChanged;

    void refreshNodesTable();
    void refreshMessagesTable(const DBC_NODE *node);
    void showEvent(QShowEvent* event);
    void closeEvent(QCloseEvent *event);
    void readSettings();
    void writeSettings();

};

#endif // DBCMAINEDITOR_H
