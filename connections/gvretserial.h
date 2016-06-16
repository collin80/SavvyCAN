#ifndef GVRETSERIAL_H
#define GVRETSERIAL_H

#include <QSerialPort>
#include <QCanBusDevice>
#include <QThread>
#include <QTimer>

/*************/
#include <QDateTime>
/*************/


#include "canframemodel.h"
#include "canconnection.h"


namespace SERIALSTATE {

enum STATE //keep this enum synchronized with the Arduino firmware project
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
    SET_SINGLEWIRE_MODE
};

}


using namespace SERIALSTATE;
class GVRetSerial : public CANConnection
{
    Q_OBJECT

public:
    GVRetSerial(QString portName);
    virtual ~GVRetSerial();

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
    void connectDevice();
    void connectionTimeout();
    void readSerialData();
    void handleTick();

private:
    void readSettings();
    void procRXChar(unsigned char);
    void sendCommValidation();

protected:
    QTimer             mTimer;
    QThread            mThread;


    bool doValidation;
    bool gotValidated;
    bool isAutoRestart;
    bool continuousTimeSync;
    QSerialPort *serial;
    int framesRapid;
    STATE rx_state;
    int rx_step;
    CANFrame buildFrame;
    int can0Baud, can1Baud;
    bool can0Enabled, can1Enabled;
    bool can0ListenOnly, can1ListenOnly;
    int deviceBuildNum;
    int deviceSingleWireMode;
    uint64_t txTimestampBasis;
    uint32_t buildTimeBasis;
};

#endif // GVRETSERIAL_H
