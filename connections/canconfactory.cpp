#include <QString>
#include <QSerialPortInfo>
#include "canconfactory.h"
#include "serialbusconnection.h"
#include "gvretserial.h"
#include "mqtt_bus.h"
#include "socketcand.h"
#include "lawicel_serial.h"
#include "canserver.h"
#include "canlogserver.h"
#include "picoflexray.h"

using namespace CANCon;

// A GVRET "port" is either a serial port or a network address. Port names on macOS contain a dot
// (cu.usbmodem1101, tty.usbserial-0001), so the dot alone cannot decide it: a name the system lists
// as a serial port, or a device path, is always serial.
static bool isSerialPortName(const QString &name)
{
    if (name.startsWith("/")) return true;  // device path or link, e.g. /dev/cu.usbmodem1101
    if (name.contains("tty") || name.contains("serial")) return true;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports)
    {
        if (port.portName() == name || port.systemLocation() == name) return true;
    }
    return false;
}

CANConnection* CanConFactory::create(type pType, QString pPortName, QString pDriverName, int pSerialSpeed, int pBusSpeed, bool pCanFd, int pDataRate, bool pListenOnly, bool pActive)
{
    switch(pType) {
    case SERIALBUS:
      return new SerialBusConnection(pPortName, pDriverName, pBusSpeed, pDataRate, pCanFd, pListenOnly, pActive);
    case GVRET_SERIAL:
        if(pPortName.contains(".") && !isSerialPortName(pPortName))
        return new GVRetSerial(pPortName, true);
        else
        return new GVRetSerial(pPortName, false);
    case REMOTE:
        return new GVRetSerial(pPortName, true);  //it's a special case of GVRET connected over TCP/IP so it uses the same class
    case LAWICEL:
        return new LAWICELSerial(pPortName, pSerialSpeed, pBusSpeed, pCanFd, pDataRate);
    case KAYAK:
        return new SocketCANd(pPortName);
    case MQTT:
        return new MQTT_BUS(pPortName);
    case CANSERVER:
        return new CANserver(pPortName);
    case CANLOGSERVER:
        return new CanLogServer(pPortName);
    case PICOFLEXRAY:
        return new PicoFlexRay(pPortName);
    default: {}
    }

    return nullptr;
}
