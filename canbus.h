#ifndef CANBus_H
#define CANBus_H


class CANConnectionContainer;

class CANBus
{
public:
    CANBus();
    CANBus(const CANBus&);
    virtual ~CANBus(){}; /*TODO: remove connection from CANBus and add CANBus as an element of CANConnection */
    int busNum;
    int speed;
    bool listenOnly;
    bool singleWire;
    bool active; //is this bus turned on?
    CANConnectionContainer* container;

    void setSpeed(int); // new speed
    void setListenOnly(bool); //bool for whether to only listen
    void setSingleWire(bool); //bool for whether to use single wire mode
    void setEnabled(bool); //whether this bus should be enabled or not.
    void setContainer(CANConnectionContainer *);
    CANConnectionContainer* getContainer();
    void setBusNum(int);
    int getSpeed();
    int getBusNum();
    bool isListenOnly();
    bool isSingleWire();
    bool isActive();
};

#endif // CANBus_H
