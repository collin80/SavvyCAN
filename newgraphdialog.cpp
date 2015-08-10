#include "newgraphdialog.h"
#include "ui_newgraphdialog.h"
#include <QColorDialog>
#include "utility.h"

NewGraphDialog::NewGraphDialog(DBCHandler *handler, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewGraphDialog)
{
    ui->setupUi(this);

    dbcHandler = handler;

    connect(ui->colorSwatch, SIGNAL(clicked(bool)), this, SLOT(colorSwatchClick()));
    connect(ui->btnAddGraph, SIGNAL(clicked(bool)), this, SLOT(addButtonClicked()));

    // Seed the random generator with current time
    QTime time = QTime::currentTime();
    qsrand((uint)time.msec());

    QPalette p = ui->colorSwatch->palette();
    //using 160 instead of 255 so that colors are always at least a little dark
    p.setColor(QPalette::Button, QColor(qrand() % 160,qrand() % 160,qrand() % 160));
    ui->colorSwatch->setPalette(p);

    connect(ui->cbMessages, SIGNAL(currentIndexChanged(int)), this, SLOT(loadSignals(int)));
    connect(ui->cbSignals, SIGNAL(currentIndexChanged(int)), this, SLOT(fillFormFromSignal(int)));

    loadMessages();
}

NewGraphDialog::~NewGraphDialog()
{
    delete ui;
}

void NewGraphDialog::addButtonClicked()
{
    accept();
}

void NewGraphDialog::colorSwatchClick()
{
    QColor newColor = QColorDialog::getColor(ui->colorSwatch->palette().button().color());

    QPalette p = ui->colorSwatch->palette();
    p.setColor(QPalette::Button, newColor);
    ui->colorSwatch->setPalette(p);

}

void NewGraphDialog::setParams(GraphParams &params)
{
    ui->txtID->setText("0x" + QString::number(params.ID, 16).toUpper());
    ui->txtBias->setText(QString::number(params.bias));
    ui->txtMask->setText("0x" + QString::number(params.mask, 16).toUpper().left(10));
    ui->txtScale->setText(QString::number(params.scale));
    ui->txtStride->setText(QString::number(params.stride));
    ui->cbSigned->setChecked(params.isSigned);
    ui->txtName->setText(params.graphName);

    if (params.endByte > -1)
    {
        ui->txtData->setText(QString::number(params.startByte) + "-" + QString::number(params.endByte));
    }
    else
    {
        ui->txtData->setText(QString::number(params.startByte));
    }

    QPalette p = ui->colorSwatch->palette();
    p.setColor(QPalette::Button, params.color);
    ui->colorSwatch->setPalette(p);
}

void NewGraphDialog::getParams(GraphParams &params)
{
    params.ID = Utility::ParseStringToNum(ui->txtID->text());
    params.bias = ui->txtBias->text().toFloat();
    params.color = ui->colorSwatch->palette().button().color();
    params.isSigned = ui->cbSigned->isChecked();
    params.mask = Utility::ParseStringToNum(ui->txtMask->text());
    params.scale = ui->txtScale->text().toFloat();
    params.stride = Utility::ParseStringToNum(ui->txtStride->text());

    QStringList values = ui->txtData->text().split('-');
    params.startByte = -1;
    params.endByte = -1;
    if (values.count() > 0)
    {
        params.startByte = values[0].toInt();
        if (values.count() > 1)
        {
            params.endByte = values[1].toInt();
        }
    }

    //now catch stupidity and bring it to defaults
    if (params.mask == 0) params.mask = 0xFFFFFFFF;
    if (fabs(params.scale) < 0.00000001) params.scale = 1.0f;
    if (params.stride < 1) params.stride = 1;
    params.graphName = ui->txtName->text();
}

void NewGraphDialog::loadMessages()
{
    ui->cbMessages->clear();
    if (dbcHandler == NULL) return;
    for (int x = 0; x < dbcHandler->dbc_messages.count(); x++)
    {
        ui->cbMessages->addItem(dbcHandler->dbc_messages[x].name);
    }
}

void NewGraphDialog::loadSignals(int idx)
{
    //messages were placed into the list in the same order as they exist
    //in the data structure so it should have been possible to just
    //look it up based on index but by name is probably safer and this operation
    //is not time critical at all.
    DBC_MESSAGE *msg = dbcHandler->findMsgByName(ui->cbMessages->currentText());

    if (msg == NULL) return;
    ui->cbSignals->clear();
    for (int x = 0; x < msg->msgSignals.count(); x++)
    {
        ui->cbSignals->addItem(msg->msgSignals[x].name);
    }
}

void NewGraphDialog::fillFormFromSignal(int idx)
{
    GraphParams params;
    DBC_MESSAGE *msg = dbcHandler->findMsgByName(ui->cbMessages->currentText());

    if (msg == NULL) return;

    DBC_SIGNAL *sig = dbcHandler->findSignalByName(msg, ui->cbSignals->currentText());

    if (sig == NULL) return;

    params.graphName = sig->name;
    params.ID = msg->ID;
    params.bias = sig->bias;
    params.scale = sig->factor;
    params.stride = 1;
    params.mask = (1 << (sig->signalSize)) - 1;
    if (sig->valType == SIGNED_INT) params.isSigned = true;
    else params.isSigned = false;
    params.color = ui->colorSwatch->palette().color(QPalette::Button);
    if (sig->intelByteOrder)
    {
        //for this ordering the byte order is reserved and starting byte
        //will be the higher value
        params.endByte = sig->startBit / 8;
        params.startByte = (sig->startBit + sig->signalSize - 1) / 8;
    }
    else
    {
        //for this ordering it goes in normal numerical order
        params.startByte = sig->startBit / 8;
        params.endByte = (sig->startBit + sig->signalSize - 1) / 8;
    }

    setParams(params);
}
