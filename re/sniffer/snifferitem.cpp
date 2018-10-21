#include <QVariant>
#include <QDebug>
#include "snifferitem.h"


SnifferItem::SnifferItem(const CANFrame& pFrame):
    mNotch(0),
    mID(pFrame.ID)
{
    mMarker = {0,0};
    mLastMarker = {0,0};
    /* that's dirty */
    update(pFrame);
    update(pFrame);
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
    return (i >= mCurrent.len) ? -1 : ((uchar*) &mCurrent.data)[i];
}

//Return whether a given data byte (by index 0-7) has incremented, deincremented, or stayed the same
//since the last message
//The If checks first that we aren't past the actual data length
// then checks whether lastMarker shows that some bits have changed in the previous 200ms cycle
// then we check if the byte in mNotch has bits set and if it does we say nothing changed (notched out)
dc SnifferItem::dataChange(uchar i) const
{
    if (i >= mCurrent.len) return dc::NO;

    uchar notch = ((uchar*) &mNotch)[i];
    uchar byt = ((uchar*) &mCurrent.data)[i];
    uchar last = ((uchar*) &mLast.data)[i];
    uchar lastMark = ((uchar*) &mLastMarker.data)[i];
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
void SnifferItem::update(const CANFrame& pFrame)
{
    /* copy current to last */
    mLast = mCurrent;
    mLastTime = mCurrentTime;

    /* copy new value */
    memcpy(&mCurrent.data, pFrame.data, 8);
    mCurrent.len = pFrame.len;
    mCurrentTime = pFrame.timestamp;

    /* update marker */
    //We "OR" our stored marker with the changed bits.
    //this accumulates changed bits into the marker
    mMarker.data |= mLast.data ^ mCurrent.data; //XOR causes only changed bits to be 1's
    mMarker.len  |= mLast.len ^ mCurrent.len;

    /* restart timeout */
    mTime.restart();
}

//Called in refresh from the model. Interval about 200ms currently.
//So, this means the marker only accumulates for 200ms then resets
void SnifferItem::updateMarker()
{
    mLastMarker = mMarker;
    mMarker = {0, 0};
}

//Notch or un-notch this snifferitem / frame
void SnifferItem::notch(bool pNotch)
{
    if(pNotch)
        mNotch |= mLastMarker.data; //add changed bits to notch value
    else
        mNotch = 0;
}
