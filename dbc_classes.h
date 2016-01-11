#ifndef DBC_CLASSES_H
#define DBC_CLASSES_H

#include <QString>
#include <QStringList>

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

class DBC_SIGNAL
{
public:
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
    DBC_NODE *receiver;
    QString unitName;
    QString comment;
    QList<DBC_ATTRIBUTE> attributes;
    QList<DBC_VAL> valList;
};

class DBC_MESSAGE
{
public:
    int ID;
    QString name;
    QString comment;
    int len;
    DBC_NODE *sender;
    QList<DBC_ATTRIBUTE> attributes;
    QList<DBC_SIGNAL> msgSignals;
    DBC_SIGNAL* multiplexorSignal;
};


#endif // DBC_CLASSES_H

