#ifndef SocketCan_H
#define SocketCan_H

#include <QObject>
#include <QCanBus>
#include <QTimer>
#include <qthread.h>

#include "canframemodel.h"
#include "canconnection.h"


class SocketCan : public CANConnection
{
    Q_OBJECT

public:
    SocketCan(QString portName);
    virtual ~SocketCan();

protected:

    virtual void piStarted();
    virtual void piStop();
    virtual void piSetBusSettings(int pBusIdx, CANBus pBus);
    virtual bool piGetBusSettings(int pBusIdx, CANBus& pBus);
    virtual void piSuspend(bool pSuspend);
    virtual void piSendFrame(const CANFrame&) ;
    virtual void piSendFrameBatch(const QList<CANFrame>&);

    void disconnectDevice();

private slots:
    void errorReceived(QCanBusDevice::CanBusError) const;
    void framesWritten(qint64 count);
    void framesReceived();
    void testConnection();

protected:
    QCanBusDevice*     mDev_p;
    QTimer             mTimer;
};


#endif // SocketCan_H
