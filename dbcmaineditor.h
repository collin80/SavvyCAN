#ifndef DBCMAINEDITOR_H
#define DBCMAINEDITOR_H

#include <QDialog>
#include <QDebug>
#include "dbchandler.h"
#include "dbcsignaleditor.h"
#include "utility.h"

namespace Ui {
class DBCMainEditor;
}

class DBCMainEditor : public QDialog
{
    Q_OBJECT

public:
    explicit DBCMainEditor(DBCHandler *handler, const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~DBCMainEditor();
    void setFileIdx(int idx);

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
    const QVector<CANFrame> *referenceFrames;
    DBCSignalEditor *sigEditor;
    int currRow;
    DBCFile *dbcFile;
    int fileIdx;
    bool inhibitCellChanged;

    void refreshNodesTable();
    void refreshMessagesTable(const DBC_NODE *node);
    void showEvent(QShowEvent* event);
    void closeEvent(QCloseEvent *event);
    void readSettings();
    void writeSettings();

};

#endif // DBCMAINEDITOR_H
