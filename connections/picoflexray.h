#ifndef PICOFLEXRAY_H
#define PICOFLEXRAY_H

#include <QStringList>
#include <QTimer>
#include "canconnection.h"
#include "picoflexrayparser.h"

struct libusb_context;
struct libusb_device_handle;

/*
 * FlexRay capture from a pico-flexray device (https://github.com/dynm/pico-flexray) over USB (libusb).
 * The device enumerates as a Panda (VID 0x3801, PID 0xddcc) with a serial number starting "picoflex" and streams
 * the FlexRay frames it captures on bulk endpoint 0x81: see PicoFlexRayParser for the record format and how frames
 * are represented. Bus 0 is FR1, bus 1 is FR2. Receive only.
 *
 * On Windows the device needs the WinUSB driver (install it once with Zadig). Built only when SavvyCAN is built
 * with libusb (HAVE_LIBUSB); otherwise the connection reports that it isn't available.
 *
 * The port is the device's USB serial number, or empty for the first pico-flexray device found.
 */
class PicoFlexRay : public CANConnection
{
    Q_OBJECT

public:
    static constexpr quint16 USB_VID = 0x3801;
    static constexpr quint16 USB_PID = 0xddcc;
    static constexpr const char *SERIAL_PREFIX = "picoflex";

    PicoFlexRay(QString serialNumber);
    virtual ~PicoFlexRay();

    static bool isAvailable(); //built with libusb
    static QStringList availableDevices(); //serial numbers of the connected pico-flexray devices

protected:
    virtual void piStarted();
    virtual void piStop();
    virtual void piSetBusSettings(int pBusIdx, CANBus pBus);
    virtual bool piGetBusSettings(int pBusIdx, CANBus& pBus);
    virtual void piSuspend(bool pSuspend);
    virtual bool piSendFrame(const CommFrame&);

private slots:
    void readDevice();
    void tryConnect();

private:
    bool openDevice();
    void closeDevice();
    void reportStatus();

    PicoFlexRayParser mParser;
    QTimer *mReadTimer = nullptr;
    QTimer *mReconnectTimer = nullptr;
    libusb_context *mContext = nullptr;
    libusb_device_handle *mHandle = nullptr;
    QByteArray mReadBuffer;
    QList<CommFrame> mFrames;
    QString mLastError;
};

#endif // PICOFLEXRAY_H
