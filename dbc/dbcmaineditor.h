#ifndef DBCMAINEDITOR_H
#define DBCMAINEDITOR_H

#include <QDialog>
#include <QDebug>
#include <QIcon>
#include "dbchandler.h"
#include "dbcsignaleditor.h"
#include "dbcmessageeditor.h"
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

private slots:
    void onTreeDoubleClicked(const QModelIndex &index);
    void onCustomMenuTree(QPoint);
    void deleteCurrentTreeItem();
    void handleSearch();

private:
    Ui::DBCMainEditor *ui;
    DBCHandler *dbcHandler;
    const QVector<CANFrame> *referenceFrames;
    DBCSignalEditor *sigEditor;
    DBCMessageEditor *msgEditor;
    DBCFile *dbcFile;
    int fileIdx;
    QIcon nodeIcon;
    QIcon messageIcon;
    QIcon signalIcon;
    QIcon multiplexorSignalIcon;
    QIcon multiplexedSignalIcon;

    void refreshTree();
    void showEvent(QShowEvent* event);
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    void readSettings();
    void writeSettings();

};

#endif // DBCMAINEDITOR_H
