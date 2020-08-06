#include <QString>
#include "canconfactory.h"
#include "serialbusconnection.h"
#include "gvretserial.h"
#include "mqtt_bus.h"

using namespace CANCon;

CANConnection* CanConFactory::create(type pType, QString pPortName, QString pDriverName)
{
    switch(pType) {
    case SERIALBUS:
        return new SerialBusConnection(pPortName, pDriverName);
    case GVRET_SERIAL:
        return new GVRetSerial(pPortName, false);
    case REMOTE:
        return new GVRetSerial(pPortName, true);  //it's a special case of GVRET connected over TCP/IP so it uses the same class
    case MQTT:
        return new MQTT_BUS(pPortName);
    default: {}
    }

    return nullptr;
}
