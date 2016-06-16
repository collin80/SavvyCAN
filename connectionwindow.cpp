#include <QCanBus>

#include "connectionwindow.h"
#include "ui_connectionwindow.h"
#include "connections/canconfactory.h"


ConnectionWindow::ConnectionWindow(CANFrameModel *cModel, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectionWindow)
{
    ui->setupUi(this);

    settings = new QSettings();

    qRegisterMetaType<CANBus>("CANBus");
    qRegisterMetaType<const CANFrame *>("const CANFrame *");
    qRegisterMetaType<const QList<CANFrame> *>("const QList<CANFrame> *");

    connModel = new CANConnectionModel();
    ui->tableConnections->setModel(connModel);

    canModel = cModel;

    ui->tableConnections->setColumnWidth(0, 50);
    ui->tableConnections->setColumnWidth(1, 110);
    ui->tableConnections->setColumnWidth(2, 110);
    ui->tableConnections->setColumnWidth(3, 110);
    ui->tableConnections->setColumnWidth(4, 75);
    ui->tableConnections->setColumnWidth(5, 75);
    ui->tableConnections->setColumnWidth(6, 75);
    ui->tableConnections->setColumnWidth(7, 75);

    int temp = settings->value("Main/DefaultConnectionType", 0).toInt();

    //currentPortName = settings->value("Main/DefaultConnectionPort", "").toString();

    //currentSpeed1 = -1;

    ui->ckSingleWire->setChecked(settings->value("Main/SingleWireMode", false).toBool());

    ui->cbSpeed->addItem(tr("<Default>"));
    ui->cbSpeed->addItem(tr("125000"));
    ui->cbSpeed->addItem(tr("250000"));
    ui->cbSpeed->addItem(tr("500000"));
    ui->cbSpeed->addItem(tr("1000000"));
    ui->cbSpeed->addItem(tr("33333"));

#ifdef Q_OS_LINUX
    ui->rbSocketCAN->setEnabled(isSocketCanAvailable());
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
    connect(connModel, &QAbstractItemModel::modelReset, this, &ConnectionWindow::handleConnSelectionChanged);
    connect(ui->btnNewConn, &QPushButton::clicked, this, &ConnectionWindow::handleNewConn);
    connect(ui->btnActivateAll, &QPushButton::clicked, this, &ConnectionWindow::handleEnableAll);
    connect(ui->btnDeactivateAll, &QPushButton::clicked, this, &ConnectionWindow::handleDisableAll);
    connect(ui->btnRemoveBus, &QPushButton::clicked, this, &ConnectionWindow::handleRemoveConn);


    qDebug() << "Serial worker thread starting";

    connect(&mTicker, SIGNAL(timeout()), this, SLOT(refreshCanList()));
    /* tick frequency has a huge impact on performances */
    /* TODO: make this configurable and part of the connection constructor to let connection configure the length of the queue */
    mTicker.setInterval(500); /*tick twice a second */
    mTicker.setSingleShot(false);
    mTicker.start();
}

ConnectionWindow::~ConnectionWindow()
{
    QList<CANConnection*>& conns = connModel->getConnections();
    CANConnection* conn_p;

    /* delete connections */
    while(!conns.isEmpty()) {
        conn_p = conns.takeFirst();
        conn_p->stop();
        delete conn_p;
    }

    delete connModel;

    mTicker.stop();
    delete settings;
    delete ui;
}

void ConnectionWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    qDebug() << "Show connectionwindow";
    handleConnTypeChanged();
}

void ConnectionWindow::refreshCanList() {
    QList<CANConnection*>::iterator conn_p;
    QList<CANConnection*>& conns = connModel->getConnections();

    CANFrame* frame_p = NULL;

    for (conn_p = conns.begin(); conn_p != conns.end(); ++conn_p) {
        while( (frame_p = (*conn_p)->getQueue().peek() ) ) {
            canModel->addFrame(*frame_p, true);
            (*conn_p)->getQueue().dequeue();
        }
    }
}

void ConnectionWindow::handleNewConn()
{
    ui->tableConnections->selectionModel()->clearSelection();
    ui->tableConnections->selectionModel()->clearCurrentIndex();
    handleConnSelectionChanged();
}

void ConnectionWindow::setSuspendAll(bool pSuspend)
{
    qDebug() << "setSuspendAll";

    QList<CANConnection*>::iterator iter;
    QList<CANConnection*>& conns = connModel->getConnections();

    for (iter = conns.begin(); iter != conns.end(); ++iter)
        (*iter)->suspend(pSuspend);
}

void ConnectionWindow::setActiveAll(bool pActive)
{
    QList<CANConnection*>::iterator iter;
    QList<CANConnection*>& conns = connModel->getConnections();
    CANBus bus;

    for (iter = conns.begin(); iter != conns.end(); ++iter) {
        for(int i=0 ; i<(*iter)->getNumBuses() ; i++) {
            if( (*iter)->getBusSettings(i, bus) ) {
                bus.active = pActive;
                (*iter)->setBusSettings(i, bus);
            }
        }
    }
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
    if (ui->rbKvaser->isChecked()) selectKvaser();
    if (ui->rbSocketCAN->isChecked()) selectSocketCan();
}


/* status */
void ConnectionWindow::connectionStatus(CANCon::status pStatus)
{
    qDebug() << "Connectionstatus changed";
    connModel->refreshView();
}


void ConnectionWindow::handleOKButton()
{
    int whichRow = ui->tableConnections->selectionModel()->currentIndex().row();

    CANConnection* conn_p = NULL;

    if (whichRow > -1)
    {
        int busId;
        CANBus bus;
        bool ret;

        conn_p = connModel->getAtIdx(whichRow, busId);
        if(!conn_p) return;

        ret = conn_p->getBusSettings(busId, bus);
        if(!ret) return;


        bus.setListenOnly(ui->ckListenOnly->isChecked());
        bus.setSingleWire(ui->ckSingleWire->isChecked());
        bus.setEnabled(ui->ckEnabled->isChecked());
        if (ui->cbSpeed->currentIndex() == 0)
        {
            bus.speed = 0;
            bus.setEnabled(false);
        }
        else if (ui->cbSpeed->currentIndex() >= 1)
        {
            bus.setSpeed(ui->cbSpeed->currentText().toInt());
        }        
        /* update bus settings */
        conn_p->setBusSettings(busId, bus);

        connModel->refreshView();
    }
    else //new connection
    {
#if 0
        if (ui->rbGVRET->isChecked())
        {

            SerialWorker *serial = new SerialWorker(canModel, connModel->rowCount());
            connect(serial, SIGNAL(busStatus(int,int,int)), this, SLOT(receiveBusStatus(int,int,int)));
            connect(serial, SIGNAL(connectionSuccess(CANConnection*)), this, SLOT(connectionSuccess(CANConnection*)));
            CANConnectionContainer* container = new CANConnectionContainer(serial);


            qDebug() << "Setup initial connection object";

            CANBus bus;
            bus.active = ui->ckEnabled->isChecked();
            bus.busNum = serial->getBusBase();
            bus.container = container;
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
                bus.container = container;
                connModel->addBus(bus);
                qDebug() << "Added bus " << bus.busNum;
            }            

            //call through signal/slot interface without using connect
            QMetaObject::invokeMethod(serial, "updatePortName",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, ui->cbPort->currentText()));
#endif

        /* create connection */
        conn_p = CanConFactory::create(getConnectionType(), getPortName());
        if(!conn_p)
            return;

        /* connect signal */
        connect(conn_p, SIGNAL(status(CANCon::status)),
                this, SLOT(connectionStatus(CANCon::status)));

        /*TODO add return value and checks */
        conn_p->start();

        for (int i=0 ; i<conn_p->getNumBuses() ; i++) {
            /* set bus configuration */
            CANBus bus;
            bus.active = ui->ckEnabled->isChecked();
            bus.listenOnly = ui->ckListenOnly->isChecked();
            bus.singleWire = ui->ckSingleWire->isChecked();

            if (ui->cbSpeed->currentIndex() < 1) bus.speed = 0;
            else bus.speed = ui->cbSpeed->currentText().toInt();

            /* update bus settings */
            conn_p->setBusSettings(i, bus);
        }

        /* add connection to model */
        connModel->add(conn_p);
    }
}


void ConnectionWindow::receiveBusStatus(int bus, int speed, int status)
{
#if 0
    qDebug() << "bus " << bus << " speed " << speed << " status " << status;
    CANBus *busRef = connModel->getBus(bus);
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
#endif
}

void ConnectionWindow::handleConnSelectionChanged()
{
    int selIdx = ui->tableConnections->selectionModel()->currentIndex().row();
    if (selIdx == -1)
    {
        ui->btnOK->setText(tr("Create New Connection"));

        ui->cbPort->setEnabled(true);
        ui->rbGVRET->setEnabled(true);
        ui->rbKvaser->setEnabled(true);
        ui->rbSocketCAN->setEnabled(true);
        ui->cbSpeed->setEnabled(false);

        ui->cbPort->setCurrentIndex(0);
        ui->ckListenOnly->setChecked(false);
        ui->ckSingleWire->setChecked(false);
        ui->ckEnabled->setChecked(true);
    }
    else
    {
        int busId;
        bool ret;
        CANBus bus;
        CANConnection* conn_p = connModel->getAtIdx(selIdx, busId);
        if(!conn_p) return;
        ret = conn_p->getBusSettings(busId, bus);
        if(!ret) return;

        ui->btnOK->setText(tr("Update Connection Settings"));

        ui->cbPort->setEnabled(false);
        ui->rbGVRET->setEnabled(false);
        ui->rbKvaser->setEnabled(false);
        ui->rbSocketCAN->setEnabled(false);
        ui->cbSpeed->setEnabled(true);

        switch(conn_p->getType()) {
            case CANCon::GVRET_SERIAL: ui->rbGVRET->setChecked(true); break;
            case CANCon::KVASER: ui->rbKvaser->setChecked(true); break;
            case CANCon::SOCKETCAN: ui->rbSocketCAN->setChecked(true); break;
            default: {}
        }

        ui->ckListenOnly->setChecked(bus.isListenOnly());
        ui->ckSingleWire->setChecked(bus.isSingleWire());
        ui->ckEnabled->setChecked(bus.isActive());
        int speed = bus.getSpeed();
        setSpeed(speed);
    }
}


void ConnectionWindow::selectSerial()
{
    /* set combobox page visible */
    ui->stPort->setCurrentWidget(ui->cbPage);

    ui->cbPort->clear();
    ports = QSerialPortInfo::availablePorts();

    for (int i = 0; i < ports.count(); i++)
    {
        ui->cbPort->addItem(ports[i].portName());
        //if (currentPortName == ports[i].portName()) ui->cbPort->setCurrentIndex(i);
    }
}

void ConnectionWindow::selectKvaser()
{
#ifdef Q_OS_WIN
    /* set combobox page visible */
    ui->stPort->setCurrentWidget(ui->cbPage);
#endif
}

void ConnectionWindow::selectSocketCan()
{
#ifdef Q_OS_LINUX
    /* set edit text page visible */
    ui->stPort->setCurrentWidget(ui->etPage);
#endif
}

void ConnectionWindow::setSpeed(int speed0)
{
    bool found = false;

    qDebug() << "Set Speed " << speed0;

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
    switch( getConnectionType() ) {
    case CANCon::GVRET_SERIAL:
    case CANCon::KVASER:
        return ui->cbPort->currentText();
    case CANCon::SOCKETCAN:
        return ui->lePort->text();
    default:
        qDebug() << "getPortName: can't get port";
    }

    return "";
}

CANCon::type ConnectionWindow::getConnectionType()
{
    if (ui->rbGVRET->isChecked()) return CANCon::GVRET_SERIAL;
    if (ui->rbKvaser->isChecked()) return CANCon::KVASER;
    if (ui->rbSocketCAN->isChecked()) return CANCon::SOCKETCAN;

    qDebug() << "getConnectionType: error";
    return CANCon::NONE;
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
    int selIdx = ui->tableConnections->selectionModel()->currentIndex().row();
    if (selIdx <0) return;

    qDebug() << "remove connection at index: " << selIdx;

    int busId;
    CANConnection* conn_p = connModel->getAtIdx(selIdx, busId);
    if(!conn_p) return;

    /* remove connection from model */
    connModel->remove(conn_p);

    /* stop and delete connection */
    conn_p->stop();
    delete conn_p;
}

void ConnectionWindow::handleRevert()
{

}

void ConnectionWindow::sendFrame(const CANFrame *frame)
{
#if 0
    CANBus *bus = connModel->getBus(frame->bus);
    if (bus == NULL) return;
    QMetaObject::invokeMethod(bus->getContainer()->getRef(), "sendFrame",
                              Qt::QueuedConnection,
                              Q_ARG(const CANFrame *, frame));
#endif
}

void ConnectionWindow::sendFrameBatch(const QList<CANFrame> *frames)
{
#if 0
    if (frames->count() == 0) return;
    CANBus *bus = connModel->getBus(frames->at(0).bus);
    if (bus == NULL) return;
    QMetaObject::invokeMethod(bus->getContainer()->getRef(), "sendFrameBatch",
                              Qt::QueuedConnection,
                              Q_ARG(const QList<CANFrame> *, frames));
#endif
}

bool ConnectionWindow::isSocketCanAvailable()
{
#ifdef Q_OS_LINUX
    foreach (const QByteArray &backend, QCanBus::instance()->plugins()) {
        if (backend == "socketcan") {
            return true;
        }
    }
#endif
    return false;
}
