#ifndef DBCNODEREBASEEDITOR_H
#define DBCNODEREBASEEDITOR_H

#include <QDialog>
#include "dbc_classes.h"
#include "dbchandler.h"

namespace Ui {
class DBCNodeRebaseEditor;
}

class DBCNodeRebaseEditor : public QDialog
{
    Q_OBJECT

public:
    explicit DBCNodeRebaseEditor(QWidget *parent = nullptr);
    ~DBCNodeRebaseEditor();
    void showEvent(QShowEvent*);
    void setNodeRef(DBC_NODE *node);
    void setFileIdx(int idx);
    bool refreshView();

signals:
    void updatedTreeInfo(DBC_MESSAGE *msg);

private:
    Ui::DBCNodeRebaseEditor *ui;

    DBCHandler *dbcHandler;
    DBC_NODE *dbcNode;
    DBCFile *dbcFile;

    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    void readSettings();
    void writeSettings();

    uint lowestMsgId;
};

#endif // DBCNODEREBASEEDITOR_H
