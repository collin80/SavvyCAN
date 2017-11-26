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
        case 1:
            return QString(tr("Type"));
        case 2:
            return QString(tr("Port"));
        case 3:
            return QString(tr("Speed"));
        case 4:
            return QString(tr("Listen Only"));
        case 5:
            return QString(tr("Single Wire"));
        case 6:
            return QString(tr("Active"));
        case 7:
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
    return 8;
}


int CANConnectionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    int rows = 0;
    QList<CANConnection*>& conns = CANConManager::getInstance()->getConnections();

    foreach(const CANConnection* conn_p, conns)
        rows += conn_p->getNumBuses();

    //qDebug() << "Num Rows: " << rows;

    return rows;
}

Qt::ItemFlags CANConnectionModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemFlag::NoItemFlags;

    int busId;
    CANConnection *conn_p = getAtIdx(index.row(), busId);
    if (!conn_p) return Qt::ItemFlag::NoItemFlags;

    //you can't set speed, single wire, or listen only on socketcan devices so
    //detect if we're using GVRET where you can and turn that functionality on
    bool editParams = false;
    if (conn_p->getType() == CANCon::GVRET_SERIAL) editParams = true;

    switch (index.column())
    {
    case 3: //speed
        if (editParams) return Qt::ItemFlag::ItemIsEditable | Qt::ItemFlag::ItemIsEnabled;
        return Qt::ItemFlag::NoItemFlags;
    case 4: //listen only
    case 5: //single wire
        if (editParams) return Qt::ItemFlag::ItemIsEditable | Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsUserCheckable;
        return Qt::ItemFlag::NoItemFlags;
    case 6: //enabled
        return Qt::ItemFlag::ItemIsEditable | Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsUserCheckable;
    default:
        return Qt::ItemFlag::ItemIsEnabled;
    }
}

bool CANConnectionModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    qDebug() << "setData: " << index.row() << ":" << index.column() << " role: " << role << " Val: " << value;

    int busId;
    CANConnection *conn_p = getAtIdx(index.row(), busId);
    if (!conn_p) return false;
    CANBus bus;
    bool ret;
    ret = conn_p->getBusSettings(busId, bus);
    if (!ret) return false;

    switch (index.column())
    {
    case 3: //speed
        bus.speed = value.toInt();
        break;
    case 4: //listen only
        bus.listenOnly = value.toBool();
        break;
    case 5: //single wire
        bus.singleWire = value.toBool();
        break;
    case 6: //active
        bus.active = value.toBool();
        break;
    }
    conn_p->setBusSettings(busId, bus);
    return true;
}

QVariant CANConnectionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    //qDebug() << "Row: " << index.row();

    int busId;
    CANConnection *conn_p = getAtIdx(index.row(), busId);
    CANBus bus;
    bool ret;
    if (!conn_p) return QVariant();
    ret = conn_p->getBusSettings(busId, bus);
    bool isSocketCAN = (conn_p->getType() == CANCon::SOCKETCAN) ? true: false;

    //qDebug() << "ConnP: " << conn_p << "  ret " << ret;

    if (role == Qt::DisplayRole) {        

        switch (index.column())
        {
            case 0: //bus
                //return QString::number(busId);
                return QString::number(index.row());
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
                if(!ret) return QVariant();
                if (!isSocketCAN) return QString::number(bus.speed);
                else return QString("N/A");
            case 4: //Listen Only
                return QVariant();
            case 5: //Single Wire
                return QVariant();
            case 6: //Status
                return QVariant();
            case 7: //Active
                 return (conn_p->getStatus()==CANCon::CONNECTED) ? "Connected" : "Not Connected";
            default: {}
        }
    }
    if (role == Qt::CheckStateRole)
    {
        switch (index.column())
        {
        case 4:
            return (bus.listenOnly) ? Qt::Checked : Qt::Unchecked;
        case 5:
            return (bus.singleWire) ? Qt::Checked : Qt::Unchecked;
        case 6:
            return (bus.active) ? Qt::Checked : Qt::Unchecked;
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
