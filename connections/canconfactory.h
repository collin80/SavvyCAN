#ifndef CANCONFACTORY_H
#define CANCONFACTORY_H

#include "canconconst.h"
#include "canconnection.h"

class CanConFactory
{
public:
    static CANConnection* create(CANCon::type, QString pPortName, QString pDriverName, int pSerialSpeed, int pBusSpeed, bool pCanFd, int pDataRate, bool pListenOnly = false, bool pActive = true);
};

#endif // CANCONFACTORY_H
