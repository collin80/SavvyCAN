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

int SnifferItem::getData(uchar i) const
{
    return (i>=mCurrent.len) ? -1 : ((uchar*) &mCurrent.data)[i];
}

dc SnifferItem::dataChange(uchar i) const
{
    if(     i<mCurrent.len &&
            ((uchar*) &mLastMarker.data)[i] &&
            !((uchar*) &mNotch)[i] )
    {
        return ((uchar*) &mCurrent.data)[i] >= ((uchar*) &mLast.data)[i] ? dc::INC : dc::DEINC;
    }

    return dc::NO;
}

int SnifferItem::elapsed() const
{
    return mTime.elapsed();
}

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
    mMarker.data |= mLast.data ^ mCurrent.data;
    mMarker.len  |= mLast.len ^ mCurrent.len;

    /* restart timeout */
    mTime.restart();
}

void SnifferItem::updateMarker()
{
    mLastMarker = mMarker;
    mMarker = {0, 0};
}

void SnifferItem::notch(bool pNotch)
{
    if(pNotch)
        mNotch |= mLastMarker.data;
    else
        mNotch = 0;
}
