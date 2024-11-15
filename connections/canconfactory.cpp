#include <QString>
#include "canconfactory.h"
#include "serialbusconnection.h"
#include "gvretserial.h"
#include "mqtt_bus.h"
#include "socketcand.h"
#include "lawicel_serial.h"
#include "canserver.h"
#include "canlogserver.h"
#include "gs_usb_driver/gs_usb.h"

using namespace CANCon;

CANConnection* CanConFactory::create(type pType, QString pPortName, QString pDriverName, int pSerialSpeed, int pBusSpeed, bool pCanFd, int pDataRate, int pSamplePoint)
{
    switch(pType) {
    case SERIALBUS:
        return new SerialBusConnection(pPortName, pDriverName);
    case GVRET_SERIAL:
        if(pPortName.contains(".") && !pPortName.contains("tty") && !pPortName.contains("serial"))
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
#ifdef GS_USB_DRIVER_ENABLED
    case GSUSB:
        {
            GSUSBDevice* device = new GSUSBDevice(pPortName, pBusSpeed, pSamplePoint);
            if (!device->isDeviceConnected()) {
                delete device;
                return nullptr;
            } else {
                return device;
            }
        }
#endif
    default: {}
    }

    return nullptr;
}
