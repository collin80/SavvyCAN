#include <QCanBus>
#include <QNetworkDatagram>
#include <QThread>

#include "gs_usb_driver/gs_usb.h"
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
    ui->tableConnections->setColumnWidth(2, 130);
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
    connect(ui->btnResetConn, &QPushButton::clicked, this, &ConnectionWindow::handleResetConn);
    connect(ui->tableConnections->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &ConnectionWindow::currentRowChanged);
    connect(ui->tabBuses, &QTabBar::currentChanged, this, &ConnectionWindow::currentTabChanged);
    connect(ui->btnSaveBus, &QPushButton::clicked, this, &ConnectionWindow::saveBusSettings);
    connect(ui->btnMoveUp, &QPushButton::clicked, this, &ConnectionWindow::moveConnUp);
    connect(ui->btnMoveDown, &QPushButton::clicked, this, &ConnectionWindow::moveConnDown);

    ui->cbBusSpeed->addItem("33333");
    ui->cbBusSpeed->addItem("50000");
    ui->cbBusSpeed->addItem("83333");
    ui->cbBusSpeed->addItem("100000");
    ui->cbBusSpeed->addItem("125000");
    ui->cbBusSpeed->addItem("250000");
    ui->cbBusSpeed->addItem("500000");
    ui->cbBusSpeed->addItem("1000000");
    //ui->cbBusSpeed->addItem("75000");
    //ui->cbBusSpeed->addItem("166666");
    //ui->cbBusSpeed->addItem("233333");
    //ui->cbBusSpeed->addItem("400000");

    rxBroadcastGVRET = new QUdpSocket(this);
    //Need to make sure it tries to share the address in case there are
    //multiple instances of SavvyCAN running.
    rxBroadcastGVRET->bind(QHostAddress::AnyIPv4, 17222, QAbstractSocket::ShareAddress);
    connect(rxBroadcastGVRET, &QUdpSocket::readyRead, this, &ConnectionWindow::readPendingDatagrams);

    //Doing the same for socketcand/kayak hosts:
    rxBroadcastKayak = new QUdpSocket(this);
    rxBroadcastKayak->bind(QHostAddress::AnyIPv4, 42000, QAbstractSocket::ShareAddress);
    connect(rxBroadcastKayak, &QUdpSocket::readyRead, this, &ConnectionWindow::readPendingDatagrams);

}


void ConnectionWindow::readPendingDatagrams()
{
    //qDebug() << "Got a UDP frame!";
    while (rxBroadcastGVRET->hasPendingDatagrams()) {
        QNetworkDatagram datagram = rxBroadcastGVRET->receiveDatagram();
        if (!remoteDeviceIPGVRET.contains(datagram.senderAddress().toString()))
        {
            remoteDeviceIPGVRET.append(datagram.senderAddress().toString());
            //qDebug() << "Add new remote IP " << datagram.senderAddress().toString();
        }
    }
    while (rxBroadcastKayak->hasPendingDatagrams()) {
        QNetworkDatagram datagram = rxBroadcastKayak->receiveDatagram();
        //qDebug() << "Broadcast Datagram: " << QString::fromUtf8(datagram.data());
        QXmlStreamReader CANBeaconXml(QString::fromUtf8(datagram.data()));
        QString KayakHost;
        QString KayakBus;
        while(!CANBeaconXml.atEnd() && !CANBeaconXml.hasError())
        {
          CANBeaconXml.readNext();
          if(CANBeaconXml.name() == QString("CANBeacon") && !CANBeaconXml.isEndElement())
                KayakHost.append(CANBeaconXml.attributes().value("name"));

          if(CANBeaconXml.name() == QString("URL"))
                KayakHost.append(" (" + CANBeaconXml.readElementText() + ')');

          //Kayak can theoretically send multiple busses over one ports
          //TODO: implement this case in socketcand.cpp
          if(CANBeaconXml.name() == QString("Bus") && !CANBeaconXml.isEndElement())
                KayakBus.append(CANBeaconXml.attributes().value("name").toUtf8() + ",");

        }
        KayakHost = KayakBus.left(KayakBus.length() - 1) + "@" + KayakHost;

        QVector<QString> connectedPorts;
        if (connModel->rowCount() > 0)
        {
            for (int i = 0; i < connModel->rowCount(); i++)
            {
                CANConnection *var_conn = connModel->getAtIdx(i);
                connectedPorts.append(var_conn->getPort());
            }
        }

        if (connectedPorts.contains(KayakHost))
        {
            remoteDeviceKayak.removeOne(KayakHost);
        }

        if (!remoteDeviceKayak.contains(KayakHost) && !connectedPorts.contains(KayakHost))
        {
            remoteDeviceKayak.append(KayakHost);
            //qDebug() << "Add new remote IP " << datagram.senderAddress().toString();
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
            HelpWindow::getRef()->showHelp("connectionwindow.md");
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
        move(Utility::constrainedWindowPos(settings.value("ConnWindow/WindowPos", QPoint(100, 100)).toPoint()));
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
        connect(conn_p, &CANConnection::debugOutput, this, &ConnectionWindow::getDebugText, Qt::UniqueConnection);
        connect(this, &ConnectionWindow::sendDebugData, conn_p, &CANConnection::debugInput, Qt::UniqueConnection);
    }
    else { //turn it off
        disconnect(conn_p, &CANConnection::debugOutput, nullptr, nullptr);
        disconnect(this, &ConnectionWindow::sendDebugData, conn_p, &CANConnection::debugInput);
    }
}

void ConnectionWindow::handleNewConn()
{
    CANCon::type newType;
    QString newPort;
    QString newDriver;
    int newSerialSpeed;
    int newBusSpeed;
    int newSamplePoint;
    bool newCanFd;
    int newDataRate;
    CANConnection *conn;

#ifdef GS_USB_DRIVER_ENABLED
    // Get GS USB devices before opening new connection window
    GSUSBDevice::getConnectedDevices(remoteDeviceGSUSB);
#endif

    NewConnectionDialog *thisDialog = new NewConnectionDialog(&remoteDeviceIPGVRET, &remoteDeviceKayak, &remoteDeviceGSUSB);
    if (thisDialog->exec() == QDialog::Accepted)
    {
        newType = thisDialog->getConnectionType();
        newPort = thisDialog->getPortName();
        newDriver = thisDialog->getDriverName();
        newSerialSpeed = thisDialog->getSerialSpeed();
        newBusSpeed = thisDialog->getBusSpeed();
        newSamplePoint = thisDialog->getSamplePoint();
        newCanFd=thisDialog->isCanFd();
        newDataRate = thisDialog->getDataRate();
        conn = create(newType, newPort, newDriver, newSerialSpeed, newBusSpeed, newCanFd, newDataRate, newSamplePoint);
        if (conn)
        {
            connModel->add(conn);
            ui->tableConnections->setCurrentIndex(connModel->index(connModel->rowCount() - 1, 1));
        }
    }
    delete thisDialog;
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

void ConnectionWindow::handleResetConn()
{
    QString port, driver;
    CANCon::type type;
    int serSpeed, busSpeed, samplePoint, dataRate;
    bool canFd;
    int offset = ui->tabBuses->currentIndex();

    int selIdx = ui->tableConnections->selectionModel()->currentIndex().row();
    if (selIdx <0) return;

    qDebug() << "remove connection at index: " << selIdx;

    CANConnection* conn_p = connModel->getAtIdx(selIdx);
    if(!conn_p) return;

    type = conn_p->getType();
    port = conn_p->getPort();
    driver = conn_p->getDriver();
    serSpeed = conn_p->getSerialSpeed();

    CANBus bus;
    if (conn_p->getBusSettings(offset, bus)) {
        busSpeed = bus.getSpeed();
        samplePoint = bus.getSamplePoint();
        dataRate = bus.getDataRate();
        canFd = bus.isCanFD();
    } else {
        qDebug() << "Could not retrieve bus settings!";
        busSpeed = 0;
        samplePoint = 0;
        dataRate = 0;
        canFd = false;
    }

    /* stop and delete connection */
    conn_p->stop();

    conn_p = nullptr;

    conn_p = create(type, port, driver, serSpeed, busSpeed, canFd, dataRate, samplePoint);
    if (conn_p) connModel->replace(selIdx, conn_p);
}

/* status */
void ConnectionWindow::connectionStatus(CANConStatus pStatus)
{
    Q_UNUSED(pStatus);

    qDebug() << "Connectionstatus changed";
    connModel->refresh();
}

void ConnectionWindow::setSuspendAll(bool pSuspend)
{
    QList<CANConnection*>& conns = CANConManager::getInstance()->getConnections();

    foreach(CANConnection* conn_p, conns)
        conn_p->suspend(pSuspend);

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

        if (conn_p->getType() == CANCon::type::GSUSB) {
#ifdef GS_USB_DRIVER_ENABLED
            QList<GSUSB_timing_t> supportedTiming;
            GSUSBDevice* device = (GSUSBDevice*) conn_p;
            int currentSpeedIndex = ui->cbBusSpeed->currentIndex();
            if ((currentSpeedIndex >= 0) && device->isDeviceConnected()){
                candle_handle handle = device->getDeviceHandle();
                GSUSBDevice::getSupportedTimings(handle, supportedTiming);
                if (supportedTiming.length() > currentSpeedIndex) {
                    GSUSB_timing_t timing = supportedTiming.at(currentSpeedIndex);
                    bus.setSpeed((int) timing.bitrate);
                    bus.setSamplePoint((int) timing.samplePoint);
                }
            }
#endif
        } else {
            bus.setSpeed(ui->cbBusSpeed->currentText().toInt());
        }

        bus.setActive(ui->ckEnable->isChecked());
        bus.setListenOnly(ui->ckListenOnly->isChecked());
        bus.setCanFD(ui->canFDEnable->isChecked());
        bus.setDataRate(ui->cbDataRate->currentText().toInt());
        conn_p->setBusSettings(offset, bus);
    }
}

void ConnectionWindow::populateBusDetails(int offset)
{
    int selIdx = ui->tableConnections->currentIndex().row();
    bool found = false;

    /* set parameters */
    if (selIdx == -1) {
        return;
    }
    else
    {
        //bool ret;
        //int numBuses;
        ui->canFDEnable->setVisible(false);
        ui->canFDEnable_label->setVisible(false);
        ui->dataRate_label->setVisible(false);
        ui->cbDataRate->setVisible(false);
        CANConnection* conn_p = connModel->getAtIdx(selIdx);
        CANBus bus;
        if(!conn_p) return;

        if (!conn_p->getBusSettings(offset, bus))
        {
            qDebug() << "Could not retrieve bus settings!";
            return;
        }

        //int busBase = CANConManager::getInstance()->getBusBase(conn_p);
        //ui->lblBusNum->setText(QString::number(busBase + offset));
        ui->ckListenOnly->setChecked(bus.isListenOnly());
        ui->ckEnable->setChecked(bus.isActive());
        if (conn_p->getType() == CANCon::type::SERIALBUS || conn_p->getType() == CANCon::type::LAWICEL)
        {
            ui->canFDEnable->setVisible(true);
            ui->canFDEnable_label->setVisible(true);
            ui->canFDEnable->setChecked(bus.isCanFD());
            ui->cbDataRate->setVisible(true);
            ui->dataRate_label->setVisible(true);
        }

        if (conn_p->getType() == CANCon::type::GSUSB) {
            ui->ckEnable->setEnabled(false);
            ui->cbBusSpeed->clear();
#ifdef GS_USB_DRIVER_ENABLED
            GSUSBDevice* device = (GSUSBDevice*) conn_p;
            int selectionIndex = 0;
            if (device->isDeviceConnected()) {
                candle_handle handle = device->getDeviceHandle();
                QList<GSUSB_timing_t> supportedTiming;
                GSUSBDevice::getSupportedTimings(handle, supportedTiming);
                foreach (const GSUSB_timing_t timing, supportedTiming) {
                    ui->cbBusSpeed->addItem(GSUSBDevice::timingToString(timing));
                    if ((bus.getSpeed() == (int) timing.bitrate) && (bus.getSamplePoint() == (int) timing.samplePoint)) {
                        found = true;
                    } else if (found == false) {
                        selectionIndex++;
                    }
                }
            }
            if (found) {
                ui->cbBusSpeed->setCurrentIndex(selectionIndex);
            }
#endif
        } else {
            ui->ckEnable->setEnabled(true);
            ui->cbBusSpeed->clear();

            ui->cbBusSpeed->addItem("33333");
            ui->cbBusSpeed->addItem("50000");
            ui->cbBusSpeed->addItem("83333");
            ui->cbBusSpeed->addItem("100000");
            ui->cbBusSpeed->addItem("125000");
            ui->cbBusSpeed->addItem("250000");
            ui->cbBusSpeed->addItem("500000");
            ui->cbBusSpeed->addItem("1000000");

            for (int i = 0; i < ui->cbBusSpeed->count(); i++) {
                if (bus.getSpeed() == ui->cbBusSpeed->itemText(i).toInt()) {
                    found = true;
                    ui->cbBusSpeed->setCurrentIndex(i);
                    break;
                }
            }
            if (!found) {
                ui->cbBusSpeed->addItem(QString::number(bus.getSpeed()));
            }
        }

        found = false;
        for (int i = 0; i < ui->cbDataRate->count(); i++)
        {
            if (bus.getDataRate() == ui->cbDataRate->itemText(i).toInt())
            {
                found = true;
                ui->cbDataRate->setCurrentIndex(i);
                break;
            }
        }
        if (!found) ui->cbDataRate->addItem(QString::number(bus.getDataRate()));
    }
}

void ConnectionWindow::currentTabChanged(int newIdx)
{
    populateBusDetails(newIdx);
}

void ConnectionWindow::currentRowChanged(const QModelIndex &current, const QModelIndex &previous)
{
    int selIdx = current.row();
    CANConnection* prevConn = connModel->getAtIdx(previous.row());
    if(prevConn != nullptr)
        disconnect(prevConn, &CANConnection::debugOutput, nullptr, nullptr);
    disconnect(this, &ConnectionWindow::sendDebugData, nullptr, nullptr);

    /* set parameters */
    if (selIdx == -1) {
        ui->groupBus->setEnabled(false);
        return;
    }
    else
    {
        //bool ret;
        ui->groupBus->setEnabled(true);
        int numBuses;

        CANConnection* conn_p = connModel->getAtIdx(selIdx);
        if(!conn_p) return;

        //because this might have already been setup during the initial setup so tear that one down and then create the normal one.
        //disconnect(conn_p, &CANConnection::debugOutput, 0, 0);

        numBuses = conn_p->getNumBuses();
        int numB = ui->tabBuses->count();
        for (int i = 0; i < numB; i++) ui->tabBuses->removeTab(0);

        int busBase = CANConManager::getInstance()->getBusBase(conn_p);

        /*if (numBuses > 1)*/ for (int i = 0; i < numBuses; i++) ui->tabBuses->addTab(QString::number(busBase + i));

        populateBusDetails(0);
        if (ui->ckEnableConsole->isChecked())
        {
            connect(conn_p, &CANConnection::debugOutput, this, &ConnectionWindow::getDebugText, Qt::UniqueConnection);
            connect(this, &ConnectionWindow::sendDebugData, conn_p, &CANConnection::debugInput, Qt::UniqueConnection);
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

CANConnection* ConnectionWindow::create(CANCon::type pTye, QString pPortName, QString pDriver, int pSerialSpeed, int pBusSpeed, bool pCanFd, int pDataRate, int pSamplePoint)
{
    CANConnection* conn_p;

    /* create connection */
    conn_p = CanConFactory::create(pTye, pPortName, pDriver, pSerialSpeed, pBusSpeed, pCanFd, pDataRate, pSamplePoint);
    if(conn_p)
    {
        /* connect signal */
        connect(conn_p, &CANConnection::status, this, &ConnectionWindow::connectionStatus);
        if (ui->ckEnableConsole->isChecked())
        {            
            //set up the debug console to operate if we've selected it. Doing so here allows debugging right away during set up
            connect(conn_p, &CANConnection::debugOutput, this, &ConnectionWindow::getDebugText, Qt::UniqueConnection);
        }
        /*TODO add return value and checks */
        conn_p->start();
    }
    return conn_p;
}


void ConnectionWindow::loadConnections()
{
#if QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 )
    qRegisterMetaTypeStreamOperators<CANBus>();
    qRegisterMetaTypeStreamOperators<QList<CANBus>>();
#endif

    QSettings settings;

    /* fill connection list */
    QVector<QString> portNames = settings.value("connections/portNames").value<QVector<QString>>();
    QVector<QString> driverNames = settings.value("connections/driverNames").value<QVector<QString>>();
    QVector<int>    devTypes = settings.value("connections/types").value<QVector<int>>();

    //don't load the connections if the three setting arrays above aren't all the same size.
    if (portNames.count() != driverNames.count() || devTypes.count() != driverNames.count()) return;

    for(int i = 0 ; i < portNames.count() ; i++)
    {
        //TODO: add serial speed and bus speed to this properly.
        CANConnection* conn_p = create((CANCon::type)devTypes[i], portNames[i], driverNames[i], 0, 0, false, 0, 875);
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
    QVector<int> serialSpeeds;
    QVector<int> busSpeeds;

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

void ConnectionWindow::moveConnUp()
{
    int selIdx = ui->tableConnections->selectionModel()->currentIndex().row();
    if (selIdx > 0)
    {
        CANConnection* selConn = connModel->getAtIdx(selIdx);
        CANConnection* prevConn = connModel->getAtIdx(selIdx - 1);
        connModel->replace(selIdx - 1, selConn);
        connModel->replace(selIdx, prevConn);
        ui->tableConnections->selectRow(selIdx - 1);
    }
}

void ConnectionWindow::moveConnDown()
{
    int selIdx = ui->tableConnections->selectionModel()->currentIndex().row();
    if (selIdx < connModel->rowCount() - 1)
    {
        CANConnection* selConn = connModel->getAtIdx(selIdx);
        CANConnection* nextConn = connModel->getAtIdx(selIdx + 1);
        connModel->replace(selIdx + 1, selConn);
        connModel->replace(selIdx, nextConn);
        ui->tableConnections->selectRow(selIdx + 1);
    }
}
