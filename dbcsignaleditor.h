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
    explicit DBCSignalEditor(DBCHandler *handler, QWidget *parent = 0);
    void setMessageRef(DBC_MESSAGE *msg);
    void showEvent(QShowEvent*);
    ~DBCSignalEditor();

private slots:
    void clickSignalList(int);
    void bitfieldClicked(int x, int y);
    void onValuesCellChanged(int row,int col);
    void onCustomMenuSignals(QPoint);
    void onCustomMenuValues(QPoint);
    void addNewSignal();
    void deleteCurrentSignal();
    void deleteCurrentValue();

private:
    Ui::DBCSignalEditor *ui;
    DBCHandler *dbcHandler;
    DBC_MESSAGE *dbcMessage;
    DBC_SIGNAL *currentSignal;
    bool inhibitCellChanged;

    void refreshSignalsList();
    void fillSignalForm(DBC_SIGNAL *sig);
    void fillValueTable(DBC_SIGNAL *sig);
    void generateUsedBits();

    void closeEvent(QCloseEvent *event);
    void readSettings();
    void writeSettings();
};

#endif // DBCSIGNALEDITOR_H
