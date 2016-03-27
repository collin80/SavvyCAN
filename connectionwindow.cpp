#include "connectionwindow.h"
#include "ui_connectionwindow.h"
#include <QtNetwork/QNetworkInterface>

ConnectionWindow::ConnectionWindow(CANFrameModel *cModel, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectionWindow)
{
    ui->setupUi(this);

    settings = new QSettings();

    connModel = new CANConnectionModel();
    ui->tableConnections->setModel(connModel);

    canModel = cModel;

    ui->tableConnections->setColumnWidth(0, 50);
    ui->tableConnections->setColumnWidth(1, 110);
    ui->tableConnections->setColumnWidth(2, 110);
    ui->tableConnections->setColumnWidth(3, 150);
    ui->tableConnections->setColumnWidth(4, 75);
    ui->tableConnections->setColumnWidth(5, 75);
    ui->tableConnections->setColumnWidth(6, 75);

    int temp = settings->value("Main/DefaultConnectionType", 0).toInt();

    //currentPortName = settings->value("Main/DefaultConnectionPort", "").toString();

    //currentSpeed1 = -1;

    ui->ckSingleWire->setChecked(settings->value("Main/SingleWireMode", false).toBool());

    ui->cbSpeed->addItem(tr("<Default>"));
    ui->cbSpeed->addItem(tr("Disabled"));
    ui->cbSpeed->addItem(tr("125000"));
    ui->cbSpeed->addItem(tr("250000"));
    ui->cbSpeed->addItem(tr("500000"));
    ui->cbSpeed->addItem(tr("1000000"));
    ui->cbSpeed->addItem(tr("33333"));

#ifdef Q_OS_LINUX
    ui->rbSocketCAN->setEnabled(true);
#endif

#ifdef Q_OS_WIN
    ui->rbKvaser->setEnabled(true);
#endif

    connect(ui->btnOK, &QAbstractButton::clicked, this, &ConnectionWindow::handleOKButton);
    connect(ui->rbGVRET, &QAbstractButton::toggled, this, &ConnectionWindow::handleConnTypeChanged);
    connect(ui->rbKvaser, &QAbstractButton::toggled, this, &ConnectionWindow::handleConnTypeChanged);
    connect(ui->rbSocketCAN, &QAbstractButton::toggled, this, &ConnectionWindow::handleConnTypeChanged);
    connect(ui->btnRevert, &QPushButton::clicked, this, &ConnectionWindow::handleRevert);
    connect(ui->tableConnections->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ConnectionWindow::handleConnSelectionChanged);
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
    handleConnTypeChanged();
}

void ConnectionWindow::handleConnTypeChanged()
{
    if (ui->rbGVRET->isChecked()) getSerialPorts();
    if (ui->rbKvaser->isChecked()) getKvaserPorts();
    if (ui->rbSocketCAN->isChecked()) getSocketcanPorts();
}

void ConnectionWindow::handleOKButton()
{
    QString conn;
    int connType = 0;

    if (ui->tableConnections->selectionModel()->currentIndex().row() >= 0)
    {

    }
    else //new connection
    {
        if (ui->rbGVRET->isChecked())
        {            
            SerialWorker *serial = new SerialWorker(canModel, connModel->rowCount());
            connect(serial, SIGNAL(busStatus(int,int,int)), this, SLOT(receiveBusStatus(int,int,int)));
            connModel->addConnection(serial);

            qDebug() << "Setup initial connection object";

            CAN_Bus bus;
            bus.active = true;
            bus.busNum = serial->getBusBase();
            bus.connection = serial;
            bus.listenOnly = ui->ckListenOnly->isChecked();
            bus.singleWire = ui->ckSingleWire->isChecked();

            if (ui->cbSpeed->currentIndex() < 1) bus.speed = 0; //default speed
            else if (ui->cbSpeed->currentIndex() == 1)
            {
                bus.speed = 0;
                bus.active = false;
            }
            else bus.speed = ui->cbSpeed->currentText().toInt();
            connModel->addBus(bus);

            int numBuses = serial->getNumBuses();
            for (int i = 1; i < numBuses; i++)
            {
                bus.active = false;
                bus.listenOnly = false;
                bus.singleWire = false;
                bus.speed = 250000;
                bus.busNum = serial->getBusBase() + i;
                bus.connection = serial;
                connModel->addBus(bus);
                qDebug() << "Added bus " << bus.busNum;
            }            

            //call through signal/slot interface without using connect
            QMetaObject::invokeMethod(serial, "updatePortName",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, ui->cbPort->currentText()));
        }
        else if (ui->rbKvaser->isChecked())
        {

        }
        else if (ui->rbSocketCAN->isChecked())
        {

        }
    }
}

void ConnectionWindow::receiveBusStatus(int bus, int speed, int status)
{
    qDebug() << "bus " << bus << " speed " << speed << " status " << status;
    CAN_Bus *busRef = connModel->getBus(bus);
    if (status & 40) busRef->setSpeed(speed);
    if (status & 8) //update enabled status
    {
        busRef->setEnabled((status & 1)?true:false);
    }
    if (status & 0x10) //update single wire status
    {
        busRef->setSingleWire((status & 2)?true:false);
    }
    if (status & 0x20) //update listen only status
    {
        busRef->setListenOnly((status & 4)?true:false);
    }
    connModel->refreshView();
}

void ConnectionWindow::handleConnSelectionChanged()
{
    int selIdx = ui->tableConnections->selectionModel()->currentIndex().row();
    if (selIdx == 0)
    {
        ui->btnOK->setText(tr("Create New Connection"));
    }
    else
    {
        ui->btnOK->setText(tr("Update Connection Settings"));
    }
}

void ConnectionWindow::getSerialPorts()
{
    ui->cbPort->clear();
    ports = QSerialPortInfo::availablePorts();

    for (int i = 0; i < ports.count(); i++)
    {
        ui->cbPort->addItem(ports[i].portName());
        //if (currentPortName == ports[i].portName()) ui->cbPort->setCurrentIndex(i);
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

void ConnectionWindow::setSpeed(int speed0)
{
    bool found = false;

    qDebug() << speed0;

    for (int i = 0; i < ui->cbSpeed->count(); i++)
    {
        if (ui->cbSpeed->itemText(i).toInt() == speed0)
        {
            ui->cbSpeed->setCurrentIndex(i);
            found = true;
        }
    }
    if (!found)
    {
        ui->cbSpeed->addItem(QString::number(speed0));
        ui->cbSpeed->setCurrentIndex(ui->cbSpeed->count() - 1);
    }

}


//-1 means leave it at whatever it booted up to. 0 means disable. Otherwise the actual rate we want.
int ConnectionWindow::getSpeed()
{
    switch (ui->cbSpeed->currentIndex())
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
        return (ui->cbSpeed->currentText().toInt());
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

void ConnectionWindow::setSWMode(bool mode)
{
    ui->ckSingleWire->setChecked(mode);
}

bool ConnectionWindow::getSWMode()
{
    if (ui->ckSingleWire->checkState() == Qt::Checked) return true;
    return false;
}

void ConnectionWindow::handleRemoveConn()
{

}

void ConnectionWindow::handleRevert()
{

}
