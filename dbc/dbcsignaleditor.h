#ifndef DBCSIGNALEDITOR_H
#define DBCSIGNALEDITOR_H

#include <QDialog>
#include "dbchandler.h"
#include "utility.h"

namespace Ui {
class DBCSignalEditor;
}

class DBCSignalEditor : public QDialog
{
    Q_OBJECT

public:
    explicit DBCSignalEditor(QWidget *parent = 0);
    void setMessageRef(DBC_MESSAGE *msg);
    void setFileIdx(int idx);
    void setSignalRef(DBC_SIGNAL *sig);
    void showEvent(QShowEvent*);
    ~DBCSignalEditor();
    void refreshView();

signals:
    void updatedTreeInfo(DBC_SIGNAL *sig);

private slots:
    void bitfieldClicked(int x, int y);
    void onValuesCellChanged(int row,int col);
    void onCustomMenuValues(QPoint);
    void deleteCurrentValue();

private:
    Ui::DBCSignalEditor *ui;
    DBCHandler *dbcHandler;
    DBC_MESSAGE *dbcMessage;
    DBC_SIGNAL *currentSignal;
    DBCFile *dbcFile;
    bool inhibitCellChanged;
    bool inhibitMsgProc;

    void fillSignalForm(DBC_SIGNAL *sig);
    void fillValueTable(DBC_SIGNAL *sig);
    void generateUsedBits();

    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    void readSettings();
    void writeSettings();
};

#endif // DBCSIGNALEDITOR_H
