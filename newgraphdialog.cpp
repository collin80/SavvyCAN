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
    connect(ui->rbSignalGraph, SIGNAL(toggled(bool)), this, SLOT(setSignalActive(bool)));
    connect(ui->rbStandardGraph, SIGNAL(toggled(bool)), this, SLOT(setStandardActive(bool)));

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

void NewGraphDialog::clearParams()
{
    ui->txtID->clear();
    ui->txtBias->clear();
    ui->txtMask->clear();
    ui->txtScale->clear();
    ui->txtStride->clear();
    ui->txtName->clear();
    ui->txtData->clear();
    ui->rbStandardGraph->setChecked(true);
    setStandardActive(true);

}

void NewGraphDialog::setParams(GraphParams &params)
{
    if (params.isDBCSignal)
    {
        clearParams();
        setSignalActive(true);
        //loadMessages();

    }
    else
    {
        setStandardActive(true);        
        ui->txtBias->setText(QString::number(params.bias));
        ui->txtMask->setText(Utility::formatNumber(params.mask));
        ui->txtScale->setText(QString::number(params.scale));
        ui->txtStride->setText(QString::number(params.stride));
        ui->cbSigned->setChecked(params.isSigned);

        if (params.endByte > -1)
        {
            ui->txtData->setText(QString::number(params.startByte) + "-" + QString::number(params.endByte));
        }
        else
        {
            ui->txtData->setText(QString::number(params.startByte));
        }
    }

    ui->txtID->setText(Utility::formatNumber(params.ID));
    ui->txtName->setText(params.graphName);
    QPalette p = ui->colorSwatch->palette();
    p.setColor(QPalette::Button, params.color);
    ui->colorSwatch->setPalette(p);
}

void NewGraphDialog::getParams(GraphParams &params)
{
    params.isDBCSignal = ui->rbSignalGraph->isChecked();
    params.color = ui->colorSwatch->palette().button().color();
    params.graphName = ui->txtName->text();

    if (params.isDBCSignal)
    {
        params.signal = ui->cbSignals->currentText();
        params.ID = Utility::ParseStringToNum(ui->txtID->text());
        params.bias = 0;
        params.isSigned = false;
        params.mask = 0;
        params.scale = 1;
        params.bias = 0;
        params.stride = 1;

    }
    else {
        params.ID = Utility::ParseStringToNum(ui->txtID->text());
        params.bias = ui->txtBias->text().toFloat();
        params.isSigned = ui->cbSigned->isChecked();
        params.mask = Utility::ParseStringToNum(ui->txtMask->text());
        params.scale = ui->txtScale->text().toFloat();
        params.stride = Utility::ParseStringToNum(ui->txtStride->text());
        params.signal = "";

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
    }
}

void NewGraphDialog::loadMessages()
{
    ui->cbMessages->clear();
    if (dbcHandler == NULL) return;
    for (int x = 0; x < dbcHandler->getFileByIdx(0)->messageHandler->getCount(); x++)
    {
        ui->cbMessages->addItem(dbcHandler->getFileByIdx(0)->messageHandler->findMsgByIdx(x)->name);
    }
}

void NewGraphDialog::loadSignals(int idx)
{
    Q_UNUSED(idx);
    //messages were placed into the list in the same order as they exist
    //in the data structure so it should have been possible to just
    //look it up based on index but by name is probably safer and this operation
    //is not time critical at all.
    DBC_MESSAGE *msg = dbcHandler->getFileByIdx(0)->messageHandler->findMsgByName(ui->cbMessages->currentText());

    if (msg == NULL) return;
    ui->cbSignals->clear();
    for (int x = 0; x < msg->sigHandler->getCount(); x++)
    {
        ui->cbSignals->addItem(msg->sigHandler->findSignalByIdx(x)->name);
    }
}

void NewGraphDialog::fillFormFromSignal(int idx)
{
    Q_UNUSED(idx);
    GraphParams params;
    DBC_MESSAGE *msg = dbcHandler->getFileByIdx(0)->messageHandler->findMsgByName(ui->cbMessages->currentText());

    if (msg == NULL) return;

    DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(ui->cbSignals->currentText());

    if (sig == NULL) return;

    params.graphName = sig->name;
    params.ID = msg->ID;
    //params.bias = sig->bias;
    //params.scale = sig->factor;
    //params.stride = 1;
    //params.mask = (1 << (sig->signalSize)) - 1;
    //if (sig->valType == SIGNED_INT) params.isSigned = true;
    //else params.isSigned = false;
    params.color = ui->colorSwatch->palette().color(QPalette::Button);
    /*
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
    */
    setParams(params);
}

void NewGraphDialog::setSignalActive(bool state)
{
    if (!state) return;
    ui->rbSignalGraph->setChecked(true);
    ui->rbStandardGraph->setChecked(false);
    ui->cbMessages->setEnabled(true);
    ui->cbSignals->setEnabled(true);
    ui->cbSigned->setEnabled(false);
    ui->txtBias->setEnabled(false);
    ui->txtData->setEnabled(false);
    ui->txtID->setEnabled(false);
    ui->txtMask->setEnabled(false);
    ui->txtScale->setEnabled(false);
    ui->txtStride->setEnabled(false);
}

void NewGraphDialog::setStandardActive(bool state)
{
    if (!state) return;
    ui->rbStandardGraph->setChecked(true);
    ui->rbSignalGraph->setChecked(false);
    ui->cbMessages->setEnabled(false);
    ui->cbSignals->setEnabled(false);
    ui->cbSigned->setEnabled(true);
    ui->txtBias->setEnabled(true);
    ui->txtData->setEnabled(true);
    ui->txtID->setEnabled(true);
    ui->txtMask->setEnabled(true);
    ui->txtScale->setEnabled(true);
    ui->txtStride->setEnabled(true);
}
