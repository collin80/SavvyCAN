#ifndef CANCONNECTION_H
#define CANCONNECTION_H

#include <Qt>
#include <QObject>
#include "can_structs.h"
#include "canframemodel.h"

class CANConnection;

class CAN_Bus
{
public:
    CAN_Bus();
    int busNum;
    int speed;
    bool listenOnly;
    bool singleWire;
    bool active; //is this bus turned on?
    CANConnection *connection;

    void setSpeed(int); // new speed
    void setListenOnly(bool); //bool for whether to only listen
    void setSingleWire(bool); //bool for whether to use single wire mode
    void setEnabled(bool); //whether this bus should be enabled or not.
    void setConnection(CANConnection *);
    void setBusNum(int);
    int getSpeed();
    int getBusNum();
    bool isListenOnly();
    bool isSingleWire();
    bool isActive();
    CANConnection *getConnection();
};


//Gentle reminder, CANConnection objects run in their own thread (each one, different thread)
//so, for the love of God, do not try to access them directly.
//Use the signal/slots system to do indirect calls.
//Please and thank you.
class CANConnection : public QObject
{
    Q_OBJECT

public:
    CANConnection(CANFrameModel *, int);
    virtual int getNumBuses();
    int getBusBase();
    virtual QString getConnTypeName();
    QString getConnPortName();

signals:
    void error(const QString &);
    void frameUpdateRapid(int);
    void frameUpdate(int);
    void connectionSuccess();
    void connectionFailure();
    void deviceInfo(int, int); //First param = driver version (or version of whatever you want), second param a status byte (listen only, single wire, etc)

public slots:
    virtual void run();
    virtual void sendFrame(const CANFrame *);
    virtual void sendFrameBatch(const QList<CANFrame> *);
    virtual void updatePortName(QString); //string version of the port to connect to. This base doesnt know a thing about this value
    virtual void stopFrameCapture(int); //pass bus number
    virtual void startFrameCapture(int); //pass bus number. Only if stopped. Defaults to started anyway
    virtual void updateBusSettings(CAN_Bus *bus); //reference to the bus that changed.

protected:
    bool quit;
    int numBuses;    
    int busBase; //first bus this class is supposed to handle
    QString portName; //for easy access later on
    QString connType; //what kind of connection this is (socketcan, kvaser, etc)
    bool isConnected; //is the whole device connected? (Is the code connected to the device itself)
    CANFrameModel *model;
};

#endif // CANCONNECTION_H
