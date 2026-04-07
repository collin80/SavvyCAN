#include "dbcsignaleditor.h"
#include "ui_dbcsignaleditor.h"
#include <QDateTime>
#include <QDebug>
#include <QMenu>
#include <QSettings>
#include <QRandomGenerator>
#include <QMessageBox>
#include <qevent.h>
#include "helpwindow.h"

DBCSignalEditor::DBCSignalEditor(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCSignalEditor)
{
    ui->setupUi(this);

    readSettings();

    dbcHandler = DBCHandler::getReference();
    dbcMessage = nullptr;
    currentSignal = nullptr;
    inhibitMsgProc = false;

    QStringList headers2;
    headers2 << "Value" << "Text";
    ui->valuesTable->setColumnCount(2);
    ui->valuesTable->setColumnWidth(0, 200);
    ui->valuesTable->setColumnWidth(1, 440);
    ui->valuesTable->setHorizontalHeaderLabels(headers2);
    ui->valuesTable->horizontalHeader()->setStretchLastSection(true);

    ui->comboType->addItem("UNSIGNED INTEGER");
    ui->comboType->addItem("SIGNED INTEGER");
    ui->comboType->addItem("SINGLE PRECISION");
    ui->comboType->addItem("DOUBLE PRECISION");
    ui->comboType->addItem("STRING");

    ui->bitfield->setMode(GridMode::SIGNAL_VIEW);

    connect(ui->bitfield, SIGNAL(gridClicked(int)), this, SLOT(bitfieldLeftClicked(int)));
    connect(ui->bitfield, SIGNAL(gridRightClicked(int)), this, SLOT(bitfieldRightClicked(int)));

    connect(ui->valuesTable, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onCustomMenuValues(QPoint)));
    ui->valuesTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->valuesTable, SIGNAL(cellChanged(int,int)), this, SLOT(onValuesCellChanged(int,int)));

    //now with 100% more lambda expressions just to make it interesting (and shorter, and easier...)
    connect(ui->cbIntelFormat, &QCheckBox::toggled,
            [=]()
            {
                if (currentSignal == nullptr) return;
                if (currentSignal->intelByteOrder != ui->cbIntelFormat->isChecked())
                {
                    dbcFile->setDirtyFlag();
                    pushToUndoBuffer();
                    currentSignal->intelByteOrder = ui->cbIntelFormat->isChecked();
                    //fillSignalForm(currentSignal);
                    refreshBitGrid();
                }
            });

    connect(ui->comboReceiver, &QComboBox::currentTextChanged,
            [=]()
            {
                if (currentSignal == nullptr) return;
                if (inhibitMsgProc) return;

                DBC_NODE *node = dbcFile->findNodeByName(ui->comboReceiver->currentText());
                if (currentSignal->receiver != node)
                {
                    dbcFile->setDirtyFlag();
                    pushToUndoBuffer();
                    currentSignal->receiver = node;
                }
            });
    connect(ui->comboType, &QComboBox::currentTextChanged,
            [=]()
            {
                if (currentSignal == nullptr) return;
                switch (ui->comboType->currentIndex())
                {
                case 0:
                    if (currentSignal->valType != UNSIGNED_INT)
                    {
                        pushToUndoBuffer();
                        currentSignal->valType = UNSIGNED_INT;
                        dbcFile->setDirtyFlag();
                        fillSignalForm(currentSignal);
                    }
                    break;
                case 1:
                    if (currentSignal->valType != SIGNED_INT)
                    {
                        pushToUndoBuffer();
                        currentSignal->valType = SIGNED_INT;
                        dbcFile->setDirtyFlag();
                        fillSignalForm(currentSignal);
                    }
                    break;
                case 2:
                    if (currentSignal->valType != SP_FLOAT)
                    {
                        pushToUndoBuffer();
                        currentSignal->valType = SP_FLOAT;
                        dbcFile->setDirtyFlag();
                        if (dbcMessage) //if we have a good msg reference we can use it to get the # of bytes expected.
                        {
                            int maxBit = ((dbcMessage->len * 8) - 32 + 7);
                            if (maxBit < 0) maxBit = 0;
                            if (currentSignal->startBit > maxBit) currentSignal->startBit = maxBit;
                        }
                        else if (currentSignal->startBit > 39) currentSignal->startBit = 39;
                        currentSignal->signalSize = 32;
                        fillSignalForm(currentSignal);
                    }
                    break;
                case 3:
                    if (currentSignal->valType != DP_FLOAT)
                    {
                        pushToUndoBuffer();
                        currentSignal->valType = DP_FLOAT;
                        dbcFile->setDirtyFlag();
                        if (dbcMessage)
                        {
                            int maxBit = ((dbcMessage->len * 8) - 64 + 7);
                            if (currentSignal->startBit > maxBit) currentSignal->startBit = maxBit;
                        }
                        else currentSignal->startBit = 7; //has to be!
                        currentSignal->signalSize = 64;
                        fillSignalForm(currentSignal);
                    }
                    break;
                case 4:
                    if (currentSignal->valType != STRING)
                    {
                        pushToUndoBuffer();
                        currentSignal->valType = STRING;
                        dbcFile->setDirtyFlag();
                        fillSignalForm(currentSignal);
                    }
                    break;
                }
            });
    connect(ui->txtBias, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == nullptr) return;
                double temp;
                bool result;
                temp = ui->txtBias->text().toDouble(&result);
                if (result)
                {
                    if (currentSignal->bias != temp)
                    {
                        pushToUndoBuffer();
                        dbcFile->setDirtyFlag();
                        currentSignal->bias = temp;
                    }
                }
            });

    connect(ui->txtMaxVal, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == nullptr) return;
                double temp;
                bool result;
                temp = ui->txtMaxVal->text().toDouble(&result);
                if (result)
                {
                    if (currentSignal->max != temp)
                    {
                        pushToUndoBuffer();
                        dbcFile->setDirtyFlag();
                        currentSignal->max = temp;
                    }
                }
            });

    connect(ui->txtMinVal, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == nullptr) return;
                double temp;
                bool result;
                temp = ui->txtMinVal->text().toDouble(&result);
                if (result)
                {
                    if (currentSignal->min != temp)
                    {
                        pushToUndoBuffer();
                        dbcFile->setDirtyFlag();
                        currentSignal->min = temp;
                    }
                }
            });

    connect(ui->txtScale, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == nullptr) return;
                double temp;
                bool result;
                temp = ui->txtScale->text().toDouble(&result);
                if (result)
                {
                    if (currentSignal->factor != temp)
                    {
                        pushToUndoBuffer();
                        dbcFile->setDirtyFlag();
                        currentSignal->factor = temp;
                    }
                }
            });

    connect(ui->txtComment, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == nullptr) return;
                if (currentSignal->comment != ui->txtComment->text().simplified().replace(' ','_'))
                {
                    pushToUndoBuffer();
                    dbcFile->setDirtyFlag();
                    currentSignal->comment = ui->txtComment->text().simplified().replace(' ', '_');
                    emit updatedTreeInfo(currentSignal);
                }
            });

    connect(ui->txtUnitName, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == nullptr) return;
                if (currentSignal->unitName != ui->txtUnitName->text().simplified().replace(' ','_'))
                {
                    pushToUndoBuffer();
                    dbcFile->setDirtyFlag();
                    currentSignal->unitName = ui->txtUnitName->text().simplified().replace(' ', '_');
                }
            });

    connect(ui->txtBitLength, &QLineEdit::textChanged,
            [=]()
            {
                if (currentSignal == nullptr) return;
                int temp;
                temp = Utility::ParseStringToNum(ui->txtBitLength->text());
                if (temp < 1) return;
                if (dbcMessage)
                {
                    if (temp > (int)(dbcMessage->len * 8)) return;
                }
                else if (temp > 64) return;

                if (currentSignal->valType == SP_FLOAT) temp = 32;
                if (currentSignal->valType == DP_FLOAT) temp = 64;

                if (currentSignal->signalSize != temp)
                {
                    pushToUndoBuffer();
                    dbcFile->setDirtyFlag();
                    currentSignal->signalSize = temp;
                    //fillSignalForm(currentSignal);
                    refreshBitGrid();
                }
            });

    connect(ui->txtName, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == nullptr) return;
                QString tempNameStr = ui->txtName->text().simplified().replace(' ', '_');
                if (tempNameStr.length() == 0) return; //can't do that!
                if (currentSignal->name != tempNameStr)
                {
                    pushToUndoBuffer();
                    dbcFile->setDirtyFlag();
                    currentSignal->name = tempNameStr;
                    refreshBitGrid();
                    //need to update the tree too.
                    emit updatedTreeInfo(currentSignal);
                }
            });

    connect(ui->txtMultiplexValues, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == nullptr) return;
                if (currentSignal->multiplexDbcString(DBC_SIGNAL::MuxStringFormat_UI) != ui->txtMultiplexValues->text())
                {
                    //TODO: could look up the multiplexor and ensure that the value is within a range that the multiplexor could return
                    QString errorString;
                    if (!currentSignal->parseDbcMultiplexUiString(ui->txtMultiplexValues->text(), errorString)) {
                        QMessageBox::critical(this, tr("Error"), tr("The multiplex values field contains errors:\n%1").arg(errorString));
                    } else {
                        pushToUndoBuffer();
                        dbcFile->setDirtyFlag();
                    }
                }
            });

    connect(ui->rbExtended, &QRadioButton::toggled,
            [=](bool state)
            {
                if (!currentSignal) return;
                if (!state) return; //we only need to handle the case where it is true

                //only do anything if this is different from the current state. It should be because we're in a toggle event but let's be sure
                if (!currentSignal->isMultiplexed || !currentSignal->isMultiplexor)
                {
                    pushToUndoBuffer();
                    dbcFile->setDirtyFlag();
                    currentSignal->isMultiplexed = true;
                    currentSignal->isMultiplexor = true;
                    //an extended multi signal cannot be the root multiplexor for a message so make sure to remove it if it was.
                    if (dbcMessage->multiplexorSignal == currentSignal) dbcMessage->multiplexorSignal = nullptr;
                    ui->txtMultiplexValues->setEnabled(currentSignal->isMultiplexed);
                    ui->cbMultiplexParent->setEnabled(currentSignal->isMultiplexed);
                    fillSignalForm(currentSignal);
                }
            });

    connect(ui->rbMultiplexed, &QRadioButton::toggled,
            [=](bool state)
            {
                if (!currentSignal) return;
                if (!state) return; //we only need to handle the case where it is true

                //only do anything if this is different from the current state. It should be because we're in a toggle event but let's be sure
                if (!currentSignal->isMultiplexed || currentSignal->isMultiplexor)
                {
                    pushToUndoBuffer();
                    dbcFile->setDirtyFlag();
                    currentSignal->isMultiplexed = true;
                    currentSignal->isMultiplexor = false;
                    //if the set multiplexor for the message was this signal then clear it
                    if (dbcMessage->multiplexorSignal == currentSignal) dbcMessage->multiplexorSignal = nullptr;
                    ui->txtMultiplexValues->setEnabled(currentSignal->isMultiplexed);
                    ui->cbMultiplexParent->setEnabled(currentSignal->isMultiplexed);
                    fillSignalForm(currentSignal);
                }
            });

    connect(ui->rbMultiplexor, &QRadioButton::toggled,
            [=](bool state)
            {
                if (!currentSignal) return;
                if (!state) return; //we only need to handle the case where it is true

                if (currentSignal->isMultiplexed || !currentSignal->isMultiplexor)
                {
                    pushToUndoBuffer();
                    dbcFile->setDirtyFlag();
                    currentSignal->isMultiplexed = false;
                    currentSignal->isMultiplexor = true;
                    //we just set that this is the multiplexor so update the message to show that as well.
                    dbcMessage->multiplexorSignal = currentSignal;
                    ui->txtMultiplexValues->setEnabled(currentSignal->isMultiplexed);
                    ui->cbMultiplexParent->setEnabled(currentSignal->isMultiplexed);
                    fillSignalForm(currentSignal);
                }
            });

    connect(ui->rbNotMulti, &QRadioButton::toggled,
            [=](bool state)
            {
                if (!currentSignal) return;
                if (!state) return; //we only need to handle the case where it is true

                if (currentSignal->isMultiplexed || currentSignal->isMultiplexor)
                {
                    pushToUndoBuffer();
                    dbcFile->setDirtyFlag();
                    currentSignal->isMultiplexed = false;
                    currentSignal->isMultiplexor = false;
                    if (dbcMessage->multiplexorSignal == currentSignal) dbcMessage->multiplexorSignal = nullptr;
                    ui->txtMultiplexValues->setEnabled(currentSignal->isMultiplexed);
                    ui->cbMultiplexParent->setEnabled(currentSignal->isMultiplexed);
                    fillSignalForm(currentSignal);
                }
            });

    connect(ui->cbMultiplexParent, &QComboBox::textActivated,
            [=]()
            {
                if (currentSignal == nullptr) return;
                if (inhibitMsgProc) return;

                //qDebug() << "Curr text: :" << ui->cbMultiplexParent->currentText();
                //try to look up the signal that we're set to now, remove this signal from existing children list
                //add it to this one, update this signal's parent multiplexor
                DBC_SIGNAL *newSig = dbcMessage->sigHandler->findSignalByName(ui->cbMultiplexParent->currentText());
                DBC_SIGNAL *oldParent = currentSignal->multiplexParent;
                if (newSig && oldParent && (newSig != oldParent))
                {
                    pushToUndoBuffer();
                    dbcFile->setDirtyFlag();
                    oldParent->multiplexedChildren.removeOne(currentSignal);
                    currentSignal->multiplexParent = newSig;
                    newSig->multiplexedChildren.append(currentSignal);
                    refreshBitGrid();
                    emit updatedTreeInfo(currentSignal);
                }
            });

    installEventFilter(this);
}

DBCSignalEditor::~DBCSignalEditor()
{
    removeEventFilter(this);
    delete ui;
}

void DBCSignalEditor::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
}

bool DBCSignalEditor::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("signaleditor.md");
            break;
        case Qt::Key_Z:
            if (keyEvent->modifiers() == Qt::ControlModifier)
            {
                popFromUndoBuffer();
            }
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void DBCSignalEditor::setFileIdx(int idx)
{
    if (idx < 0 || idx > dbcHandler->getFileCount() - 1) return;
    dbcFile = dbcHandler->getFileByIdx(idx);

    for (int x = 0; x < dbcFile->dbc_nodes.count(); x++)
    {
        ui->comboReceiver->addItem(dbcFile->dbc_nodes[x]->name);
    }
}

void DBCSignalEditor::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("DBCSignalEditor/WindowSize", QSize(1000, 600)).toSize());
        move(Utility::constrainedWindowPos(settings.value("DBCSignalEditor/WindowPos", QPoint(100, 100)).toPoint()));
    }
}

void DBCSignalEditor::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("DBCSignalEditor/WindowSize", size());
        settings.setValue("DBCSignalEditor/WindowPos", pos());
    }
}


void DBCSignalEditor::setMessageRef(DBC_MESSAGE *msg)
{
    dbcMessage = msg;
}

void DBCSignalEditor::setSignalRef(DBC_SIGNAL *sig)
{
    currentSignal = sig;
}


void DBCSignalEditor::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    fillSignalForm(currentSignal);
    fillValueTable(currentSignal);
}

void DBCSignalEditor::refreshView()
{
    fillSignalForm(currentSignal);
    fillValueTable(currentSignal);
}

void DBCSignalEditor::onValuesCellChanged(int row,int col)
{
    if (inhibitCellChanged) return;

    if (row == ui->valuesTable->rowCount() - 1)
    {
        DBC_VAL_ENUM_ENTRY newVal;
        newVal.value = 0;
        newVal.descript = "No Description";
        currentSignal->valList.append(newVal);
        qDebug() << "Created new entry in value list";

        //QTableWidgetItem *widgetVal = new QTableWidgetItem(QString::number(newVal.value));
        //ui->valuesTable->setItem(row, 0, widgetVal);
        //QTableWidgetItem *widgetDesc = new QTableWidgetItem(newVal.descript);
        //ui->valuesTable->setItem(row, 1, widgetDesc);

        //add the blank at the end again
        ui->valuesTable->insertRow(ui->valuesTable->rowCount());
    }

    if (col == 0)
    {
        currentSignal->valList[row].value = Utility::ParseStringToNum(ui->valuesTable->item(row, col)->text());
    }
    else if (col == 1)
    {
        currentSignal->valList[row].descript = ui->valuesTable->item(row, col)->text().simplified().replace(' ', '_');
    }
}

void DBCSignalEditor::onCustomMenuValues(QPoint point)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(tr("Delete currently selected value"), this, SLOT(deleteCurrentValue()));

    menu->popup(ui->valuesTable->mapToGlobal(point));
}

void DBCSignalEditor::deleteCurrentValue()
{
    int currIdx = ui->valuesTable->currentRow();
    if (currIdx > -1)
    {
        ui->valuesTable->removeRow(currIdx);
        currentSignal->valList.removeAt(currIdx);
    }
}

/* fillSignalForm also handles group "enabled" state */
/* WARNING: fillSignalForm can be called recursively since it is in the listener of cbIntelFormat */
void DBCSignalEditor::fillSignalForm(DBC_SIGNAL *sig)
{
    //sanity checks
    if (!dbcMessage) return;
    if (!dbcMessage->sigHandler) return;

    inhibitMsgProc = true;

    if (sig == nullptr) {
        //ui->groupBox->setEnabled(false);
        ui->txtName->setText("");
        ui->txtBias->setText("");
        ui->txtBitLength->setText("");
        ui->txtComment->setText("");
        ui->txtMaxVal->setText("");
        ui->txtMinVal->setText("");
        ui->txtScale->setText("");
        ui->txtUnitName->setText("");
        ui->txtMultiplexValues->setText("");
        ui->rbMultiplexed->setChecked(false);
        ui->rbMultiplexor->setChecked(false);
        ui->rbNotMulti->setChecked(true);
        ui->comboReceiver->setCurrentIndex(0);
        ui->comboType->setCurrentIndex(0);
        inhibitMsgProc = false;
        return;
    }

    /* we have a signal */
    //ui->groupBox->setEnabled(true);

    ui->txtName->setText(sig->name);
    ui->txtBias->setText(QString::number(sig->bias));
    ui->txtBitLength->setText(QString::number(sig->signalSize));
    ui->txtMultiplexValues->setText(sig->multiplexDbcString(DBC_SIGNAL::MuxStringFormat_UI));
    ui->txtComment->setText(sig->comment);
    ui->txtMaxVal->setText(QString::number(sig->max));
    ui->txtMinVal->setText(QString::number(sig->min));
    ui->txtScale->setText(QString::number(sig->factor));
    ui->txtUnitName->setText(sig->unitName);
    if (sig->isMultiplexed && sig->isMultiplexor)
    {
        ui->rbMultiplexed->setChecked(false);
        ui->rbMultiplexor->setChecked(false);
        ui->rbNotMulti->setChecked(false);
        ui->rbExtended->setChecked(true);
    }
    else
    {
        ui->rbMultiplexed->setChecked(sig->isMultiplexed);
        ui->rbMultiplexor->setChecked(sig->isMultiplexor);
        ui->rbNotMulti->setChecked( !(sig->isMultiplexor | sig->isMultiplexed) );
    }
    qDebug() << sig->isMultiplexor << "*" << sig->isMultiplexed;

    ui->cbMultiplexParent->clear();

    int numSigs = dbcMessage->sigHandler->getCount();
    QList<DBC_SIGNAL *> sigs = dbcMessage->sigHandler->getSignalsAsList();
    for (int i = 0; i < numSigs; i++)
    {
        DBC_SIGNAL *sig_iter = sigs[i];
        if (sig_iter && sig_iter->isMultiplexor && (sig_iter != sig))
        {
            //only add this entry if there are no other entries with that name yet
            if (ui->cbMultiplexParent->findText(sig_iter->name) == -1)
                ui->cbMultiplexParent->addItem(sig_iter->name);
            if (sig->multiplexParent == sig_iter) ui->cbMultiplexParent->setCurrentIndex(ui->cbMultiplexParent->count() - 1);
        }
    }

    ui->txtMultiplexValues->setEnabled(sig->isMultiplexed);
    ui->cbMultiplexParent->setEnabled(sig->isMultiplexed);

    ui->cbIntelFormat->setChecked(sig->intelByteOrder);

    switch (sig->valType)
    {
    case UNSIGNED_INT:
        ui->comboType->setCurrentIndex(0);
        break;
    case SIGNED_INT:
        ui->comboType->setCurrentIndex(1);
        break;
    case SP_FLOAT:
        ui->comboType->setCurrentIndex(2);
        break;
    case DP_FLOAT:
        ui->comboType->setCurrentIndex(3);
        break;
    case STRING:
        ui->comboType->setCurrentIndex(4);
        break;
    }

    for (int i = 0; i < ui->comboReceiver->count(); i++)
    {
        if (ui->comboReceiver->itemText(i) == sig->receiver->name)
        {
            ui->comboReceiver->setCurrentIndex(i);
            break;
        }
    }

    refreshBitGrid();

    inhibitMsgProc = false;
}

void DBCSignalEditor::refreshBitGrid()
{
    unsigned char bitpattern[64];

    memset(bitpattern, 0, 64); //clear it out
    ui->bitfield->setReference(bitpattern, false);
    ui->bitfield->updateData(bitpattern, true);

    /*ui->bitfield->clearSignalNames();
    QList<DBC_SIGNAL *> sigs = dbcMessage->sigHandler->getSignalsAsList();
    for (int x = 0; x < dbcMessage->sigHandler->getCount(); x++)
    {
        DBC_SIGNAL *sig = sigs[x];
        //only set a signal name for signals which match multiplexparent with our currentsignal
        if (!sig->multiplexParent || ((sig->multiplexParent == currentSignal->multiplexParent) && (sig->multiplexesIdenticalToSignal(currentSignal)) ))
        {
            ui->bitfield->setSignalNames(x, sig->name);
            //qDebug() << sig->name << sig->multiplexParent;
        }
    }*/

    generateUsedBits();
    memset(bitpattern, 0, 64); //clear it out first.

    int startBit, endBit;

    startBit = currentSignal->startBit;

    bitpattern[startBit / 8] |= 1 << (startBit % 8); //make the start bit a different color to set it apart
    ui->bitfield->setReference(bitpattern, false);

    if (currentSignal->intelByteOrder)
    {
        endBit = startBit + currentSignal->signalSize - 1;
        if (startBit < 0) startBit = 0;
        if (endBit > 511) endBit = 511;
        for (int y = startBit; y <= endBit; y++)
        {
            int byt = y / 8;
            bitpattern[byt] |= 1 << (y % 8);
        }
    }
    else //big endian / motorola format
    {
        //much more irritating than the intel version...
        int size = currentSignal->signalSize;
        while (size > 0)
        {
            int byt = startBit / 8;
            bitpattern[byt] |= 1 << (startBit % 8);
            size--;
            if ((startBit % 8) == 0) startBit += 15;
            else startBit--;
            if (startBit > 511) startBit = 511;
        }
    }

    ui->bitfield->updateData(bitpattern, true);

}

/* fillValueTable also handles "enabled" state */
void DBCSignalEditor::fillValueTable(DBC_SIGNAL *sig)
{
    int rowIdx;

    inhibitCellChanged = true;

    ui->valuesTable->clearContents();
    ui->valuesTable->setRowCount(0);

    if (sig == nullptr) {
        ui->valuesTable->setEnabled(false);
        inhibitCellChanged = false;
        return;
    }

    ui->valuesTable->setEnabled(true);

    for (int i = 0; i < sig->valList.count(); i++)
    {
        QTableWidgetItem *val = new QTableWidgetItem(Utility::formatNumber((uint64_t)sig->valList[i].value));
        QTableWidgetItem *desc = new QTableWidgetItem(sig->valList[i].descript);
        rowIdx = ui->valuesTable->rowCount();
        ui->valuesTable->insertRow(rowIdx);
        ui->valuesTable->setItem(rowIdx, 0, val);
        ui->valuesTable->setItem(rowIdx, 1, desc);
    }

    //Add a blank row at the end to allow for adding records easily
    ui->valuesTable->insertRow(ui->valuesTable->rowCount());

    inhibitCellChanged = false;
}

//Left clicking the grid sets the "starting bit" for the current signal
void DBCSignalEditor::bitfieldLeftClicked(int bit)
{
    if (currentSignal == nullptr) return;

    pushToUndoBuffer();
    currentSignal->startBit = bit;
    if (currentSignal->valType == SP_FLOAT)
    {
        if (dbcMessage)
        {
            int maxBit = ((dbcMessage->len * 8) - 32 + 7);
            if (maxBit < 0) maxBit = 0;
            if (currentSignal->startBit > maxBit) currentSignal->startBit = maxBit;
        }
        else if (currentSignal->startBit > 31) currentSignal->startBit = 39;
    }

    if (currentSignal->valType == DP_FLOAT)
    {
        if (dbcMessage)
        {
            int maxBit = ((dbcMessage->len * 8) - 64 + 7);
            if (maxBit < 0) maxBit = 0;
            if (currentSignal->startBit > maxBit) currentSignal->startBit = maxBit;
        }
        else currentSignal->startBit = 7;
    }
    fillSignalForm(currentSignal);
}

//Right clicking the grid starts editing on whichever signal currently "owns" that bit.
//If there is no other signal then nothing happens (right now).
//Would be possible to create a new signal in that case
void DBCSignalEditor::bitfieldRightClicked(int bit)
{
    //will return -1 if there is no signal there. Otherwise, returns signal number
    //which is quite luckily also the index into the signal handler table
    QString sigName = ui->bitfield->getUsedSignalName(bit);
    if (sigName.length() < 1) return;

    pushToUndoBuffer(); // undo to resume editing the previous signal

    currentSignal = dbcMessage->sigHandler->findSignalByName(sigName);

    if (currentSignal)
    {
        fillSignalForm(currentSignal);
        fillValueTable(currentSignal);
    }
}


void DBCSignalEditor::generateUsedBits()
{
    uint8_t usedBits[64];
    int startBit, endBit;

    memset(usedBits, 0, 64);

    if (!dbcMessage || !dbcMessage->sigHandler) return;

    QList<DBC_SIGNAL *> sigs = dbcMessage->sigHandler->getSignalsAsList();

    for (int x = 0; x < dbcMessage->sigHandler->getCount(); x++)
    {
        DBC_SIGNAL *sig = sigs[x];

        //only pay attention to this signal if it's multiplexParent matches currentSignal or is null

        if (sig->multiplexParent)
        {
            if (sig->multiplexParent != currentSignal->multiplexParent) continue; //go thee away!
            if (!sig->multiplexesIdenticalToSignal(currentSignal)) continue; //buzz off
        }
        startBit = sig->startBit;

        if (sig->intelByteOrder)
        {
            endBit = startBit + sig->signalSize - 1;
            if (startBit < 0) startBit = 0;
            int maxBit = (dbcMessage->len * 8) - 1;
            if (endBit > maxBit) endBit = maxBit;
            for (int y = startBit; y <= endBit; y++)
            {
                int byt = y / 8;
                usedBits[byt] |= 1 << (y % 8);
                ui->bitfield->setUsedSignalName(y, sig->name);
            }
        }
        else //big endian / motorola format
        {
            //much more irritating than the intel version...
            int size = sig->signalSize;
            while (size > 0)
            {
                int byt = startBit / 8;
                usedBits[byt] |= 1 << (startBit % 8);
                ui->bitfield->setUsedSignalName(startBit, sig->name);
                size--;
                if ((startBit % 8) == 0) startBit += 15;
                else startBit--;
                int maxBit = (dbcMessage->len * 8) - 1;
                if (startBit > maxBit) startBit = maxBit;
            }
        }
    }
    ui->bitfield->setUsed(usedBits, false);
    ui->bitfield->setBytesToDraw(dbcMessage->len);
}

//Copy the current signal in its entirety to the undo buffer. Just for safe keeping
//Called before an edit is done to save the state so we can revert if necessary
void DBCSignalEditor::pushToUndoBuffer()
{
    if (!currentSignal) return;
    //store a copy of the pointer so that if we need to pop we can pop to the proper place
    currentSignal->self = currentSignal;
    undoBuffer.append(*currentSignal); //save the whole thing
    qDebug() << "Pushing to undo buffer";
}

//Pop the last copy of a signal from the stack and begin editing it
void DBCSignalEditor::popFromUndoBuffer()
{
    if (undoBuffer.empty())
    {
        dbcFile->clearDirtyFlag(); //TODO: Don't do this. Implement per-item dirty flags.
        qDebug() << "Undo buffer empty";
        return; //can't pop if there are no stored entries!
    }
    qDebug() << "Popping undo buffer";
    DBC_SIGNAL sig = undoBuffer.back();
    undoBuffer.pop_back();
    currentSignal = sig.self; //restore the pointer
    *currentSignal = sig; //write the contents into the memory pointed to

    fillSignalForm(currentSignal);
    fillValueTable(currentSignal);
}
