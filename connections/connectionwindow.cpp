#include <QCanBus>
#include <QNetworkDatagram>
#include <QThread>

#include "connectionwindow.h"
#include "mainwindow.h"
#include "helpwindow.h"
#include "ui_connectionwindow.h"
#include "connections/canconfactory.h"
#include "connections/canconmanager.h"
#include "canbus.h"
#include <QSettings>
#include <connections/newconnectiondialog.h>

ConnectionWindow::ConnectionWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectionWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    QSettings settings;

    qRegisterMetaType<CANBus>("CANBus");
    qRegisterMetaType<const CANFrame *>("const CANFrame *");
    qRegisterMetaType<const QList<CANFrame> *>("const QList<CANFrame> *");


    //List of devices with details. None of it can be edited. connection type, serialbus type, port name, number of buses, status
    connModel = new CANConnectionModel(this);
    ui->tableConnections->setModel(connModel);
    ui->tableConnections->setColumnWidth(0, 100);
    ui->tableConnections->setColumnWidth(1, 100);
    ui->tableConnections->setColumnWidth(2, 100);
    ui->tableConnections->setColumnWidth(3, 70);
    ui->tableConnections->setColumnWidth(4, 200);
    QHeaderView *HorzHdr = ui->tableConnections->horizontalHeader();
    HorzHdr->setStretchLastSection(true); //causes the data column to automatically fill the tableview

    ui->textConsole->setEnabled(false);
    ui->btnClearDebug->setEnabled(false);
    ui->btnSendHex->setEnabled(false);
    ui->btnSendText->setEnabled(false);
    ui->lineSend->setEnabled(false);

    if (settings.value("Main/SaveRestoreConnections", false).toBool())
    {
        /* load connection configuration */
        loadConnections();
    }    

    connect(ui->btnDisconnect, &QPushButton::clicked, this, &ConnectionWindow::handleRemoveConn);
    connect(ui->btnSendHex, &QPushButton::clicked, this, &ConnectionWindow::handleSendHex);
    connect(ui->btnSendText, &QPushButton::clicked, this, &ConnectionWindow::handleSendText);
    connect(ui->ckEnableConsole, &QCheckBox::toggled, this, &ConnectionWindow::consoleEnableChanged);
    connect(ui->btnClearDebug, &QPushButton::clicked, this, &ConnectionWindow::handleClearDebugText);
    connect(ui->btnNewConnection, &QPushButton::clicked, this, &ConnectionWindow::handleNewConn);
    connect(ui->tableConnections->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &ConnectionWindow::currentRowChanged);
    connect(ui->tabBuses, &QTabBar::currentChanged, this, &ConnectionWindow::currentTabChanged);
    connect(ui->btnSaveBus, &QPushButton::clicked, this, &ConnectionWindow::saveBusSettings);

    ui->cbBusSpeed->addItem("50000");
    ui->cbBusSpeed->addItem("100000");
    ui->cbBusSpeed->addItem("125000");
    ui->cbBusSpeed->addItem("250000");
    ui->cbBusSpeed->addItem("500000");
    ui->cbBusSpeed->addItem("1000000");

    rxBroadcast = new QUdpSocket(this);
    rxBroadcast->bind(QHostAddress::AnyIPv4, 17222);

    connect(rxBroadcast, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));

}


void ConnectionWindow::readPendingDatagrams()
{
    while (rxBroadcast->hasPendingDatagrams()) {
        QNetworkDatagram datagram = rxBroadcast->receiveDatagram();
        if (!remoteDeviceIP.contains(datagram.senderAddress().toString()))
        {
            remoteDeviceIP.append(datagram.senderAddress().toString());
        }
    }
}
ConnectionWindow::~ConnectionWindow()
{
    QList<CANConnection*>& conns = CANConManager::getInstance()->getConnections();
    CANConnection* conn_p;

    /* save configuration */
    saveConnections();

    /* delete connections */
    while(!conns.isEmpty())
    {
        conn_p = conns.takeFirst();
        conn_p->stop();
        delete conn_p;
    }

    delete ui;
}


void ConnectionWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    qDebug() << "Show connectionwindow";
    installEventFilter(this);
    readSettings();
    ui->tableConnections->selectRow(0);
    currentRowChanged(ui->tableConnections->currentIndex(), ui->tableConnections->currentIndex());
}

void ConnectionWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    removeEventFilter(this);
    writeSettings();
}

bool ConnectionWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("connectionwindow.html");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    //return false;
}

void ConnectionWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("ConnWindow/WindowSize", QSize(956, 665)).toSize());
        move(settings.value("ConnWindow/WindowPos", QPoint(100, 100)).toPoint());
    }
}

void ConnectionWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("ConnWindow/WindowSize", size());
        settings.setValue("ConnWindow/WindowPos", pos());
    }
}

void ConnectionWindow::consoleEnableChanged(bool checked) {
    ui->textConsole->setEnabled(checked);
    ui->btnClearDebug->setEnabled(checked);
    ui->btnSendHex->setEnabled(checked);
    ui->btnSendText->setEnabled(checked);
    ui->lineSend->setEnabled(checked);

    int selIdx = ui->tableConnections->currentIndex().row();

    if (selIdx == -1)
        return;

    CANConnection* conn_p = connModel->getAtIdx(selIdx);

    if (checked) { //enable console
        connect(conn_p, SIGNAL(debugOutput(QString)), this, SLOT(getDebugText(QString)));
        connect(this, SIGNAL(sendDebugData(QByteArray)), conn_p, SLOT(debugInput(QByteArray)));
    }
    else { //turn it off
        disconnect(conn_p, SIGNAL(debugOutput(QString)), nullptr, nullptr);
        disconnect(this, SIGNAL(sendDebugData(QByteArray)), conn_p, SLOT(debugInput(QByteArray)));
    }
}

void ConnectionWindow::handleNewConn()
{
    NewConnectionDialog *thisDialog = new NewConnectionDialog(&remoteDeviceIP);
    CANCon::type newType;
    QString newPort;
    QString newDriver;
    CANConnection *conn;

    if (thisDialog->exec() == QDialog::Accepted)
    {
        newType = thisDialog->getConnectionType();
        newPort = thisDialog->getPortName();
        newDriver = thisDialog->getDriverName();
        conn = create(newType, newPort, newDriver);
        if (conn) connModel->add(conn);
    }
    delete thisDialog;
}

/* status */
void ConnectionWindow::connectionStatus(CANConStatus pStatus)
{
    Q_UNUSED(pStatus);

    qDebug() << "Connectionstatus changed";
    connModel->refresh();
}

void ConnectionWindow::saveBusSettings()
{
    int selIdx = ui->tableConnections->currentIndex().row();
    int offset = ui->tabBuses->currentIndex();

    /* set parameters */
    if (selIdx == -1) {
        return;
    }
    else
    {
        CANConnection* conn_p = connModel->getAtIdx(selIdx);
        CANBus bus;
        if(!conn_p) return;

        if (!conn_p->getBusSettings(offset, bus))
        {
            qDebug() << "Could not retrieve bus settings!";
            return;
        }

        bus.setSpeed(ui->cbBusSpeed->currentText().toInt());
        bus.setActive(ui->ckEnable->isChecked());
        bus.setListenOnly(ui->ckListenOnly->isChecked());
        conn_p->setBusSettings(offset, bus);
    }
}

void ConnectionWindow::populateBusDetails(int offset)
{
    int selIdx = ui->tableConnections->currentIndex().row();

    /* set parameters */
    if (selIdx == -1) {
        return;
    }
    else
    {
        bool ret;
        int numBuses;

        CANConnection* conn_p = connModel->getAtIdx(selIdx);
        CANBus bus;
        if(!conn_p) return;

        if (!conn_p->getBusSettings(offset, bus))
        {
            qDebug() << "Could not retrieve bus settings!";
            return;
        }

        int busBase = CANConManager::getInstance()->getBusBase(conn_p);
        ui->lblBusNum->setText(QString::number(busBase + offset));
        ui->ckListenOnly->setChecked(bus.isListenOnly());
        ui->ckEnable->setChecked(bus.isActive());

        bool found = false;
        for (int i = 0; i < ui->cbBusSpeed->count(); i++)
        {
            if (bus.getSpeed() == ui->cbBusSpeed->itemText(i).toInt())
            {
                found = true;
                ui->cbBusSpeed->setCurrentIndex(i);
                break;
            }
        }
        if (!found) ui->cbBusSpeed->addItem(QString::number(bus.getSpeed()));
    }
}

void ConnectionWindow::currentTabChanged(int newIdx)
{
    populateBusDetails(newIdx);
}

void ConnectionWindow::currentRowChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);

    int selIdx = current.row();

    disconnect(connModel->getAtIdx(previous.row()), SIGNAL(debugOutput(QString)), 0, 0);
    disconnect(this, SIGNAL(sendDebugData(QByteArray)), connModel->getAtIdx(previous.row()), SLOT(debugInput(QByteArray)));

    /* set parameters */
    if (selIdx == -1) {
        ui->groupBus->setEnabled(false);
        return;
    }
    else
    {
        bool ret;
        ui->groupBus->setEnabled(true);
        int numBuses;

        CANConnection* conn_p = connModel->getAtIdx(selIdx);
        if(!conn_p) return;

        numBuses = conn_p->getNumBuses();
        int numB = ui->tabBuses->count();
        for (int i = 0; i < numB; i++) ui->tabBuses->removeTab(0);

        if (numBuses > 1) for (int i = 0; i < numBuses; i++) ui->tabBuses->addTab(QString::number(i+1));

        populateBusDetails(0);
        if (ui->ckEnableConsole->isChecked())
        {
            connect(conn_p, SIGNAL(debugOutput(QString)), this, SLOT(getDebugText(QString)));
            connect(this, SIGNAL(sendDebugData(QByteArray)), conn_p, SLOT(debugInput(QByteArray)));
        }
    }
}

void ConnectionWindow::getDebugText(QString debugText) {
    ui->textConsole->append(debugText);
}

void ConnectionWindow::handleClearDebugText() {
    ui->textConsole->clear();
}

void ConnectionWindow::handleSendHex() {
    QByteArray bytes;
    QStringList tokens = ui->lineSend->text().split(' ');
    foreach (QString token, tokens) {
        bytes.append(token.toInt(nullptr, 16));
    }
    emit sendDebugData(bytes);
}

void ConnectionWindow::handleSendText() {
    QByteArray bytes;
    bytes = ui->lineSend->text().toLatin1();
    bytes.append('\r'); //add carriage return for line ending
    emit sendDebugData(bytes);
}

void ConnectionWindow::handleRemoveConn()
{
    int selIdx = ui->tableConnections->selectionModel()->currentIndex().row();
    if (selIdx <0) return;

    qDebug() << "remove connection at index: " << selIdx;

    CANConnection* conn_p = connModel->getAtIdx(selIdx);
    if(!conn_p) return;

    /* remove connection from model & manager */
    connModel->remove(conn_p);

    /* stop and delete connection */
    conn_p->stop();
    delete conn_p;

    /* select first connection in list */
    ui->tableConnections->selectRow(0);
}

CANConnection* ConnectionWindow::create(CANCon::type pTye, QString pPortName, QString pDriver)
{
    CANConnection* conn_p;

    /* create connection */
    conn_p = CanConFactory::create(pTye, pPortName, pDriver);
    if(conn_p)
    {
        /* connect signal */
        connect(conn_p, SIGNAL(status(CANConStatus)),
                this, SLOT(connectionStatus(CANConStatus)));

        /*TODO add return value and checks */
        conn_p->start();
    }
    return conn_p;
}


void ConnectionWindow::loadConnections()
{
    qRegisterMetaTypeStreamOperators<CANBus>();
    qRegisterMetaTypeStreamOperators<QList<CANBus>>();

    QSettings settings;

    /* fill connection list */
    QVector<QString> portNames = settings.value("connections/portNames").value<QVector<QString>>();
    QVector<QString> driverNames = settings.value("connections/driverNames").value<QVector<QString>>();
    QVector<int>    devTypes = settings.value("connections/types").value<QVector<int>>();

    //don't load the connections if the three setting arrays above aren't all the same size.
    if (portNames.count() != driverNames.count() || devTypes.count() != driverNames.count()) return;

    for(int i = 0 ; i < portNames.count() ; i++)
    {
        CANConnection* conn_p = create((CANCon::type)devTypes[i], portNames[i], driverNames[i]);
        /* add connection to model */
        connModel->add(conn_p);
    }

    if (connModel->rowCount() > 0) {
        ui->tableConnections->selectRow(0);
    }
}

void ConnectionWindow::saveConnections()
{
    QList<CANConnection*>& conns = CANConManager::getInstance()->getConnections();

    QSettings settings;
    QVector<QString> portNames;
    QVector<int> devTypes;
    QVector<QString> driverNames;

    /* save connections */
    foreach(CANConnection* conn_p, conns)
    {
        portNames.append(conn_p->getPort());
        devTypes.append(conn_p->getType());
        driverNames.append(conn_p->getDriver());
    }

    settings.setValue("connections/portNames", QVariant::fromValue(portNames));
    settings.setValue("connections/types", QVariant::fromValue(devTypes));
    settings.setValue("connections/driverNames", QVariant::fromValue(driverNames));
}
