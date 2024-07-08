#include "canbus.h"
#include <QDebug>
#include <QObject>

CANBus::CANBus()
{
  speed = 500000;
  listenOnly = false;
  singleWire = false;
  active = false;
  canFD = false;
  dataRate = 2000000;
}

bool CANBus::operator==(const CANBus& bus) const
{
  return speed == bus.speed && listenOnly == bus.listenOnly && singleWire == bus.singleWire && active == bus.active &&
         canFD == bus.canFD && dataRate == bus.dataRate;
}

void CANBus::setSpeed(int newSpeed)
{
  // qDebug() << "CANBUS SetSpeed = " << newSpeed;
  speed = newSpeed;
}

void CANBus::setListenOnly(bool mode)
{
  // qDebug() << "CANBUS SetListenOnly = " << mode;
  listenOnly = mode;
}

void CANBus::setSingleWire(bool mode)
{
  // qDebug() << "CANBUS SetSingleWire = " << mode;
  singleWire = mode;
}

void CANBus::setActive(bool mode)
{
  // qDebug() << "CANBUS SetEnabled = " << mode;
  active = mode;
}

void CANBus::setCanFD(bool mode)
{
  // qDebug() << "CANBUS setCanFD = " << mode;
  canFD = mode;
}

int CANBus::getSpeed() const
{
  return speed;
}

int CANBus::getDataRate() const
{
  return dataRate;
}

void CANBus::setDataRate(int newSpeed)
{
  // qDebug() << "CANBUS SetSpeed = " << newSpeed;
  dataRate = newSpeed;
}

bool CANBus::isListenOnly() const
{
  return listenOnly;
}

bool CANBus::isSingleWire() const
{
  return singleWire;
}

bool CANBus::isActive() const
{
  return active;
}

bool CANBus::isCanFD() const
{
  return canFD;
}

QDataStream& operator<<(QDataStream& pStream, const CANBus& pCanBus)
{
  pStream << pCanBus.speed;
  pStream << pCanBus.listenOnly;
  pStream << pCanBus.singleWire;
  pStream << pCanBus.active;
  // FIXME CANFD settings missing
  return pStream;
}

QDataStream& operator>>(QDataStream& pStream, CANBus& pCanBus)
{
  pStream >> pCanBus.speed;
  pStream >> pCanBus.listenOnly;
  pStream >> pCanBus.singleWire;
  pStream >> pCanBus.active;
  return pStream;
}
