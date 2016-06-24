#ifndef TESTCANCON_H
#define TESTCANCON_H

#include <QObject>
#include "canconconst.h"
#include "canconnection.h"

class TestCanCon: public QObject
{
    Q_OBJECT
public:
    TestCanCon(CANCon::type, QString pPortName, int pNbBus);
private:
    CANCon::type mType;
    QString      mPortName;
    int          mNbBus;

private slots:
    void create();
    void connectToDevice();
    void recvFrames();
    void suspend();
    void filter();
    void filter_data();
    void write();

private:
    bool pCreate(CANConnection*& pConn_p);
    bool pConfig(CANConnection* pConn_p);
    bool pValidateFrame(CANConnection* pConn_p, CANFrame* pCan_p);
};

#endif // TESTCANCON_H
