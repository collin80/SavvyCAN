#ifndef CANLOGSERVER_H
#define CANLOGSERVER_H

#include <stdio.h>

#include <QCanBusDevice>
#include <QThread>
#include <QTimer>
#include <QTcpSocket>

/*************/
#include <QDateTime>
/*************/

#include "canframemodel.h"
#include "canconnection.h"
#include "canconmanager.h"

class CanLogServer : public CANConnection
{
    Q_OBJECT

public:
    CanLogServer(QString serverAddress);
    virtual ~CanLogServer();

// Interface
protected:
    virtual void piStarted();
    virtual void piStop();
    virtual void piSetBusSettings(int pBusIdx, CANBus pBus);
    virtual bool piGetBusSettings(int pBusIdx, CANBus& pBus);
    virtual void piSuspend(bool pSuspend);
    virtual bool piSendFrame(const CANFrame&);

private slots:
    void readNetworkData();
    void networkConnected();
    void networkDisconnected();

// Utility
private:
    void readSettings();
    void connectToDevice();
    void disconnectFromDevice();
    void heartbeat();

// Attributes
protected:
     QTcpSocket *m_ptcpSocket = nullptr;
     QString m_qsAddress;

//    QUdpSocket *_udpClient;

//    QTimer  *_heartbeatTimer;
};
#endif // CANLOGSERVER_H
