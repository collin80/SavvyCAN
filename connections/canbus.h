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
    virtual ~CANBus(){};

    int speed;
    bool listenOnly;
    bool singleWire;
    bool active; //is this bus turned on?


    void setSpeed(int); // new speed
    void setListenOnly(bool); //bool for whether to only listen
    void setSingleWire(bool); //bool for whether to use single wire mode
    void setEnabled(bool); //whether this bus should be enabled or not.
    int getSpeed();
    bool isListenOnly();
    bool isSingleWire();
    bool isActive();
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
