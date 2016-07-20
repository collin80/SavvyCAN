#ifndef SNIFFERITEM_H
#define SNIFFERITEM_H

#include <QVariant>
#include <QTime>
#include "can_structs.h"

struct fstCan
{
    quint64 data;
    int len;
};

enum dc
{
    NO,
    INC,
    DEC
};


class SnifferItem
{
public:
    explicit SnifferItem(const CANFrame& pFrame);
    virtual ~SnifferItem();

    quint64 getId() const;
    float getDelta() const;
    int getData(uchar i) const;
    dc dataChange(uchar) const;
    int elapsed() const;
    void update(const CANFrame& pFrame);
    void updateMarker();
    void notch(bool);
private:
    quint64         mID;
    struct fstCan   mLast;
    struct fstCan   mCurrent;
    struct fstCan   mLastMarker;
    struct fstCan   mMarker;
    quint64         mNotch;
    quint64         mLastTime;
    quint64         mCurrentTime;

    QTime           mTime;
};

#endif // SNIFFERITEM_H
