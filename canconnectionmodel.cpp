#include "canconnectionmodel.h"
#include "connections/canconnection.h"
#include "connections/canconmanager.h"

CANConnectionModel::CANConnectionModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

CANConnectionModel::~CANConnectionModel()
{
}


QVariant CANConnectionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal)
    {
        switch (section)
        {
        case 0:
            return QString(tr("Bus"));
            break;
        case 1:
            return QString(tr("Type"));
            break;
        case 2:
            return QString(tr("Port"));
            break;
        case 3:
            return QString(tr("Speed"));
            break;
        case 4:
            return QString(tr("Listen Only"));
            break;
        case 5:
            return QString(tr("Single Wire"));
            break;
        case 6:
            return QString(tr("Status"));
            break;
        case 7:
            return QString(tr("Active"));
            break;
        }
    }

    else
        return QString::number(section + 1);

    return QVariant();
}


int CANConnectionModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 7;
}


int CANConnectionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    int rows=0;
    QList<CANConnection*>& conns = CANConManager::getInstance()->getConnections();

    foreach(const CANConnection* conn_p, conns)
        rows+=conn_p->getNumBuses();

    return rows;
}


QVariant CANConnectionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();


    if (role == Qt::DisplayRole) {
        int busId;
        CANConnection *conn_p = getAtIdx(index.row(), busId);
        if(!conn_p)
            return QVariant();

        CANBus bus;
        bool ret;
        ret = conn_p->getBusSettings(busId, bus);
        if(!ret) return QVariant();

        switch (index.column())
        {
            case 0: //bus
                //return QString::number(busId);
                return QString::number(index.row());
                break;
            case 1: //type
                if (conn_p)
                    switch (conn_p->getType()) {
                        case CANCon::KVASER: return "KVASER";
                        case CANCon::SOCKETCAN: return "SocketCAN";
                        case CANCon::GVRET_SERIAL: return "GVRET";
                        default: {}
                    }
                else qDebug() << "Tried to show connection type but connection was NULL";
                break;
            case 2: //port
                if (conn_p) return conn_p->getPort();
                else qDebug() << "Tried to show connection port but connection was NULL";
                break;
            case 3: //speed
                return QString::number(bus.speed);
            case 4: //Listen Only
                return (bus.listenOnly) ? "True" : "False";
            case 5: //Single Wire
                return (bus.singleWire) ? "True" : "False";
            case 6: //Status
                return (conn_p->getStatus()==CANCon::CONNECTED) ? "Connected" : "Not Connected";
            case 7: //Active
                return (bus.active) ? "True" : "False";
            default: {}
        }
    }

    return QVariant();
}


void CANConnectionModel::add(CANConnection* pConn_p)
{
    CANConManager* manager = CANConManager::getInstance();

    connect(pConn_p, SIGNAL(notify()), manager, SLOT(refreshCanList()));

    beginResetModel();
    manager->getConnections().append(pConn_p);
    endResetModel();
}


void CANConnectionModel::remove(CANConnection* pConn_p)
{
    CANConManager* manager = CANConManager::getInstance();

    disconnect(pConn_p, 0, manager, 0);

    beginResetModel();
    manager->getConnections().removeOne(pConn_p);
    endResetModel();
}


CANConnection* CANConnectionModel::getAtIdx(int pIdx, int& pBusId) const
{
    if (pIdx < 0)
        return NULL;

    int i=0;
    QList<CANConnection*>& conns = CANConManager::getInstance()->getConnections();

    foreach(CANConnection* conn_p, conns)
    {
        if( i <= pIdx && pIdx < i+conn_p->getNumBuses() ) {
            pBusId = pIdx - i;
            return conn_p;
        }

        i+= conn_p->getNumBuses();
    }

    return NULL;
}


void CANConnectionModel::refresh(int pIndex)
{
    QModelIndex begin;
    QModelIndex end;

    if(pIndex>=0) {
        begin   = createIndex(pIndex, 0);
        end     = createIndex(pIndex, columnCount()-1);
    }
    else {
        begin   = createIndex(0, 0);
        end     = createIndex(rowCount()-1, columnCount()-1);
    }
    dataChanged(begin, end, QVector<int>(Qt::DisplayRole));
}
