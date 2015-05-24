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
    frames.reserve(10000000); //yes, I'm preallocating 10 million entries in this list. I don't think anyone will exceed this.
    dbcHandler = NULL;
    interpretFrames = false;
    overwriteDups = false;
}

void CANFrameModel::setDBCHandler(DBCHandler *handler)
{
    dbcHandler = handler;
}

void CANFrameModel::setInterpetMode(bool mode)
{
    interpretFrames = mode;
}

void CANFrameModel::setOverwriteMode(bool mode)
{
    overwriteDups = mode;
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
            //now, if we're supposed to interpret the data and the DBC handler is loaded then use it
            if (dbcHandler != NULL)
            {
                DBC_MESSAGE *msg = dbcHandler->findMsgByID(thisFrame.ID);
                if (msg != NULL)
                {
                    for (int j = 0; j < msg->msgSignals.length(); j++)
                    {

                    }
                }
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
    mutex.lock();
    if (autoRefresh) beginInsertRows(QModelIndex(), frames.count() + 1, frames.count() + 1);
    frames.append(frame);
    if (autoRefresh) endInsertRows();
    mutex.unlock();
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

//issue a refresh for the last num entries in the model.
//used by the serial worker to do batch updates so it doesn't
//have to send thousands of messages per second
void CANFrameModel::sendBulkRefresh(int num)
{
    //qDebug() << "Bulk refresh of " << num;
    //the next three lines protect against a crash in case someone clicked clear frames
    //in between the time we got some frames and the time this was called
    //otherwise it's possible that the grid is in an odd state.
    if (num == 0) return;
    if (frames.count() == 0) return;
    if (num > frames.count()) num = frames.count();
    beginInsertRows(QModelIndex(), frames.count() - num, frames.count() - 1);
    endInsertRows();
}

void CANFrameModel::clearFrames()
{
    mutex.lock();
    this->beginResetModel();
    frames.clear();
    this->endResetModel();
    mutex.unlock();
}

//Is this safe? Maybe not but if we don't change it then that's OK
//Is it the best C++ practice? Probably not. This breaks the MVC paradigm
//but, it's for a good cause.
QVector<CANFrame>* CANFrameModel::getListReference()
{
    return &frames;
}
