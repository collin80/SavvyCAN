#ifndef J1939_HANDLER_H
#define J1939_HANDLER_H

#include <Qt>
#include <QObject>
#include <QDebug>
#include "can_structs.h"

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

#endif // J1939_HANDLER_H
