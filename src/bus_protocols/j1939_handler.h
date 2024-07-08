#ifndef J1939_HANDLER_H
#define J1939_HANDLER_H

#include "main/can_structs.h"

#include <QDebug>
#include <QObject>
#include <Qt>

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

#endif  // J1939_HANDLER_H
