#ifndef DBCNODEDUPLICATEEDITOR_H
#define DBCNODEDUPLICATEEDITOR_H

#include <QDialog>
#include "dbc_classes.h"
#include "dbchandler.h"

namespace Ui {
class DBCNodeDuplicateEditor;
}

class DBCNodeDuplicateEditor : public QDialog
{
    Q_OBJECT

public:
    explicit DBCNodeDuplicateEditor(QWidget *parent = nullptr);
    ~DBCNodeDuplicateEditor();
    void showEvent(QShowEvent*);
    void setNodeRef(DBC_NODE *node);
    void setFileIdx(int idx);
    bool refreshView();

signals:
    void updatedTreeInfo(DBC_MESSAGE *msg);
    void createNode(QString nodeName);
    void cloneMessageToNode(DBC_NODE *parentNode, DBC_MESSAGE *source, uint newMsgId);
    void nodeAdded();

private:
    Ui::DBCNodeDuplicateEditor *ui;

    DBCHandler *dbcHandler;
    DBC_NODE *dbcNode;
    DBCFile *dbcFile;

    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    void readSettings();
    void writeSettings();

    uint lowestMsgId;
};

#endif // DBCNODEDUPLICATEEDITOR_H
