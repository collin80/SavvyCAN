#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QDateTime>
#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include "can_structs.h"
#include "canframemodel.h"

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
class SerialWorker : public QObject
{
    Q_OBJECT

public:
    SerialWorker(CANFrameModel *model, QObject *parent = 0);
    ~SerialWorker();
    void readSettings();
    void targetFrameID(int);

signals: //we emit signals
    void error(const QString &);    
    void frameUpdateRapid(int); //sent *much* more rapidly than the above signal - one param for # of frames
    void connectionSuccess(int, int);
    void connectionFailure();
    void deviceInfo(int, int);
    void gotTargettedFrame(int);

private slots: //we receive things in slots
    void readSerialData();    
    void connectionTimeout();
    void handleTick();
    void handleReconnect();

public slots:
    void run();
    void setSerialPort(QSerialPortInfo*);
    void closeSerialPort();
    void sendFrame(const CANFrame *, int);
    void sendFrameBatch(const QList<CANFrame> *);
    void updateBaudRates(int, int);
    void stopFrameCapture();
    void startFrameCapture(); //only need to call this if previously stopped. Otherwise it's the default

private:
    QString portName;
    bool quit;
    bool connected;
    bool capturing;
    bool doValidation;
    bool gotValidated;
    bool isAutoRestart;
    QSerialPort *serial;
    QSerialPortInfo *currentPort;
    CANFrameModel *canModel;
    QTimer *ticker;    
    QMutex sendBulkMutex;        
    int framesRapid;
    int targetID;
    STATE rx_state;
    int rx_step;
    CANFrame *buildFrame;
    int can0Baud, can1Baud;
    bool can0Enabled, can1Enabled;
    int deviceBuildNum;
    int deviceSingleWireMode;
    uint64_t txTimestampBasis;

    void procRXChar(unsigned char);
    void sendCommValidation();
};

#endif // SERIALTHREAD_H
