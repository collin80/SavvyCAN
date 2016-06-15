#include <QString>
#include "canconfactory.h"
#include "socketcan.h"
#include "gvretserial.h"

using namespace CANCon;

CANConnection* CanConFactory::create(type pType, QString pPortName)
{
    switch(pType) {
        case SOCKETCAN:
            return new SocketCanConnection(pPortName);
        case GVRET_SERIAL:
            return new GVRetSerial(pPortName);
        default: {}
    }

    return NULL;
}
