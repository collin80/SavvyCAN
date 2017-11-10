#ifndef CAN_STRUCTS_H
#define CAN_STRUCTS_H

#include <QObject>
#include <QVector>
#include <stdint.h>

struct CANFrame
{
public:
    uint32_t ID;
    uint32_t bus;
    bool extended;
    bool isReceived; //did we receive this or send it?
    uint32_t len;
    unsigned char data[8];
    uint64_t timestamp;

    friend bool operator<(const CANFrame& l, const CANFrame& r)
    {
        return l.timestamp < r.timestamp;
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

