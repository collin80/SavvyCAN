#ifndef SOCKETCANCONNECTION_H
#define SOCKETCANCONNECTION_H

#include <QObject>
#include <QCanBus>
#include <QTimer>
#include <qthread.h>

#include "canframemodel.h"
#include "canconnection.h"


class SocketCanConnection : public CANConnection
{
    Q_OBJECT

public:
    SocketCanConnection(QString portName);
    virtual ~SocketCanConnection();

signals:
    void error(const QString &);

    void status(CANCon::status);
    void deviceInfo(int, int); //First param = driver version (or version of whatever you want), second param a status byte

    //bus number, bus speed, status (bit 0 = enabled, 1 = single wire, 2 = listen only)
    //3 = Use value stored for enabled, 4 = use value passed for single wire, 5 = use value passed for listen only
    //6 = use value passed for speed. This allows bus status to be updated but set that some things aren't really
    //being passed. Just set for things that really are being updated.
    void busStatus(int, int, int);

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


#endif // SOCKETCANCONNECTION_H
