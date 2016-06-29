#include <QCanBus>
#include <QThread>

#include "connectionwindow.h"
#include "ui_connectionwindow.h"
#include "connections/canconfactory.h"
#include "connections/canconmanager.h"
#include "canbus.h"



ConnectionWindow::ConnectionWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectionWindow)
{
    ui->setupUi(this);

    QSettings settings;

    qRegisterMetaType<CANBus>("CANBus");
    qRegisterMetaType<const CANFrame *>("const CANFrame *");
    qRegisterMetaType<const QList<CANFrame> *>("const QList<CANFrame> *");

    qRegisterMetaTypeStreamOperators<QVector<QString>>();
    qRegisterMetaTypeStreamOperators<QVector<int>>();
    qRegisterMetaTypeStreamOperators<CANBus>();
    qRegisterMetaTypeStreamOperators<QList<CANBus>>();


    connModel = new CANConnectionModel(this);
    ui->tableConnections->setModel(connModel);
    ui->tableConnections->setColumnWidth(0, 50);
    ui->tableConnections->setColumnWidth(1, 110);
    ui->tableConnections->setColumnWidth(2, 110);
    ui->tableConnections->setColumnWidth(3, 110);
    ui->tableConnections->setColumnWidth(4, 75);
    ui->tableConnections->setColumnWidth(5, 75);
    ui->tableConnections->setColumnWidth(6, 75);
    ui->tableConnections->setColumnWidth(7, 75);

    //int temp = settings.value("Main/DefaultConnectionType", 0).toInt();
    ui->ckSingleWire->setChecked(settings.value("Main/SingleWireMode", false).toBool());

    ui->cbSpeed->addItem(tr("<Default>"));
    ui->cbSpeed->addItem(tr("125000"));
    ui->cbSpeed->addItem(tr("250000"));
    ui->cbSpeed->addItem(tr("500000"));
    ui->cbSpeed->addItem(tr("1000000"));
    ui->cbSpeed->addItem(tr("33333"));

    /* load connection configuration */
    loadConnections();

#ifdef Q_OS_LINUX
    ui->rbSocketCAN->setEnabled(isSocketCanAvailable());
#endif

#ifdef Q_OS_WIN
    ui->rbKvaser->setEnabled(true);
#endif

    connect(ui->btnOK, &QAbstractButton::clicked, this, &ConnectionWindow::handleOKButton);
    connect(ui->rbGVRET, &QAbstractButton::clicked, this, &ConnectionWindow::handleConnTypeChanged);
    connect(ui->rbKvaser, &QAbstractButton::clicked, this, &ConnectionWindow::handleConnTypeChanged);
    connect(ui->rbSocketCAN, &QAbstractButton::clicked, this, &ConnectionWindow::handleConnTypeChanged);
    connect(ui->btnRevert, &QPushButton::clicked, this, &ConnectionWindow::handleRevert);
    connect(ui->tableConnections->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &ConnectionWindow::currentRowChanged);
    connect(ui->btnNewConn, &QPushButton::clicked, this, &ConnectionWindow::handleNewConn);
    connect(ui->btnActivateAll, &QPushButton::clicked, this, &ConnectionWindow::handleEnableAll);
    connect(ui->btnDeactivateAll, &QPushButton::clicked, this, &ConnectionWindow::handleDisableAll);
    connect(ui->btnRemoveBus, &QPushButton::clicked, this, &ConnectionWindow::handleRemoveConn);
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
    ui->tableConnections->selectRow(0);
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


void ConnectionWindow::handleNewConn()
{
    ui->tableConnections->setCurrentIndex(QModelIndex());
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
    Q_UNUSED(pStatus);

    qDebug() << "Connectionstatus changed";
    connModel->refresh();
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

        connModel->refresh(whichRow);
    }
    else if( ! CANConManager::getInstance()->getByName(getPortName()) )
    {
        /* create connection */
        conn_p = create(getConnectionType(), getPortName());
        if(!conn_p)
            return;

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



void ConnectionWindow::currentRowChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);

    int selIdx = current.row();

    /* enable / diable connection type */
    ui->stPort->setEnabled(selIdx==-1);
    ui->gbType->setEnabled(selIdx==-1);
    ui->lPort->setEnabled(selIdx==-1);

    /* set parameters */
    if (selIdx == -1)
    {
        ui->btnOK->setText(tr("Create New Connection"));
        ui->rbGVRET->setChecked(true);
        ui->cbPort->setCurrentIndex(0);
        ui->ckListenOnly->setChecked(false);
        ui->ckSingleWire->setChecked(false);
        ui->ckEnabled->setChecked(false);
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

        setPortName(conn_p->getType(), conn_p->getPort());
        setSpeed(bus.getSpeed());

        ui->ckListenOnly->setChecked(bus.isListenOnly());
        ui->ckSingleWire->setChecked(bus.isSingleWire());
        ui->ckEnabled->setChecked(bus.isActive());
        /* this won't be called if elements are disabled */
    }

    handleConnTypeChanged();
}


void ConnectionWindow::selectSerial()
{
    /* set combobox page visible */
    ui->stPort->setCurrentWidget(ui->cbPage);

    ui->cbPort->clear();
    ports = QSerialPortInfo::availablePorts();

    for (int i = 0; i < ports.count(); i++)
        ui->cbPort->addItem(ports[i].portName());
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

void ConnectionWindow::setPortName(CANCon::type pType, QString pPortName)
{
    switch(pType)
    {
        case CANCon::GVRET_SERIAL:
        {
            ui->rbGVRET->setChecked(true);

            break;
        }
        case CANCon::KVASER:
        {
            ui->rbKvaser->setChecked(true);
            break;
        }
        case CANCon::SOCKETCAN:
        {
            ui->rbSocketCAN->setChecked(true);
            ui->lePort->setText(pPortName);
            break;
        }
        default: {}
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

    /* remove connection from model & manager */
    connModel->remove(conn_p);

    /* stop and delete connection */
    conn_p->stop();
    delete conn_p;
}

void ConnectionWindow::handleRevert()
{

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


CANConnection* ConnectionWindow::create(CANCon::type pTye, QString pPortName)
{
    CANConnection* conn_p;

    /* create connection */
    conn_p = CanConFactory::create(pTye, pPortName);
    if(conn_p)
    {
        /* connect signal */
        connect(conn_p, SIGNAL(status(CANCon::status)),
                this, SLOT(connectionStatus(CANCon::status)));

        /*TODO add return value and checks */
        conn_p->start();
    }
    return conn_p;
}


void ConnectionWindow::loadConnections()
{
    QSettings settings;

    /* fill connection list */
    QVector<QString> portNames = settings.value("connections/portNames").value<QVector<QString>>();
    QVector<int>    devTypes = settings.value("connections/types").value<QVector<int>>();
    QList<CANBus> busses = settings.value("connections/busses").value<QList<CANBus>>();


    for(int i=0 ; i<portNames.count() ; i++)
    {
        CANConnection* conn_p = create((CANCon::type)devTypes[i], portNames[i]);
        if(conn_p)
        {
            for(int j=0 ; j<conn_p->getNumBuses() ; j++)
                conn_p->setBusSettings(j, busses.takeFirst());
        }
        /* add connection to model */
        connModel->add(conn_p);
    }
}

void ConnectionWindow::saveConnections()
{
    QList<CANConnection*>& conns = CANConManager::getInstance()->getConnections();

    QSettings settings;
    QVector<QString> portNames;
    QVector<int> devTypes;
    QList<CANBus> busses;

    /* delete connections */
    foreach(CANConnection* conn_p, conns)
    {
        portNames.append(conn_p->getPort());
        devTypes.append(conn_p->getType());

        for(int i=0 ; i<conn_p->getNumBuses() ; i++)
        {
            CANBus bus;
            conn_p->getBusSettings(i, bus);
            busses.append(bus);
        }
    }

    settings.setValue("connections/portNames", QVariant::fromValue(portNames));
    settings.setValue("connections/types", QVariant::fromValue(devTypes));
    settings.setValue("connections/busses", QVariant::fromValue(busses));
}
