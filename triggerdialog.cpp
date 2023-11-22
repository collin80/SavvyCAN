#include "triggerdialog.h"
#include "ui_triggerdialog.h"
#include <QMessageBox>

TriggerDialog::TriggerDialog(QList<Trigger> trigs, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TriggerDialog)
{
    ui->setupUi(this);
    triggers.clear();
    triggers.append(trigs); //make sure it's just a clone of the existing triggers

    //if there are no triggers then create a default one
    if (triggers.count() == 0)
    {
        Trigger trig;
        trig.ID = 0;
        trig.bus = 0;
        trig.maxCount = 0;
        trig.milliseconds = 100;
        trig.sigName = "";
        trig.sigValueDbl = 0.0;
        trig.sigValueInt = 0;
        trig.triggerMask = TriggerMask::TRG_MS;
        triggers.append(trig);
    }
    lastMsgID = 0xFFFFFFFF;
    inhibitUpdates = false;

    dbcHandler = DBCHandler::getReference();
    /*
      getting the messages possible is complicated by the fact that multiple DBC files
      can be loaded and each one can be associated with a different bus. So, this is
      tough. At first I suppose we need to list all possible messages but if the user
      sets a bus then we should recalculate and only show messages possible on that bus.
    */
    ui->cmMsgID->clear();
    QStringList entries;
    int numFiles = dbcHandler->getFileCount();
    for (int i = 0; i < numFiles; i++)
    {
        DBCFile *file = dbcHandler->getFileByIdx(i);
        int numMsgs = file->messageHandler->getCount();
        for (int j = 0; j < numMsgs; j++)
        {
            DBC_MESSAGE *msg = file->messageHandler->findMsgByIdx(j);
            entries.append(("0x" + QString::number(msg->ID, 16) + " ("  + msg->name + ")"));
        }
    }
    entries.sort();
    ui->cmMsgID->addItems(entries);

    connect(ui->listTriggers, &QListWidget::currentRowChanged, this, &TriggerDialog::FillDetails);
    connect(ui->btnDelete, &QPushButton::clicked, this, &TriggerDialog::deleteSelectedTrigger);
    connect(ui->btnNew, &QPushButton::clicked, this, &TriggerDialog::addNewTrigger);
    connect(ui->btnSave, &QPushButton::clicked, this, &TriggerDialog::saveAndExit);
    connect(ui->cbBus, &QCheckBox::stateChanged, this, &TriggerDialog::handleCheckboxes);
    connect(ui->cbMillis, &QCheckBox::stateChanged, this, &TriggerDialog::handleCheckboxes);
    connect(ui->cbMaxCount, &QCheckBox::stateChanged, this, &TriggerDialog::handleCheckboxes);
    connect(ui->cbMsgID, &QCheckBox::stateChanged, this, &TriggerDialog::handleCheckboxes);
    connect(ui->cbSigValue, &QCheckBox::stateChanged, this, &TriggerDialog::handleCheckboxes);
    connect(ui->cbSignal, &QCheckBox::stateChanged, this, &TriggerDialog::handleCheckboxes);
    connect(ui->txtMaxCount, &QLineEdit::textEdited, this, &TriggerDialog::regenerateCurrentListItem);
    connect(ui->cmMsgID, &QComboBox::currentTextChanged, this, &TriggerDialog::regenerateCurrentListItem);
    connect(ui->cmSignal, &QComboBox::currentTextChanged, this, &TriggerDialog::regenerateCurrentListItem);
    connect(ui->txtBus, &QLineEdit::textEdited, this, &TriggerDialog::regenerateCurrentListItem);
    connect(ui->txtDelay, &QLineEdit::textEdited, this, &TriggerDialog::regenerateCurrentListItem);
    connect(ui->txtSigValue, &QLineEdit::textEdited, this, &TriggerDialog::regenerateCurrentListItem);
}

TriggerDialog::~TriggerDialog()
{
    delete ui;
}

QList<Trigger> TriggerDialog::getUpdatedTriggers()
{
    return triggers;
}

void TriggerDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    regenerateList();
}

void TriggerDialog::regenerateList()
{
    ui->listTriggers->clear();
    //iterate over the QList of triggers, show them in the list,
    //load the first item in the list, and display it broken down on the right.
    foreach(Trigger trig, triggers)
    {
        ui->listTriggers->addItem(buildEntry(trig));
    }
    ui->listTriggers->setCurrentRow(0);
    FillDetails();
}

QString TriggerDialog::buildEntry(Trigger trig)
{
    QString build;
    if (trig.triggerMask & TriggerMask::TRG_BUS)
    {
        build = build + "BUS" + QString::number(trig.bus) +  " ";
    }

    if (trig.triggerMask & TriggerMask::TRG_ID)
    {
        build = build + "ID0x" + QString::number(trig.ID, 16) +  " ";
    }

    if (trig.triggerMask & TriggerMask::TRG_SIGNAL)
    {
        build = build + "SIG[" + trig.sigName;
        if (trig.triggerMask & TriggerMask::TRG_SIGVAL)
        {
            build = build + ";" + QString::number(trig.sigValueDbl);
        }
        build = build + "] ";
    }

    if (trig.triggerMask & TriggerMask::TRG_COUNT)
    {
        build = build + QString::number(trig.maxCount) +  "X ";
    }

    if (trig.triggerMask & TriggerMask::TRG_MS)
    {
        build = build + QString::number(trig.milliseconds) +  "MS ";
    }

    return build;
}

void TriggerDialog::handleCheckboxes()
{
    ui->txtBus->setEnabled(ui->cbBus->checkState());
    ui->txtDelay->setEnabled(ui->cbMillis->checkState());
    ui->txtMaxCount->setEnabled(ui->cbMaxCount->checkState());
    ui->cmMsgID->setEnabled(ui->cbMsgID->checkState());
    ui->cmSignal->setEnabled(ui->cbSignal->checkState());
    ui->txtSigValue->setEnabled(ui->cbSigValue->checkState());

    if (ui->cbBus->checkState() == Qt::Unchecked) ui->txtBus->setText("");
    if (ui->cbMillis->checkState() == Qt::Unchecked) ui->txtDelay->setText("");
    if (ui->cbMaxCount->checkState() == Qt::Unchecked) ui->txtMaxCount->setText("");
    if (ui->cbMsgID->checkState() == Qt::Unchecked) ui->cmMsgID->setCurrentText("");
    if (ui->cbSignal->checkState() == Qt::Unchecked) ui->cmSignal->setCurrentText("");
    if (ui->cbSigValue->checkState() == Qt::Unchecked) ui->txtSigValue->setText("");

    regenerateCurrentListItem();
}

//load all the values from the editing screen and place them into the structure
//also, redo the list entry so it shows the proper text
void TriggerDialog::regenerateCurrentListItem()
{
    if (inhibitUpdates) return;
    int row = ui->listTriggers->currentRow();
    Trigger trig;
    trig.ID = 0;
    trig.bus = 0;
    trig.maxCount = 0;
    trig.milliseconds = 0;
    trig.sigName = "";
    trig.sigValueDbl = 0.0;
    trig.triggerMask = 0;

    if (ui->cbMsgID->isChecked() && ui->cmMsgID->currentText().length() > 0)
    {
        trig.triggerMask |= TriggerMask::TRG_ID;
        trig.ID = ui->cmMsgID->currentText()
            .replace("0x", "")
            .split("(")[0]
            .toInt(nullptr, 16);
        if (trig.ID != lastMsgID)
        {
            lastMsgID = trig.ID;
            //now, reload signals combobox
            ui->cmSignal->clear();
            QStringList entries;
            //danger - this isn't doing a great job of making sure we're using
            //the right message. Should at least search by message name instead
            //if possible.
            DBC_MESSAGE *msg = dbcHandler->findMessage(trig.ID);
            int numSigs = msg->sigHandler->getCount();
            for (int s = 0; s < numSigs; s++)
            {
                DBC_SIGNAL *sig = msg->sigHandler->findSignalByIdx(s);

                entries.append(sig->name);
            }
            entries.sort();
            ui->cmSignal->addItems(entries);
        }
    }

    if (ui->cbBus->isChecked() && ui->txtBus ->text().length() > 0)
    {
        trig.triggerMask |= TriggerMask::TRG_BUS;
        trig.bus = ui->txtBus->text().toInt();
    }

    if (ui->cbMillis->isChecked() && ui->txtDelay->text().length() > 0)
    {
        trig.triggerMask |= TriggerMask::TRG_MS;
        trig.milliseconds = ui->txtDelay->text().toInt();
    }

    if (ui->cbMaxCount->isChecked() && ui->txtMaxCount->text().length() > 0)
    {
        trig.triggerMask |= TriggerMask::TRG_COUNT;
        trig.maxCount = ui->txtMaxCount->text().toInt();
    }

    if (ui->cbSigValue->isChecked() && ui->txtSigValue->text().length() > 0)
    {
        trig.triggerMask |= TriggerMask::TRG_SIGVAL;
        trig.sigValueDbl = ui->txtSigValue->text().toDouble();
    }

    if (ui->cbSignal->isChecked() && ui->cmSignal->currentText().length() > 0)
    {
        trig.triggerMask |= TriggerMask::TRG_SIGNAL;
        trig.sigName = ui->cmSignal->currentText().trimmed();
    }

    triggers[row] = trig;
    ui->listTriggers->item(row)->setText(buildEntry(trig));
}

void TriggerDialog::FillDetails()
{
    int selectedRow = ui->listTriggers->currentRow();
    if (selectedRow < 0) return;
    inhibitUpdates = true; //this function makes a lot of changes and they should not trigger the normal updates

    if (triggers[selectedRow].triggerMask & TriggerMask::TRG_BUS)
    {
        ui->txtBus->setEnabled(true);
        ui->txtBus->setText(QString::number(triggers[selectedRow].bus));
        ui->cbBus->setCheckState(Qt::Checked);
    }
    else
    {
        ui->txtBus->setEnabled(false);
        ui->cbBus->setCheckState(Qt::Unchecked);
    }

    if (triggers[selectedRow].triggerMask & TriggerMask::TRG_ID)
    {
        ui->cmMsgID->setEnabled(true);
        ui->cbMsgID->setCheckState(Qt::Checked);
        QString build = "0x" + QString::number(triggers[selectedRow].ID, 16);

        DBC_MESSAGE *msg = dbcHandler->findMessage(triggers[selectedRow].ID);
        if (msg)
        {
            build = build + " ("  + msg->name + ")";
        }
        ui->cmMsgID->setCurrentText(build);
    }
    else
    {
        ui->cmMsgID->setEnabled(false);
        ui->cbMsgID->setCheckState(Qt::Unchecked);
    }

    if (triggers[selectedRow].triggerMask & TriggerMask::TRG_COUNT)
    {
        ui->txtMaxCount->setEnabled(true);
        ui->txtMaxCount->setText(QString::number(triggers[selectedRow].maxCount));
        ui->cbMaxCount->setCheckState(Qt::Checked);
    }
    else
    {
        ui->txtMaxCount->setEnabled(false);
        ui->cbMaxCount->setCheckState(Qt::Unchecked);
    }

    if (triggers[selectedRow].triggerMask & TriggerMask::TRG_MS)
    {
        ui->txtDelay->setEnabled(true);
        ui->txtDelay->setText(QString::number(triggers[selectedRow].milliseconds));
        ui->cbMillis->setCheckState(Qt::Checked);
    }
    else
    {
        ui->txtDelay->setEnabled(false);
        ui->cbMillis->setCheckState(Qt::Unchecked);
    }

    if (triggers[selectedRow].triggerMask & TriggerMask::TRG_SIGNAL)
    {
        ui->cmSignal->setEnabled(true);
        ui->cbSignal->setCheckState(Qt::Checked);
        ui->cmSignal->setCurrentText(triggers[selectedRow].sigName);
    }
    else
    {
        ui->cmSignal->setEnabled(false);
        ui->cbSignal->setCheckState(Qt::Unchecked);
    }

    if (triggers[selectedRow].triggerMask & TriggerMask::TRG_SIGVAL)
    {
        ui->txtSigValue->setEnabled(true);
        ui->txtSigValue->setText(QString::number(triggers[selectedRow].sigValueDbl));
        ui->cbSigValue->setCheckState(Qt::Checked);
    }
    else
    {
        ui->txtSigValue->setEnabled(false);
        ui->cbSigValue->setCheckState(Qt::Unchecked);
    }

    inhibitUpdates = false;
}

void TriggerDialog::addNewTrigger()
{
    Trigger newTrig;
    newTrig.ID = 0;
    newTrig.bus = 0;
    newTrig.maxCount = 0;
    newTrig.milliseconds = 100;
    newTrig.sigName = "";
    newTrig.sigValueDbl = 0.0;
    newTrig.sigValueInt = 0;
    newTrig.triggerMask = TriggerMask::TRG_MS;
    triggers.append(newTrig);
    ui->listTriggers->addItem(buildEntry(newTrig));
}

void TriggerDialog::deleteSelectedTrigger()
{
    if (triggers.count() == 1)
    {
        QMessageBox::information(this, "Error", "Cannot delete the last trigger");
        return;
    }
}

void TriggerDialog::saveAndExit()
{
    accept();
}
