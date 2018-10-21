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
    Bus        = 0, ///< A sequential number describing the bus
    Type       = 1, ///< The CAN driver/backend type, e.g. GVRET, peakcan, or socketcan
    Port       = 2, ///< The CAN hardware port, e.g. can0 for socketcan
    Speed      = 3, ///< The bus speed in bit/second
    ListenOnly = 4, ///< True if the bus is in listen-only mode
    SingleWire = 5, ///< True if the bus operates in single-wire mode
    Active     = 6, ///< True if the bus is activated for sending and receiving
    Status     = 7  ///< The bus status as text message
};

QVariant CANConnectionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal)
    {
        switch (Column(section))
        {
        case Column::Bus:
            return QString(tr("Bus"));
        case Column::Type:
            return QString(tr("Type"));
        case Column::Port:
            return QString(tr("Port"));
        case Column::Speed:
            return QString(tr("Speed"));
        case Column::ListenOnly:
            return QString(tr("Listen Only"));
        case Column::SingleWire:
            return QString(tr("Single Wire"));
        case Column::Active:
            return QString(tr("Active"));
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

    switch (Column(index.column()))
    {
    case Column::Speed:
        if (editParams) return Qt::ItemFlag::ItemIsEditable | Qt::ItemFlag::ItemIsEnabled;
        return Qt::ItemFlag::NoItemFlags;
    case Column::ListenOnly:
    case Column::SingleWire:
        if (editParams) return Qt::ItemFlag::ItemIsEditable | Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsUserCheckable;
        return Qt::ItemFlag::NoItemFlags;
    case Column::Active:
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

    switch (Column(index.column()))
    {
    case Column::Speed:
        bus.speed = value.toInt();
        break;
    case Column::ListenOnly:
        bus.listenOnly = value.toBool();
        break;
    case Column::SingleWire:
        bus.singleWire = value.toBool();
        break;
    case Column::Active:
        bus.active = value.toBool();
        break;
    default: {}
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
    bool isSocketCAN = (conn_p->getType() == CANCon::SERIALBUS) ? true: false;

    //qDebug() << "ConnP: " << conn_p << "  ret " << ret;

    if (role == Qt::DisplayRole) {

        switch (Column(index.column()))
        {
            case Column::Bus:
                //return QString::number(busId);
                return QString::number(index.row());
            case Column::Type:
                if (conn_p)
                    switch (conn_p->getType()) {
                        case CANCon::KVASER: return "KVASER";
                        case CANCon::SERIALBUS: return "SerialBus";
                        case CANCon::GVRET_SERIAL: return "GVRET";
                        default: {}
                    }
                else qDebug() << "Tried to show connection type but connection was NULL";
                break;
            case Column::Port:
                if (conn_p) return conn_p->getPort();
                else qDebug() << "Tried to show connection port but connection was NULL";
                break;
            case Column::Speed:
                if(!ret) return QVariant();
                if (!isSocketCAN) return QString::number(bus.speed);
                else return QString("N/A");
            case Column::ListenOnly:
                return QVariant();
            case Column::SingleWire:
                return QVariant();
            case Column::Active:
                return QVariant();
            case Column::Status:
                 return (conn_p->getStatus()==CANCon::CONNECTED) ? "Connected" : "Not Connected";
        }
    }
    if (role == Qt::CheckStateRole)
    {
        switch (Column(index.column()))
        {
        case Column::ListenOnly:
            return (bus.listenOnly) ? Qt::Checked : Qt::Unchecked;
        case Column::SingleWire:
            return (bus.singleWire) ? Qt::Checked : Qt::Unchecked;
        case Column::Active:
            return (bus.active) ? Qt::Checked : Qt::Unchecked;
        default: {}
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
