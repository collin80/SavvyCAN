#ifndef DEVICESCANNER_H
#define DEVICESCANNER_H

#include <QString>
#include <QList>
#include <QVector>
#include "connections/canconconst.h"

/*
 * A device SavvyCAN detected on this computer, with a best-guess driver so the user can connect
 * without first knowing which connection type to pick. Produced by DeviceScanner from the sources
 * SavvyCAN can already enumerate (serial ports, QtSerialBus plugins, libusb pico-flexray, and the
 * GVRET/Kayak UDP broadcasts the connection window receives).
 */
struct DetectedDevice {
    enum Confidence {
        Certain, // the device identifies itself unambiguously (pico-flexray, a QtSerialBus device, a broadcast)
        Likely,  // a USB VID/PID or descriptor strongly suggests this driver
        Unknown  // a generic USB-serial chip shared by many boards - the user should confirm the protocol
    };

    QString displayName;              // e.g. "pico-flexray EB10…", "ESP32RET (GVRET)", "CH340 serial device"
    QString location;                 // e.g. "COM7", "PCAN_USBBUS1", "192.168.4.1", a USB serial number
    QString detail;                   // tooltip extra: VID:PID, manufacturer, etc.
    CANCon::type type = CANCon::NONE; // best-guess connection type
    QString driver;                   // QtSerialBus plugin name when type==SERIALBUS, else empty
    QString portArg;                  // value handed to NewConnectionDialog::setPortName()
    Confidence confidence = Unknown;
};

class DeviceScanner
{
public:
    // Scan every source SavvyCAN can enumerate. gvretIps / kayakHosts are the hosts already
    // discovered via UDP broadcast (passed from the connection window), listed as network devices.
    static QList<DetectedDevice> scan(const QVector<QString> &gvretIps,
                                      const QVector<QString> &kayakHosts);

    // Pure identification of a serial adapter from its USB metadata. Separated out so it can be
    // unit-tested without hardware. 0 vid/pid mean "not reported".
    static DetectedDevice identifySerial(quint16 vid, quint16 pid,
                                         const QString &portName,
                                         const QString &description,
                                         const QString &manufacturer,
                                         const QString &serialNumber);
};

#endif // DEVICESCANNER_H
