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
            SerialWorker *serial = new SerialWorker(canModel, 0);
            connModel->addConnection(serial);
        }
        else if (ui->rbKvaser->isChecked())
        {

        }
        else if (ui->rbSocketCAN->isChecked())
        {

        }
    }

/*
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
    currentSpeed1 = getSpeed();

    settings->setValue("Main/DefaultConnectionPort", currentPortName);
    settings->setValue("Main/DefaultConnectionType", connType);
    settings->setValue("Main/SingleWireMode", ui->ckSingleWire->isChecked());

    emit updateConnectionSettings(conn, getPortName(), getSpeed());

    this->close();
*/
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
