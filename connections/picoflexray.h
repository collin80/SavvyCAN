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
 * the FlexRay frames it captures on bulk endpoint 0x81: see PicoFlexRayParser for the record formats and how frames
 * are represented. On connecting it asks for record format 2 (device timestamps, CRC flags, CRC-failed frames);
 * stock firmware refuses and keeps sending format 1. Bus 0 is FR1, bus 1 is FR2. Sending: see piSendFrame().
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

    //Vendor bulk-OUT ops the firmware understands (see pico-flexray src/panda_usb.c handle_vendor_out_payload).
    static constexpr unsigned char VENDOR_OUT_OVERRIDE = 0x90; //submit a replacement slice for an injection rule
    static constexpr unsigned char VENDOR_OUT_ENABLE = 0x91;   //turn the injector on or off globally

    /*
     * Builds the op-0x90 bulk-OUT payload that submits frame as a replacement slice, or an empty array (with the
     * reason in error, if given) when the frame can't be injected. This is exactly what piSendFrame writes to the
     * device, so it is what the tests check.
     *
     * The frame's slot ID becomes the rule target_id, payload byte 0 (the cycle count on capture) becomes the
     * rule cycle_base the firmware matches on, and the rest of the payload (payload[1..]) is the replacement data:
     * it maps onto the slot's payload bytes, and the firmware splices the rule's replace window out of it into the
     * slot's cached frame. The firmware expects the slice to start with an AUTOSAR E2E CRC-8 (init 0xf1) over that
     * data as a transport check, which we prepend here (it is a separate byte, not a slot byte).
     *
     * SavvyCAN has no copy of the firmware's rule table, so it cannot validate the length: payload[1..] must be
     * replace_offset + replace_len bytes for the target rule, and only the replace_len bytes inside the window are
     * actually transmitted (the rest are positional padding the firmware ignores). A wrong length, slot ID or
     * cycle base is silently dropped by the device.
     */
    static QByteArray overridePayloadFor(const CommFrame &frame, QString *error = nullptr);

    //op-0x91 payload: [0x91][enabled]
    static QByteArray injectorEnablePayload(bool enabled);

    //AUTOSAR E2E Profile CRC-8 (polynomial 0x1D, MSB-first, no final XOR), matching the device firmware.
    static quint8 e2eCrc8(const unsigned char *data, int len, quint8 init);

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
    bool vendorWrite(const QByteArray &data, const char *what); //bulk-OUT write to the injector endpoint
    void applyInjectorEnable(); //sync the injector on/off state to the bus listen-only settings

    PicoFlexRayParser mParser;
    QTimer *mReadTimer = nullptr;
    QTimer *mReconnectTimer = nullptr;
    libusb_context *mContext = nullptr;
    libusb_device_handle *mHandle = nullptr;
    QByteArray mReadBuffer;
    QList<CommFrame> mFrames;
    QString mLastError;
    bool mRecordFormat2 = false; //the firmware accepted record format 2
    bool mInjectorEnabled = false; //true once a bus is taken out of listen-only; sent to the device as op 0x91
};

#endif // PICOFLEXRAY_H
