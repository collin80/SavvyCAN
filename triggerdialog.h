#ifndef TRIGGERDIALOG_H
#define TRIGGERDIALOG_H

#include <QDialog>
#include "can_trigger_structs.h"
#include "dbc/dbchandler.h"

namespace Ui {
class TriggerDialog;
}

class TriggerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TriggerDialog(QList<Trigger> triggers, QWidget *parent = nullptr);
    QList<Trigger> getUpdatedTriggers();
    void showEvent(QShowEvent*);
    ~TriggerDialog();
    QString buildEntry(Trigger trig);

private:
    Ui::TriggerDialog *ui;
    DBCHandler *dbcHandler;
    int lastMsgID;
    bool inhibitUpdates;

    QList<Trigger> triggers;

    void FillDetails();
    void addNewTrigger();
    void deleteSelectedTrigger();
    void saveAndExit();
    void regenerateList();
    void handleCheckboxes();
    void regenerateCurrentListItem();
};

#endif // TRIGGERDIALOG_H
