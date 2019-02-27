#include <QCanBus>
#include <QThread>

#include "connectionwindow.h"
#include "mainwindow.h"
#include "helpwindow.h"
#include "ui_connectionwindow.h"
#include "connections/canconfactory.h"
#include "connections/canconmanager.h"
#include "canbus.h"
#include <QSettings>

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

    connModel = new CANConnectionModel(this);
    ui->tableConnections->setModel(connModel);
    ui->tableConnections->setColumnWidth(0, 40);
    ui->tableConnections->setColumnWidth(1, 70);
    ui->tableConnections->setColumnWidth(2, 70);
    ui->tableConnections->setColumnWidth(3, 70);
    ui->tableConnections->setColumnWidth(4, 70);
    ui->tableConnections->setColumnWidth(5, 70);
    ui->tableConnections->setColumnWidth(6, 70);
    ui->tableConnections->setColumnWidth(7, 90);
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

    ui->rbSocketCAN->setEnabled(isSerialBusAvailable());

    connect(ui->btnOK, &QAbstractButton::clicked, this, &ConnectionWindow::handleOKButton);
    connect(ui->rbGVRET, &QAbstractButton::clicked, this, &ConnectionWindow::handleConnTypeChanged);
    connect(ui->rbSocketCAN, &QAbstractButton::clicked, this, &ConnectionWindow::handleConnTypeChanged);
    connect(ui->rbRemote, &QAbstractButton::clicked, this, &ConnectionWindow::handleConnTypeChanged);
    connect(ui->cbDeviceType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConnectionWindow::handleDeviceTypeChanged);
    connect(ui->tableConnections->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &ConnectionWindow::currentRowChanged);
    connect(ui->btnActivateAll, &QPushButton::clicked, this, &ConnectionWindow::handleEnableAll);
    connect(ui->btnDeactivateAll, &QPushButton::clicked, this, &ConnectionWindow::handleDisableAll);
    connect(ui->btnReconnect, &QPushButton::clicked, this, &ConnectionWindow::handleReconnect);
    connect(ui->btnRemoveBus, &QPushButton::clicked, this, &ConnectionWindow::handleRemoveConn);
    connect(ui->btnClearDebug, &QPushButton::clicked, this, &ConnectionWindow::handleClearDebugText);
    connect(ui->btnSendHex, &QPushButton::clicked, this, &ConnectionWindow::handleSendHex);
    connect(ui->btnSendText, &QPushButton::clicked, this, &ConnectionWindow::handleSendText);
    connect(ui->ckEnableConsole, &QCheckBox::toggled, this, &ConnectionWindow::consoleEnableChanged);

    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);
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
    handleConnTypeChanged();
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

void ConnectionWindow::setSuspendAll(bool pSuspend)
{
    QList<CANConnection*>& conns = CANConManager::getInstance()->getConnections();

    foreach(CANConnection* conn_p, conns)
        conn_p->suspend(pSuspend);

    connModel->refresh();
}


void ConnectionWindow::setActiveAll(bool pActive)
{
    CANBus bus;
    QList<CANConnection*>& conns = CANConManager::getInstance()->getConnections();

    foreach(CANConnection* conn_p, conns)
    {
        for(int i=0 ; i<conn_p->getNumBuses() ; i++) {
            if( conn_p->getBusSettings(i, bus) ) {
                bus.active = pActive;
                conn_p->setBusSettings(i, bus);
            }
        }
    }

    connModel->refresh();
}

void ConnectionWindow::handleReconnect()
{
    int selIdx = ui->tableConnections->selectionModel()->currentIndex().row();
    if (selIdx <0) return;

    int busId;
    CANConnection* conn_p = connModel->getAtIdx(selIdx, busId);
    if(!conn_p) return;

    conn_p->stop();
    conn_p->start();
}

void ConnectionWindow::consoleEnableChanged(bool checked) {
    ui->textConsole->setEnabled(checked);
    ui->btnClearDebug->setEnabled(checked);
    ui->btnSendHex->setEnabled(checked);
    ui->btnSendText->setEnabled(checked);
    ui->lineSend->setEnabled(checked);

    QList<CANConnection*>& conns = CANConManager::getInstance()->getConnections();

    foreach(CANConnection* conn_p, conns)
    {
        if (checked) { //enable console
            connect(conn_p, SIGNAL(debugOutput(QString)), this, SLOT(getDebugText(QString)));
            connect(this, SIGNAL(sendDebugData(QByteArray)), conn_p, SLOT(debugInput(QByteArray)));
        }
        else { //turn it off
            disconnect(conn_p, SIGNAL(debugOutput(QString)), nullptr, nullptr);
            disconnect(this, SIGNAL(sendDebugData(QByteArray)), conn_p, SLOT(debugInput(QByteArray)));
        }
    }
}

void ConnectionWindow::handleNewConn()
{
    ui->tableConnections->setCurrentIndex(QModelIndex());
    currentRowChanged(ui->tableConnections->currentIndex(), ui->tableConnections->currentIndex());
}


void ConnectionWindow::handleEnableAll()
{
    setActiveAll(true);
}

void ConnectionWindow::handleDisableAll()
{
    setActiveAll(false);
}

void ConnectionWindow::handleConnTypeChanged()
{
    if (ui->rbGVRET->isChecked()) selectSerial();
    if (ui->rbSocketCAN->isChecked()) selectSocketCan();
    if (ui->rbRemote->isChecked()) selectRemote();
}

void ConnectionWindow::handleDeviceTypeChanged()
{
    ui->cbPort->clear();
    canDevices = QCanBus::instance()->availableDevices(ui->cbDeviceType->currentText());

    for (int i = 0; i < canDevices.count(); i++)
        ui->cbPort->addItem(canDevices[i].name());
}

/* status */
void ConnectionWindow::connectionStatus(CANConStatus pStatus)
{
    Q_UNUSED(pStatus);

    qDebug() << "Connectionstatus changed";
    connModel->refresh();
}


void ConnectionWindow::handleOKButton()
{
    CANConnection* conn_p = nullptr;

    if( ! CANConManager::getInstance()->getByName(getPortName()) )
    {
        /* create connection */
        //qDebug() << "Create connection type: " << getConnectionType() << " port: " << getPortName() << " driver: " << getDriverName();
        conn_p = create(getConnectionType(), getPortName(), getDriverName());
        if(!conn_p)
            return;
        /* add connection to model */
        connModel->add(conn_p);
        consoleEnableChanged(ui->ckEnableConsole->isChecked());
    }
}

void ConnectionWindow::currentRowChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);

    int selIdx = current.row();

    int busId;

    disconnect(connModel->getAtIdx(previous.row(), busId), SIGNAL(debugOutput(QString)), 0, 0);
    disconnect(this, SIGNAL(sendDebugData(QByteArray)), connModel->getAtIdx(previous.row(), busId), SLOT(debugInput(QByteArray)));
return;

    /* enable / diable connection type */
    ui->stPort->setEnabled(selIdx==-1);
    ui->gbType->setEnabled(selIdx==-1);
    ui->lPort->setEnabled(selIdx==-1);

    /* set parameters */
    if (selIdx == -1)
    {
        ui->btnOK->setText(tr("Create New Connection"));
        ui->rbGVRET->setChecked(true);
        setSpeed(0);
        setPortName(CANCon::GVRET_SERIAL, "", "");
    }
    else
    {
        bool ret;
        CANBus bus;
        CANConnection* conn_p = connModel->getAtIdx(selIdx, busId);
        if(!conn_p) return;

        if (ui->ckEnableConsole->isChecked()) { //only connect if console is actually enabled
            connect(conn_p, SIGNAL(debugOutput(QString)), this, SLOT(getDebugText(QString)));
            connect(this, SIGNAL(sendDebugData(QByteArray)), conn_p, SLOT(debugInput(QByteArray)));
        }

        ret = conn_p->getBusSettings(busId, bus);
        if(!ret) return;

        ui->btnOK->setText(tr("Update Connection Settings"));
        setSpeed(bus.getSpeed());
        setPortName(conn_p->getType(), conn_p->getPort(), conn_p->getDriver());
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

void ConnectionWindow::selectSerial()
{
    ui->lPort->setText("Port:");
    /* set combobox page visible */
    ui->stPort->setCurrentWidget(ui->cbPage);
    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);

    ui->cbPort->clear();
    ports = QSerialPortInfo::availablePorts();

    for (int i = 0; i < ports.count(); i++)
        ui->cbPort->addItem(ports[i].portName());
}

void ConnectionWindow::selectSocketCan()
{
    ui->lPort->setText("Port:");
    /* set edit text page visible */
    ui->stPort->setCurrentWidget(ui->cbPage);
    ui->lblDeviceType->setHidden(false);
    ui->cbDeviceType->setHidden(false);

    ui->cbDeviceType->clear();
    QStringList plugins;
    plugins = QCanBus::instance()->plugins();
    for (int i = 0; i < plugins.count(); i++)
        ui->cbDeviceType->addItem(plugins[i]);
}

void ConnectionWindow::selectRemote()
{
    ui->lPort->setText("IP Address:");
    ui->stPort->setCurrentWidget(ui->etPage);
    ui->lblDeviceType->setHidden(true);
    ui->cbDeviceType->setHidden(true);
}

void ConnectionWindow::setSpeed(int speed0)
{
    Q_UNUSED(speed0);
}

void ConnectionWindow::setPortName(CANCon::type pType, QString pPortName, QString pDriver)
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
    handleConnTypeChanged();

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
            ui->lePort->setText(pPortName);
            break;
        }
        default: {}
    }
}


//-1 means leave it at whatever it booted up to. 0 means disable. Otherwise the actual rate we want.
int ConnectionWindow::getSpeed()
{
    return -1;
}

QString ConnectionWindow::getPortName()
{
    switch( getConnectionType() ) {
    case CANCon::GVRET_SERIAL:
    case CANCon::SERIALBUS:
        return ui->cbPort->currentText();
    case CANCon::REMOTE:
        return ui->lePort->text();
    default:
        qDebug() << "getPortName: can't get port";
    }

    return "";
}

QString ConnectionWindow::getDriverName()
{
    if (getConnectionType() == CANCon::SERIALBUS)
    {
        return ui->cbDeviceType->currentText();
    }

    return "";
}

CANCon::type ConnectionWindow::getConnectionType()
{
    if (ui->rbGVRET->isChecked()) return CANCon::GVRET_SERIAL;
    if (ui->rbSocketCAN->isChecked()) return CANCon::SERIALBUS;
    if (ui->rbRemote->isChecked()) return CANCon::REMOTE;
    qDebug() << "getConnectionType: error";
    return CANCon::NONE;
}


void ConnectionWindow::setSWMode(bool mode)
{
    Q_UNUSED(mode);
}

bool ConnectionWindow::getSWMode()
{
    return false;
}

void ConnectionWindow::handleRemoveConn()
{
    int selIdx = ui->tableConnections->selectionModel()->currentIndex().row();
    if (selIdx <0) return;

    qDebug() << "remove connection at index: " << selIdx;

    int busId;
    CANConnection* conn_p = connModel->getAtIdx(selIdx, busId);
    if(!conn_p) return;

    /* remove connection from model & manager */
    connModel->remove(conn_p);

    /* stop and delete connection */
    conn_p->stop();
    delete conn_p;

    /* select first connection in list */
    ui->tableConnections->selectRow(0);
}

void ConnectionWindow::handleRevert()
{

}


bool ConnectionWindow::isSerialBusAvailable()
{
    if (QCanBus::instance()->plugins().count() > 0) return true;
    return false;
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
