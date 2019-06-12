#include "canframemodel.h"

#include <QFile>
#include <QApplication>
#include <QPalette>
#include <QDateTime>
#include "utility.h"

CANFrameModel::~CANFrameModel()
{
    frames.clear();
    filteredFrames.clear();
    filters.clear();
}

int CANFrameModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (filteredFrames.data())
    {
        int rows = filteredFrames.count();
        return rows;
    }

     //just in case somehow data is invalid which I have seen before.
    //But, this should not happen so issue a debugging message too
    qDebug() << "Invalid data for filteredFrames. Returning 0.";
    return 0;
}

int CANFrameModel::totalFrameCount()
{
    int count;
    count = frames.count();
    return count;
}

int CANFrameModel::columnCount(const QModelIndex &index) const
{
    Q_UNUSED(index);
    return (int)Column::NUM_COLUMN;
}

CANFrameModel::CANFrameModel(QObject *parent)
    : QAbstractTableModel(parent)
{

    if (QSysInfo::WordSize > 32)
    {
        qDebug() << "64 bit OS detected. Requesting a large preallocation";
        preallocSize = 10000000;
    }
    else //if compiling for 32 bit you can't ask for gigabytes of preallocation so tone it down.
    {
        qDebug() << "32 bit OS detected. Requesting a much restricted prealloc";
        preallocSize = 2000000;
    }

    frames.reserve(preallocSize);
    filteredFrames.reserve(preallocSize); //the goal is to prevent a reallocation from ever happening

    dbcHandler = DBCHandler::getReference();
    interpretFrames = false;
    overwriteDups = false;
    useHexMode = true;
    timeSeconds = false;
    timeOffset = 0;
    needFilterRefresh = false;
    lastUpdateNumFrames = 0;
    timeFormat =  "MMM-dd HH:mm:ss.zzz";
    sortDirAsc = false;
}

void CANFrameModel::setHexMode(bool mode)
{
    if (useHexMode != mode)
    {
        this->beginResetModel();
        useHexMode = mode;
        Utility::decimalMode = !useHexMode;
        this->endResetModel();
    }
}

void CANFrameModel::setSecondsMode(bool mode)
{
    if (Utility::secondsMode != mode)
    {
        this->beginResetModel();
        Utility::secondsMode = mode;
        this->endResetModel();
    }
}

void CANFrameModel::setSysTimeMode(bool mode)
{
    if (Utility::sysTimeMode != mode)
    {
        this->beginResetModel();
        Utility::sysTimeMode = mode;
        this->endResetModel();
    }
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

void CANFrameModel::setTimeFormat(QString format)
{
    Utility::timeFormat = format;
    beginResetModel(); //reset model to show new time format
    endResetModel();
}

/*
 * Scan all frames for the smallest timestamp and offset all timestamps so that smallest one is at 0
*/
void CANFrameModel::normalizeTiming()
{
    mutex.lock();
    if (frames.count() == 0) return;
    timeOffset = frames[0].timestamp;

    //find the absolute lowest timestamp in the whole time. Needed because maybe timestamp was reset in the middle.
    for (int j = 0; j < frames.count(); j++)
    {
        if (frames[j].timestamp < timeOffset) timeOffset = frames[j].timestamp;
    }

    for (int i = 0; i < frames.count(); i++)
    {
        frames[i].timestamp -= timeOffset;
    }

    this->beginResetModel();
    for (int i = 0; i < filteredFrames.count(); i++)
    {
        filteredFrames[i].timestamp -= timeOffset;
    }
    this->endResetModel();

    mutex.unlock();
}

void CANFrameModel::setOverwriteMode(bool mode)
{
    beginResetModel();
    overwriteDups = mode;
    recalcOverwrite();
    endResetModel();
}

void CANFrameModel::setFilterState(unsigned int ID, bool state)
{
    if (!filters.contains(ID)) return;
    filters[ID] = state;
    sendRefresh();
}

void CANFrameModel::setAllFilters(bool state)
{
    QMap<int, bool>::iterator it;
    for (it = filters.begin(); it != filters.end(); ++it)
    {
        it.value() = state;
    }
    sendRefresh();
}

/*
 * There is probably a more correct way to have done this but below are several functions that collectively implement
 * quicksort on the columns and interpret the columns numerically. But, correct or not, this implementation is quite fast
 * and sorts the columns properly.
*/
uint64_t CANFrameModel::getCANFrameVal(int row, Column col)
{
    uint64_t temp = 0;
    if (row >= frames.count()) return 0;
    CANFrame frame = frames[row];
    switch (col)
    {
    case Column::TimeStamp:
        if (overwriteDups) return frame.timedelta;
        return frame.timestamp;
    case Column::FrameId:
        return frame.ID;
    case Column::Extended:
        if (frame.extended) return 1;
        return 0;
    case Column::Remote:
        if (overwriteDups) return frame.frameCount;
        if (frame.remote) return 1;
        return 0;
    case Column::Direction:
        if (frame.isReceived) return 1;
        return 0;
    case Column::Bus:
        return frame.bus;
    case Column::Length:
        return frame.len;
    case Column::ASCII: //sort both the same for now
    case Column::Data:
        for (int i = 0; i < frame.len; i++) temp += ((uint64_t)frame.data[i] << (56 - (8 * i)));
        //qDebug() << temp;
        return temp;
    }
}

void CANFrameModel::qSortCANFrameAsc(QVector<CANFrame> *frames, Column column, int lowerBound, int upperBound)
{
    int p, i, j;
    qDebug() << "Lower " << lowerBound << " Upper" << upperBound;
    if (lowerBound < upperBound)
    {
        uint64_t piv = getCANFrameVal(lowerBound + (upperBound - lowerBound) / 2, column);
        i = lowerBound - 1;
        j = upperBound + 1;
        for (;;){
            do {
                i++;
            } while ((i < upperBound) && getCANFrameVal(i, column) < piv);

            do
            {
                j--;
            } while ((j > lowerBound) && getCANFrameVal(j, column) > piv);
            if (i < j) {
                CANFrame temp = frames->at(i);
                frames->replace(i, frames->at(j));
                frames->replace(j, temp);
            }
            else {p = j; break;}
        }

        qSortCANFrameAsc(frames, column, lowerBound, p);
        qSortCANFrameAsc(frames, column, p+1, upperBound);
    }
}

void CANFrameModel::qSortCANFrameDesc(QVector<CANFrame> *frames, Column column, int lowerBound, int upperBound)
{
    int p, i, j;
    qDebug() << "Lower " << lowerBound << " Upper" << upperBound;
    if (lowerBound < upperBound)
    {
        uint64_t piv = getCANFrameVal(lowerBound + (upperBound - lowerBound) / 2, column);
        i = lowerBound - 1;
        j = upperBound + 1;
        for (;;){
            do {
                i++;
            } while ((i < upperBound) && getCANFrameVal(i, column) > piv);

            do
            {
                j--;
            } while ((j > lowerBound) && getCANFrameVal(j, column) < piv);
            if (i < j) {
                CANFrame temp = frames->at(i);
                frames->replace(i, frames->at(j));
                frames->replace(j, temp);
            }
            else {p = j; break;}
        }

        qSortCANFrameDesc(frames, column, lowerBound, p);
        qSortCANFrameDesc(frames, column, p+1, upperBound);
    }
}

void CANFrameModel::sortByColumn(int column)
{
    sortDirAsc = !sortDirAsc;
    //beginResetModel();
    if (sortDirAsc) qSortCANFrameAsc(&frames, Column(column), 0, frames.count()-1);
    else qSortCANFrameDesc(&frames, Column(column), 0, frames.count()-1);
    //endResetModel();
    sendRefresh();
}

//End of custom sorting code

void CANFrameModel::recalcOverwrite()
{
    if (!overwriteDups) return; //no need to do a thing if mode is disabled

    qDebug() << "recalcOverwrite called in model";

    int lastUnique = 0;
    bool found;

    mutex.lock();
    beginResetModel();

    //Look at the current list of frames and turn it into just a list of unique IDs
    QHash<uint64_t, CANFrame> overWriteFrames;
    uint64_t idAugmented; //id in lower 29 bits, bus number shifted up 29 bits
    foreach(CANFrame frame, frames)
    {
        idAugmented = frame.ID;
        idAugmented = idAugmented + (frame.bus << 29ull);
        if (!overWriteFrames.contains(idAugmented))
        {
            frame.timedelta = 0;
            frame.frameCount = 1;
            overWriteFrames.insert(idAugmented, frame);
        }
        else
        {
            frame.timedelta = frame.timestamp - overWriteFrames[idAugmented].timestamp;
            frame.frameCount = overWriteFrames[idAugmented].frameCount + 1;
            overWriteFrames[idAugmented] = frame;
        }
    }
    //Then replace the old list of frames with just the unique list
    frames.clear();
    frames.append(overWriteFrames.values().toVector());

    filteredFrames.clear();
    filteredFrames.reserve(preallocSize);

    for (int i = 0; i < frames.count(); i++)
    {
        if (filters[frames[i].ID])
        {
            filteredFrames.append(frames[i]);
        }
    }

    endResetModel();
    mutex.unlock();
}

QVariant CANFrameModel::data(const QModelIndex &index, int role) const
{
    int dLen;
    QString tempString;
    CANFrame thisFrame;
    static bool rowFlip = false;
    QVariant ts;

    if (!index.isValid())
        return QVariant();

    if (index.row() >= (filteredFrames.count()))
        return QVariant();

    thisFrame = filteredFrames.at(index.row());

    if (role == Qt::BackgroundColorRole)
    {
        if (dbcHandler != NULL && interpretFrames)
        {
            DBC_MESSAGE *msg = dbcHandler->findMessage(thisFrame);
            if (msg != NULL)
            {
                return msg->bgColor;
            }
        }
        rowFlip = (index.row() % 2);
        if (rowFlip) return QApplication::palette().color(QPalette::Base);
        else return QApplication::palette().color(QPalette::AlternateBase);
    }

    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignLeft;
    }

    if (role == Qt::TextColorRole)
    {
        if (dbcHandler != NULL && interpretFrames)
        {
            DBC_MESSAGE *msg = dbcHandler->findMessage(thisFrame);
            if (msg != NULL)
            {
                return msg->fgColor;
            }
        }
        return QApplication::palette().color(QPalette::WindowText);
    }

    if (role == Qt::DisplayRole) {
        switch (Column(index.column()))
        {
        case Column::TimeStamp:            
            //Reformatting the output a bit with custom code
            if (overwriteDups)
            {
                if (timeSeconds) return QString::number(thisFrame.timedelta / 1000000.0, 'f', 5);
                return QString::number(thisFrame.timedelta);
            }
            else ts = Utility::formatTimestamp(thisFrame.timestamp);
            if (ts.type() == QVariant::Double) return QString::number(ts.toDouble(), 'f', 5); //never scientific notation, 5 decimal places
            if (ts.type() == QVariant::LongLong) return QString::number(ts.toLongLong()); //never scientific notion, all digits shown
            if (ts.type() == QVariant::DateTime) return ts.toDateTime().toString(timeFormat); //custom set format for dates and times
            return Utility::formatTimestamp(thisFrame.timestamp);
        case Column::FrameId:
            return Utility::formatCANID(thisFrame.ID, thisFrame.extended);
        case Column::Extended:
            return QString::number(thisFrame.extended);
        case Column::Remote:
            if (!overwriteDups) return QString::number(thisFrame.remote);
            return QString::number(thisFrame.frameCount);
        case Column::Direction:
            if (thisFrame.isReceived) return QString(tr("Rx"));
            return QString(tr("Tx"));
        case Column::Bus:
            return QString::number(thisFrame.bus);
        case Column::Length:
            return QString::number(thisFrame.len);
        case Column::ASCII:
            if (thisFrame.ID >= 0x7FFFFFF0ull)
            {
                tempString.append("MARK ");
                tempString.append(QString::number(thisFrame.ID & 0x7));
                return tempString;
            }
            dLen = thisFrame.len;
            if (!thisFrame.remote) {
                if (dLen < 0) dLen = 0;
                if (dLen > 8) dLen = 8;
                for (int i = 0; i < dLen; i++)
                {
                    quint8 byt = thisFrame.data[i];
                    //0x20 through 0x7E are printable characters. Outside of that range they aren't. So use dots instead
                    if (byt < 0x20) byt = 0x2E; //dot character
                    if (byt > 0x7E) byt = 0x2E;
                    tempString.append(QString::fromUtf8((char *)&byt, 1));
                }
            }
            return tempString;
        case Column::Data:
            dLen = thisFrame.len;
            if (dLen < 0) dLen = 0;
            if (dLen > 8) dLen = 8;
            //if (useHexMode) tempString.append("0x ");
            if (thisFrame.remote) {
                return tempString;
            }
            for (int i = 0; i < dLen; i++)
            {
                if (useHexMode) tempString.append( QString::number(thisFrame.data[i], 16).toUpper().rightJustified(2, '0'));
                else tempString.append(QString::number(thisFrame.data[i], 10));
                tempString.append(" ");
            }
            //now, if we're supposed to interpret the data and the DBC handler is loaded then use it
            if (dbcHandler != NULL && interpretFrames)
            {
                DBC_MESSAGE *msg = dbcHandler->findMessage(thisFrame);
                if (msg != NULL)
                {
                    tempString.append("   <Interpreted>\n");
                    tempString.append(msg->name + "\n" + msg->comment + "\n");
                    for (int j = 0; j < msg->sigHandler->getCount(); j++)
                    {
                        QString sigString;
                        DBC_SIGNAL* sig = msg->sigHandler->findSignalByIdx(j);
                        if (sig->processAsText(thisFrame, sigString))
                        {
                            tempString.append(sigString);
                            tempString.append("\n");
                        }
                        else if (sig->isMultiplexed && overwriteDups) //wasn't in this exact frame but is in the message. Use cached value
                        {
                            tempString.append(sig->makePrettyOutput(sig->cachedValue.toDouble(), sig->cachedValue.toLongLong()));
                            tempString.append("\n");
                        }
                    }
                }
            }
            return tempString;
        default:
            return tempString;
        }
    }

    return QVariant();
}

QVariant CANFrameModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal)
    {
        switch (Column(section))
        {
        case Column::TimeStamp:
            if (overwriteDups) return QString(tr("Time Delta"));
            return QString(tr("Timestamp"));
        case Column::FrameId:
            return QString(tr("ID"));
        case Column::Extended:
            return QString(tr("Ext"));
        case Column::Remote:
            if (!overwriteDups) return QString(tr("RTR"));
            return QString(tr("Cnt"));
        case Column::Direction:
            return QString(tr("Dir"));
        case Column::Bus:
            return QString(tr("Bus"));
        case Column::Length:
            return QString(tr("Len"));
        case Column::ASCII:
            return QString(tr("ASCII"));
        case Column::Data:
            return QString(tr("Data"));
        default:
            return QString("");
        }
    }

    else
        return QString::number(section + 1);

    return QVariant();
}


void CANFrameModel::addFrame(const CANFrame& frame, bool autoRefresh = false)
{
    /*TODO: remove mutex */
    mutex.lock();
    CANFrame tempFrame;
    tempFrame = frame;
    tempFrame.timestamp -= timeOffset;

    lastUpdateNumFrames++;

    //if this ID isn't found in the filters list then add it and show it by default
    if (!filters.contains(tempFrame.ID))
    {
        filters.insert(tempFrame.ID, true);
        needFilterRefresh = true;
    }

    if (!overwriteDups)
    {
        frames.append(tempFrame);
        if (filters[tempFrame.ID])
        {
            if (autoRefresh) beginInsertRows(QModelIndex(), filteredFrames.count(), filteredFrames.count());
            tempFrame.frameCount = 1;
            filteredFrames.append(tempFrame);
            if (autoRefresh) endInsertRows();
        }
    }
    else //yes, overwrite dups
    {
        bool found = false;
        for (int i = 0; i < frames.count(); i++)
        {
            if ( (frames[i].ID == tempFrame.ID) && (frames[i].bus == tempFrame.bus) )
            {
                tempFrame.frameCount = frames[i].frameCount + 1;
                tempFrame.timedelta = tempFrame.timestamp - frames[i].timestamp;
                frames.replace(i, tempFrame);
                found = true;
                break;
            }
        }
        if (!found)
        {
            frames.append(tempFrame);
            if (filters[tempFrame.ID])
            {
                if (autoRefresh) beginInsertRows(QModelIndex(), filteredFrames.count(), filteredFrames.count());
                tempFrame.frameCount = 1;
                tempFrame.timedelta = 0;
                filteredFrames.append(tempFrame);
                if (autoRefresh) endInsertRows();
            }
        }
        else
        {
            for (int j = 0; j < filteredFrames.count(); j++)
            {
                if ( (filteredFrames[j].ID == tempFrame.ID) && (filteredFrames[j].bus == tempFrame.bus) )
                {
                    if (autoRefresh) beginResetModel();
                    filteredFrames.replace(j, tempFrame);
                    if (autoRefresh) endResetModel();
                }
            }
        }
    }

    mutex.unlock();
}


void CANFrameModel::addFrames(const CANConnection*, const QVector<CANFrame>& pFrames)
{
    foreach(const CANFrame& frame, pFrames)
    {
        addFrame(frame);
    }
    if (overwriteDups) //if in overwrite mode we'll update every time frames come in
    {
        beginResetModel();
        endResetModel();
    }
}

void CANFrameModel::sendRefresh()
{
    qDebug() << "Sending mass refresh";
    QVector<CANFrame> tempContainer;
    int count = frames.count();
    for (int i = 0; i < count; i++)
    {
        if (filters[frames[i].ID])
        {
            tempContainer.append(frames[i]);
        }
    }
    mutex.lock();
    beginResetModel();
    filteredFrames.clear();
    filteredFrames.reserve(preallocSize);
    filteredFrames.append(tempContainer);

    lastUpdateNumFrames = 0;
    endResetModel();
    mutex.unlock();
}

void CANFrameModel::sendRefresh(int pos)
{
    beginInsertRows(QModelIndex(), pos, pos);
    endInsertRows();
}

//issue a refresh for the last num entries in the model.
//used by the serial worker to do batch updates so it doesn't
//have to send thousands of messages per second
int CANFrameModel::sendBulkRefresh()
{
    //int num = filteredFrames.count() - lastUpdateNumFrames;
    if (lastUpdateNumFrames <= 0) return 0;

    if (lastUpdateNumFrames == 0 && !overwriteDups) return 0;
    if (filteredFrames.count() == 0) return 0;

    qDebug() << "Bulk refresh of " << lastUpdateNumFrames;

    beginResetModel();
    endResetModel();

    int num = lastUpdateNumFrames;
    lastUpdateNumFrames = 0;

    return num;
}

void CANFrameModel::clearFrames()
{
    mutex.lock();
    this->beginResetModel();
    frames.clear();
    filteredFrames.clear();
    filters.clear();
    frames.reserve(preallocSize);
    filteredFrames.reserve(preallocSize);
    this->endResetModel();
    lastUpdateNumFrames = 0;
    mutex.unlock();

    emit updatedFiltersList();
}

/*
 * Since the getListReference function returns readonly
 * you can't insert frames with it. Instead this function
 * allows for a mass import of frames into the model
 */
void CANFrameModel::insertFrames(const QVector<CANFrame> &newFrames)
{
    //not resetting the model here because the serial worker automatically does a bulk refresh every 1/4 second
    //and that refresh will cause the view to update. If you do both it usually ends up thinking you have
    //double the number of frames.
    //beginResetModel();
    mutex.lock();
    int insertedFiltered = 0;
    for (int i = 0; i < newFrames.count(); i++)
    {
        frames.append(newFrames[i]);
        if (!filters.contains(newFrames[i].ID))
        {
            filters.insert(newFrames[i].ID, true);
            needFilterRefresh = true;
        }
        if (filters[newFrames[i].ID])
        {
            insertedFiltered++;
            filteredFrames.append(newFrames[i]);
        }
    }
    lastUpdateNumFrames = newFrames.count();
    mutex.unlock();
    //endResetModel();
    //beginInsertRows(QModelIndex(), filteredFrames.count() + 1, filteredFrames.count() + insertedFiltered);
    //endInsertRows();
    if (needFilterRefresh) emit updatedFiltersList();
}

int CANFrameModel::getIndexFromTimeID(unsigned int ID, double timestamp)
{
    int bestIndex = -1;
    uint64_t intTimeStamp = timestamp * 1000000l;
    for (int i = 0; i < frames.count(); i++)
    {
        if ((frames[i].ID == ID))
        {
            if (frames[i].timestamp <= intTimeStamp) bestIndex = i;
            else break; //drop out of loop as soon as we pass the proper timestamp
        }
    }
    return bestIndex;
}

void CANFrameModel::loadFilterFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    int ID;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    filters.clear();

    while (!inFile->atEnd()) {
        line = inFile->readLine().simplified();
        if (line.length() > 2)
        {
            QList<QByteArray> tokens = line.split(',');
            ID = tokens[0].toInt(NULL, 16);
            if (tokens[1].toUpper() == "T") filters.insert(ID, true);
                else filters.insert(ID, false);
        }
    }
    inFile->close();

    sendRefresh();

    emit updatedFiltersList();
}

void CANFrameModel::saveFilterFile(QString filename)
{
    QFile *outFile = new QFile(filename);

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QMap<int, bool>::const_iterator it;
    for (it = filters.begin(); it != filters.end(); ++it)
    {
        outFile->write(QString::number(it.key(), 16).toUtf8());
        outFile->putChar(',');
        if (it.value()) outFile->putChar('T');
            else outFile->putChar('F');
        outFile->write("\n");
    }
    outFile->close();
}

bool CANFrameModel::needsFilterRefresh()
{
    bool temp = needFilterRefresh;
    needFilterRefresh = false;
    return temp;
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

const QVector<CANFrame>* CANFrameModel::getFilteredListReference() const
{
    return &filteredFrames;
}

const QMap<int, bool>* CANFrameModel::getFiltersReference() const
{
    return &filters;
}
