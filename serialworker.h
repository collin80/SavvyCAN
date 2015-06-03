#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QDateTime>
#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include "can_structs.h"
#include "canframemodel.h"

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
    SerialWorker(CANFrameModel *model, QObject *parent = 0);
    ~SerialWorker();

signals: //we emit signals
    void error(const QString &);
    void frameUpdateTick(int, int); //update interested parties about the # of frames that have come in
    void connectionSuccess(int, int);
    void connectionFailure();
    void deviceInfo(int, int);

private slots: //we receive things in slots
    void readSerialData();    
    void connectionTimeout();
    void handleTick();

public slots:
    void setSerialPort(QSerialPortInfo*);
    void closeSerialPort();
    void sendFrame(const CANFrame *, int);
    void updateBaudRates(int, int);
    void stopFrameCapture();
    void startFrameCapture(); //only need to call this if previously stopped. Otherwise it's the default

private:
    QString portName;
    bool quit;
    bool connected;
    bool capturing;
    QSerialPort *serial;
    CANFrameModel *canModel;
    QTimer *ticker;
    QTime *elapsedTime;
    int framesPerSec;
    int gotFrames;
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
