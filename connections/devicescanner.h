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
    QString labelKey;                 // matches CANConnection::getLabelKey(): used to look up the user's saved label
    bool hasUserLabel = false;        // displayName is the user's saved label (don't overwrite it on probe)
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

    // Opt-in active probe: briefly open the serial port and run the GVRET handshake
    // (0xF1 0x07 GET_DEVICE_INFO, 0xF1 0x0C GET_NUM_BUSES, 0xF1 0x16 GET_FD). Returns true if a
    // GVRET device answered; `details` gets a human-readable summary (firmware build, bus count,
    // per-bus CAN FD capability + data rate) and `anyFd` is set if any bus can do CAN FD. Fails
    // cleanly if the port is busy / not GVRET. Only called on explicit user action, never during
    // the passive scan (which never opens a port).
    static bool probeGvret(const QString &portName, QString &details, bool &anyFd);
};

#endif // DEVICESCANNER_H
