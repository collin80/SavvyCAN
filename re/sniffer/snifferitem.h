#ifndef SNIFFERITEM_H
#define SNIFFERITEM_H

#include <QVariant>
#include <QTime>
#include "can_structs.h"

struct fstCan
{
    quint8 data[8];
    quint32 dataTimestamp[8];
    int len;
};

enum dc
{
    NO,
    INC,
    DEINC
};


class SnifferItem
{
public:
    explicit SnifferItem(const CANFrame& pFrame, quint32 seq);
    virtual ~SnifferItem();

    quint64 getId() const;
    float getDelta() const;
    int getData(uchar i) const;
    quint32 getDataTimestamp(uchar i) const;
    dc dataChange(uchar) const;
    int elapsed() const;
    void update(const CANFrame& pFrame, quint32 timeSeq);
    void updateMarker();
    void notch(bool);

private:
    quint32         mID;
    struct fstCan   mLast;
    struct fstCan   mCurrent;
    struct fstCan   mLastMarker;
    struct fstCan   mMarker;
    quint8          mNotch[8];
    quint64         mLastTime;
    quint64         mCurrentTime;

    QTime           mTime;
};

#endif // SNIFFERITEM_H
