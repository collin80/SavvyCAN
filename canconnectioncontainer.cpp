#include "canconnectioncontainer.h"

CANConnectionContainer::CANConnectionContainer(CANConnection *conn)
{
    thread = new QThread();
    connection = conn;
    connection->moveToThread(thread);    
}

CANConnectionContainer::~CANConnectionContainer()
{
    //have to stop the actual execution first before deleting
    delete thread;
    delete connection;
}
