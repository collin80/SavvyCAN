#ifndef CAN_TRIGGER_STRUCTS_H
#define CAN_TRIGGER_STRUCTS_H

#include "can_structs.h"

#include <QList>
#include <QUuid>

enum TriggerMask
{
    TRG_ID = 1,
    TRG_MS = 2,
    TRG_COUNT = 4,
    TRG_BUS = 8,
    TRG_SIGNAL = 16,
    TRG_SIGVAL = 32,
};

//Stores a single trigger.
class Trigger
{
public:
    bool readyCount; //ready to do millisecond ticks?
    int ID; //which ID to match against
    int milliseconds; //interval for triggering
    int msCounter; //how many MS have ticked since last trigger
    int maxCount; //max # of these frames to trigger for
    int currCount; //how many we have triggered for so far.
    int bus; //which bus to monitor (-1 if we aren't picky)
    QString sigName; //which signal to trigger on
    int64_t sigValueInt; //value to trigger on (integer)
    double sigValueDbl; //value to trigger on (floating point)
    uint32_t triggerMask;
};

//referece for a source location for a single modifier.
//If ID is zero then this is a numeric operand. The number is then
//stored in databyte
//If ID is -1 then this is the temporary storage register. This is a shadow
//register used to accumulate the results of a multi operation modifier.
//if ID is -2 then this is a look up of our own data bytes stored in the class data.
//Of course, if the ID is positive then we grab bytes or signals from newest message with that ID
class ModifierOperand
{
public:
    int ID;
    int bus;
    int databyte;
    bool notOper; //should a bitwise NOT be applied to this prior to doing the actual calculation?
    QString signalName; //if ID is positive and there is text in here then we'll look up the signal and use its value
};

//list of operations that can be done between the two operands
enum ModifierOperationType
{
    ADDITION,
    SUBTRACTION,
    MULTIPLICATION,
    DIVISION,
    AND,
    OR,
    XOR,
    MOD
};

//A single modifier operation
class ModifierOp
{
public:
    ModifierOperand first;
    ModifierOperand second;
    ModifierOperationType operation;
};

//All the operations that entail a single modifier. For instance D0+1+D2 is two operations
class Modifier
{
public:
    int destByte; //if -1 then target one of this ID's signals instead
    QString signalName;
    QList<ModifierOp> operations;
};

//A single line from the data grid. Inherits from CANFrame so it stores a canbus frame
//plus the extra stuff for the data grid
class FrameSendData : public CANFrame
{
public:
    bool enabled;
    int count;
    QList<Trigger> triggers;
    QList<Modifier> modifiers;
};

#endif // CAN_TRIGGER_STRUCTS_H

