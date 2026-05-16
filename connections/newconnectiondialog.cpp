#include <QCanBus>
#include "newconnectiondialog.h"
#include "ui_newconnectiondialog.h"
#include "gs_usb_driver/gs_usb_definitions.h"
#include "gs_usb_driver/gs_usb.h"

NewConnectionDialog::NewConnectionDialog(QVector<QString>* gvretips, QVector<QString>* kayakhosts, QVector<candle_handle>* gsusbIDs, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewConnectionDialog),
    remoteDeviceIPGVRET(gvretips),
    remoteBusKayak(kayakhosts),
    remoteDeviceGSUSB(gsusbIDs)
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
    connect(ui->rbKayak, &QAbstractButton::clicked, this, &NewConnectionDialog::handleConnTypeChanged);
    connect(ui->rbMQTT, &QAbstractButton::clicked, this, &NewConnectionDialog::handleConnTypeChanged);
    connect(ui->rbLawicel, &QAbstractButton::clicked, this, &NewConnectionDialog::handleConnTypeChanged);
    connect(ui->rbCANserver, &QAbstractButton::clicked, this, &NewConnectionDialog::handleConnTypeChanged);
    connect(ui->rbCanlogserver, &QAbstractButton::clicked, this, &NewConnectionDialog::handleConnTypeChanged);
#ifdef GS_USB_DRIVER_ENABLED
    connect(ui->rbGSUSB, &QAbstractButton::clicked, this, &NewConnectionDialog::handleConnTypeChanged);
#else
    ui->rbGSUSB->setVisible(false);
#endif

    connect(ui->cbDeviceType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewConnectionDialog::handleDeviceTypeChanged);
    connect(ui->cbPort, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewConnectionDialog::handlePortChanged);
    connect(ui->btnOK, &QPushButton::clicked, this, &NewConnectionDialog::handleCreateButton);

    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);
    ui->cbCANSpeed->setHidden(true);
    ui->cbSerialSpeed->setHidden(true);
    ui->lblCANSpeed->setHidden(true);
    ui->lblSerialSpeed->setHidden(true);
    ui->cbCanFd->setHidden(true);
    ui->cbDataRate->setHidden(true);
    ui->lblDataRate->setHidden(true);

    selectSerial();

    qDebug() << "Was passed " << remoteDeviceIPGVRET->count() << " remote GVRET IPs";
    qDebug() << "Was passed " << remoteBusKayak->count() << " remote Kayak Busses";
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
    if (ui->rbLawicel->isChecked()) selectLawicel();
    if (ui->rbRemote->isChecked()) selectRemote();
    if (ui->rbKayak->isChecked()) selectKayak();
    if (ui->rbMQTT->isChecked()) selectMQTT();
    if (ui->rbCANserver->isChecked()) selectCANserver();
    if (ui->rbCanlogserver->isChecked()) selectCANlogserver();
    if (ui->rbGSUSB->isChecked()) selectGSUSB();
}

void NewConnectionDialog::handleDeviceTypeChanged()
{

    ui->cbPort->clear();
    canDevices = QCanBus::instance()->availableDevices(ui->cbDeviceType->currentText());

    for (int i = 0; i < canDevices.count(); i++)
        ui->cbPort->addItem(canDevices[i].name());
}

void NewConnectionDialog::handlePortChanged()
{
#ifdef GS_USB_DRIVER_ENABLED
    if (ui->rbGSUSB->isChecked()) {
        // GS USB is checked - attempt to populate bitrate based on device capability
        ui->cbCANSpeed->clear();
        int currentIndex = ui->cbPort->currentIndex();
        if ((currentIndex >= 0) && (currentIndex < remoteDeviceGSUSB->length())) {
            QList<GSUSB_timing_t> supportedTiming;
            candle_handle handle = remoteDeviceGSUSB->at(currentIndex);
            GSUSBDevice::getSupportedTimings(handle, supportedTiming);
            foreach (const GSUSB_timing_t timing, supportedTiming) {
                ui->cbCANSpeed->addItem(GSUSBDevice::timingToString(timing));
            }
        }
    }
#endif
}

void NewConnectionDialog::selectLawicel()
{
    ui->lPort->setText("Serial Port:");

    ui->cbPort->setEditable(true);
    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);

    ui->cbCANSpeed->setHidden(false);
    ui->cbSerialSpeed->setHidden(false);
    ui->lblCANSpeed->setHidden(false);
    ui->lblSerialSpeed->setHidden(false);
    ui->cbCanFd->setHidden(false);
    ui->cbDataRate->setHidden(false);
    ui->lblDataRate->setHidden(false);

    ui->cbPort->clear();
    ports = QSerialPortInfo::availablePorts();

    for (int i = 0; i < ports.count(); i++)
        ui->cbPort->addItem(ports[i].portName());

    ui->cbCANSpeed->clear();
    ui->cbCANSpeed->addItem("10000");
    ui->cbCANSpeed->addItem("20000");
    ui->cbCANSpeed->addItem("50000");
    ui->cbCANSpeed->addItem("83333");
    ui->cbCANSpeed->addItem("100000");
    ui->cbCANSpeed->addItem("125000");
    ui->cbCANSpeed->addItem("250000");
    ui->cbCANSpeed->addItem("500000");
    ui->cbCANSpeed->addItem("1000000");

    if (ui->cbDataRate->count() == 0)
    {
        ui->cbDataRate->addItem("1000000");
        ui->cbDataRate->addItem("2000000");
        ui->cbDataRate->addItem("4000000");
        ui->cbDataRate->addItem("5000000");
    }
    if (ui->cbSerialSpeed->count() == 0)
    {
        ui->cbSerialSpeed->addItem("115200");
        ui->cbSerialSpeed->addItem("150000");
        ui->cbSerialSpeed->addItem("250000");
        ui->cbSerialSpeed->addItem("500000");
        ui->cbSerialSpeed->addItem("1000000");
        ui->cbSerialSpeed->addItem("2000000");
        ui->cbSerialSpeed->addItem("3000000");
    }

}

void NewConnectionDialog::selectSerial()
{
    ui->lPort->setText("Serial Port:");

    ui->cbPort->setEditable(true);
    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);
    ui->cbCANSpeed->setHidden(true);
    ui->cbSerialSpeed->setHidden(true);
    ui->lblCANSpeed->setHidden(true);
    ui->lblSerialSpeed->setHidden(true);
    ui->cbCanFd->setHidden(true);
    ui->cbDataRate->setHidden(true);
    ui->lblDataRate->setHidden(true);

    ui->cbPort->clear();
    ports = QSerialPortInfo::availablePorts();

    for (int i = 0; i < ports.count(); i++)
        ui->cbPort->addItem(ports[i].portName());
}

void NewConnectionDialog::selectSocketCan()
{
    ui->lPort->setText("Port:");
    ui->cbPort->setEditable(true);
    ui->lblDeviceType->setHidden(false);
    ui->cbDeviceType->setHidden(false);
    ui->cbCANSpeed->setHidden(true);
    ui->cbSerialSpeed->setHidden(true);
    ui->lblCANSpeed->setHidden(true);
    ui->lblSerialSpeed->setHidden(true);
    ui->cbCanFd->setHidden(true);
    ui->cbDataRate->setHidden(true);
    ui->lblDataRate->setHidden(true);

    ui->cbDeviceType->clear();
    QStringList plugins;
    plugins = QCanBus::instance()->plugins();
    for (int i = 0; i < plugins.count(); i++)
        ui->cbDeviceType->addItem(plugins[i]);

}

void NewConnectionDialog::selectRemote()
{
    ui->lPort->setText("IP Address:");

    ui->cbPort->setEditable(true);
    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);
    ui->cbCANSpeed->setHidden(true);
    ui->cbSerialSpeed->setHidden(true);
    ui->lblCANSpeed->setHidden(true);
    ui->lblSerialSpeed->setHidden(true);
    ui->cbCanFd->setHidden(true);
    ui->cbDataRate->setHidden(true);
    ui->lblDataRate->setHidden(true);

    ui->cbPort->clear();
    foreach(QString pName, *remoteDeviceIPGVRET)
    {
        ui->cbPort->addItem(pName);
    }
}

void NewConnectionDialog::selectKayak()
{
    ui->lPort->setText("Available Bus(ses):");

    ui->cbPort->setEditable(true);
    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);
    ui->cbCANSpeed->setHidden(true);
    ui->cbSerialSpeed->setHidden(true);
    ui->lblCANSpeed->setHidden(true);
    ui->lblSerialSpeed->setHidden(true);
    ui->cbCanFd->setHidden(true);
    ui->cbDataRate->setHidden(true);
    ui->lblDataRate->setHidden(true);

    ui->cbPort->clear();
    foreach(QString pName, *remoteBusKayak)
    {
        ui->cbPort->addItem(pName);
    }
}

void NewConnectionDialog::selectGSUSB()
{
    ui->lPort->setText("Port:");
    ui->cbPort->setEditable(false);
    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);
    ui->cbCANSpeed->setHidden(false);
    ui->cbSerialSpeed->setHidden(true);
    ui->lblCANSpeed->setHidden(false);
    ui->lblSerialSpeed->setHidden(true);
    ui->cbCanFd->setHidden(true);
    ui->cbDataRate->setHidden(true);
    ui->lblDataRate->setHidden(true);

    ui->cbCANSpeed->clear();
    ui->cbPort->clear();
#ifdef GS_USB_DRIVER_ENABLED
    foreach(candle_handle handle, *remoteDeviceGSUSB) {
        ui->cbPort->addItem(GSUSBDevice::handleToDeviceIDString(handle));
    }
#endif
}

void NewConnectionDialog::selectMQTT()
{
    ui->lPort->setText("Topic Name:");

    ui->cbPort->setEditable(true);
    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);
    ui->cbCANSpeed->setHidden(true);
    ui->cbSerialSpeed->setHidden(true);
    ui->lblCANSpeed->setHidden(true);
    ui->lblSerialSpeed->setHidden(true);
    ui->cbCanFd->setHidden(true);
    ui->cbDataRate->setHidden(true);
    ui->lblDataRate->setHidden(true);

    ui->cbPort->clear();
}

void NewConnectionDialog::selectCANserver()
{
    ui->lPort->setText("CANserver IP Address:");

    ui->cbPort->setEditable(true);
    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);
    ui->cbCANSpeed->setHidden(true);
    ui->cbSerialSpeed->setHidden(true);
    ui->lblCANSpeed->setHidden(true);
    ui->lblSerialSpeed->setHidden(true);
    ui->cbCanFd->setHidden(true);
    ui->cbDataRate->setHidden(true);
    ui->lblDataRate->setHidden(true);

    ui->cbPort->clear();
}

void NewConnectionDialog::selectCANlogserver()
{
    ui->lPort->setText("CANlogserver IP Address:");

    ui->cbPort->setEditable(true);
    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);
    ui->cbCANSpeed->setHidden(true);
    ui->cbSerialSpeed->setHidden(true);
    ui->lblCANSpeed->setHidden(true);
    ui->lblSerialSpeed->setHidden(true);
    ui->cbCanFd->setHidden(true);
    ui->cbDataRate->setHidden(true);
    ui->lblDataRate->setHidden(true);

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
            break;
        case CANCon::REMOTE:
            ui->rbRemote->setChecked(true);
            break;
        case CANCon::KAYAK:
            ui->rbKayak->setChecked(true);
            break;
        case CANCon::MQTT:
            ui->rbMQTT->setChecked(true);
            break;
        case CANCon::LAWICEL:
            ui->rbLawicel->setChecked(true);
            break;
        case CANCon::CANSERVER:
            ui->rbCANserver->setChecked(true);
            break;
        case CANCon::CANLOGSERVER:
            ui->rbCanlogserver->setChecked(true);
            break;
        case CANCon::GSUSB:
            ui->rbGSUSB->setChecked(true);
            break;
        default: {}
    }

    /* refresh names whenever needed */
    //handleConnTypeChanged();

    switch(pType)
    {
        case CANCon::GSUSB:
            // Note that this function isn't currently called
            // For now, just fall through to default functionality
        case CANCon::GVRET_SERIAL:
        case CANCon::LAWICEL:
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
        case CANCon::KAYAK:
        {
            int idx = ui->cbPort->findText(pPortName);
            if (idx > -1) ui->cbPort->setCurrentIndex(idx);
            else ui->cbPort->addItem(pPortName);
            break;
        }
        case CANCon::MQTT:
            ui->cbPort->setCurrentText(pPortName);
            break;
        case CANCon::CANSERVER:
        case CANCon::CANLOGSERVER:
        {
            ui->cbPort->setCurrentText(pPortName);
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
    case CANCon::LAWICEL:
        return ui->cbPort->currentText();
    case CANCon::KAYAK:
        return ui->cbPort->currentText();
    case CANCon::CANSERVER:
    case CANCon::CANLOGSERVER:
    case CANCon::GSUSB:
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

int NewConnectionDialog::getSerialSpeed()
{
    if (getConnectionType() == CANCon::LAWICEL)
    {
        return ui->cbSerialSpeed->currentText().toInt();
    }
    else return 0;
}

int NewConnectionDialog::getBusSpeed()
{
    if (getConnectionType() == CANCon::LAWICEL) {
        return ui->cbCANSpeed->currentText().toInt();
    } else if (getConnectionType() == CANCon::GSUSB) {
#ifdef GS_USB_DRIVER_ENABLED
        QList<GSUSB_timing_t> supportedTiming;
        int currentDeviceIndex = ui->cbPort->currentIndex();
        int currentSpeedIndex = ui->cbCANSpeed->currentIndex();
        if ((currentDeviceIndex >= 0) && (currentSpeedIndex >= 0)) {
            candle_handle handle = remoteDeviceGSUSB->at(currentDeviceIndex);
            GSUSBDevice::getSupportedTimings(handle, supportedTiming);
            if (supportedTiming.length() > currentSpeedIndex) {
                return supportedTiming.at(currentSpeedIndex).bitrate;
            }
        }
#endif
        return 0;
    } else {
        return 0;
    }
}

int NewConnectionDialog::getSamplePoint()
{
    if (getConnectionType() == CANCon::GSUSB) {
#ifdef GS_USB_DRIVER_ENABLED
        QList<GSUSB_timing_t> supportedTiming;
        int currentDeviceIndex = ui->cbPort->currentIndex();
        int currentSpeedIndex = ui->cbCANSpeed->currentIndex();
        if ((currentDeviceIndex >= 0) && (currentSpeedIndex >= 0)) {
            candle_handle handle = remoteDeviceGSUSB->at(currentDeviceIndex);
            GSUSBDevice::getSupportedTimings(handle, supportedTiming);
            if (supportedTiming.length() > currentSpeedIndex) {
                return supportedTiming.at(currentSpeedIndex).samplePoint;
            }
        }
#endif
        return 0;
    } else {
        return 875;
    }
}

CANCon::type NewConnectionDialog::getConnectionType()
{
    if (ui->rbGVRET->isChecked()) return CANCon::GVRET_SERIAL;
    if (ui->rbSocketCAN->isChecked()) return CANCon::SERIALBUS;
    if (ui->rbRemote->isChecked()) return CANCon::REMOTE;
    if (ui->rbKayak->isChecked()) return CANCon::KAYAK;
    if (ui->rbMQTT->isChecked()) return CANCon::MQTT;
    if (ui->rbLawicel->isChecked()) return CANCon::LAWICEL;
    if (ui->rbCANserver->isChecked()) return CANCon::CANSERVER;
    if (ui->rbCanlogserver->isChecked()) return CANCon::CANLOGSERVER;
    if (ui->rbGSUSB->isChecked()) return CANCon::GSUSB;
    qDebug() << "getConnectionType: error";

    return CANCon::NONE;
}

bool NewConnectionDialog::isSerialBusAvailable()
{
    if (QCanBus::instance()->plugins().count() > 0) return true;
    return false;
}

int NewConnectionDialog::getDataRate()
{
    if (getConnectionType() == CANCon::LAWICEL)
    {
        return ui->cbDataRate->currentText().toInt();
    }
    else return 0;
}

bool NewConnectionDialog::isCanFd()
 {
     if (getConnectionType() == CANCon::LAWICEL)
     {
         return ui->cbCanFd;
     }
     else return 0;
 }
