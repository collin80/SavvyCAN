#include "connections/canconnection.h"
#include "canconnectionmodel.h"

CANConnectionModel::CANConnectionModel(QObject *parent)
    : QAbstractTableModel(parent)
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
    return 7;
}


int CANConnectionModel::rowCount(const QModelIndex &parent) const {
    int rows=0;
    QList<CANConnection*>::const_iterator iter;

    for (iter = mConns.begin() ; iter != mConns.end() ; ++iter) {
        rows+=(*iter)->getNumBuses();
    }

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
                return QString::number(busId);
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
    beginResetModel();
    mConns.append(pConn_p);
    endResetModel();
}


void CANConnectionModel::remove(CANConnection* pConn_p)
{
    beginResetModel();
    mConns.removeOne(pConn_p);
    endResetModel();
}


QList<CANConnection*>& CANConnectionModel::getConnections()
{
    return mConns;
}


CANConnection* CANConnectionModel::getAtIdx(int pIdx, int& pBusId) const
{
    if (pIdx < 0)
        return NULL;

    int i=0;
    QList<CANConnection*>::const_iterator iter = mConns.begin();

    for (iter = mConns.begin() ; iter != mConns.end() ; ++iter) {
        if( i <= pIdx && pIdx < i+(*iter)->getNumBuses() ) {
            pBusId = pIdx - i;
            return (*iter);
        }

        i+= (*iter)->getNumBuses();
    }

    return NULL;
}


void CANConnectionModel::refreshView()
{
    beginResetModel();
    endResetModel();
}
