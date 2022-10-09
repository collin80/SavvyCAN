#ifndef LAWICELSERIAL_H
#define LAWICELSERIAL_H

#include <QSerialPort>
#include <QCanBusDevice>
#include <QThread>
#include <QTimer>

/*************/
#include <QDateTime>
/*************/

#include "canframemodel.h"
#include "canconnection.h"
#include "canconmanager.h"

class LAWICELSerial : public CANConnection
{
    Q_OBJECT

public:
    LAWICELSerial(QString portName, int serialSpeed, int lawicelSpeed);
    virtual ~LAWICELSerial();

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
    void rebuildLocalTimeBasis();
    void sendToSerial(const QByteArray &bytes);
    void sendDebug(const QString debugText);

protected:
    QTimer             mTimer;
    QThread            mThread;
    QString            mBuildLine;

    bool isAutoRestart;
    QSerialPort *serial;
    int framesRapid;
    CANFrame buildFrame;
    bool can0Enabled;
    bool can0ListenOnly;
};

#endif // LAWICELSERIAL_H
