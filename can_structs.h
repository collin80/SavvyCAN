#ifndef CAN_STRUCTS_H
#define CAN_STRUCTS_H

#include <QObject>
#include <QVector>
#include <stdint.h>
#include <QCanBusFrame>

//Now inherits from the built-in CAN frame class from Qt. This should be more future proof and easier to integrate with other code

struct CANFrame : public QCanBusFrame
{
public:
    int bus;
    bool isReceived; //did we receive this or send it?
    uint64_t timedelta;
    uint32_t frameCount; //used in overwrite mode

    friend bool operator<(const CANFrame& l, const CANFrame& r)
    {
        qint64 lStamp = l.timeStamp().seconds() * 1000000 + l.timeStamp().microSeconds();
        qint64 rStamp = r.timeStamp().seconds() * 1000000 + r.timeStamp().microSeconds();
        return lStamp < rStamp;
    }

    CANFrame()
    {
        setFrameId(0);
        bus = 0;
        setExtendedFrameFormat(false);
        setFrameType(QCanBusFrame::DataFrame);
        isReceived = true;
        timedelta = 0;
        frameCount = 1;
    }
};

class CANFltObserver
{
public:
    quint32 id;
    quint32 mask;
    QObject * observer; //used to target the specific object that setup this filter

    bool operator ==(const CANFltObserver &b) const
    {
        if ( (id == b.id) && (mask == b.mask) && (observer == b.observer) ) return true;

        return false;
    }
};

#endif // CAN_STRUCTS_H

