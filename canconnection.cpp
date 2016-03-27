#include "canconnection.h"

CAN_Bus::CAN_Bus()
{
    speed = 250000;
    listenOnly = false;
    singleWire = false;
    active = false;
    connection = NULL;
    busNum = 0;
}

void CAN_Bus::setSpeed(int newSpeed)
{
    speed = newSpeed;
}

void CAN_Bus::setListenOnly(bool mode)
{
    listenOnly = mode;
}

void CAN_Bus::setSingleWire(bool mode)
{
    singleWire = mode;
}

void CAN_Bus::setEnabled(bool mode)
{
    active = mode;
}

void CAN_Bus::setConnection(CANConnection *conn)
{
    connection = conn;
}

void CAN_Bus::setBusNum(int num)
{
    busNum = num;
}

int CAN_Bus::getSpeed()
{
    return speed;
}

int CAN_Bus::getBusNum()
{
    return busNum;
}

bool CAN_Bus::isListenOnly()
{
    return listenOnly;
}

bool CAN_Bus::isSingleWire()
{
    return singleWire;
}

bool CAN_Bus::isActive()
{
    return active;
}

CANConnection* CAN_Bus::getConnection()
{
    return connection;
}

CANConnection::CANConnection(CANFrameModel *pModel, int base)
{
    model = pModel;
    numBuses = getNumBuses();
    busBase = base;
}

int CANConnection::getNumBuses()
{
    return 1;
}

int CANConnection::getBusBase()
{
    return busBase;
}

QString CANConnection::getConnTypeName()
{
    return QString("Generic");
}

QString CANConnection::getConnPortName()
{
    return portName;
}

void CANConnection::run()
{

}

void CANConnection::sendFrame(const CANFrame * frame)
{

}

void CANConnection::sendFrameBatch(const QList<CANFrame> *frames)
{

}

void CANConnection::updatePortName(QString portName)
{
    this->portName = portName;
}

void CANConnection::stopFrameCapture(int bus)
{

}

void CANConnection::startFrameCapture(int bus)
{

}

void CANConnection::updateBusSettings(CAN_Bus *bus)
{

}
