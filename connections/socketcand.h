#ifndef SOCKETCAND_H
#define SOCKETCAND_H

//#include <QSerialPort>
//#include <QCanBusDevice>
#include <QTimer>
#include <QTcpSocket>
#include <QHostAddress>
//#include <QUdpSocket>

/*************/
#include <QDateTime>
/*************/

#include "canframemodel.h"
#include "canconnection.h"
#include "canconmanager.h"


namespace KAYAKSTATE {

enum MODE
{
    IDLE, //NO_BUS in socketcand doc
    BCM,
    SWITCHING2RAW,
    RAWMODE,
    ISOTP
};

}

using namespace KAYAKSTATE;
class SocketCANd : public CANConnection
{
    Q_OBJECT

public:
    SocketCANd(QString portName);
    virtual ~SocketCANd();

protected:

    virtual void piStarted();
    virtual void piStop();
    virtual void piSetBusSettings(int pBusIdx, CANBus pBus);
    virtual bool piGetBusSettings(int pBusIdx, CANBus& pBus);
    virtual void piSuspend(bool pSuspend);
    virtual bool piSendFrame(const CANFrame&);

    void disconnectDevice();

private slots:
    void connectDevice();
    void checkConnection();
    void readTCPData(int busNum);
    void invokeReadTCPData();
    void deviceConnected(int busNum);
    void switchToRawMode(int busNum);
    QString decodeFrames(QString, int busNum);

private:
    void procRXData(QString, int busNum);
    void sendBytesToTCP(const QByteArray &bytes, int busNum);
    void sendStringToTCP(const char* data, int busNum);
    void sendDebug(const QString debugText);

protected:
    QTimer mTimer;
    bool reconnecting;
    QVarLengthArray<QTcpSocket*> tcpClient;
    QHostAddress hostIP;
    int hostPort;
    QList<QString> hostCanIDs;
    int framesRapid;
    QByteArray buildData;
    QVarLengthArray<MODE> rx_state;
    CANFrame buildFrame;
    QVarLengthArray<QString> unprocessedData;
};


#endif // SOCKETCAND_H
