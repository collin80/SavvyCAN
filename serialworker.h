#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QDateTime>
#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include "can_structs.h"
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
class SerialWorker : public CANConnection
{
    Q_OBJECT

public:
    SerialWorker(CANFrameModel *, int);
    ~SerialWorker();
    void readSettings();
    int getNumBuses();
    QString getConnTypeName() override;

private slots: //we receive things in slots
    void readSerialData();    
    void connectionTimeout();
    void handleTick();
    void handleReconnect();

public slots:
    void run() override;
    void setSerialPort(QSerialPortInfo*);
    void closeSerialPort();
    void sendFrame(const CANFrame *) override;
    void sendFrameBatch(const QList<CANFrame> *) override;
    void updateBaudRates(unsigned int, unsigned int);
    //void stopFrameCapture(int) override;
    //void startFrameCapture(int)  override;
    void updatePortName(QString) override; //string version of the port to connect to. This base doesnt know a thing about this value
    void updateBusSettings(CANBus) override;

private:    
    bool doValidation;
    bool gotValidated;
    bool isAutoRestart;
    bool continuousTimeSync;
    QSerialPort *serial;
    QSerialPortInfo *currentPort;
    QTimer *ticker;    
    QMutex sendBulkMutex;        
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

    void procRXChar(unsigned char);
    void sendCommValidation();
};

#endif // SERIALTHREAD_H
