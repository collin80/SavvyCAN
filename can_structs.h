#ifndef CAN_STRUCTS_H
#define CAN_STRUCTS_H

#include <QVector>
#include <stdint.h>

class CANFrame
{
public:
    int ID;
    int bus;
    bool extended;
    bool isReceived; //did we receive this or send it?
    int len;
    unsigned char data[8];
    uint64_t timestamp;
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
    int ID;
    int bus;
    bool extended;
    bool isReceived;
    int len; //# of bytes this message should have (as reported)
    int actualSize; //# we actually got
    QVector<unsigned char> data;
    uint64_t timestamp;
};

#endif // CAN_STRUCTS_H

