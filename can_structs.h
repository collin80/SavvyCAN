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
};

class CANFlt
{
public:
    quint32 id;
    quint32 mask;
    QObject * observer; //used to target the specific object that setup this filter

    bool operator ==(const CANFlt &b) const
    {
        if ( (id == b.id) && (mask == b.mask) && (observer == b.observer) ) return true;

        return false;
    }
};

struct J1939ID
{
public:
    int src;
    int dest;
    int pgn;
    int pf;
    int ps;
    int priority;
    bool isBroadcast;
};

//the same as the CANFrame struct but with arbitrary data size.
struct ISOTP_MESSAGE
{
public:
    uint32_t ID;
    int bus;
    bool extended;
    bool isReceived;
    int len; //# of bytes this message should have (as reported)
    int actualSize; //# we actually got
    QVector<unsigned char> data;
    uint64_t timestamp;
};

#endif // CAN_STRUCTS_H

