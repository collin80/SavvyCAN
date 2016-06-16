#include <QThread>
#include "canconnection.h"

CANConnection::CANConnection(QString pPort,
                             CANCon::type pType,
                             int pNumBuses,
                             int pQueueLen,
                             bool pUseThread) :
    mQueue(),
    mNumBuses(pNumBuses),
    mPort(pPort),
    mType(pType),
    mIsCapSuspended(false),
    mStatus(CANCon::NOT_CONNECTED),
    mThread_p(NULL)
{
    qDebug() << "CANConnection()";

    /* register types */
    qRegisterMetaType<CANBus>("CANBus");
    qRegisterMetaType<CANFrame>("CANFrame");
    qRegisterMetaType<CANCon::status>("CANCon::status");

    /* set queue size */
    mQueue.setSize(pQueueLen); /*TODO add check on returned value */

    /* allocate buses */
    mBus        = new CANBus[mNumBuses];
    mConfigured = new bool[mNumBuses];

    for(int i=0 ; i<mNumBuses ; i++)
        mConfigured[i] = false;

    /* if needed, create a thread and move ourself into it */
    if(pUseThread) {
        mThread_p = new QThread();
    }
}


CANConnection::~CANConnection()
{
    qDebug() << "~CANConnection()";
    /* stop and delete thread */
    if(mThread_p) {
        mThread_p->quit();
        mThread_p->wait();
        delete mThread_p;
        mThread_p = NULL;
    }

    /* delete bus table */
    delete[] mBus;
    mBus = NULL;
    /* configured table */
    delete[] mConfigured;
    mConfigured = NULL;
    /* delete queue table */
    mQueue.setSize(0);
}


void CANConnection::start()
{
    if( mThread_p && (mThread_p != QThread::currentThread()) )
    {
        /* move ourself to the thread */
        moveToThread(mThread_p); /*TODO handle errors */
        /* connect started() */
        connect(mThread_p, SIGNAL(started()), this, SLOT(start()));
        /* start the thread */
        mThread_p->start(QThread::HighPriority);
        return;
    }

    /* in multithread case, this will be called before entering thread event loop */
    return piStarted();
}


void CANConnection::suspend(bool pSuspend)
{
    /* execute in mThread_p context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "suspend",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(bool, pSuspend));
        return;
    }

    return piSuspend(pSuspend);
}


void CANConnection::stop()
{
    /* 1) execute in mThread_p context */
    if( mThread_p && (mThread_p != QThread::currentThread()) )
    {
        /* if thread is finished, it means we call this function for the second time so we can leave */
        if( !mThread_p->isFinished() )
        {
            /* we need to call piStop() */
            QMetaObject::invokeMethod(this, "stop",
                                      Qt::BlockingQueuedConnection);
            /* 3) stop thread */
            mThread_p->quit();
            if(!mThread_p->wait()) {
                qDebug() << "can't stop thread";
            }
        }
        return;
    }

    /* 2) call piStop in mThread context */
    return piStop();
}


bool CANConnection::getBusSettings(int pBusIdx, CANBus& pBus)
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        bool ret;
        QMetaObject::invokeMethod(this, "getBusSettings",
                                  Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(bool, ret),
                                  Q_ARG(int , pBusIdx),
                                  Q_ARG(CANBus& , pBus));
        return ret;
    }

    return piGetBusSettings(pBusIdx, pBus);
}


void CANConnection::setBusSettings(int pBusIdx, CANBus pBus)
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "setBusSettings",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(int, pBusIdx),
                                  Q_ARG(CANBus, pBus));
        return;
    }

    return piSetBusSettings(pBusIdx, pBus);
}


void CANConnection::sendFrame(const CANFrame& pFrame)
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "sendFrame",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(const CANFrame&, pFrame));
        return;
    }

    return piSendFrame(pFrame);
}


void CANConnection::sendFrameBatch(const QList<CANFrame>& pFrames)
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "sendFrameBatch",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(const QList<CANFrame>&, pFrames));
        return;
    }

    return piSendFrameBatch(pFrames);
}


int CANConnection::getNumBuses() {
    return mNumBuses;
}


bool CANConnection::isConfigured(int pBusId) {
    if( pBusId < 0 || pBusId >= mNumBuses)
        return false;
    return mConfigured[pBusId];
}

void CANConnection::setConfigured(int pBusId, bool pConfigured) {
    if( pBusId < 0 || pBusId >= mNumBuses)
        return;
    mConfigured[pBusId] = pConfigured;
}


bool CANConnection::getBusConfig(int pBusId, CANBus& pBus) {
    if( pBusId < 0 || pBusId >= mNumBuses || !isConfigured(pBusId))
        return false;

    pBus = mBus[pBusId];
    return true;
}


void CANConnection::setBusConfig(int pBusId, CANBus& pBus) {
    if( pBusId < 0 || pBusId >= mNumBuses)
        return;

    mConfigured[pBusId] = true;
    mBus[pBusId] = pBus;
}


QString CANConnection::getPort() {
    return mPort;
}


LFQueue<CANFrame>& CANConnection::getQueue() {
    return mQueue;
}


CANCon::type CANConnection::getType() {
    return mType;
}


CANCon::status CANConnection::getStatus() {
    return (CANCon::status) mStatus.load();
}


void CANConnection::setStatus(CANCon::status pStatus) {
    mStatus.store(pStatus);
}

bool CANConnection::isCapSuspended() {
    return mIsCapSuspended;
}

void CANConnection::setCapSuspended(bool pIsSuspended) {
    mIsCapSuspended = pIsSuspended;
}

