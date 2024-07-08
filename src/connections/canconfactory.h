#ifndef CANCONFACTORY_H
#define CANCONFACTORY_H

#include "connections/canconconst.h"
#include "connections/canconnection.h"

class CanConFactory
{
public:
  static CANConnection* create(CANCon::type, QString pPortName, QString pDriverName, int pSerialSpeed, int pBusSpeed,
                               bool pCanFd, int pDataRate);
};

#endif  // CANCONFACTORY_H
