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

enum class Column {
    Type       = 0, ///< The CAN driver/backend type, e.g. GVRET, peakcan, or socketcan
    Subtype    = 1, ///< Mostly used by SerialBus devices to pick the sub type
    Port       = 2, ///< The CAN hardware port, e.g. can0 for socketcan
    NumBuses   = 3, ///< Number of buses exposed by this device. Usually non-GVRET devices will just have one
    Status     = 4  ///< The bus status as text message
};

QVariant CANConnectionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal)
    {
        switch (Column(section))
        {
        case Column::Type:
            return QString(tr("Type"));
        case Column::Subtype:
            return QString(tr("Subtype"));
        case Column::Port:
            return QString(tr("Port"));
        case Column::NumBuses:
            return QString(tr("Buses"));
        case Column::Status:
            return QString(tr("Status"));
        }
    }

    else
        return QString::number(section + 1);

    return QVariant();
}

int CANConnectionModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 5;
}


int CANConnectionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    QList<CANConnection*>& conns = CANConManager::getInstance()->getConnections();

    return conns.count();
}

QVariant CANConnectionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    //qDebug() << "Row: " << index.row();

    CANConnection *conn_p = getAtIdx(index.row());

    if (!conn_p) return QVariant();

    //bool isSocketCAN = (conn_p->getType() == CANCon::SERIALBUS) ? true: false;

    if (role == Qt::DisplayRole) {

        switch (Column(index.column()))
        {
            case Column::Type:
                if (conn_p)
                    switch (conn_p->getType()) {
                        case CANCon::KVASER: return "KVASER";
                        case CANCon::SERIALBUS: return "SerialBus";
                        case CANCon::GVRET_SERIAL: return "GVRET";
                        case CANCon::KAYAK: return "socketcand";
                        case CANCon::LAWICEL: return "LAWICEL";
                        case CANCon::CANSERVER: return "CANserver";
                        case CANCon::CANLOGSERVER: return "CanLogServer";
                        case CANCon::GSUSB: return "gs_usb";
                        default: {}
                    }
                else qDebug() << "Tried to show connection type but connection was nullptr";
                break;
            case Column::Port:
                if (conn_p) return conn_p->getPort();
                else qDebug() << "Tried to show connection port but connection was nullptr";
                break;
            case Column::Subtype:
                return conn_p->getDriver();
                break;
            case Column::NumBuses:
                return conn_p->getNumBuses();
                break;
            case Column::Status:
                 return (conn_p->getStatus()==CANCon::CONNECTED) ? "Connected" : "Not Connected";
        }
    }
    return QVariant();
}


void CANConnectionModel::add(CANConnection* pConn_p)
{
    CANConManager* manager = CANConManager::getInstance();

    beginResetModel();
    manager->add(pConn_p);
    endResetModel();
}


void CANConnectionModel::remove(CANConnection* pConn_p)
{
    CANConManager* manager = CANConManager::getInstance();

    beginResetModel();
    manager->remove(pConn_p);
    endResetModel();
}

void CANConnectionModel::replace(int idx , CANConnection* pConn_p)
{
    CANConManager* manager = CANConManager::getInstance();

    beginResetModel();
    manager->replace(idx, pConn_p);
    endResetModel();
}

CANConnection* CANConnectionModel::getAtIdx(int pIdx) const
{
    if (pIdx < 0)
        return nullptr;

    QList<CANConnection*>& conns = CANConManager::getInstance()->getConnections();

    return conns.at(pIdx);
}

void CANConnectionModel::refresh(int pIndex)
{
    Q_UNUSED(pIndex)

    beginResetModel();
    endResetModel();
    /*
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
    dataChanged(begin, end, QVector<int>(Qt::DisplayRole)); */
}
