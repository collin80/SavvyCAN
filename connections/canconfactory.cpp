#include <QString>
#include "canconfactory.h"
#include "serialbusconnection.h"
#include "gvretserial.h"
#include "mqtt_bus.h"
#include "socketcand.h"
#include "lawicel_serial.h"

using namespace CANCon;

CANConnection* CanConFactory::create(type pType, QString pPortName, QString pDriverName, int pSerialSpeed, int pBusSpeed)
{
    switch(pType) {
    case SERIALBUS:
        return new SerialBusConnection(pPortName, pDriverName);
    case GVRET_SERIAL:
        if(pPortName.contains("."))
        return new GVRetSerial(pPortName, true);
        else
        return new GVRetSerial(pPortName, false);
    case REMOTE:
        return new GVRetSerial(pPortName, true);  //it's a special case of GVRET connected over TCP/IP so it uses the same class
    case LAWICEL:
        return new LAWICELSerial(pPortName, pSerialSpeed, pBusSpeed);
    case KAYAK:
        return new SocketCANd(pPortName);
    case MQTT:
        return new MQTT_BUS(pPortName);
    default: {}
    }

    return nullptr;
}
