#include "newgraphdialog.h"
#include "ui_newgraphdialog.h"
#include <QColorDialog>
#include <QRandomGenerator>
#include "utility.h"
#include "helpwindow.h"

NewGraphDialog::NewGraphDialog(DBCHandler *handler, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewGraphDialog)
{
    ui->setupUi(this);

    dbcHandler = handler;

    connect(ui->colorSwatch, SIGNAL(clicked(bool)), this, SLOT(colorSwatchClick()));
    connect(ui->fillSwatch, SIGNAL(clicked(bool)), this, SLOT(fillSwatchClick()));
    connect(ui->btnAddGraph, SIGNAL(clicked(bool)), this, SLOT(addButtonClicked()));

    QPalette p = ui->colorSwatch->palette();
    //using 160 instead of 255 so that colors are always at least a little dark
    p.setColor(QPalette::Button, QColor(QRandomGenerator::global()->bounded(160),QRandomGenerator::global()->bounded(160), QRandomGenerator::global()->bounded(160)));
    ui->colorSwatch->setPalette(p);

    QPalette p2 = ui->fillSwatch->palette();
    p2.setColor(QPalette::Button, QColor(128,128,128,0)); //light gray, no opacity so it is disabled by default
    ui->fillSwatch->setPalette(p2);

    ui->coPointStyle->addItem("None");
    ui->coPointStyle->addItem("Dot");
    ui->coPointStyle->addItem("Cross");
    ui->coPointStyle->addItem("Plus");
    ui->coPointStyle->addItem("Circle");
    ui->coPointStyle->addItem("Disc");
    ui->coPointStyle->addItem("Square");
    ui->coPointStyle->addItem("Diamond");
    ui->coPointStyle->addItem("Star");
    ui->coPointStyle->addItem("Triangle");
    ui->coPointStyle->addItem("TriangleInverted");
    ui->coPointStyle->addItem("Cross Inside Square");
    ui->coPointStyle->addItem("Plus Inside Square");
    ui->coPointStyle->addItem("Cross Inside Circle");
    ui->coPointStyle->addItem("Plus Inside Circle");
    ui->coPointStyle->addItem("Peace Sign");

    connect(ui->cbMessages, SIGNAL(currentIndexChanged(int)), this, SLOT(loadSignals(int)));
    connect(ui->gridData, SIGNAL(gridClicked(int,int)), this, SLOT(bitfieldClicked(int,int)));
    connect(ui->txtDataLen, SIGNAL(textChanged(QString)), this, SLOT(handleDataLenUpdate()));
    connect(ui->cbIntel, SIGNAL(toggled(bool)), this, SLOT(drawBitfield()));
    connect(ui->btnCopySignal, SIGNAL(clicked(bool)), this, SLOT(copySignalToParamsUI()));
    connect(ui->cbSignals, SIGNAL(currentIndexChanged(int)), this, SLOT(drawBitfield()));

    startBit = 0;
    dataLen = 1;
    assocSignal = nullptr;

    loadMessages();

    installEventFilter(this);
}

NewGraphDialog::~NewGraphDialog()
{
    removeEventFilter(this);
    delete ui;
}

void NewGraphDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    loadMessages();
    drawBitfield();
    qDebug() << "S" << ui->gridData->geometry();
}

bool NewGraphDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("graphsetup.md");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
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

void NewGraphDialog::fillSwatchClick()
{
    QColor newColor = QColorDialog::getColor(ui->fillSwatch->palette().button().color(), nullptr, "Pick A Color", QColorDialog::ShowAlphaChannel);

    QPalette p = ui->fillSwatch->palette();
    p.setColor(QPalette::Button, newColor);
    ui->fillSwatch->setPalette(p);
}

//check whether the current values on the left match the signal selected on the right
void NewGraphDialog::checkSignalAgreement()
{
    GraphParams testingParams;
    bool bAgree = true;
    bool sigSigned = false;
    DBC_SIGNAL *sig = nullptr;
    DBC_MESSAGE *msg = nullptr;

    if (dbcHandler == nullptr) return;
    if (dbcHandler->getFileCount() == 0) return;

    msg = dbcHandler->findMessage(ui->cbMessages->currentText());
    if (msg)
    {
        sig = msg->sigHandler->findSignalByName(ui->cbSignals->currentText());
    }
    else
    {
        ui->lblMsgStatus->setText("Msg ID doesn't exist in DBC");
        return;
    }

    if (sig)
    {
        if (sig->valType == SIGNED_INT) sigSigned = true;

        testingParams.ID = Utility::ParseStringToNum(ui->txtID->text());
        testingParams.bias = ui->txtBias->text().toFloat();
        testingParams.isSigned = ui->cbSigned->isChecked();
        testingParams.intelFormat = ui->cbIntel->isChecked();
        testingParams.scale = ui->txtScale->text().toFloat();
        testingParams.startBit = startBit;
        testingParams.numBits = dataLen;

        if (testingParams.ID != msg->ID) bAgree = false;
        if (fabs(testingParams.bias - sig->bias) > 0.01) bAgree = false;
        if (testingParams.isSigned != sigSigned) bAgree = false;
        if (testingParams.intelFormat != sig->intelByteOrder) bAgree = false;
        if (fabs(testingParams.scale - sig->factor) > 0.01) bAgree = false;
        if (testingParams.startBit != sig->startBit) bAgree = false;
        if (testingParams.numBits != sig->signalSize) bAgree = false;
    }
    else
    {
        ui->lblMsgStatus->setText("Signal name doesn't exist in DBC");
        return;
    }

    if (bAgree)
    {
        ui->lblMsgStatus->setText("Graph params match this signal");
        //assocSignal = sig;
    }
    else
    {
        ui->lblMsgStatus->setText("Signal and Graph Params do not match");
        //assocSignal = nullptr; //ya done broke it, null the associated signal since the user is changing things
    }
}

void NewGraphDialog::clearParams()
{
    ui->txtID->clear();
    ui->txtBias->clear();
    ui->txtMask->clear();
    ui->txtScale->clear();
    ui->txtStride->clear();
    ui->txtName->clear();
}

void NewGraphDialog::setParams(GraphParams &params)
{
    ui->txtBias->setText(QString::number(params.bias));
    ui->txtMask->setText(Utility::formatNumber(params.mask));
    ui->txtScale->setText(QString::number(params.scale));
    ui->txtStride->setText(QString::number(params.stride));
    ui->cbSigned->setChecked(params.isSigned);
    ui->cbIntel->setChecked(params.intelFormat);

    startBit = params.startBit;
    dataLen = params.numBits;
    ui->txtDataLen->setText(QString::number(dataLen));
    ui->txtID->setText(Utility::formatCANID(params.ID));
    ui->txtName->setText(params.graphName);
    QPalette p = ui->colorSwatch->palette();
    p.setColor(QPalette::Button, params.lineColor);
    ui->colorSwatch->setPalette(p);
    QPalette p2 = ui->fillSwatch->palette();
    p2.setColor(QPalette::Button, params.fillColor);
    ui->fillSwatch->setPalette(p2);
    ui->cbOnlyPoints->setChecked(params.drawOnlyPoints);
    ui->spinLineWidth->setValue(params.lineWidth);
    ui->coPointStyle->setCurrentIndex(params.pointType);

    assocSignal = params.associatedSignal;

    loadMessages();
    loadSignals(0);
    drawBitfield();
    checkSignalAgreement();
}

void NewGraphDialog::getParams(GraphParams &params)
{
    params.lineColor = ui->colorSwatch->palette().button().color();
    params.fillColor = ui->fillSwatch->palette().button().color();
    params.graphName = ui->txtName->text();

    params.lineWidth = ui->spinLineWidth->value();
    params.drawOnlyPoints = ui->cbOnlyPoints->isChecked();
    params.pointType = ui->coPointStyle->currentIndex();

    params.ID = Utility::ParseStringToNum(ui->txtID->text());
    params.bias = ui->txtBias->text().toFloat();
    params.isSigned = ui->cbSigned->isChecked();
    params.intelFormat = ui->cbIntel->isChecked();
    params.mask = Utility::ParseStringToNum(ui->txtMask->text());
    params.scale = ui->txtScale->text().toFloat();
    params.stride = Utility::ParseStringToNum(ui->txtStride->text());

    params.startBit = startBit;
    params.numBits = dataLen;

    params.associatedSignal = assocSignal;

    //now catch stupidity and bring it to defaults
    if (params.mask == 0) params.mask = 0xFFFFFFFF;
    if (fabs(params.scale) < 0.00000001) params.scale = 1.0f;
    if (params.stride < 1) params.stride = 1;
}

void NewGraphDialog::loadMessages()
{
    DBC_MESSAGE *msg;
    ui->cbMessages->clear();
    if (dbcHandler == nullptr) return;
    if (dbcHandler->getFileCount() == 0) return;
    for (int y = 0; y < dbcHandler->getFileCount(); y++)
    {
        for (int x = 0; x < dbcHandler->getFileByIdx(y)->messageHandler->getCount(); x++)
        {
            msg = dbcHandler->getFileByIdx(y)->messageHandler->findMsgByIdx(x);
            if (msg)
            {
                ui->cbMessages->addItem(msg->name);
                if (assocSignal && msg->name == assocSignal->parentMessage->name)
                {
                    ui->cbMessages->setCurrentIndex(ui->cbMessages->count() -1);
                    //qDebug() << "Found my parent";
                }
            }
        }
    }
    ui->cbMessages->model()->sort(0);
}

void NewGraphDialog::loadSignals(int idx)
{
    Q_UNUSED(idx);
    //search through all DBC files in order to try to find a message with the given name
    DBC_MESSAGE *msg = dbcHandler->findMessage(ui->cbMessages->currentText());
    DBC_SIGNAL *sig;

    if (msg == nullptr) return;

    ui->cbSignals->clear();
    for (int x = 0; x < msg->sigHandler->getCount(); x++)
    {
        sig = msg->sigHandler->findSignalByIdx(x);
        if (sig)
        {
            ui->cbSignals->addItem(sig->name);
            if (assocSignal && sig->name == assocSignal->name)
            {
                ui->cbSignals->setCurrentIndex(ui->cbSignals->count() - 1);
                //qDebug() << "Found me";
            }
        }
    }
    ui->cbSignals->model()->sort(0);
    checkSignalAgreement();
}

void NewGraphDialog::bitfieldClicked(int x,int y)
{
    int bit = (y * 8 + (7-x));

    qDebug() << "Clicked bit: " << bit;
    startBit = bit;
    drawBitfield();    
}

void NewGraphDialog::drawBitfield()
{
    qDebug() << "Draw Bitfield";
    int64_t bitField = 0;
    int endBit, sBit;

    bitField |= 1ull << (startBit); //make the start bit a different color to set it apart
    ui->gridData->setReference((unsigned char *)&bitField, false);

    if (ui->cbIntel->isChecked())
    {
        endBit = startBit + dataLen - 1;
        if (startBit < 0) startBit = 0;
        if (endBit > 63) endBit = 63;
        for (int y = startBit; y <= endBit; y++)
        {
            bitField |= 1ull << y;
        }
    }
    else //big endian / motorola format
    {
        //much more irritating than the intel version...
        int size = dataLen;
        sBit = startBit;
        while (size > 0)
        {
            bitField |= 1ull << sBit;
            size--;
            if ((sBit % 8) == 0) sBit += 15;
            else sBit--;
            if (sBit > 63) sBit = 63;
        }
    }

    ui->gridData->updateData((unsigned char *)&bitField, true);

    checkSignalAgreement();
}

void NewGraphDialog::handleDataLenUpdate()
{
    dataLen = ui->txtDataLen->text().toInt();
    if (dataLen < 1) dataLen = 1;
    if (dataLen > 63) dataLen = 63;
    drawBitfield();
    checkSignalAgreement();
}

void NewGraphDialog::copySignalToParamsUI()
{
    assocSignal = nullptr;
    DBC_MESSAGE *msg = nullptr;

    for(int i = 0; i < dbcHandler->getFileCount(); i++)
    {
        msg = dbcHandler->getFileByIdx(i)->messageHandler->findMsgByName(ui->cbMessages->currentText());
        if (msg) break;
    }

    if (!msg) return;
    DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(ui->cbSignals->currentText());
    if (!sig) return;

    startBit = sig->startBit;
    ui->txtBias->setText(QString::number(sig->bias));
    ui->txtDataLen->setText(QString::number(sig->signalSize));
    ui->txtID->setText(Utility::formatCANID(msg->ID));
    ui->txtMask->setText("0xFFFFFFFF");
    ui->txtName->setText(sig->name);
    ui->txtScale->setText(QString::number(sig->factor));
    ui->txtStride->setText("1");
    ui->cbIntel->setChecked(sig->intelByteOrder);
    if (sig->valType == SIGNED_INT) ui->cbSigned->setChecked(true);
        else ui->cbSigned->setChecked(false);
    drawBitfield();
    assocSignal = sig;
    checkSignalAgreement();
}
