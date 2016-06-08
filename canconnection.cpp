#include "canconnection.h"



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

void CANConnection::updateBusSettings(CANBus bus)
{

}
