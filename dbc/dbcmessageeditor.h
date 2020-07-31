#ifndef DBCMESSAGEEDITOR_H
#define DBCMESSAGEEDITOR_H

#include <QDialog>
#include "dbc_classes.h"
#include "dbchandler.h"

namespace Ui {
class DBCMessageEditor;
}

class DBCMessageEditor : public QDialog
{
    Q_OBJECT

public:
    explicit DBCMessageEditor(QWidget *parent = nullptr);
    ~DBCMessageEditor();
    void showEvent(QShowEvent*);
    void setMessageRef(DBC_MESSAGE *msg);
    void setFileIdx(int idx);
    void refreshView();

signals:
    void updatedTreeInfo(DBC_MESSAGE *msg);

private:
    Ui::DBCMessageEditor *ui;

    DBCHandler *dbcHandler;
    DBC_MESSAGE *dbcMessage;
    DBCFile *dbcFile;
    bool suppressEditCallbacks;

    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    void readSettings();
    void writeSettings();
    void generateSampleText();
};

#endif // DBCMESSAGEEDITOR_H
