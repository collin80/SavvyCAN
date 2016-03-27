#ifndef CANCONNECTIONCONTAINER_H
#define CANCONNECTIONCONTAINER_H

#include "canconnection.h"
#include <Qt>
#include <QThread>
#include "mainwindow.h"

class CANConnectionContainer : public QObject
{
    Q_OBJECT
public:
    CANConnectionContainer(CANConnection *conn);
    ~CANConnectionContainer();

    CANConnection* getRef();

private:
    CANConnection *connection;
    QThread *thread;
};

#endif // CANCONNECTIONCONTAINER_H
