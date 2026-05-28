#ifndef SERIALBUSCONNECTION_H
#define SERIALBUSCONNECTION_H

#include "canconnection.h"
#include "canframemodel.h"

#include <QCanBusDevice>
#include <QTimer>

/*
QCanBusDevice::UserKey
0x00000001 - enable silent mode
0x00000002 - enable loopback mode
0x00000004 - disable auto retransmissions
0x00000008 - enable terminator
0x00000010 - enable automatic bus off recovery
*/

#define EN_SILENT_MODE                  0x00000001
#define EN_LOOPBACK_MODE                0x00000002
#define DIS_AUTO_RETRANSMISSIONS        0x00000004
#define EN_TERMINATOR                   0x00000008
#define EN_AUTOMATIC_BUSOFF_RECOVERY    0x00000010

class SerialBusConnection : public CANConnection
{
    Q_OBJECT

public:
  SerialBusConnection(QString portName, QString driverName, int pBusSpeed,
		      int pDataRate, bool pCanFd, bool pListenOnly = false, bool pActive = true);
    virtual ~SerialBusConnection();

protected:

    virtual void piStarted() override;
    virtual void piStop() override;
    virtual void piSetBusSettings(int pBusIdx, CANBus pBus) override;
    virtual bool piGetBusSettings(int pBusIdx, CANBus& pBus) override;
    virtual void piSuspend(bool pSuspend) override;
    virtual bool piSendFrame(const CommFrame&) override;

    void disconnectDevice();

private slots:
    void errorReceived(QCanBusDevice::CanBusError) const;
    void framesWritten(qint64 count);
    void framesReceived();
    void testConnection();
    void deviceStateChanged(QCanBusDevice::CanBusDeviceState state);

protected:
    QCanBusDevice     *mDev_p = nullptr;
    uint64_t           mConnectTime = 0; // system time (µs) when device connected, for relative-timestamp devices like PCAN
    QTimer            *mTimer = nullptr;
    int                mNoFrameSeconds = 0;       // timer ticks (1 s each) since last real frame while CONNECTED
    bool               mHadFrameSinceConnect = false; // true once a valid frame arrives after connecting
    int                mReconnectAttempts = 0;    // consecutive connectDevice() calls in NOT_CONNECTED state
};


#endif // SERIALBUSCONNECTION_H
