#ifndef CANBus_H
#define CANBus_H
#include <QDataStream>
#include "can_structs.h"

class CANBus
{
public:
    CANBus();
    CANBus(const CANBus&);
    bool operator==(const CANBus&) const;
    CANBus& operator=(const CANBus& other) = default;
    //virtual ~CANBus(){}

    int speed;
    bool listenOnly;
    bool singleWire;
    bool active; //is this bus turned on?
    bool canFD;

    void setSpeed(int); // new speed
    void setListenOnly(bool); //bool for whether to only listen
    void setSingleWire(bool); //bool for whether to use single wire mode
    void setActive(bool); //whether this bus should be enabled or not.
    void setCanFD(bool); // enable or disable CANFD support
    int getSpeed();
    bool isListenOnly();
    bool isSingleWire();
    bool isActive();
    bool isCanFD();
};

QDataStream& operator<<( QDataStream & pStream, const CANBus& pCanBus );
QDataStream & operator>>(QDataStream & pStream, CANBus& pCanBus);

Q_DECLARE_METATYPE(CANBus);

struct BusData {
    CANBus             mBus;
    bool               mConfigured;
    QVector<CANFltObserver>    mTargettedFrames;
};

#endif // CANBus_H
