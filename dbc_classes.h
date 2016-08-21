#ifndef DBC_CLASSES_H
#define DBC_CLASSES_H

#include <QString>
#include <QStringList>
#include "can_structs.h"

/*classes to encapsulate data from a DBC file. Really, the stuff of interest
  are the nodes, messages, signals, attributes, and comments.

  These things sort of form a hierarchy. Nodes send and receive messages.
  Messages are comprised of signals. Signals and messages have attributes.
  All of them can have comments.
*/

enum DBC_SIG_VAL_TYPE
{
    UNSIGNED_INT,
    SIGNED_INT,
    SP_FLOAT,
    DP_FLOAT,
    STRING
};

enum DBC_ATTRIBUTE_VAL_TYPE
{
    HEX,
    QFLOAT,
    QSTRING,
    ENUM
};

class DBC_ATTRIBUTE
{
public:
    QString name;
    DBC_ATTRIBUTE_VAL_TYPE type;
    double startVal;
    double endVal;
    QStringList enumVals; //also used for STRING type but then you're assured to have only one
};

class DBC_VAL
{
public:
    int value;
    QString descript;
};

class DBC_NODE
{
public:
    QString name;
    QString comment;
    QList<DBC_ATTRIBUTE> attributes;
};

class DBC_MESSAGE; //forward reference so that DBC_SIGNAL can compile before we get to real definition of DBC_MESSAGE

class DBC_SIGNAL
{
public: //TODO: this is sloppy. It shouldn't all be public!
    QString name;
    int startBit;
    int signalSize;
    bool intelByteOrder; //true is obviously little endian. False is big endian
    bool isMultiplexor;
    bool isMultiplexed;
    int multiplexValue;
    DBC_SIG_VAL_TYPE valType;
    double factor;
    double bias;
    double min;
    double max;
    DBC_NODE *receiver; //it is fast to have a pointer but dangerous... Make sure to walk the whole tree and delete everything so nobody has stale references.
    DBC_MESSAGE *parentMessage;
    QString unitName;
    QString comment;
    QList<DBC_ATTRIBUTE> attributes;
    QList<DBC_VAL> valList;

    bool processAsText(const CANFrame &frame, QString &outString);
    bool processAsInt(const CANFrame &frame, int32_t &outValue);
    bool processAsDouble(const CANFrame &frame, double &outValue);
};

class DBCSignalHandler; //forward declaration to keep from having to include dbchandler.h in this file and thus create a loop

class DBC_MESSAGE
{
public:
    DBC_MESSAGE();

    uint32_t ID;
    QString name;
    QString comment;
    unsigned int len;
    DBC_NODE *sender;
    QList<DBC_ATTRIBUTE> attributes;
    DBCSignalHandler *sigHandler;
    DBC_SIGNAL* multiplexorSignal;
};


#endif // DBC_CLASSES_H

