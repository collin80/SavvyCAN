#include "canconmanager.h"


CANConManager* CANConManager::mInstance = NULL;

CANConManager* CANConManager::getInstance()
{
    if(!mInstance)
        mInstance = new CANConManager();

    return mInstance;
}


CANConManager::CANConManager(QObject *parent): QObject(parent)
{
    connect(&mTimer, SIGNAL(timeout()), this, SLOT(refreshCanList()));
    mTimer.setInterval(500); /*tick twice a second */
    mTimer.setSingleShot(false);
    mTimer.start();
}


CANConManager::~CANConManager()
{
    mTimer.stop();
    mInstance = NULL;
}


void CANConManager::add(CANConnection* pConn_p)
{
    connect(pConn_p, SIGNAL(notify()), this, SLOT(refreshCanList()));
    mConns.append(pConn_p);
}


void CANConManager::remove(CANConnection* pConn_p)
{
    disconnect(pConn_p, 0, this, 0);
    mConns.removeOne(pConn_p);
}


void CANConManager::refreshCanList()
{
    QObject* sender_p = QObject::sender();

    if( sender_p != &mTimer)
    {
        /* if we are not the sender, the signal is coming from a connection */
        /* refresh only the given connection */
        if(mConns.contains((CANConnection*) sender_p))
            refreshConnection((CANConnection*)sender_p);
    }
    else
    {
        foreach (CANConnection* conn_p, mConns)
            refreshConnection((CANConnection*)conn_p);
    }
}


QList<CANConnection*>& CANConManager::getConnections()
{
    return mConns;
}


CANConnection* CANConManager::getByName(const QString& pName) const
{
    foreach(CANConnection* conn_p, mConns)
    {
        if(conn_p->getPort() == pName)
            return conn_p;
    }

    return NULL;
}


void CANConManager::refreshConnection(CANConnection* pConn_p)
{
    CANFrame* frame_p = NULL;
    QVector<CANFrame> frames;

    while( (frame_p = pConn_p->getQueue().peek() ) ) {
        frames.append(*frame_p);
        pConn_p->getQueue().dequeue();
    }

    if(frames.size())
        emit framesReceived(pConn_p, frames);
}
