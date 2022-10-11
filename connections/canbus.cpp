#include <QObject>
#include <QDebug>
#include "canbus.h"

CANBus::CANBus()
{
    speed       = 250000;
    listenOnly  = false;
    singleWire  = false;
    active      = false;
    canFD       = false;
}


CANBus::CANBus(const CANBus& pBus) :
    speed(pBus.speed),
    listenOnly(pBus.listenOnly),
    singleWire(pBus.singleWire),
    active(pBus.active),
    canFD(pBus.canFD) {}


bool CANBus::operator==(const CANBus& bus) const{
    return  speed == bus.speed &&
            listenOnly == bus.listenOnly &&
            singleWire == bus.singleWire &&
            active == bus.active &&
            canFD == bus.canFD;
}

void CANBus::setSpeed(int newSpeed){
    //qDebug() << "CANBUS SetSpeed = " << newSpeed;
    speed = newSpeed;
}

void CANBus::setListenOnly(bool mode){
    //qDebug() << "CANBUS SetListenOnly = " << mode;
    listenOnly = mode;
}

void CANBus::setSingleWire(bool mode){
    //qDebug() << "CANBUS SetSingleWire = " << mode;
    singleWire = mode;
}

void CANBus::setActive(bool mode){
    //qDebug() << "CANBUS SetEnabled = " << mode;
    active = mode;
}

void CANBus::setCanFD(bool mode){
    //qDebug() << "CANBUS setCanFD = " << mode;
    canFD = mode;
}

int CANBus::getSpeed(){
    return speed;
}

bool CANBus::isListenOnly(){
    return listenOnly;
}

bool CANBus::isSingleWire(){
    return singleWire;
}

bool CANBus::isActive(){
    return active;
}

bool CANBus::isCanFD(){
    return canFD;
}


QDataStream& operator<<( QDataStream & pStream, const CANBus& pCanBus )
{
    pStream << pCanBus.speed;
    pStream << pCanBus.listenOnly;
    pStream << pCanBus.singleWire;
    pStream << pCanBus.active;
    return pStream;
}

QDataStream & operator>>(QDataStream & pStream, CANBus& pCanBus)
{
    pStream >> pCanBus.speed;
    pStream >> pCanBus.listenOnly;
    pStream >> pCanBus.singleWire;
    pStream >> pCanBus.active;
    return pStream;
}
