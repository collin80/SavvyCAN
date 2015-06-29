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
    timeOffset = 0;
}

void CANFrameModel::setHexMode(bool mode)
{
    if (useHexMode != mode)
    {
        this->beginResetModel();
        useHexMode = mode;
        this->endResetModel();
    }
}

void CANFrameModel::setDBCHandler(DBCHandler *handler)
{
    dbcHandler = handler;
}

void CANFrameModel::setInterpetMode(bool mode)
{
    //if the state of interpretFrames changes then we need to reset the model
    //so that QT will refresh the view properly
    if (interpretFrames != mode)
    {
        this->beginResetModel();
        interpretFrames = mode;
        this->endResetModel();
    }
}

void CANFrameModel::normalizeTiming()
{
    if (frames.count() == 0) return;
    this->beginResetModel();
    timeOffset = frames[0].timestamp;
    for (int i = 0; i < frames.count(); i++)
    {
        frames[i].timestamp -= timeOffset;
    }
    this->endResetModel();
}

void CANFrameModel::setOverwriteMode(bool mode)
{
    overwriteDups = mode;
}

void CANFrameModel::recalcOverwrite()
{
    if (!overwriteDups) return; //no need to do a thing if mode is disabled

    int lastUnique = 0;
    bool found;

    beginResetModel();
    for (int i = 1; i < frames.count(); i++)
    {
        found = false;
        for (int j = 0; j <= lastUnique; j++)
        {
            if (frames[i].ID == frames[j].ID)
            {
                frames.replace(j, frames[i]);
                found = true;
                break;
            }
        }
        if (!found)
        {
            lastUnique++;
            frames.replace(lastUnique, frames[i]);
        }
    }

    while (frames.count() > lastUnique) frames.removeLast();

    endResetModel();
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
            if (useHexMode)
                return QString::number(thisFrame.ID, 16).toUpper().rightJustified(2,'0');
            else
                return QString::number(thisFrame.ID).rightJustified(3,'0');
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
                if (useHexMode) tempString.append(QString::number(thisFrame.data[i], 16).toUpper().rightJustified(2,'0'));
                    else tempString.append(QString::number(thisFrame.data[i]).rightJustified(3,'0'));
                tempString.append(" ");
            }
            //now, if we're supposed to interpret the data and the DBC handler is loaded then use it
            if (dbcHandler != NULL && interpretFrames)
            {
                DBC_MESSAGE *msg = dbcHandler->findMsgByID(thisFrame.ID);
                if (msg != NULL)
                {
                    tempString.append("\r\n");
                    for (int j = 0; j < msg->msgSignals.length(); j++)
                    {

                        tempString.append(dbcHandler->processSignal(thisFrame, msg->msgSignals.at(j)));
                        tempString.append("\r\n");
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
    frame.timestamp -= timeOffset;
    if (!overwriteDups)
    {
        if (autoRefresh) beginInsertRows(QModelIndex(), frames.count() + 1, frames.count() + 1);
        frames.append(frame);
        if (autoRefresh) endInsertRows();
    }
    else
    {
        bool found = false;
        for (int i = 0; i < frames.count(); i++)
        {
            if (frames[i].ID == frame.ID)
            {
                if (autoRefresh) beginResetModel();
                frames.replace(i, frame);
                if (autoRefresh) endResetModel();
                found = true;
                break;
            }
        }
        if (!found)
        {
            if (autoRefresh) beginInsertRows(QModelIndex(), frames.count() + 1, frames.count() + 1);
            frames.append(frame);
            if (autoRefresh) endInsertRows();
        }
    }
    mutex.unlock();
}

void CANFrameModel::sendRefresh()
{
    beginResetModel();
    endResetModel();
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

    if (!overwriteDups)
    {
        if (num > frames.count()) num = frames.count();
        beginInsertRows(QModelIndex(), frames.count() - num, frames.count() - 1);
        endInsertRows();
    }
    else
    {
        sendRefresh();
    }
}

void CANFrameModel::clearFrames()
{
    mutex.lock();
    this->beginResetModel();
    frames.clear();
    this->endResetModel();
    mutex.unlock();
}

/*
 * Since the getListReference function returns readonly
 * you can't insert frames with it. Instead this function
 * allows for a mass import of frames into the model
 */
void CANFrameModel::insertFrames(const QVector<CANFrame> &newFrames)
{
    beginInsertRows(QModelIndex(), frames.count() + 1, frames.count() + newFrames.count());
    for (int i = 0; i < newFrames.count(); i++)
    {
        frames.append(newFrames[i]);
    }
    endInsertRows();
}

/*
 *This used to not be const correct but it is now. So, there's little harm in
 * allowing external code to peek at our frames. There's just no touching.
 * This ability to get a direct read-only reference speeds up a variety of
 * external code that needs to access frames directly and doesn't care about
 * this model's normal output mechanism.
 */
const QVector<CANFrame>* CANFrameModel::getListReference() const
{
    return &frames;
}
