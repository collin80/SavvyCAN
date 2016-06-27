#include <QObject>
#include "canbus.h"

CANBus::CANBus()
{
    speed = 250000;
    listenOnly = false;
    singleWire = false;
    active = false;
}


CANBus::CANBus(const CANBus& pBus) :
    speed(pBus.speed),
    listenOnly(pBus.listenOnly),
    singleWire(pBus.singleWire),
    active(pBus.active) {}


bool CANBus::operator==(const CANBus& bus) const{
    return  speed == bus.speed &&
            listenOnly == bus.listenOnly &&
            singleWire == bus.singleWire &&
            active == bus.active;
}


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

int CANBus::getSpeed()
{
    return speed;
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

