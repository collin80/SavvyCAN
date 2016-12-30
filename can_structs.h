#ifndef CAN_STRUCTS_H
#define CAN_STRUCTS_H

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

struct CANFlt
{
    quint32 id;
    quint32 mask;
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

