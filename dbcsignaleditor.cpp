#include "dbcsignaleditor.h"
#include "ui_dbcsignaleditor.h"
#include <QDebug>
#include <QSettings>

DBCSignalEditor::DBCSignalEditor(DBCHandler *handler, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCSignalEditor)
{
    ui->setupUi(this);

    readSettings();

    dbcHandler = handler;
    dbcMessage = NULL;

    QStringList headers2;
    headers2 << "Value" << "Text";
    ui->valuesTable->setColumnCount(2);
    ui->valuesTable->setColumnWidth(0, 200);
    ui->valuesTable->setColumnWidth(1, 440);
    ui->valuesTable->setHorizontalHeaderLabels(headers2);
    ui->valuesTable->horizontalHeader()->setStretchLastSection(true);

    ui->comboType->addItem("INTEGER");
    ui->comboType->addItem("SINGLE PRECISION");
    ui->comboType->addItem("DOUBLE PRECISION");
    ui->comboType->addItem("STRING");

    for (int x = 0; x < dbcHandler->dbc_nodes.count(); x++)
    {
        ui->comboReceiver->addItem(dbcHandler->dbc_nodes[x].name);
    }

    connect(ui->signalsList, SIGNAL(currentRowChanged(int)), this, SLOT(clickSignalList(int)));
    connect(ui->bitfield, SIGNAL(gridClicked(int,int)), this, SLOT(bitfieldClicked(int,int)));
}

DBCSignalEditor::~DBCSignalEditor()
{
    delete ui;
}

void DBCSignalEditor::closeEvent(QCloseEvent *event)
{
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
    if (dbcMessage->msgSignals.count() > 0)
    {
        fillSignalForm(&dbcMessage->msgSignals[0]);
        fillValueTable(&dbcMessage->msgSignals[0]);
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

    ui->txtName->setText(sig->name);
    ui->txtBias->setText(QString::number(sig->bias));
    ui->txtBitLength->setText(QString::number(sig->signalSize));
    ui->txtComment->setText(sig->comment);
    ui->txtMaxVal->setText(QString::number(sig->max));
    ui->txtMinVal->setText(QString::number(sig->min));
    ui->txtScale->setText(QString::number(sig->factor));
    ui->txtUnitName->setText(sig->unitName);

    memset(bitpattern, 0, 8); //clear it out first.
    int startBit, endBit, startByte, bitWithinByteStart;

    startBit = sig->startBit;
    startByte = startBit / 8;
    bitWithinByteStart = startBit % 8;
    if (sig->intelByteOrder)
    {
        bitWithinByteStart = 7 - bitWithinByteStart;
        startBit = (startByte * 8) + bitWithinByteStart;
    }

    endBit = startBit + sig->signalSize - 1;
    if (endBit > 63)
    {
        endBit = 63;
    }

    bitpattern[startBit / 8] |= 1 << (startBit % 8);
    ui->bitfield->setReference(bitpattern, false);

    for (int y = startBit + 1; y <= endBit; y++)
    {
        int byt = y / 8;
        bitpattern[byt] |= 1 << (y % 8);
    }    
    ui->bitfield->updateData(bitpattern, true);

    ui->cbIntelFormat->setChecked(sig->intelByteOrder);

    switch (sig->valType)
    {
    case UNSIGNED_INT:
    case SIGNED_INT:
        ui->comboType->setCurrentIndex(0);
        break;
    case SP_FLOAT:
        ui->comboType->setCurrentIndex(1);
        break;
    case DP_FLOAT:
        ui->comboType->setCurrentIndex(2);
        break;
    case STRING:
        ui->comboType->setCurrentIndex(3);
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
    ui->valuesTable->clearContents();
    ui->valuesTable->setRowCount(0);
    for (int i = 0; i < sig->valList.count(); i++)
    {
        QTableWidgetItem *val = new QTableWidgetItem(QString::number(sig->valList[i].value));
        QTableWidgetItem *desc = new QTableWidgetItem(sig->valList[i].descript);
        rowIdx = ui->valuesTable->rowCount();
        ui->valuesTable->insertRow(rowIdx);
        ui->valuesTable->setItem(rowIdx, 0, val);
        ui->valuesTable->setItem(rowIdx, 1, desc);
    }
}

void DBCSignalEditor::clickSignalList(int row)
{
    if (row < 0) return;
    //qDebug() << ui->signalsList->item(row)->text();
    DBC_SIGNAL *thisSig = dbcHandler->findSignalByName(dbcMessage, ui->signalsList->item(row)->text());
    if (thisSig == NULL) return;
    fillSignalForm(thisSig);
    fillValueTable(thisSig);

}

void DBCSignalEditor::bitfieldClicked(int x, int y)
{
    int bit = (x) + (y * 8);
    DBC_SIGNAL *thisSig = dbcHandler->findSignalByIdx(dbcMessage, ui->signalsList->currentRow());
    if (thisSig == NULL) return;
    thisSig->startBit = bit;
    fillSignalForm(thisSig);
}
