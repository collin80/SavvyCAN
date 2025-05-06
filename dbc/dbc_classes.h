#ifndef DBC_CLASSES_H
#define DBC_CLASSES_H

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVariant>
#include "can_structs.h"

/*classes to encapsulate data from a DBC file. Really, the stuff of interest
  are the nodes, messages, signals, attributes, and comments.

  These things sort of form a hierarchy. Nodes send and receive messages.
  Messages are comprised of signals. Nodes, signals, and messages potentially have attribute values.
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
    ATTR_INT,
    ATTR_FLOAT,
    ATTR_STRING,
    ATTR_ENUM,
};

enum DBC_ATTRIBUTE_TYPE
{
    ATTR_TYPE_GENERAL,
    ATTR_TYPE_NODE,
    ATTR_TYPE_MESSAGE,
    ATTR_TYPE_SIG,
    ATTR_TYPE_ANY
};

class DBC_ATTRIBUTE
{
public:
    QString name;
    DBC_ATTRIBUTE_VAL_TYPE valType;
    DBC_ATTRIBUTE_TYPE attrType;
    double upper, lower;
    QStringList enumVals;
    QVariant defaultValue;
};

class DBC_ATTRIBUTE_VALUE
{
public:
    QString attrName;
    QVariant value;
};

class DBC_VAL_ENUM_ENTRY
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
    QString sourceFileName;
    QList<DBC_ATTRIBUTE_VALUE> attributes;

    DBC_ATTRIBUTE_VALUE *findAttrValByName(QString name);
    DBC_ATTRIBUTE_VALUE *findAttrValByIdx(int idx);

    friend bool operator<(const DBC_NODE& l, const DBC_NODE& r)
    {
        return (l.name.toLower() < r.name.toLower());
    }
};

class DBC_MESSAGE; //forward reference so that DBC_SIGNAL can compile before we get to real definition of DBC_MESSAGE
class DBC_SIGNAL;

class DBC_SIGNAL
{
public: //TODO: this is sloppy. It shouldn't all be public!
    DBC_SIGNAL() = default;

    enum DbcMuxStringFormat {
        MuxStringFormat_DbcFile,
        MuxStringFormat_UI
    };
    QString name;
    int startBit = 1;
    int signalSize = 1;
    bool intelByteOrder = false; //true is obviously little endian. False is big endian

    bool isMultiplexor = false;
    bool isMultiplexed = false;
    void addMultiplexRange(int min, int max);
    int multiplexHighValue = 0;
    int multiplexLowValue = 0;
    bool hasExtendedMultiplexing = false;
    QList<DBC_SIGNAL *> multiplexedChildren;
    DBC_SIGNAL *multiplexParent = nullptr;
    QString multiplexDbcString(DbcMuxStringFormat fmt = MuxStringFormat_DbcFile) const;
    void copyMultiplexValuesFromSignal(const DBC_SIGNAL &signal);
    bool parseDbcMultiplexUiString(const QString &multiplexes, QString &errorString);
    bool multiplexesIdenticalToSignal(DBC_SIGNAL *other) const;

    DBC_SIG_VAL_TYPE valType = DBC_SIG_VAL_TYPE::UNSIGNED_INT;
    double factor = 1.0;
    double bias = 0;
    double min = 0;
    double max = 1;
    DBC_NODE *receiver = nullptr; //it is fast to have a pointer but dangerous... Make sure to walk the whole tree and delete everything so nobody has stale references.
    DBC_MESSAGE *parentMessage = nullptr;
    QString unitName;
    QString comment;
    QVariant cachedValue;
    QList<DBC_ATTRIBUTE_VALUE> attributes;
    QList<DBC_VAL_ENUM_ENTRY> valList;
    DBC_SIGNAL *self;

    bool processAsText(const CANFrame &frame, QString &outString, bool outputName = true, bool outputUnit = true);
    bool processAsInt(const CANFrame &frame, int32_t &outValue);
    bool processAsDouble(const CANFrame &frame, double &outValue);
    bool getValueString(int64_t intVal, QString &outString);
    QString makePrettyOutput(double floatVal, int64_t intVal, bool outputName = true, bool isInteger = false, bool outputUnit = true);
    QString processSignalTree(const CANFrame &frame);
    DBC_ATTRIBUTE_VALUE *findAttrValByName(QString name);
    DBC_ATTRIBUTE_VALUE *findAttrValByIdx(int idx);
    bool isSignalInMessage(const CANFrame &frame);
    bool isValueMatchingMultiplex(int val) const;


    friend bool operator<(const DBC_SIGNAL& l, const DBC_SIGNAL& r)
    {
        return (l.name.toLower() < r.name.toLower());
    }
private:
    QList<QPair<int, int>> multiplexLowAndHighValues;
};

class DBCSignalHandler; //forward declaration to keep from having to include dbchandler.h in this file and thus create a loop

class DBC_MESSAGE
{
public:
    DBC_MESSAGE();

    uint32_t ID;
    bool extendedID;
    QString name;
    QString comment;
    unsigned int len;
    DBC_NODE *sender;
    QColor bgColor;
    QColor fgColor;
    QList<DBC_ATTRIBUTE_VALUE> attributes;
    DBCSignalHandler *sigHandler;
    DBC_SIGNAL* multiplexorSignal;

    DBC_ATTRIBUTE_VALUE *findAttrValByName(QString name);
    DBC_ATTRIBUTE_VALUE *findAttrValByIdx(int idx);

    friend bool operator<(const DBC_MESSAGE& l, const DBC_MESSAGE& r)
    {
        return (l.name.toLower() < r.name.toLower());
    }
};


#endif // DBC_CLASSES_H

