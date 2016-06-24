#ifndef CANBus_H
#define CANBus_H


class CANBus
{
public:
    CANBus();
    CANBus(const CANBus&);
    bool operator==(const CANBus&) const;
    virtual ~CANBus(){}; /*TODO: remove connection from CANBus and add CANBus as an element of CANConnection */
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

#endif // CANBus_H
