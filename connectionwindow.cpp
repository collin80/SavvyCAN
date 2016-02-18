#include "connectionwindow.h"
#include "ui_connectionwindow.h"
#include <QtNetwork/QNetworkInterface>

ConnectionWindow::ConnectionWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectionWindow)
{
    ui->setupUi(this);

    settings = new QSettings();

    int temp = settings->value("Main/DefaultConnectionType", 0).toInt();

    if (temp == 0) currentConnType = ConnectionType::GVRET_SERIAL;
    if (temp == 1) currentConnType = ConnectionType::KVASER;
    if (temp == 2) currentConnType = ConnectionType::SOCKETCAN;

    currentPortName = settings->value("Main/DefaultConnectionPort", "").toString();

    currentSpeed1 = -1;
    currentSpeed2 = -1;    

    ui->ckSingleWire->setChecked(settings->value("Main/SingleWireMode", false).toBool());

    ui->cbSpeed0->addItem(tr("<Default>"));
    ui->cbSpeed0->addItem(tr("Disabled"));
    ui->cbSpeed0->addItem(tr("125000"));
    ui->cbSpeed0->addItem(tr("250000"));
    ui->cbSpeed0->addItem(tr("500000"));
    ui->cbSpeed0->addItem(tr("1000000"));
    ui->cbSpeed0->addItem(tr("33333"));

    ui->cbSpeed1->addItem(tr("<Default>"));
    ui->cbSpeed1->addItem(tr("Disabled"));
    ui->cbSpeed1->addItem(tr("125000"));
    ui->cbSpeed1->addItem(tr("250000"));
    ui->cbSpeed1->addItem(tr("500000"));
    ui->cbSpeed1->addItem(tr("1000000"));
    ui->cbSpeed1->addItem(tr("33333"));

#ifdef Q_OS_LINUX
    ui->rbSocketCAN->setEnabled(true);
#endif

#ifdef Q_OS_WIN
    ui->rbKvaser->setEnabled(true);
#endif

    connect(ui->btnOK, SIGNAL(clicked(bool)), this, SLOT(handleOKButton()));
}

ConnectionWindow::~ConnectionWindow()
{
    delete settings;
    delete ui;
}

void ConnectionWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    qDebug() << "Show connectionwindow";
    if (ui->rbGVRET->isChecked()) getSerialPorts();
    if (ui->rbKvaser->isChecked()) getKvaserPorts();
    if (ui->rbSocketCAN->isChecked()) getSocketcanPorts();
}

void ConnectionWindow::handleOKButton()
{
    QString conn;
    int connType = 0;

    if (ui->rbGVRET->isChecked())
    {
        conn = "GVRET";
        connType = 0;
        currentConnType = ConnectionType::GVRET_SERIAL;
    }

    if (ui->rbKvaser->isChecked())
    {
        conn = "KVASER";
        connType = 1;
        currentConnType = ConnectionType::KVASER;
    }

    if (ui->rbSocketCAN->isChecked())
    {
        conn = "SOCKETCAN";
        connType = 2;
        currentConnType = ConnectionType::SOCKETCAN;
    }

    currentPortName = getPortName();
    currentSpeed1 = getSpeed0();
    currentSpeed2 = getSpeed1();

    settings->setValue("Main/DefaultConnectionPort", currentPortName);
    settings->setValue("Main/DefaultConnectionType", connType);
    settings->setValue("Main/SingleWireMode", ui->ckSingleWire->isChecked());

    emit updateConnectionSettings(conn, getPortName(), getSpeed0(), getSpeed1());

    this->close();
}

void ConnectionWindow::getSerialPorts()
{
    ui->cbPort->clear();
    ports = QSerialPortInfo::availablePorts();

    for (int i = 0; i < ports.count(); i++)
    {
        ui->cbPort->addItem(ports[i].portName());
        if (currentPortName == ports[i].portName()) ui->cbPort->setCurrentIndex(i);
    }
}

void ConnectionWindow::getKvaserPorts()
{

}

void ConnectionWindow::getSocketcanPorts()
{
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    QString interfaceName;

    ui->cbPort->clear();

    foreach (QNetworkInterface interface, interfaces)
    {
        interfaceName = interface.name().toLower();
        qDebug() << "Interface: " << interface.name();
        if (interfaceName.contains("can"))
        {
            ui->cbPort->addItem(interfaceName);
        }
    }
}

void ConnectionWindow::setSpeeds(int speed0, int speed1)
{
    bool found = false;

    qDebug() << speed0 << "X" << speed1;

    for (int i = 0; i < ui->cbSpeed0->count(); i++)
    {
        if (ui->cbSpeed0->itemText(i).toInt() == speed0)
        {
            ui->cbSpeed0->setCurrentIndex(i);
            found = true;
        }
    }
    if (!found)
    {
        ui->cbSpeed0->addItem(QString::number(speed0));
        ui->cbSpeed0->setCurrentIndex(ui->cbSpeed0->count() - 1);
    }

    found = false;
    for (int i = 0; i < ui->cbSpeed1->count(); i++)
    {
        if (ui->cbSpeed1->itemText(i).toInt() == speed1)
        {
            ui->cbSpeed1->setCurrentIndex(i);
            found = true;
        }
    }
    if (!found)
    {
        ui->cbSpeed1->addItem(QString::number(speed1));
        ui->cbSpeed1->setCurrentIndex(ui->cbSpeed1->count() - 1);
    }
}


//-1 means leave it at whatever it booted up to. 0 means disable. Otherwise the actual rate we want.
int ConnectionWindow::getSpeed0()
{
    switch (ui->cbSpeed0->currentIndex())
    {
    case -1:
        return -1;
        break;
    case 0:
        return -1;
        break;
    case 1:
        return 0;
        break;
    default:
        return (ui->cbSpeed0->currentText().toInt());
        break;
    }
}

int ConnectionWindow::getSpeed1()
{
    switch (ui->cbSpeed1->currentIndex())
    {
    case -1:
        return -1;
        break;
    case 0:
        return -1;
        break;
    case 1:
        return 0;
        break;
    default:
        return (ui->cbSpeed1->currentText().toInt());
        break;
    }
}

QString ConnectionWindow::getPortName()
{
    return (ui->cbPort->currentText());
}

ConnectionType::ConnectionType ConnectionWindow::getConnectionType()
{
    if (ui->rbGVRET->isChecked()) return ConnectionType::GVRET_SERIAL;
    if (ui->rbKvaser->isChecked()) return ConnectionType::KVASER;
    if (ui->rbSocketCAN->isChecked()) return ConnectionType::SOCKETCAN;
}

void ConnectionWindow::setCAN1SWMode(bool mode)
{
    ui->ckSingleWire->setChecked(mode);
}

bool ConnectionWindow::getCAN1SWMode()
{
    if (ui->ckSingleWire->checkState() == Qt::Checked) return true;
    return false;
}
