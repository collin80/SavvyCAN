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

public slots:
    /* One-shot probe: forces a disconnect+reconnect to update the connection status.
     * Called when the connection window is opened so stale CONNECTED state (e.g. after
     * a silent USB unplug on PCAN/macOS) is detected immediately. Runs in worker thread. */
    void probeConnectionState();

    /* Reconnect using stored bus config. Posted via Qt::QueuedConnection from the main-thread
     * auto-reconnect timer so it works even when the worker-thread timer is stalled. */
    void autoReconnect() override;

private slots:
    void errorReceived(QCanBusDevice::CanBusError);
    void framesWritten(qint64 count);
    void framesReceived();
    void testConnection();
    void deviceStateChanged(QCanBusDevice::CanBusDeviceState state);
    void recreateDevice();

private:
    void applyBusConfig(const CANBus &bus); // sets bitrate/FD/listenOnly params on mDev_p

protected:
    QCanBusDevice     *mDev_p = nullptr;
    uint64_t           mConnectTime = 0; // system time (µs) when device connected, for relative-timestamp devices like PCAN
    QTimer            *mTimer = nullptr;
    int                mNoFrameSeconds = 0;       // timer ticks (1 s each) since last real frame while CONNECTED
    bool               mHadFrameSinceConnect = false; // true once a valid frame arrives after connecting
    int                mReconnectAttempts = 0;    // consecutive connectDevice() calls in NOT_CONNECTED state
};


#endif // SERIALBUSCONNECTION_H
