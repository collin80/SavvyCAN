#ifndef DBCMAINEDITOR_H
#define DBCMAINEDITOR_H

#include <QDialog>
#include <QDebug>
#include <QIcon>
#include <QTreeWidget>
#include <QRandomGenerator>
#include "dbchandler.h"
#include "dbcsignaleditor.h"
#include "dbcmessageeditor.h"
#include "dbcnodeeditor.h"
#include "dbcnoderebaseeditor.h"
#include "dbcnodeduplicateeditor.h"
#include "utility.h"

namespace Ui {
class DBCMainEditor;
}

class DBCMainEditor : public QDialog
{
    Q_OBJECT

public:
    explicit DBCMainEditor(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~DBCMainEditor();
    void setFileIdx(int idx);

public slots:
    void updatedNode(DBC_NODE *node);
    void updatedMessage(DBC_MESSAGE *msg);
    void updatedSignal(DBC_SIGNAL *sig);

private slots:
    void onTreeDoubleClicked(const QModelIndex &index);
    void onTreeContextMenu(const QPoint & pos);
    void currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *prev);
    void onCustomMenuTree(QPoint);
    void deleteCurrentTreeItem();
    void deleteNode(DBC_NODE *node);
    void deleteMessage(DBC_MESSAGE *msg);
    void deleteSignal(DBC_SIGNAL *sig);
    void handleSearch();
    void handleSearchForward();
    void handleSearchBackward();
    void newNode(QString nodeName);
    void newNode();
    void copyMessageToNode(DBC_NODE *node, DBC_MESSAGE *source, uint newMsgId);
    void newMessage();
    void newSignal();    
    void onRebaseMessages();
    void onDuplicateNode();

private:
    Ui::DBCMainEditor *ui;
    DBCHandler *dbcHandler;
    const QVector<CANFrame> *referenceFrames;
    DBCSignalEditor *sigEditor;
    DBCMessageEditor *msgEditor;
    DBCNodeEditor *nodeEditor;
    DBCNodeRebaseEditor *nodeRebaseEditor;
    DBCNodeDuplicateEditor *nodeDuplicateEditor;
    DBCFile *dbcFile;
    int fileIdx;
    QIcon nodeIcon;
    QIcon messageIcon;
    QIcon signalIcon;
    QIcon multiplexorSignalIcon;
    QIcon multiplexedSignalIcon;
    QList<QTreeWidgetItem *> searchItems;
    int searchItemPos;
    //bidirectional mapping of QTreeWidget items back and forth to DBC objects
    QMap<DBC_NODE*, QTreeWidgetItem *> nodeToItem;
    QMap<DBC_MESSAGE*, QTreeWidgetItem *> messageToItem;
    QMap<DBC_SIGNAL*, QTreeWidgetItem *> signalToItem;
    QMap<QTreeWidgetItem*, DBC_NODE*> itemToNode;
    QMap<QTreeWidgetItem*, DBC_MESSAGE*> itemToMessage;
    QMap<QTreeWidgetItem*, DBC_SIGNAL*> itemToSignal;
    QRandomGenerator randGen;

    void showEvent(QShowEvent* event);
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    void readSettings();
    void writeSettings();
    void refreshTree();
    void processSignalToTree(QTreeWidgetItem *parent, DBC_SIGNAL *sig);
    uint32_t getParentMessageID(QTreeWidgetItem *cell);
    QString createSignalText(DBC_SIGNAL *sig);
};

#endif // DBCMAINEDITOR_H
