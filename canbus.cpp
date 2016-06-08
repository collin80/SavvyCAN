#include <QObject>
#include "canbus.h"

CANBus::CANBus()
{
    speed = 250000;
    listenOnly = false;
    singleWire = false;
    active = false;
    container = Q_NULLPTR;
    busNum = 0;
}

CANBus::CANBus(const CANBus& pBus) :
    speed(pBus.speed),
    listenOnly(pBus.listenOnly),
    singleWire(pBus.singleWire),
    active(pBus.active),
    container(pBus.container), /* TODO: check if container is really needed */
    busNum(pBus.busNum) {}


void CANBus::setSpeed(int newSpeed)
{
    speed = newSpeed;
}

void CANBus::setListenOnly(bool mode)
{
    listenOnly = mode;
}

void CANBus::setSingleWire(bool mode)
{
    singleWire = mode;
}

void CANBus::setEnabled(bool mode)
{
    active = mode;
}

void CANBus::setContainer(CANConnectionContainer* pContainer)
{
    container = pContainer;
}

void CANBus::setBusNum(int num)
{
    busNum = num;
}

int CANBus::getSpeed()
{
    return speed;
}

int CANBus::getBusNum()
{
    return busNum;
}

bool CANBus::isListenOnly()
{
    return listenOnly;
}

bool CANBus::isSingleWire()
{
    return singleWire;
}

bool CANBus::isActive()
{
    return active;
}

CANConnectionContainer* CANBus::getContainer()
{
    return container;
}
