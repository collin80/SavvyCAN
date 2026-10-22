#ifndef CANBus_H
#define CANBus_H
#include <QDataStream>
#include "can_structs.h"

class CANBus
{
    int speed;
    int samplePoint;
    bool listenOnly;
    bool singleWire;
    bool active; //is this bus turned on?
    bool canFD;
    int dataRate;

    friend QDataStream& operator<<(QDataStream & pStream, const CANBus& pCanBus);
    friend QDataStream& operator>>(QDataStream & pStream, CANBus& pCanBus);
public:
    CANBus();

    bool operator==(const CANBus&) const;

    void setSpeed(int); // new speed
    void setSamplePoint(int);
    void setListenOnly(bool); //bool for whether to only listen
    void setSingleWire(bool); //bool for whether to use single wire mode
    void setActive(bool); //whether this bus should be enabled or not.
    void setCanFD(bool); // enable or disable CANFD support
    void setDataRate(int newSpeed);

    int getSpeed() const;
    int getSamplePoint() const;
    int getDataRate() const;
    bool isListenOnly() const;
    bool isSingleWire() const;
    bool isActive() const;
    bool isCanFD() const;
};

QDataStream& operator<<(QDataStream & pStream, const CANBus& pCanBus);
QDataStream& operator>>(QDataStream & pStream, CANBus& pCanBus);

Q_DECLARE_METATYPE(CANBus);

struct BusData {
    CANBus             mBus;
    bool               mConfigured = {};
    QVector<CANFltObserver>    mTargettedFrames;
};

#endif // CANBus_H
