#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include "can_structs.h"

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
    GET_DEVICE_INFO
};

class SerialWorker : public QObject
{
    Q_OBJECT

public:
    SerialWorker(QObject *parent = 0);
    ~SerialWorker();

signals: //we emit signals
    void error(const QString &);
    void receivedFrame(CANFrame *);
    void connectionSuccess(int, int);
    void connectionFailure();
    void deviceInfo(int, int);

private slots: //we receive things in slots
    void readSerialData();    
    void connectionTimeout();

public slots:
    void setSerialPort(QSerialPortInfo*);
    void sendFrame(const CANFrame *, int);
    void updateBaudRates(int, int);

private:
    QString portName;
    bool quit;
    bool connected;
    QSerialPort *serial;
    STATE rx_state;
    int rx_step;
    CANFrame *buildFrame;
    int can0Baud, can1Baud;
    bool can0Enabled, can1Enabled;
    int deviceBuildNum;
    int deviceSingleWireMode;

    void procRXChar(unsigned char);
};

#endif // SERIALTHREAD_H
