#include <QVariant>
#include <QDebug>
#include "snifferitem.h"


SnifferItem::SnifferItem(const CANFrame& pFrame, quint32 seq):
    mID(pFrame.frameId())
{
    const unsigned char *data = reinterpret_cast<const unsigned char *>(pFrame.payload().constData());
    int dataLen = pFrame.payload().length();

    for (int i = 0; i < dataLen; i++) {
        mNotch[i] = 0;
        mMarker.data[i] = 0;
        mMarker.dataTimestamp[i] = 0;
        if (i < dataLen) mCurrent.data[i] = data[i];
        else mCurrent.data[i] = 0;
        mCurrent.dataTimestamp[i] = seq;
    }
    mLastMarker = mMarker;
    mCurrent.len = dataLen;

    /* that's dirty */
    update(pFrame, seq, false);
    update(pFrame, seq, false); //anyone know why we're doing this twice?!
}


SnifferItem::~SnifferItem()
{

}

quint64 SnifferItem::getId() const
{
    return mID;
}

float SnifferItem::getDelta() const
{
    return ((float)(mCurrentTime-mLastTime))/1000000;
}

//Get a data byte by index 0-7 (but not more than the length of the actual frame)
int SnifferItem::getData(uchar i) const
{
    return (i >= mCurrent.len) ? -1 : mCurrent.data[i];
}

quint8 SnifferItem::getNotchPattern(uchar i) const
{
    return (i >= mCurrent.len) ? -1 : mNotch[i];
}

quint8 SnifferItem::getLastData(uchar i) const
{
    return (i >= mLast.len) ? -1 : mLast.data[i];
}

quint32 SnifferItem::getDataTimestamp(uchar i) const
{
    return (i >= mCurrent.len) ? 0 : mCurrent.dataTimestamp[i];
}

quint32 SnifferItem::getSeqInterval(uchar i) const
{
    return mCurrSeqVal - getDataTimestamp(i);
}

//Return whether a given data byte (by index 0-7) has incremented, deincremented, or stayed the same
//since the last message
//The If checks first that we aren't past the actual data length
// then checks whether lastMarker shows that some bits have changed in the previous 200ms cycle
// then we check if the byte in mNotch has bits set and if it does we say nothing changed (notched out)
dc SnifferItem::dataChange(uchar i) const
{
    if (i >= mCurrent.len) return dc::NO;

    uchar notch = mNotch[i];
    uchar byt = mCurrent.data[i];
    uchar last = mLast.data[i];
    uchar lastMark = mLastMarker.data[i];
    if( lastMark )
    {
        if (!notch) //if no notching is set
            return (byt >= last ? dc::INC : dc::DEINC);
        else //mNotch contained a bit pattern so use masks and do things more complicated
        {
            byt &= ~notch; //mask off bits that were notched
            if (byt == 0) return dc::NO; //and if result is null then nothing changed (other than maybe notched bits and they don't count)
            last &= ~notch; //need to mask last too
            if (last == byt) return dc::NO;
            return (byt >= last ? dc::INC : dc::DEINC); //then compare the masked copies to see which way the bit(s) went
        }
    }

    return dc::NO;
}



int SnifferItem::elapsed() const
{
    return mTime.elapsed();
}

//called when a new frame comes in that matches our same ID
//timeSeq is stored so we can figure out the last time a specific byte was updated
//mute is used to specify whether to mask the byte against the notching filter
//in order to hide any updates of the notched bits. This is toggleable
void SnifferItem::update(const CANFrame& pFrame, quint32 timeSeq, bool mute)
{
    unsigned char maskedCurr, maskedData;
    //qDebug() << "update with ts: " << timeSeq;
    /* copy current to last */
    mLast = mCurrent;
    mLastTime = mCurrentTime;
    mCurrSeqVal = timeSeq;

    const unsigned char *data = reinterpret_cast<const unsigned char *>(pFrame.payload().constData());
    int dataLen = pFrame.payload().length();

    /* copy new value */
    for (int i = 0; i < dataLen; i++)
    {
        maskedData = data[i];
        if (mute) maskedData &= ~mNotch[i];
        maskedCurr = mCurrent.data[i];
        if (mute) maskedCurr &= ~mNotch[i];
        if (maskedCurr != maskedData)
        {
            mCurrent.data[i] = data[i];
            mCurrent.dataTimestamp[i] = timeSeq;
        }
    }
    mCurrent.len = dataLen;
    mCurrentTime = pFrame.timeStamp().microSeconds();

    /* update marker */
    //We "OR" our stored marker with the changed bits.
    //this accumulates changed bits into the marker
    for (int i = 0 ; i < 8; i++) mMarker.data[i] |= mLast.data[i] ^ mCurrent.data[i]; //XOR causes only changed bits to be 1's
    mMarker.len  |= mLast.len ^ mCurrent.len;

    /* restart timeout */
    mTime.restart();
}

//Called in refresh from the model. Interval about 200ms currently.
//So, this means the marker only accumulates for 200ms then resets
void SnifferItem::updateMarker()
{
    mLastMarker = mMarker;
    for (int i = 0; i < 8; i++) mMarker.data[i] = 0;
}

//Notch or un-notch this snifferitem / frame
void SnifferItem::notch(bool pNotch)
{
    if(pNotch)
    {
        for (int i = 0; i < 8; i++) mNotch[i] |= mLastMarker.data[i]; //add changed bits to notch value
    }

    else
        for (int i = 0; i < 8; i++) mNotch[i] = 0;
}
