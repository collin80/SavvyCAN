#include "canframemodel.h"

int CANFrameModel::rowCount(const QModelIndex &parent) const
{
    return frames.count();
}

int CANFrameModel::columnCount(const QModelIndex &index) const
{
    return 6;
}

CANFrameModel::CANFrameModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    frames.reserve(100000); //ask for at least 100,000 entries. This gives good performance up to this limit
}

QVariant CANFrameModel::data(const QModelIndex &index, int role) const
{
    QString tempString;
    if (!index.isValid())
        return QVariant();

    if (index.row() >= frames.count())
        return QVariant();

    if (role == Qt::DisplayRole) {
        CANFrame thisFrame = frames.at(index.row());
        switch (index.column())
        {
        case 0: //timestamp

            return QString::number(thisFrame.timestamp);
            break;
        case 1: //id
            return QString::number(thisFrame.ID, 16).toUpper().rightJustified(2,'0');
            break;
        case 2: //ext
            return QString::number(thisFrame.extended);
            break;
        case 3: //bus
            return QString::number(thisFrame.bus);
            break;
        case 4: //len
            return QString::number(thisFrame.len);
            break;
        case 5: //data
            for (int i = 0; i < thisFrame.len; i++)
            {
                tempString.append(QString::number(thisFrame.data[i], 16).toUpper().rightJustified(2,'0'));
                tempString.append(" ");
            }
            return tempString;
            break;
        default:
            return QVariant();
        }
    }
    else
        return QVariant();
}

QVariant CANFrameModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal)
    {
        switch (section)
        {
        case 0:
            return QString(tr("Timestamp"));
            break;
        case 1:
            return QString(tr("ID"));
            break;
        case 2:
            return QString(tr("Ext"));
            break;
        case 3:
            return QString(tr("Bus"));
            break;
        case 4:
            return QString(tr("Len"));
            break;
        case 5:
            return QString(tr("Data"));
            break;
        }
    }

    else
        return QString::number(section + 1);
}

void CANFrameModel::addFrame(CANFrame &frame, bool autoRefresh = false)
{
    if (autoRefresh) beginInsertRows(QModelIndex(), frames.count() + 1, frames.count() + 1);
    frames.append(frame);
    if (autoRefresh) endInsertRows();
}

void CANFrameModel::sendRefresh()
{
    beginInsertRows(QModelIndex(), 0, frames.count());
    endInsertRows();
}

void CANFrameModel::sendRefresh(int pos)
{
    beginInsertRows(QModelIndex(), pos, pos);
    endInsertRows();
}


void CANFrameModel::clearFrames()
{
    beginRemoveRows(QModelIndex(), 0, frames.count());
    frames.clear();
    endRemoveRows();
}
