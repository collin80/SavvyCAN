#include "canframemodel.h"

#include <QFile>
#include <QApplication>
#include <QPalette>
#include <QDateTime>
#include <QSettings>
#include "utility.h"

CANFrameModel::~CANFrameModel()
{
    frames.clear();
    filteredFrames.clear();
    filters.clear();
    busFilters.clear();
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
    int maxFramesDefault;
    if (QSysInfo::WordSize > 32)
    {
        qDebug() << "64 bit OS detected. Requesting a large preallocation";
        maxFramesDefault = 10000000;
    }
    else //if compiling for 32 bit you can't ask for gigabytes of preallocation so tone it down.
    {
        qDebug() << "32 bit OS detected. Requesting a much restricted prealloc";
        maxFramesDefault = 2000000;
    }

    QSettings settings;
    preallocSize = settings.value("Main/MaximumFrames", maxFramesDefault).toInt();

    //Each CANFrame object takes up 56 bytes and we're allocating two arrays here so take the
    //# of pre-alloc frames and multiply by 112 to get the RAM usage. This is around 1GiB for the default.

    //the goal is to prevent a reallocation from ever happening
    frames.reserve(preallocSize);
    //this is pretty wasteful. We're storing all frames twice. It may be better for filteredFrames to be a list of pointers.
    //Pointers take up 8 bytes instead of 56 so this is quite a savings for RAM usage. But, then filteredFrames would
    //work differently from frames above so the two could not be used interchangeably. Still think about what can be done.
    filteredFrames.reserve(preallocSize);

    dbcHandler = DBCHandler::getReference();
    interpretFrames = false;
    overwriteDups = false;
    filtersPersistDuringClear = false;
    useHexMode = true;
    timeStyle = TS_MICROS;
    timeOffset = 0;
    needFilterRefresh = false;
    lastUpdateNumFrames = 0;
    timeFormat =  "MMM-dd HH:mm:ss.zzz";
    sortDirAsc = false;
    bytesPerLine = 8;
}

void CANFrameModel::setBytesPerLine(int bpl)
{
    bytesPerLine = bpl;
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

void CANFrameModel::setTimeStyle(TimeStyle newStyle)
{
    if (timeStyle != newStyle)
    {
        this->beginResetModel();
        timeStyle = newStyle;
        Utility::timeStyle = newStyle;
        this->endResetModel();
    }
}

void CANFrameModel::setInterpretMode(bool mode)
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

bool CANFrameModel::getInterpretMode()
{
    return interpretFrames;
}

void CANFrameModel::setTimeFormat(QString format)
{
    Utility::timeFormat = format;
    timeFormat = format;
    beginResetModel(); //reset model to show new time format
    endResetModel();
}

void CANFrameModel::setIgnoreDBCColors(bool mode)
{
    if(ignoreDBCColors != mode)
    {
        beginResetModel(); //reset model to update the view
        ignoreDBCColors = mode;
        endResetModel();
    }
}

/*
 * Scan all frames for the smallest timestamp and offset all timestamps so that smallest one is at 0
*/
void CANFrameModel::normalizeTiming()
{
    mutex.lock();
    if (frames.count() == 0) 
    {
        mutex.unlock();
        return;
    }
    timeOffset = frames[0].timeStamp().microSeconds();
    qint64 prevStamp = 0;

    //find the absolute lowest timestamp in the whole time. Needed because maybe timestamp was reset in the middle.
    for (int j = 0; j < frames.count(); j++)
    {
        if (frames[j].timeStamp().microSeconds() < timeOffset) timeOffset = frames[j].timeStamp().microSeconds();
    }

    for (int i = 0; i < frames.count(); i++)
    {
        qint64 thisStamp = frames[i].timeStamp().microSeconds() - timeOffset;
        if (thisStamp <= prevStamp)
        {
            timeOffset -= prevStamp;
        }
        frames[i].setTimeStamp(QCanBusFrame::TimeStamp(0, thisStamp));
    }

    this->beginResetModel();
    for (int i = 0; i < filteredFrames.count(); i++)
    {
        filteredFrames[i].setTimeStamp(QCanBusFrame::TimeStamp(0, filteredFrames[i].timeStamp().microSeconds() - timeOffset));
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

void CANFrameModel::setClearMode(bool mode)
{
    filtersPersistDuringClear = mode;
}

void CANFrameModel::setFilterState(unsigned int ID, bool state)
{
    if (!filters.contains(ID)) return;
    filters[ID] = state;
    sendRefresh();
}

void CANFrameModel::setBusFilterState(unsigned int BusID, bool state)
{
    if (!busFilters.contains(BusID)) return;
    busFilters[BusID] = state;
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
uint64_t CANFrameModel::getCANFrameVal(QVector<CANFrame> *frames, int row, Column col)
{
    uint64_t temp = 0;
    if (row >= frames->count()) return 0;
    CANFrame frame = frames->at(row);
    switch (col)
    {
    case Column::TimeStamp:
        if (overwriteDups) return frame.timedelta;
        return frame.timeStamp().microSeconds();
    case Column::FrameId:
        return frame.frameId();
    case Column::Extended:
        if (frame.hasExtendedFrameFormat()) return 1;
        return 0;
    case Column::Remote:
        if (overwriteDups) return frame.frameCount;
        if (frame.frameType() == QCanBusFrame::RemoteRequestFrame) return 1;
        return 0;
    case Column::Direction:
        if (frame.isReceived) return 1;
        return 0;
    case Column::Bus:
        return static_cast<uint64_t>(frame.bus);
    case Column::Length:
        return static_cast<uint64_t>(frame.payload().length());
    case Column::ASCII: //sort both the same for now
    case Column::Data:
        for (int i = 0; i < std::min(frame.payload().length(), 8); i++) temp += (static_cast<uint64_t>(frame.payload()[i]) << (56 - (8 * i)));
        //qDebug() << temp;
        return temp;
    case Column::NUM_COLUMN:
        return 0;
    }
    return 0;
}

void CANFrameModel::qSortCANFrameAsc(QVector<CANFrame> *frames, Column column, int lowerBound, int upperBound)
{
    int p, i, j;
    qDebug() << "Lower " << lowerBound << " Upper" << upperBound;
    if (lowerBound < upperBound)
    {
        uint64_t piv = getCANFrameVal(frames, lowerBound + (upperBound - lowerBound) / 2, column);
        i = lowerBound - 1;
        j = upperBound + 1;
        for (;;){
            do {
                i++;
            } while ((i < upperBound) && getCANFrameVal(frames, i, column) < piv);

            do
            {
                j--;
            } while ((j > lowerBound) && getCANFrameVal(frames, j, column) > piv);
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
        uint64_t piv = getCANFrameVal(frames, lowerBound + (upperBound - lowerBound) / 2, column);
        i = lowerBound - 1;
        j = upperBound + 1;
        for (;;){
            do {
                i++;
            } while ((i < upperBound) && getCANFrameVal(frames, i, column) > piv);

            do
            {
                j--;
            } while ((j > lowerBound) && getCANFrameVal(frames, j, column) < piv);
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
    if (sortDirAsc) qSortCANFrameAsc(&filteredFrames, Column(column), 0, filteredFrames.count()-1);
    else qSortCANFrameDesc(&filteredFrames, Column(column), 0, filteredFrames.count()-1);

    mutex.lock();
    beginResetModel();
    endResetModel();
    mutex.unlock();
}

//End of custom sorting code

void CANFrameModel::recalcOverwrite()
{
    if (!overwriteDups) return; //no need to do a thing if mode is disabled

    qDebug() << "recalcOverwrite called in model";

    mutex.lock();
    beginResetModel();

    //Look at the current list of frames and turn it into just a list of unique IDs
    QHash<uint64_t, CANFrame> overWriteFrames;
    uint64_t idAugmented; //id in lower 29 bits, bus number shifted up 29 bits
    foreach(CANFrame frame, frames)
    {
        if (frame.frameType() != frame.DataFrame) continue;

        idAugmented = frame.frameId();
        idAugmented = idAugmented + (frame.bus << 29ull);
        if (filters[frame.frameId()] && busFilters[frame.bus])
        {
            if (!overWriteFrames.contains(idAugmented))
            {
                frame.timedelta = 0;
                frame.frameCount = 1;
                overWriteFrames.insert(idAugmented, frame);
            }
            else
            {
                frame.timedelta = frame.timeStamp().microSeconds() - overWriteFrames[idAugmented].timeStamp().microSeconds();
                frame.frameCount = overWriteFrames[idAugmented].frameCount + 1;
                overWriteFrames[idAugmented] = frame;
            }
        }
    }
    //Then replace the old list of frames with just the unique list
    //frames.clear();
    //frames.append(overWriteFrames.values().toVector());
    //frames.reserve(preallocSize);

    filteredFrames.clear();
    filteredFrames.append(overWriteFrames.values().toVector());
    filteredFrames.reserve(preallocSize);

    /*for (int i = 0; i < frames.count(); i++)
    {
        if (filters[frames[i].frameId()] && busFilters[frames[i].bus])
        {
            filteredFrames.append(frames[i]);
        }
    }*/

    endResetModel();
    mutex.unlock();
}

QVariant CANFrameModel::data(const QModelIndex &index, int role) const
{
    QString tempString;
    CANFrame thisFrame;
    static bool rowFlip = false;
    QVariant ts;

    if (!index.isValid())
        return QVariant();

    if (index.row() >= (filteredFrames.count()))
        return QVariant();

    thisFrame = filteredFrames.at(index.row());

    const unsigned char *data = reinterpret_cast<const unsigned char *>(thisFrame.payload().constData());
    int dataLen = thisFrame.payload().count();

    if (role == Qt::BackgroundRole)
    {
        if (dbcHandler != nullptr && interpretFrames && !ignoreDBCColors)
        {
            DBC_MESSAGE *msg = dbcHandler->findMessage(thisFrame);
            if (msg != nullptr)
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
        switch(Column(index.column()))
        {
        case Column::TimeStamp:
            return Qt::AlignRight;
        case Column::FrameId:
        case Column::Direction:
        case Column::Extended:
        case Column::Bus:
        case Column::Remote:
        case Column::Length:
            return Qt::AlignHCenter;
        default:
            return Qt::AlignLeft;
        }
    }

    if (role == Qt::ForegroundRole)
    {
        if (dbcHandler != nullptr && interpretFrames && !ignoreDBCColors)
        {
            DBC_MESSAGE *msg = dbcHandler->findMessage(thisFrame);
            if (msg != nullptr)
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
                if (timeStyle == TS_SECONDS) return QString::number(thisFrame.timedelta / 1000000.0, 'f', 5);
                return QString::number(thisFrame.timedelta);
            }
            else ts = Utility::formatTimestamp(thisFrame.timeStamp().microSeconds());
            if (ts.type() == QVariant::Double) return QString::number(ts.toDouble(), 'f', 5); //never scientific notation, 5 decimal places
            if (ts.type() == QVariant::LongLong) return QString::number(ts.toLongLong()); //never scientific notion, all digits shown
            if (ts.type() == QVariant::DateTime) return ts.toDateTime().toString(timeFormat); //custom set format for dates and times
            return Utility::formatTimestamp(thisFrame.timeStamp().microSeconds());
        case Column::FrameId:
            return Utility::formatCANID(thisFrame.frameId(), thisFrame.hasExtendedFrameFormat());
        case Column::Extended:
            return QString::number(thisFrame.hasExtendedFrameFormat());
        case Column::Remote:
            if (!overwriteDups) return QString::number(thisFrame.frameType() == QCanBusFrame::RemoteRequestFrame);
            return QString::number(thisFrame.frameCount);
        case Column::Direction:
            if (thisFrame.isReceived) return QString(tr("Rx"));
            return QString(tr("Tx"));
        case Column::Bus:
            return QString::number(thisFrame.bus);
        case Column::Length:
            return QString::number(dataLen);
        case Column::ASCII:
            if (thisFrame.frameId() >= 0x7FFFFFF0ull)
            {
                tempString.append("MARK ");
                tempString.append(QString::number(thisFrame.frameId() & 0x7));
                return tempString;
            }
            if (thisFrame.frameType() == QCanBusFrame::DataFrame) {
                if (dataLen < 0) dataLen = 0;
                //if (dLen > 8) dLen = 8;
                for (int i = 0; i < dataLen; i++)
                {
                    char byt = thisFrame.payload()[i];
                    //0x20 through 0x7E are printable characters. Outside of that range they aren't. So use dots instead
                    if (byt < 0x20) byt = 0x2E; //dot character
                    if (byt > 0x7E) byt = 0x2E;
                    tempString.append(QString::fromUtf8(&byt, 1));
                    if (!((i+1) % bytesPerLine) && (i != (dataLen - 1))) tempString.append("\n");
                }
            }
            if (thisFrame.frameType() == QCanBusFrame::ErrorFrame)
            {
                 tempString = "ERROR";
            }
            return tempString;
        case Column::Data:
            if (dataLen < 0) dataLen = 0;
            //if (useHexMode) tempString.append("0x ");
            if (thisFrame.frameType() == QCanBusFrame::RemoteRequestFrame) {
                return tempString;
            }
            for (int i = 0; i < dataLen; i++)
            {
                if (useHexMode) tempString.append( QString::number(data[i], 16).toUpper().rightJustified(2, '0'));
                else tempString.append(QString::number(data[i], 10));
                if (!((i+1) % bytesPerLine) && (i != (dataLen - 1))) tempString.append("\n");
                else tempString.append(" ");
            }
            if (thisFrame.frameType() == thisFrame.ErrorFrame)
            {
                if (thisFrame.error() & thisFrame.TransmissionTimeoutError) tempString.append("\nTX Timeout");
                if (thisFrame.error() & thisFrame.LostArbitrationError) tempString.append("\nLost Arbitration");
                if (thisFrame.error() & thisFrame.ControllerError) tempString.append("\nController Error");
                if (thisFrame.error() & thisFrame.ProtocolViolationError) tempString.append("\nProtocol Violation");
                if (thisFrame.error() & thisFrame.TransceiverError) tempString.append("\nTransceiver Error");
                if (thisFrame.error() & thisFrame.MissingAcknowledgmentError) tempString.append("\nMissing ACK");
                if (thisFrame.error() & thisFrame.BusOffError) tempString.append("\nBus OFF");
                if (thisFrame.error() & thisFrame.BusError) tempString.append("\nBus ERR");
                if (thisFrame.error() & thisFrame.ControllerRestartError) tempString.append("\nController restart err");
                if (thisFrame.error() & thisFrame.UnknownError) tempString.append("\nUnknown error type");
            }
            //TODO: technically the actual returned bytes for an error frame encode some more info. Not interpreting it yet.

            //now, if we're supposed to interpret the data and the DBC handler is loaded then use it
            if ( (dbcHandler != nullptr) && interpretFrames && (thisFrame.frameType() == thisFrame.DataFrame) )
            {
                DBC_MESSAGE *msg = dbcHandler->findMessage(thisFrame);
                if (msg != nullptr)
                {
                    tempString.append("   <" + msg->name + ">\n");
                    if (msg->comment.length() > 1) tempString.append(msg->comment + "\n");
                    for (int j = 0; j < msg->sigHandler->getCount(); j++)
                    {                        
                        QString sigString;
                        DBC_SIGNAL* sig = msg->sigHandler->findSignalByIdx(j);

                        if ( (sig->multiplexParent == nullptr) && sig->processAsText(thisFrame, sigString))
                        {
                            tempString.append(sigString);
                            tempString.append("\n");
                            if (sig->isMultiplexor)
                            {
                                qDebug() << "Multiplexor. Diving into the tree";
                                tempString.append(sig->processSignalTree(thisFrame));
                            }
                        }
                        else if (sig->isMultiplexed && overwriteDups) //wasn't in this exact frame but is in the message. Use cached value
                        {
                            bool isInteger = false;
                            if (sig->valType == UNSIGNED_INT || sig->valType == SIGNED_INT) isInteger = true;
                            tempString.append(sig->makePrettyOutput(sig->cachedValue.toDouble(), sig->cachedValue.toLongLong(), true, isInteger));
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

bool CANFrameModel::any_filters_are_configured(void)
{
    for (auto const &val : filters)
    {
        if (val == true)
            continue;
        else
            return true;
    }
    return false;
}

bool CANFrameModel::any_busfilters_are_configured(void)
{
    for (auto const &val : busFilters)
    {
        if (val == true)
            continue;
        else
            return true;
    }
    return false;
}


void CANFrameModel::addFrame(const CANFrame& frame, bool autoRefresh = false)
{
    /*TODO: remove mutex */
    mutex.lock();
    CANFrame tempFrame;
    tempFrame = frame;

    tempFrame.setTimeStamp(QCanBusFrame::TimeStamp(0, tempFrame.timeStamp().microSeconds() - timeOffset));

    lastUpdateNumFrames++;

    //if this ID isn't found in the filters list then add it and show it by default
    if (!filters.contains(tempFrame.frameId()))
    {
        // if there are any filters already configured, leave the new filter disabled
        if (any_filters_are_configured())
            filters.insert(tempFrame.frameId(), false);
        else
            filters.insert(tempFrame.frameId(), true);
        needFilterRefresh = true;
    }

    //if this BusID isn't found in the busFilters list then add it and show it by default
    if (!busFilters.contains(tempFrame.bus))
    {
        // if there are any busFilters already configured, leave the new filter disabled
        if (any_busfilters_are_configured())
            busFilters.insert(tempFrame.bus, false);
        else
            busFilters.insert(tempFrame.bus, true);
        needFilterRefresh = true;
    }

    if (!overwriteDups)
    {
        try
        {
            frames.append(tempFrame);

            if (filters[tempFrame.frameId()] && busFilters[tempFrame.bus])
            {
                if (autoRefresh) beginInsertRows(QModelIndex(), filteredFrames.count(), filteredFrames.count());
                tempFrame.frameCount = 1;
                filteredFrames.append(tempFrame);
                if (autoRefresh) endInsertRows();
            }
        }
        catch (const std::exception& ex)
        {
            qDebug() << "addFrame failed to append. App is probably going to crash. frames.length(): " << frames.length() << " Exception: " << ex.what();
        }
    }
    else //yes, overwrite dups
    {
        bool found = false;
//        for (int i = 0; i < frames.count(); i++)
//        {
//            if ( (frames[i].frameId() == tempFrame.frameId()) && (frames[i].bus == tempFrame.bus) )
//            {
//                tempFrame.frameCount = frames[i].frameCount + 1;
//                tempFrame.timedelta = tempFrame.timeStamp().microSeconds() - frames[i].timeStamp().microSeconds();
//                frames.replace(i, tempFrame);
//                found = true;
//                break;
//            }
//        }
        for (int i = 0; i < filteredFrames.count(); i++)
        {
            if ( (filteredFrames[i].frameId() == tempFrame.frameId()) && (filteredFrames[i].bus == tempFrame.bus) )
            {
                tempFrame.frameCount = filteredFrames[i].frameCount + 1;
                tempFrame.timedelta = tempFrame.timeStamp().microSeconds() - filteredFrames[i].timeStamp().microSeconds();
                filteredFrames.replace(i, tempFrame);
                found = true;
                break;
            }
        }
        frames.append(tempFrame);
        if (!found)
        {
            //frames.append(tempFrame);
            if (filters[tempFrame.frameId()] && busFilters[tempFrame.bus])
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
                if ( (filteredFrames[j].frameId() == tempFrame.frameId()) && (filteredFrames[j].bus == tempFrame.bus) )
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
    if(frames.length() > frames.capacity() * 0.99)
    {
        mutex.lock();
        qDebug() << "Frames count: " << frames.length() << " of " << frames.capacity() << " capacity, removing first " << (int)(frames.capacity() * 0.05) << " frames";
        frames.remove(0, (int)(frames.capacity() * 0.05));
        qDebug() << "Frames removed, new count: " << frames.length();
        mutex.unlock();
    }

    if(filteredFrames.length() > filteredFrames.capacity() * 0.99)
    {
        mutex.lock();
        qDebug() << "filteredFrames count: " << filteredFrames.length() << " of " << filteredFrames.capacity() << " capacity, removing first " << (int)(filteredFrames.capacity() * 0.05) << " frames";
        filteredFrames.remove(0, (int)(filteredFrames.capacity() * 0.05));
        qDebug() << "filteredFrames removed, new count: " << filteredFrames.length();
        mutex.unlock();
    }

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

    if(overwriteDups)
    {
        recalcOverwrite();
    }
    else
    {
        QVector<CANFrame> tempContainer;
        int count = frames.count();
        for (int i = 0; i < count; i++)
        {
            if (filters[frames[i].frameId()] && busFilters[frames[i].bus])
            {
                tempContainer.append(frames[i]);
            }
        }

        mutex.lock();
        beginResetModel();
        filteredFrames.clear();
        filteredFrames.append(tempContainer);
        filteredFrames.reserve(preallocSize);
        lastUpdateNumFrames = 0;
        endResetModel();
        mutex.unlock();
    }
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
    //if (filteredFrames.count() == 0) return 0;

    //qDebug() << "Bulk refresh of " << lastUpdateNumFrames;

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
    if(filtersPersistDuringClear == false)
    {
        filters.clear();
        busFilters.clear();
    }
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
        if (!filters.contains(newFrames[i].frameId()))
        {
            filters.insert(newFrames[i].frameId(), true);
            needFilterRefresh = true;
        }
        if (filters[newFrames[i].frameId()])
        {
            busFilters.insert(newFrames[i].bus, true);
            needFilterRefresh = true;
        }
        if (filters[newFrames[i].frameId()] && busFilters[newFrames[i].bus])
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
    int64_t intTimeStamp = static_cast<int64_t> (timestamp * 1000000l);
    for (int i = 0; i < frames.count(); i++)
    {
        if ((frames[i].frameId() == ID))
        {
            if (frames[i].timeStamp().microSeconds() <= intTimeStamp) bestIndex = i;
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
    busFilters.clear();

    while (!inFile->atEnd()) {
        line = inFile->readLine().simplified();
        if (line.length() > 2)
        {
            QList<QByteArray> tokens = line.split(',');
            ID = tokens[0].toInt(nullptr, 16);
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

const QMap<int, bool>* CANFrameModel::getBusFiltersReference() const
{
    return &busFilters;
}
