#ifndef CANCONNECTIONCONTAINER_H
#define CANCONNECTIONCONTAINER_H

#include "canconnection.h"
#include <QThread>

class CANConnectionContainer
{
public:
    CANConnectionContainer(CANConnection *conn);
    ~CANConnectionContainer();

    CANConnection* getRef();


private:
    CANConnection *connection;
    QThread *thread;
};

#endif // CANCONNECTIONCONTAINER_H
