#include <QCanBus>
#include "newconnectiondialog.h"
#include "ui_newconnectiondialog.h"

NewConnectionDialog::NewConnectionDialog(QVector<QString>* ips, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewConnectionDialog),
    remoteDeviceIP(ips)
{
    ui->setupUi(this);

    if (isSerialBusAvailable())
    {
        ui->rbSocketCAN->setEnabled(true);
    }
    else
    {
        ui->rbSocketCAN->setEnabled(false);
        QString errorString;
        const QList<QCanBusDeviceInfo> devices = QCanBus::instance()->availableDevices(QStringLiteral("socketcan"), &errorString);
        if (!errorString.isEmpty()) ui->rbSocketCAN->setToolTip(errorString);
    }


    connect(ui->rbGVRET, &QAbstractButton::clicked, this, &NewConnectionDialog::handleConnTypeChanged);
    connect(ui->rbSocketCAN, &QAbstractButton::clicked, this, &NewConnectionDialog::handleConnTypeChanged);
    connect(ui->rbRemote, &QAbstractButton::clicked, this, &NewConnectionDialog::handleConnTypeChanged);
    connect(ui->rbMQTT, &QAbstractButton::clicked, this, &NewConnectionDialog::handleConnTypeChanged);

    connect(ui->cbDeviceType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewConnectionDialog::handleDeviceTypeChanged);
    connect(ui->btnOK, &QPushButton::clicked, this, &NewConnectionDialog::handleCreateButton);

    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);
    selectSerial();

    qDebug() << "Was passed " << remoteDeviceIP->count() << " remote IPs";
}

NewConnectionDialog::~NewConnectionDialog()
{
    delete ui;
}

void NewConnectionDialog::handleCreateButton()
{
    accept();
}

void NewConnectionDialog::handleConnTypeChanged()
{
    if (ui->rbGVRET->isChecked()) selectSerial();
    if (ui->rbSocketCAN->isChecked()) selectSocketCan();
    if (ui->rbRemote->isChecked()) selectRemote();

}

void NewConnectionDialog::handleDeviceTypeChanged()
{

    ui->cbPort->clear();
    canDevices = QCanBus::instance()->availableDevices(ui->cbDeviceType->currentText());

    for (int i = 0; i < canDevices.count(); i++)
        ui->cbPort->addItem(canDevices[i].name());
}

void NewConnectionDialog::selectSerial()
{
    ui->lPort->setText("Serial Port:");

    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);

    ui->cbPort->clear();
    ports = QSerialPortInfo::availablePorts();

    for (int i = 0; i < ports.count(); i++)
        ui->cbPort->addItem(ports[i].portName());
}

void NewConnectionDialog::selectSocketCan()
{
    ui->lPort->setText("Port:");
    ui->lblDeviceType->setHidden(false);
    ui->cbDeviceType->setHidden(false);

    ui->cbDeviceType->clear();
    QStringList plugins;
    plugins = QCanBus::instance()->plugins();
    for (int i = 0; i < plugins.count(); i++)
        ui->cbDeviceType->addItem(plugins[i]);

}

void NewConnectionDialog::selectRemote()
{
    ui->lPort->setText("IP Address:");
    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);
    ui->cbPort->clear();
    foreach(QString pName, *remoteDeviceIP)
    {
        ui->cbPort->addItem(pName);
    }
}

void NewConnectionDialog::selectMQTT()
{
    ui->lPort->setText("Topic Name:");
    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);
    ui->cbPort->clear();
}

void NewConnectionDialog::setPortName(CANCon::type pType, QString pPortName, QString pDriver)
{

    switch(pType)
    {
        case CANCon::GVRET_SERIAL:
            ui->rbGVRET->setChecked(true);
            break;
        case CANCon::SERIALBUS:
            ui->rbSocketCAN->setChecked(true);
            //you can't configure any of the below three with socketcan so dim them out
            break;
        default: {}
    }

    /* refresh names whenever needed */
    //handleConnTypeChanged();

    switch(pType)
    {
        case CANCon::GVRET_SERIAL:
        {
            int idx = ui->cbPort->findText(pPortName);
            if( idx<0 ) idx=0;
            ui->cbPort->setCurrentIndex(idx);
            break;
        }
        case CANCon::SERIALBUS:
        {
            int idx = ui->cbDeviceType->findText(pDriver);
            if (idx < 0) idx = 0;
            ui->cbDeviceType->setCurrentIndex(idx);
            idx = ui->cbPort->findText(pPortName);
            if( idx < 0 ) idx = 0;
            ui->cbPort->setCurrentIndex(idx);
            break;
        }
        case CANCon::REMOTE:
        {
            int idx = ui->cbPort->findText(pPortName);
            if (idx > -1) ui->cbPort->setCurrentIndex(idx);
            else ui->cbPort->addItem(pPortName);
            break;
        }
        default: {}
    }
}

QString NewConnectionDialog::getPortName()
{
    switch( getConnectionType() ) {
    case CANCon::GVRET_SERIAL:
    case CANCon::SERIALBUS:
    case CANCon::REMOTE:
    case CANCon::MQTT:
        return ui->cbPort->currentText();
    default:
        qDebug() << "getPortName: can't get port";
    }

    return "";
}

QString NewConnectionDialog::getDriverName()
{
    if (getConnectionType() == CANCon::SERIALBUS)
    {
        return ui->cbDeviceType->currentText();
    }
    return "N/A";
}

CANCon::type NewConnectionDialog::getConnectionType()
{
    if (ui->rbGVRET->isChecked()) return CANCon::GVRET_SERIAL;
    if (ui->rbSocketCAN->isChecked()) return CANCon::SERIALBUS;
    if (ui->rbRemote->isChecked()) return CANCon::REMOTE;
    if (ui->rbMQTT->isChecked()) return CANCon::MQTT;
    qDebug() << "getConnectionType: error";

    return CANCon::NONE;
}

bool NewConnectionDialog::isSerialBusAvailable()
{
    if (QCanBus::instance()->plugins().count() > 0) return true;
    return false;
}
