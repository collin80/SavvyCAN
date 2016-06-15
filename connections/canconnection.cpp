#include "canconnection.h"

CANConnection::CANConnection(QString pPort, CANCon::type pType, int pNumBuses) :
    mQueue(),
    mNumBuses(pNumBuses),
    mPort(pPort),
    mType(pType),
    mIsCapSuspended(false),
    mStatus(CANCon::NOT_CONNECTED)
{
    qDebug() << "CANConnection()";

    /* register types */
    qRegisterMetaType<CANBus>("CANBus");
    qRegisterMetaType<CANCon::status>("CANCon::status");

    mBus        = new CANBus[mNumBuses];
    mConfigured = new bool[mNumBuses];

    for(int i=0 ; i<mNumBuses ; i++)
        mConfigured[i] = false;
}


CANConnection::~CANConnection()
{
    qDebug() << "~CANConnection()";

    delete[] mBus;
    mBus = NULL;
    delete[] mConfigured;
    mConfigured = NULL;
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

