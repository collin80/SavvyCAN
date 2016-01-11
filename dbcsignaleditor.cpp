#include "dbcsignaleditor.h"
#include "ui_dbcsignaleditor.h"
#include <QDateTime>
#include <QDebug>
#include <QMenu>
#include <QSettings>

DBCSignalEditor::DBCSignalEditor(DBCHandler *handler, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCSignalEditor)
{
    ui->setupUi(this);

    readSettings();

    qsrand(QDateTime::currentMSecsSinceEpoch());

    dbcHandler = handler;
    dbcMessage = NULL;
    currentSignal = NULL;

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

    for (int x = 0; x < dbcHandler->dbc_nodes.count(); x++)
    {
        ui->comboReceiver->addItem(dbcHandler->dbc_nodes[x].name);
    }

    connect(ui->signalsList, SIGNAL(currentRowChanged(int)), this, SLOT(clickSignalList(int)));
    connect(ui->bitfield, SIGNAL(gridClicked(int,int)), this, SLOT(bitfieldClicked(int,int)));
    connect(ui->signalsList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onCustomMenuSignals(QPoint)));
    ui->signalsList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->valuesTable, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onCustomMenuValues(QPoint)));
    ui->valuesTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->valuesTable, SIGNAL(cellChanged(int,int)), this, SLOT(onValuesCellChanged(int,int)));

    //now with 100% more lambda expressions just to make it interesting (and shorter, and easier...)
    connect(ui->cbIntelFormat, &QCheckBox::toggled,
            [=]()
            {
                if (currentSignal == NULL) return;
                currentSignal->intelByteOrder = ui->cbIntelFormat->isChecked();
                if (currentSignal->valType == SP_FLOAT || currentSignal->valType == DP_FLOAT)
                    currentSignal->intelByteOrder = false;
                fillSignalForm(currentSignal);
            });

    connect(ui->comboReceiver, &QComboBox::currentTextChanged,
            [=]()
            {
                if (currentSignal == NULL) return;
                currentSignal->receiver = dbcHandler->findNodeByName(ui->comboReceiver->currentText());
            });
    connect(ui->comboType, &QComboBox::currentTextChanged,
            [=]()
            {
                if (currentSignal == NULL) return;
                switch (ui->comboType->currentIndex())
                {
                case 0:
                    currentSignal->valType = UNSIGNED_INT;
                    break;
                case 1:
                    currentSignal->valType = SIGNED_INT;
                    break;
                case 2:
                    currentSignal->valType = SP_FLOAT;
                    currentSignal->intelByteOrder = false;
                    if (currentSignal->startBit > 39) currentSignal->startBit = 39;
                    currentSignal->signalSize = 32;
                    break;
                case 3:
                    currentSignal->valType = DP_FLOAT;
                    currentSignal->intelByteOrder = false;
                    currentSignal->startBit = 7; //has to be!
                    currentSignal->signalSize = 64;
                    break;
                case 4:
                    currentSignal->valType = STRING;
                    break;                    
                }
                fillSignalForm(currentSignal);
            });
    connect(ui->txtBias, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == NULL) return;
                double temp;
                bool result;
                temp = ui->txtBias->text().toDouble(&result);
                if (result) currentSignal->bias = temp;
            });

    connect(ui->txtMaxVal, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == NULL) return;
                double temp;
                bool result;
                temp = ui->txtMaxVal->text().toDouble(&result);
                if (result) currentSignal->max = temp;
            });

    connect(ui->txtMinVal, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == NULL) return;
                double temp;
                bool result;
                temp = ui->txtMinVal->text().toDouble(&result);
                if (result) currentSignal->min = temp;
            });
    connect(ui->txtScale, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == NULL) return;
                double temp;
                bool result;
                temp = ui->txtScale->text().toDouble(&result);
                if (result) currentSignal->factor = temp;
            });
    connect(ui->txtComment, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == NULL) return;
                currentSignal->comment = ui->txtComment->text();
            });

    connect(ui->txtUnitName, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == NULL) return;
                currentSignal->unitName = ui->txtUnitName->text();
            });
    connect(ui->txtBitLength, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == NULL) return;
                int temp;
                temp = Utility::ParseStringToNum(ui->txtBitLength->text());
                if (temp < 0) return;
                if (temp > 63) return;
                if (currentSignal->valType != SP_FLOAT && currentSignal->valType != DP_FLOAT)
                    currentSignal->signalSize = temp;
                fillSignalForm(currentSignal);
            });
    connect(ui->txtName, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == NULL) return;
                currentSignal->name = ui->txtName->text();
                //need to update the list too.
                ui->signalsList->currentItem()->setText(currentSignal->name);
            });

    connect(ui->txtMultiplexValue, &QLineEdit::editingFinished,
            [=]()
            {
                if (currentSignal == NULL) return;
                int temp;
                temp = Utility::ParseStringToNum(ui->txtMultiplexValue->text());
                //TODO: could look up the multiplexor and ensure that the value is within a range that the multiplexor could return
                currentSignal->multiplexValue = temp;
            });
    connect(ui->rbMultiplexed, &QRadioButton::toggled,
            [=](bool state)
            {
                if (state) //signal is now set as a multiplexed signal
                {
                    currentSignal->isMultiplexed = true;
                    currentSignal->isMultiplexor = false;
                    //if the set multiplexor for the message was this signal then clear it
                    if (dbcMessage->multiplexorSignal == currentSignal) dbcMessage->multiplexorSignal = NULL;
                }
            });

    connect(ui->rbMultiplexor, &QRadioButton::toggled,
            [=](bool state)
            {
                if (state) //signal is now set as a multiplexed signal
                {
                    //don't allow this signal to be a multiplexor if there is already one for this message.
                    //if (dbcMessage->multiplexorSignal != currentSignal && dbcMessage->multiplexorSignal != NULL) return; //I spoke too soon above...
                    currentSignal->isMultiplexed = false;
                    currentSignal->isMultiplexor = true;
                    //we just set that this is the multiplexor so update the message to show that as well.
                    dbcMessage->multiplexorSignal = currentSignal;
                }
            });

    connect(ui->rbNotMulti, &QRadioButton::toggled,
            [=](bool state)
            {
                if (state) //signal is now set as a multiplexed signal
                {
                    currentSignal->isMultiplexed = false;
                    currentSignal->isMultiplexor = false;
                    if (dbcMessage->multiplexorSignal == currentSignal) dbcMessage->multiplexorSignal = NULL;
                }
            });
}

DBCSignalEditor::~DBCSignalEditor()
{
    delete ui;
}

void DBCSignalEditor::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
}

void DBCSignalEditor::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("DBCSignalEditor/WindowSize", QSize(800, 572)).toSize());
        move(settings.value("DBCSignalEditor/WindowPos", QPoint(100, 100)).toPoint());
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

void DBCSignalEditor::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    refreshSignalsList();
    currentSignal = NULL;
    if (dbcMessage->msgSignals.count() > 0)
    {        
        currentSignal = &dbcMessage->msgSignals[0];
        fillSignalForm(currentSignal);
        fillValueTable(currentSignal);
    }
}

void DBCSignalEditor::onValuesCellChanged(int row,int col)
{
    if (inhibitCellChanged) return;
    if (row == ui->valuesTable->rowCount() - 1)
    {
        DBC_VAL newVal;
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
        currentSignal->valList[row].descript = ui->valuesTable->item(row, col)->text();
    }
}

void DBCSignalEditor::onCustomMenuSignals(QPoint point)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(tr("Add a new signal"), this, SLOT(addNewSignal()));
    menu->addAction(tr("Delete currently selected signal"), this, SLOT(deleteCurrentSignal()));

    menu->popup(ui->signalsList->mapToGlobal(point));
}

void DBCSignalEditor::onCustomMenuValues(QPoint point)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(tr("Delete currently selected value"), this, SLOT(deleteCurrentValue()));

    menu->popup(ui->valuesTable->mapToGlobal(point));
}

void DBCSignalEditor::addNewSignal()
{
    int num = qrand() % 70000;
    QString newName = "SIGNAL" + QString::number(num);
    DBC_SIGNAL newSig;
    newSig.name = newName;
    newSig.bias = 0.0;
    newSig.factor = 1.0;
    newSig.intelByteOrder = true;
    newSig.max = 0.0;
    newSig.min = 0.0;
    newSig.receiver = &dbcHandler->dbc_nodes[0];
    newSig.signalSize = 1;
    newSig.startBit = 0;
    newSig.valType = UNSIGNED_INT;
    newSig.isMultiplexed = false;
    newSig.isMultiplexor = false;
    newSig.multiplexValue = 0;
    newSig.parentMessage = dbcMessage;
    ui->signalsList->addItem(newName);
    dbcMessage->msgSignals.append(newSig);
    if (dbcMessage->msgSignals.count() == 1) clickSignalList(0);
}

void DBCSignalEditor::deleteCurrentSignal()
{
    int currIdx = ui->signalsList->currentRow();
    if (currIdx > -1)
    {
        delete(ui->signalsList->item(currIdx));
        dbcMessage->msgSignals.removeAt(currIdx);
        currentSignal = NULL;
        currIdx = ui->signalsList->currentRow();
        if (currIdx > -1) currentSignal = &dbcMessage->msgSignals[currIdx];
        fillSignalForm(currentSignal);
        fillValueTable(currentSignal);
    }
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

void DBCSignalEditor::refreshSignalsList()
{
    ui->signalsList->clear();
    for (int x = 0; x < dbcMessage->msgSignals.count(); x++)
    {
        DBC_SIGNAL sig = dbcMessage->msgSignals.at(x);
        ui->signalsList->addItem(sig.name);
    }
}

void DBCSignalEditor::fillSignalForm(DBC_SIGNAL *sig)
{
    unsigned char bitpattern[8];

    generateUsedBits();

    if (sig == NULL)
    {
        ui->txtName->setText("");
        ui->txtBias->setText("");
        ui->txtBitLength->setText("");
        ui->txtComment->setText("");
        ui->txtMaxVal->setText("");
        ui->txtMinVal->setText("");
        ui->txtScale->setText("");
        ui->txtUnitName->setText("");
        ui->txtMultiplexValue->setText("");
        ui->rbMultiplexed->setChecked(false);
        ui->rbMultiplexor->setChecked(false);
        ui->rbNotMulti->setChecked(true);
        memset(bitpattern, 0, 8); //clear it out
        ui->bitfield->setReference(bitpattern, false);
        ui->bitfield->updateData(bitpattern, true);
        ui->comboReceiver->setCurrentIndex(0);
        ui->comboType->setCurrentIndex(0);
        return;
    }

    ui->txtName->setText(sig->name);
    ui->txtBias->setText(QString::number(sig->bias));
    ui->txtBitLength->setText(QString::number(sig->signalSize));
    ui->txtMultiplexValue->setText(QString::number(sig->multiplexValue));
    ui->txtComment->setText(sig->comment);
    ui->txtMaxVal->setText(QString::number(sig->max));
    ui->txtMinVal->setText(QString::number(sig->min));
    ui->txtScale->setText(QString::number(sig->factor));
    ui->txtUnitName->setText(sig->unitName);
    ui->rbMultiplexed->setChecked(sig->isMultiplexed);
    ui->rbMultiplexor->setChecked(sig->isMultiplexor);
    ui->rbNotMulti->setChecked( !(sig->isMultiplexor | sig->isMultiplexed) );
    qDebug() << sig->isMultiplexor << "*" << sig->isMultiplexed;

    memset(bitpattern, 0, 8); //clear it out first.

    int startBit, endBit;

    startBit = sig->startBit;

    bitpattern[startBit / 8] |= 1 << (startBit % 8); //make the start bit a different color to set it apart
    ui->bitfield->setReference(bitpattern, false);

    if (sig->intelByteOrder)
    {
        endBit = startBit + sig->signalSize - 1;
        if (startBit < 0) startBit = 0;
        if (endBit > 63) endBit = 63;
        for (int y = startBit; y <= endBit; y++)
        {
            int byt = y / 8;
            bitpattern[byt] |= 1 << (y % 8);
        }
    }
    else //big endian / motorola format
    {
        //much more irritating than the intel version...
        int size = sig->signalSize;
        while (size > 0)
        {
            int byt = startBit / 8;
            bitpattern[byt] |= 1 << (startBit % 8);
            size--;
            if ((startBit % 8) == 0) startBit += 15;            
            else startBit--;
            if (startBit > 63) startBit = 63;
        }
    }

    ui->bitfield->updateData(bitpattern, true);
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
}

void DBCSignalEditor::fillValueTable(DBC_SIGNAL *sig)
{
    int rowIdx;

    inhibitCellChanged = true;

    ui->valuesTable->clearContents();
    ui->valuesTable->setRowCount(0);

    if (sig == NULL) {
        inhibitCellChanged = false;
        return;
    }

    for (int i = 0; i < sig->valList.count(); i++)
    {
        QTableWidgetItem *val = new QTableWidgetItem(Utility::formatNumber(sig->valList[i].value));
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

void DBCSignalEditor::clickSignalList(int row)
{
    if (row < 0) return;
    //qDebug() << ui->signalsList->item(row)->text();

    DBC_SIGNAL *thisSig = dbcHandler->findSignalByName(dbcMessage, ui->signalsList->item(row)->text());
    if (thisSig == NULL) return;
    currentSignal = thisSig;
    fillSignalForm(thisSig);
    fillValueTable(thisSig);

}

void DBCSignalEditor::bitfieldClicked(int x, int y)
{
    int bit = (7 - x) + (y * 8);
    if (currentSignal == NULL) return;
    currentSignal->startBit = bit;
    if (currentSignal->valType == SP_FLOAT)
    {
        if (currentSignal->startBit > 31) currentSignal->startBit = 39;
    }

    if (currentSignal->valType == DP_FLOAT)
        currentSignal->startBit = 7;
    fillSignalForm(currentSignal);
}

void DBCSignalEditor::generateUsedBits()
{
    uint8_t usedBits[8] = {0,0,0,0,0,0,0,0};
    int startBit, endBit;

    for (int x = 0; x < dbcMessage->msgSignals.count(); x++)
    {
        DBC_SIGNAL sig = dbcMessage->msgSignals.at(x);

        startBit = sig.startBit;

        if (sig.intelByteOrder)
        {
            endBit = startBit + sig.signalSize - 1;
            if (startBit < 0) startBit = 0;
            if (endBit > 63) endBit = 63;
            for (int y = startBit; y <= endBit; y++)
            {
                int byt = y / 8;
                usedBits[byt] |= 1 << (y % 8);
            }
        }
        else //big endian / motorola format
        {
            //much more irritating than the intel version...
            int size = sig.signalSize;
            while (size > 0)
            {
                int byt = startBit / 8;
                usedBits[byt] |= 1 << (startBit % 8);
                size--;
                if ((startBit % 8) == 0) startBit += 15;
                else startBit--;
                if (startBit > 63) startBit = 63;
            }
        }
    }
    ui->bitfield->setUsed(usedBits, false);
}
