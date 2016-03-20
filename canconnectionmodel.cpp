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
            return QString(tr("Active"));
            break;
        }
    }

    else
        return QString::number(section + 1);

    return QVariant();
}

int CANConnectionModel::rowCount(const QModelIndex &parent) const
{
    return buses.count();
}

int CANConnectionModel::columnCount(const QModelIndex &parent) const
{
    return 7;
}

QVariant CANConnectionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= (buses.count()))
        return QVariant();

    if (role == Qt::DisplayRole) {
        CAN_Bus bus = buses[index.row()];
        CANConnection *conn = bus.connection;
        switch (index.column())
        {
        case 0: //bus
            return QString::number(bus.busNum);
            break;
        case 1: //type
            if (conn) conn->getConnTypeName();
            break;
        case 2: //port
            if (conn) conn->getConnPortName();
            break;
        case 3: //speed
            return QString::number(bus.speed);
            break;
        case 4: //Listen Only
            if (bus.listenOnly) return QString("True");
                else return QString("False");
            break;
        case 5: //Single Wire
            if (bus.singleWire) return QString("True");
                else return QString("False");
            break;
        case 6: //Active
            if (bus.active) return QString("True");
                else return QString("False");
            break;
        default:
            return QVariant();
        }
    }
    else
        return QVariant();
}

void CANConnectionModel::addConnection(CANConnection *conn)
{
    CAN_Bus bus;
    beginResetModel();
    CANConnectionContainer *cont = new CANConnectionContainer(conn);
    connections.append(cont);

    int numBuses = conn->getNumBuses();
    for (int i = 0; i < numBuses; i++)
    {
        bus.busNum = conn->getBusBase() + i;
        buses.append(bus);
    }

    endResetModel();
}

CAN_Bus* CANConnectionModel::findBusByNum(int bus)
{
    for (int i = 0; i < buses.count(); i++)
    {
        if (buses[i].busNum == bus)
        {
            return &buses[i];
        }
    }
    return NULL;
}

