#ifndef DBCSIGNALEDITOR_H
#define DBCSIGNALEDITOR_H

#include <QDialog>
#include "dbchandler.h"

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

private:
    Ui::DBCSignalEditor *ui;
    DBCHandler *dbcHandler;
    DBC_MESSAGE *dbcMessage;

    void refreshSignalsList();
    void fillSignalForm(DBC_SIGNAL *sig);
    void fillValueTable(DBC_SIGNAL *sig);
};

#endif // DBCSIGNALEDITOR_H
