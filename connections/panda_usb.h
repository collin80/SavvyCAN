#ifndef PANDA_USB_H
#define PANDA_USB_H

#ifdef WITH_PANDA

#include <QTimer>
#include <QDateTime>
#include <QStringList>
#include <libusb.h>

#include "canconnection.h"
#include "canconmanager.h"

class PandaUSB : public CANConnection
{
    Q_OBJECT

public:
    PandaUSB(QString portName, int busSpeed);
    virtual ~PandaUSB();

    // Enumerate connected Panda devices; returns USB serial number strings.
    static QStringList connected();

protected:
    virtual void piStarted() override;
    virtual void piStop() override;
    virtual void piSetBusSettings(int pBusIdx, CANBus pBus) override;
    virtual bool piGetBusSettings(int pBusIdx, CANBus& pBus) override;
    virtual void piSuspend(bool pSuspend) override;
    virtual bool piSendFrame(const CANFrame& frame) override;

private slots:
    void rxPoll();
    void sendHeartbeat();

private:
    void connectDevice();
    void disconnectDevice();
    bool ctrlWrite(uint8_t request, uint16_t value, uint16_t index);
    bool ctrlRead(uint8_t request, uint16_t value, uint16_t index, uint8_t* buf, int len);
    void parseRxBuffer(const uint8_t* buf, int len);
    bool packFrame(const CANFrame& frame, QByteArray& out);
    void sendDebug(const QString& text);

    libusb_context*       mCtx = nullptr;
    libusb_device_handle* mHandle = nullptr;
    QTimer                mHeartbeatTimer;
    QTimer                mRxTimer;
    qint64                mTimeBasis = 0;
    uint8_t               mRxBuf[16384];

    // Panda USB identifiers
    static const uint16_t VID1 = 0xBBAA;
    static const uint16_t PID1 = 0xDDEE;
    static const uint16_t VID2 = 0x3801;
    static const uint16_t PID2 = 0xDDCC;

    // Maps DLC index (0-15) to byte count per the panda protocol
    static const int dlcToLen[16];

    static bool isPandaVidPid(uint16_t vid, uint16_t pid);
};

#endif // WITH_PANDA

#endif // PANDA_USB_H
