#ifndef CAN_STRUCTS_H
#define CAN_STRUCTS_H

struct CANFrame
{
public:
    int ID;
    int bus;
    bool extended;
    int len;
    unsigned char data[8];
    long long timestamp;
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
#endif // CAN_STRUCTS_H

