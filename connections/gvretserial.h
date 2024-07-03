#ifndef GVRETSERIAL_H
#define GVRETSERIAL_H

#include <QSerialPort>
#include <QCanBusDevice>
#include <QThread>
#include <QTimer>
#include <QTcpSocket>
#include <QUdpSocket>

/*************/
#include <QDateTime>
/*************/

#include "canframemodel.h"
#include "canconnection.h"
#include "canconmanager.h"

namespace SERIALSTATE {

enum STATE
{
    IDLE,
    GET_COMMAND,
    BUILD_CAN_FRAME,
    TIME_SYNC,
    GET_DIG_INPUTS,
    GET_ANALOG_INPUTS,
    SET_DIG_OUTPUTS,
    SETUP_CANBUS,
    GET_CANBUS_PARAMS,
    GET_DEVICE_INFO,
    SET_SINGLEWIRE_MODE,
    GET_NUM_BUSES,
    GET_EXT_BUSES,
    BUILD_FD_FRAME,
    GET_FD_SETTINGS
};

}

using namespace SERIALSTATE;
class GVRetSerial : public CANConnection
{
    Q_OBJECT

public:
    GVRetSerial(QString portName, bool useTcp);
    virtual ~GVRetSerial();

protected:

    virtual void piStarted();
    virtual void piStop();
    virtual void piSetBusSettings(int pBusIdx, CANBus pBus);
    virtual bool piGetBusSettings(int pBusIdx, CANBus& pBus);
    virtual void piSuspend(bool pSuspend);
    virtual bool piSendFrame(const CANFrame&) ;

    void disconnectDevice();

public slots:
    void debugInput(QByteArray bytes);

private slots:
    void connectDevice();
    void connectionTimeout();
    void readSerialData();
    void serialError(QSerialPort::SerialPortError err);
    void deviceConnected();
    void handleTick();

private:
    void readSettings();
    void procRXChar(unsigned char);
    void sendCommValidation();
    void rebuildLocalTimeBasis();
    void sendToSerial(const QByteArray &bytes);
    void sendDebug(const QString debugText);

protected:
    QTimer             mTimer;
    QThread            mThread;

    bool doValidation;
    int validationCounter;
    bool isAutoRestart;
    bool continuousTimeSync;
    bool useTcp;
    bool espSerialMode; //special serial mode for ESP32 based boards - no flow control and much slower serial baud speed
    QSerialPort *serial;
    QTcpSocket *tcpClient;
    QUdpSocket *udpClient;
    int framesRapid;
    STATE rx_state;
    int rx_step;
    CANFrame buildFrame;
    qint64 buildTimestamp;
    quint32 buildId;
    QByteArray buildData;
    int can0Baud, can1Baud, swcanBaud, lin1Baud, lin2Baud;
    bool can0Enabled, can1Enabled, swcanEnabled, lin1Enabled, lin2Enabled;
    bool can0ListenOnly, can1ListenOnly, swcanListenOnly;
    int deviceBuildNum;
    int deviceSingleWireMode;
    uint32_t buildTimeBasis;
    int32_t timeBasis;
    uint64_t lastSystemTimeBasis;
    uint64_t timeAtGVRETSync;
};

#endif // GVRETSERIAL_H
