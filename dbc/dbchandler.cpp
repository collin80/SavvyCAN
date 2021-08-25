#include "dbchandler.h"

#include <QFile>
#include <QRegularExpression>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "utility.h"
#include "connections/canconmanager.h"

DBCHandler* DBCHandler::instance = nullptr;

DBC_SIGNAL* DBCSignalHandler::findSignalByIdx(int idx)
{
    if (sigs.count() == 0) return nullptr;
    if (idx < 0) return nullptr;
    if (idx >= sigs.count()) return nullptr;

    return &sigs[idx];
}

DBC_SIGNAL* DBCSignalHandler::findSignalByName(QString name)
{
    if (sigs.count() == 0) return nullptr;
    for (int i = 0; i < sigs.count(); i++)
    {
        if (sigs[i].name.compare(name, Qt::CaseInsensitive) == 0)
        {
            return &sigs[i];
        }
    }
    return nullptr;
}

bool DBCSignalHandler::addSignal(DBC_SIGNAL &sig)
{
    sigs.append(sig);
    return true;
}

bool DBCSignalHandler::removeSignal(DBC_SIGNAL *sig)
{
    qDebug() << "Total # of signals: " << getCount();
    for (int i = 0; i < getCount(); i++)
    {
        if (sigs[i].name == sig->name)
        {
            sigs.removeAt(i);
            qDebug() << "Removed signal at idx " << i;
        }
    }
    return true;
}

bool DBCSignalHandler::removeSignal(int idx)
{
    if (sigs.count() == 0) return false;
    if (idx < 0) return false;
    if (idx >= sigs.count()) return false;
    sigs.removeAt(idx);
    return true;
}

bool DBCSignalHandler::removeSignal(QString name)
{
    bool foundSome = false;
    if (sigs.count() == 0) return false;
    for (int i = sigs.count() - 1; i >= 0; i--)
    {
        if (sigs[i].name.compare(name, Qt::CaseInsensitive) == 0)
        {
            sigs.removeAt(i);
            foundSome = true;
        }
    }
    return foundSome;
}

void DBCSignalHandler::removeAllSignals()
{
    sigs.clear();
}

int DBCSignalHandler::getCount()
{
    return sigs.count();
}

void DBCSignalHandler::sort()
{
    std::sort(sigs.begin(), sigs.end());
}

DBC_MESSAGE* DBCMessageHandler::findMsgByID(uint32_t id)
{
    if (messages.count() == 0) return nullptr;
    for (int i = 0; i < messages.count(); i++)
    {
        if (matchingCriteria == J1939)
        {
            // include data page and extended data page in the pgn
            uint32_t pgn = (id & 0x3FFFF00) >> 8;
            if ( (pgn & 0xFF00) <= 0xEF00 )
            {
                // PDU1 format
                pgn &= 0x3FF00;
                if ((messages[i].ID & 0x3FF0000) == (pgn << 8))
                {
                    return &messages[i];
                }
            }
            else
            {
                // PDU2 format
                if ((messages[i].ID & 0x3FFFF00) == (pgn << 8))
                {
                    return &messages[i];
                }
            }
        }
        else if (matchingCriteria == GMLAN)
        {
            // Match the bits 14-26 (Arbitration Id) of GMLAN 29bit header
            uint32_t arbId = id &0x3FFE000;
            if ( (arbId != 0) && (messages[i].ID & 0x3FFE000) == arbId )
                return &messages[i];
        }
        else
        {
            if ( messages[i].ID == id )
            {
                return &messages[i];
            }
        }
    }
    return nullptr;
}

DBC_MESSAGE* DBCMessageHandler::findMsgByIdx(int idx)
{
    if (messages.count() == 0) return nullptr;
    if (idx < 0) return nullptr;
    if (idx >= messages.count()) return nullptr;
    return &messages[idx];
}

DBC_MESSAGE* DBCMessageHandler::findMsgByName(QString name)
{
    if (messages.count() == 0) return nullptr;
    for (int i = 0; i < messages.count(); i++)
    {
        if (messages[i].name.compare(name, Qt::CaseInsensitive) == 0)
        {
            return &messages[i];
        }
    }
    return nullptr;
}

//allow for finding a message just by part of the name
DBC_MESSAGE* DBCMessageHandler::findMsgByPartialName(QString name)
{
    if (messages.count() == 0) return nullptr;
    for (int i = 0; i < messages.count(); i++)
    {
        if (messages[i].name.contains(name, Qt::CaseInsensitive))
        {
            return &messages[i];
        }
    }
    return nullptr;
}

bool DBCMessageHandler::addMessage(DBC_MESSAGE &msg)
{
    messages.append(msg);
    return true;
}

bool DBCMessageHandler::removeMessage(DBC_MESSAGE *msg)
{
    qDebug() << "Total # of messages: " << getCount();
    for (int i = 0; i < getCount(); i++)
    {
        if (messages[i].name == msg->name)
        {
            messages.removeAt(i);
            qDebug() << "Removed message at idx " << i;
        }
    }
    return true;
}

bool DBCMessageHandler::removeMessageByIndex(int idx)
{
    if (messages.count() == 0) return false;
    if (idx < 0) return false;
    if (idx >= messages.count()) return false;
    messages.removeAt(idx);
    return true;
}

bool DBCMessageHandler::removeMessage(uint32_t ID)
{
    bool foundSome = false;
    if (messages.count() == 0) return false;
    for (int i = messages.count() - 1; i >= 0; i--)
    {
        if (messages[i].ID == ID)
        {
            messages.removeAt(i);
            foundSome = true;
        }
    }
    return foundSome;
}

bool DBCMessageHandler::removeMessage(QString name)
{
    bool foundSome = false;
    if (messages.count() == 0) return false;
    for (int i = messages.count() - 1; i >= 0; i--)
    {
        if (messages[i].name.compare(name, Qt::CaseInsensitive) == 0)
        {
            messages.removeAt(i);
            foundSome = true;
        }
    }
    return foundSome;
}

void DBCMessageHandler::removeAllMessages()
{
    messages.clear();
}

int DBCMessageHandler::getCount()
{
    return messages.count();
}

void DBCMessageHandler::sort()
{
    std::sort(messages.begin(), messages.end());
    for (int i = 0; i < messages.count(); i++)
    {
        messages[i].sigHandler->sort();
    }
}

bool DBCMessageHandler::filterLabeling()
{
    return filterLabelingEnabled;
}

void DBCMessageHandler::setFilterLabeling(bool filterLabeling)
{
    filterLabelingEnabled = filterLabeling;
}

MatchingCriteria_t DBCMessageHandler::getMatchingCriteria()
{
    return matchingCriteria;
}

void DBCMessageHandler::setMatchingCriteria(MatchingCriteria_t _matchingCriteria)
{
    matchingCriteria = _matchingCriteria;
}

DBCFile::DBCFile()
{
    messageHandler = new DBCMessageHandler;
    messageHandler->setMatchingCriteria(EXACT);
    messageHandler->setFilterLabeling(false);
    isDirty = false;
    fileName = "<Unsaved File>";
}

DBCFile::DBCFile(const DBCFile& cpy) : QObject()
{
    messageHandler = new DBCMessageHandler;
    for (int i = 0 ; i < cpy.messageHandler->getCount() ; i++)
        messageHandler->addMessage(*cpy.messageHandler->findMsgByIdx(i));

    messageHandler->setMatchingCriteria(cpy.messageHandler->getMatchingCriteria());
    messageHandler->setFilterLabeling(cpy.messageHandler->filterLabeling());
    fileName = cpy.fileName;
    filePath = cpy.filePath;
    assocBuses = cpy.assocBuses;
    dbc_nodes.clear();
    dbc_nodes.append(cpy.dbc_nodes);
    dbc_attributes.clear();
    dbc_attributes.append(cpy.dbc_attributes);
    isDirty = cpy.isDirty;
}

DBCFile& DBCFile::operator=(const DBCFile& cpy)
{
    if (this != &cpy) // protect against invalid self-assignment
    {
        messageHandler = cpy.messageHandler;
        fileName = cpy.fileName;
        filePath = cpy.filePath;
        assocBuses = cpy.assocBuses;
        dbc_nodes.clear();
        dbc_nodes.append(cpy.dbc_nodes);
        dbc_attributes.clear();
        dbc_attributes.append(cpy.dbc_attributes);
    }
    return *this;
}

void DBCFile::sort()
{
    std::sort(dbc_nodes.begin(), dbc_nodes.end()); //sort node names
    messageHandler->sort(); //sort messages, each of which sorts its signals too
}

DBC_NODE* DBCFile::findNodeByIdx(int idx)
{
    if (idx < 0) return nullptr;
    if (idx >= dbc_nodes.count()) return nullptr;
    return &dbc_nodes[idx];
}

DBC_NODE* DBCFile::findNodeByName(QString name)
{
    if (dbc_nodes.length() == 0) return nullptr;
    for (int i = 0; i < dbc_nodes.length(); i++)
    {
        if (dbc_nodes[i].name.compare(name, Qt::CaseInsensitive) == 0)
        {
            return &dbc_nodes[i];
        }
    }
    return nullptr;
}

QString DBCFile::getFullFilename()
{
    return filePath + fileName;
}

QString DBCFile::getFilename()
{
    return fileName;
}

QString DBCFile::getPath()
{
    return filePath;
}

int DBCFile::getAssocBus()
{
    return assocBuses;
}

void DBCFile::setAssocBus(int bus)
{
    if (bus < -1) return;
    // To allow setting bus numbers even before connection is configured, do not enforce "valid" bus numbers
    //int numBuses = CANConManager::getInstance()->getNumBuses();
    //if (bus >= numBuses) return;
    assocBuses = bus;
}

DBC_ATTRIBUTE *DBCFile::findAttributeByName(QString name, DBC_ATTRIBUTE_TYPE type)
{
    if (dbc_attributes.length() == 0) return nullptr;
    for (int i = 0; i < dbc_attributes.length(); i++)
    {
        if (dbc_attributes[i].name.compare(name, Qt::CaseInsensitive) == 0 && ((type == dbc_attributes[i].attrType) || (type == ATTR_TYPE_ANY) ) )
        {
            return &dbc_attributes[i];
        }
    }
    return nullptr;
}

DBC_ATTRIBUTE *DBCFile::findAttributeByIdx(int idx)
{
    if (idx < 0) return nullptr;
    if (idx >= dbc_attributes.count()) return nullptr;
    return &dbc_attributes[idx];
}

void DBCFile::findAttributesByType(DBC_ATTRIBUTE_TYPE typ, QList<DBC_ATTRIBUTE> *list)
{
    if (!list) return;
    list->clear();
    foreach (DBC_ATTRIBUTE attr, dbc_attributes)
    {
        if (attr.attrType == typ) list->append(attr);
    }
}

//there's no external way to clear the flag. It is only cleared when the file is saved by this object.
void DBCFile::setDirtyFlag()
{
    isDirty = true;
}

bool DBCFile::getDirtyFlag()
{
    return isDirty;
}

DBC_MESSAGE* DBCFile::parseMessageLine(QString line)
{
    QRegularExpression regex;
    QRegularExpressionMatch match;

    DBC_MESSAGE *msgPtr;

    qDebug() << "Found a BO line";
    regex.setPattern("^BO\\_ (\\w+) (\\w+) *: (\\w+) (\\w+)");
    match = regex.match(line);
    //captured 1 = the ID in decimal
    //captured 2 = The message name
    //captured 3 = the message length
    //captured 4 = the NODE responsible for this message
    if (match.hasMatch())
    {
        DBC_MESSAGE msg;
        uint32_t ID = match.captured(1).toULong(); //the ID is always stored in decimal format
        msg.ID = ID & 0x1FFFFFFFul;
        msg.extendedID = (ID & 80000000ul) ? true : false;
        msg.name = match.captured(2);
        msg.len = match.captured(3).toUInt();
        msg.sender = findNodeByName(match.captured(4));
        if (!msg.sender) msg.sender = findNodeByIdx(0);
        messageHandler->addMessage(msg);
        msgPtr = messageHandler->findMsgByID(msg.ID);
    }
    else msgPtr = nullptr;
    return msgPtr;
}

DBC_SIGNAL* DBCFile::parseSignalLine(QString line, DBC_MESSAGE *msg)
{
    QRegularExpression regex;
    QRegularExpressionMatch match;

    int offset = 0;
    bool isMessageMultiplexor = false;
    //bool isMultiplexed = false;
    DBC_SIGNAL sig;

    sig.multiplexLowValue = 0;
    sig.multiplexHighValue = 0;
    sig.isMultiplexed = false;
    sig.isMultiplexor = false;

    qDebug() << "Found a SG line";
    regex.setPattern("^SG\\_ *(\\w+) +M *: *(\\d+)\\|(\\d+)@(\\d+)([\\+|\\-]) \\(([0-9.+\\-eE]+),([0-9.+\\-eE]+)\\) \\[([0-9.+\\-eE]+)\\|([0-9.+\\-eE]+)\\] \\\"(.*)\\\" (.*)");

    match = regex.match(line);
    if (match.hasMatch())
    {
        qDebug() << "Multiplexor signal";
        isMessageMultiplexor = true;
        sig.isMultiplexor = true;
    }
    else
    {
        regex.setPattern("^SG\\_ *(\\w+) +m(\\d+) *: *(\\d+)\\|(\\d+)@(\\d+)([\\+|\\-]) \\(([0-9.+\\-eE]+),([0-9.+\\-eE]+)\\) \\[([0-9.+\\-eE]+)\\|([0-9.+\\-eE]+)\\] \\\"(.*)\\\" (.*)");
        match = regex.match(line);
        if (match.hasMatch())
        {
            qDebug() << "Multiplexed signal";
            //isMultiplexed = true;
            sig.isMultiplexed = true;
            sig.multiplexLowValue = match.captured(2).toInt();
            sig.multiplexHighValue = sig.multiplexLowValue;
            offset = 1;
        }
        else
        {
            regex.setPattern("^SG\\_ *(\\w+) +m(\\d+)M *: *(\\d+)\\|(\\d+)@(\\d+)([\\+|\\-]) \\(([0-9.+\\-eE]+),([0-9.+\\-eE]+)\\) \\[([0-9.+\\-eE]+)\\|([0-9.+\\-eE]+)\\] \\\"(.*)\\\" (.*)");
            match = regex.match(line);
            if (match.hasMatch())
            {
                qDebug() << "Extended Multiplexor Signal";
                sig.isMultiplexor = true; //we don't set the local isMessageMultiplexor variable because this isn't the top level multiplexor
                sig.isMultiplexed = true; //but, it is both a multiplexor and multiplexed
                sig.multiplexLowValue = match.captured(2).toInt();
                sig.multiplexHighValue = sig.multiplexLowValue;
                offset = 1;
            }
            else
            {
                qDebug() << "standard signal";
                regex.setPattern("^SG\\_ *(\\w+) *: *(\\d+)\\|(\\d+)@(\\d+)([\\+|\\-]) \\(([0-9.+\\-eE]+),([0-9.+\\-eE]+)\\) \\[([0-9.+\\-eE]+)\\|([0-9.+\\-eE]+)\\] \\\"(.*)\\\" (.*)");
                match = regex.match(line);
                sig.isMultiplexed = false;
                sig.isMultiplexor = false;
            }
        }
    }

    //captured 1 is the signal name
    //captured 2 would be multiplex value if this is a multiplex signal. Then offset the rest of these by 1
    //captured 2 is the starting bit
    //captured 3 is the length in bits
    //captured 4 is the byte order / value type
    //captured 5 specifies signed/unsigned for ints
    //captured 6 is the scaling factor
    //captured 7 is the offset
    //captured 8 is the minimum value
    //captured 9 is the maximum value
    //captured 10 is the unit
    //captured 11 is the receiving node

    if (match.hasMatch())
    {
        sig.name = match.captured(1);
        sig.startBit = match.captured(2 + offset).toInt();
        sig.signalSize = match.captured(3 + offset).toInt();
        int val = match.captured(4 + offset).toInt();
        if (val < 2)
        {
            if (match.captured(5 + offset) == "+") sig.valType = UNSIGNED_INT;
            else sig.valType = SIGNED_INT;
        }
        switch (val)
        {
        case 0: //big endian mode
            sig.intelByteOrder = false;
            break;
        case 1: //little endian mode
            sig.intelByteOrder = true;
            break;
        case 2:
            sig.valType = SP_FLOAT;
            break;
        case 3:
            sig.valType = DP_FLOAT;
            break;
        case 4:
            sig.valType = STRING;
            break;
        case 5: //single point float in little endian
            sig.valType = SP_FLOAT;
            sig.intelByteOrder = true;
            break;
        case 6: //double point float in little endian
            sig.valType = DP_FLOAT;
            sig.intelByteOrder = true;
            break;
        }
        sig.factor = match.captured(6 + offset).toDouble();
        sig.bias = match.captured(7 + offset).toDouble();
        sig.min = match.captured(8 + offset).toDouble();
        sig.max = match.captured(9 + offset).toDouble();
        sig.unitName = match.captured(10 + offset);
        if (match.captured(11 + offset).contains(','))
        {
            QString tmp = match.captured(11 + offset).split(',')[0];
            sig.receiver = findNodeByName(tmp);            
        }
        else sig.receiver = findNodeByName(match.captured(11 + offset));

        if (!sig.receiver) sig.receiver = findNodeByIdx(0); //apply default if there was no match

        sig.parentMessage = msg;
        if (msg)
        {
            msg->sigHandler->addSignal(sig);
            if (isMessageMultiplexor) msg->multiplexorSignal = msg->sigHandler->findSignalByName(sig.name);
            return msg->sigHandler->findSignalByName(sig.name);
        }
        else return nullptr;
    }

    return nullptr;
}

//SG_MUL_VAL_ 2024 S1_PID_0D_VehicleSpeed S1 13-13;
bool DBCFile::parseSignalMultiplexValueLine(QString line)
{
    QRegularExpression regex;
    QRegularExpressionMatch match;

    qDebug() << "Found a multiplex definition line";
    regex.setPattern("^SG\\_MUL\\_VAL\\_ (\\d+) (\\w+) (\\w+) (\\d+)\\-(\\d+);");
    match = regex.match(line);
    //captured 1 is message ID
    //Captured 2 is signal name
    //Captured 3 is parent multiplexor
    //captured 4 is the lower bound
    //captured 5 is the upper bound
    if (match.hasMatch())
    {
        DBC_MESSAGE *msg = messageHandler->findMsgByID(match.captured(1).toULong() & 0x1FFFFFFFUL);
        if (msg != nullptr)
        {
            DBC_SIGNAL *thisSignal = msg->sigHandler->findSignalByName(match.captured(2));
            if (thisSignal != nullptr)
            {
                DBC_SIGNAL *parentSignal = msg->sigHandler->findSignalByName(match.captured(3));
                if (parentSignal != nullptr)
                {
                    //now need to add "thisSignal" to the children multiplexed signals of "parentSignal"
                    parentSignal->multiplexedChildren.append(thisSignal);
                    thisSignal->multiplexParent = parentSignal;
                    return true;
                }
            }
        }
    }
    return false;
}

bool DBCFile::parseValueLine(QString line)
{
    QRegularExpression regex;
    QRegularExpressionMatch match;

    qDebug() << "Found a value definition line";
    regex.setPattern("^VAL\\_ (\\w+) (\\w+) (.*);");
    match = regex.match(line);
    //captured 1 is the ID to match against
    //captured 2 is the signal name to match against
    //captured 3 is a series of values in the form (number "text") that is, all sep'd by spaces
    if (match.hasMatch())
    {
        //qDebug() << "Data was: " << match.captured(3);
        DBC_MESSAGE *msg = messageHandler->findMsgByID(match.captured(1).toULong() & 0x1FFFFFFFul);
        if (msg != nullptr)
        {
            DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(match.captured(2));
            if (sig != nullptr)
            {
                QString tokenString = match.captured(3);
                DBC_VAL_ENUM_ENTRY val;
                while (tokenString.length() > 2)
                {
                    regex.setPattern("(\\d+) \\\"(.*?)\\\"(.*)");
                    match = regex.match(tokenString);
                    if (match.hasMatch())
                    {
                        val.value = match.captured(1).toULong() & 0x1FFFFFFFul;
                        val.descript = match.captured(2);
                        //qDebug() << "sig val " << val.value << " desc " <<val.descript;
                        sig->valList.append(val);
                        int rightSize = tokenString.length() - match.captured(1).length() - match.captured(2).length() - 4;
                        if (rightSize > 0) tokenString = tokenString.right(rightSize);
                        else tokenString = "";
                        //qDebug() << "New token string: " << tokenString;
                    }
                    else tokenString = "";
                }
                return true;
            }
        }
    }
    return false;
}

bool DBCFile::parseAttributeLine(QString line)
{
    QRegularExpression regex;
    QRegularExpressionMatch match;

    regex.setPattern("^BA\\_ \\\"*(\\w+)\\\"* BO\\_ (\\d+) \\\"*([#\\w]+)\\\"*");
    match = regex.match(line);
    //captured 1 is the attribute name
    //captured 2 is the message ID number (frame ID)
    //captured 3 is the attribute value
    if (match.hasMatch())
    {
        qDebug() << "Found an attribute setting line for a message";
        DBC_ATTRIBUTE *foundAttr = findAttributeByName(match.captured(1));
        if (foundAttr)
        {
            qDebug() << "That attribute does exist";
            DBC_MESSAGE *foundMsg = messageHandler->findMsgByID(match.captured(2).toUInt() & 0x1FFFFFFFul);
            if (foundMsg)
            {
                qDebug() << "It references a valid, registered message";
                DBC_ATTRIBUTE_VALUE *foundAttrVal = foundMsg->findAttrValByName(match.captured(1));
                if (foundAttrVal) {
                    foundAttrVal->value = processAttributeVal(match.captured(3), foundAttr->valType);
                }
                else
                {
                    DBC_ATTRIBUTE_VALUE val;
                    val.attrName = match.captured(1);
                    val.value = processAttributeVal(match.captured(3), foundAttr->valType);
                    foundMsg->attributes.append(val);
                }
            }
        }
    }

    regex.setPattern("^BA\\_ \\\"*(\\w+)\\\"* SG\\_ (\\d+) \\\"*(\\w+)\\\"* \\\"*([#\\w]+)\\\"*");
    match = regex.match(line);
    //captured 1 is the attribute name
    //captured 2 is the message ID number (frame ID)
    //captured 3 is the signal name to bind to
    //captured 4 is the attribute value
    if (match.hasMatch())
    {
        qDebug() << "Found an attribute setting line for a signal";
        DBC_ATTRIBUTE *foundAttr = findAttributeByName(match.captured(1));
        if (foundAttr)
        {
            qDebug() << "That attribute does exist";
            DBC_MESSAGE *foundMsg = messageHandler->findMsgByID(match.captured(2).toUInt() & 0x1FFFFFFFUL);
            if (foundMsg)
            {
                qDebug() << "It references a valid, registered message";
                DBC_SIGNAL *foundSig = foundMsg->sigHandler->findSignalByName(match.captured(3));
                if (foundSig)
                {
                    DBC_ATTRIBUTE_VALUE *foundAttrVal = foundSig->findAttrValByName(match.captured(1));
                    if (foundAttrVal) foundAttrVal->value = processAttributeVal(match.captured(3), foundAttr->valType);
                    else
                    {
                        DBC_ATTRIBUTE_VALUE val;
                        val.attrName = match.captured(1);
                        val.value = processAttributeVal(match.captured(3), foundAttr->valType);
                        foundSig->attributes.append(val);
                    }
                }
            }
        }
    }

    regex.setPattern("^BA\\_ \\\"*(\\w+)\\\"* BU\\_ \\\"*(\\w+)\\\"* \\\"*([#\\w]+)\\\"*");
    match = regex.match(line);
    //captured 1 is the attribute name
    //captured 2 is the name of the node
    //captured 3 is the attribute value
    if (match.hasMatch())
    {
        qDebug() << "Found an attribute setting line for a node";
        DBC_ATTRIBUTE *foundAttr = findAttributeByName(match.captured(1));
        if (foundAttr)
        {
            qDebug() << "That attribute does exist";
            DBC_NODE *foundNode = findNodeByName(match.captured(2));
            if (foundNode)
            {
                qDebug() << "References a valid node name";
                DBC_ATTRIBUTE_VALUE *foundAttrVal = foundNode->findAttrValByName(match.captured(1));
                if (foundAttrVal) foundAttrVal->value = processAttributeVal(match.captured(3), foundAttr->valType);
                else
                {
                    DBC_ATTRIBUTE_VALUE val;
                    val.attrName = match.captured(1);
                    val.value = processAttributeVal(match.captured(3), foundAttr->valType);
                    foundNode->attributes.append(val);
                }
            }
            return true;
        }
    }

    return false;
}

bool DBCFile::parseDefaultAttrLine(QString line)
{
    QRegularExpression regex;
    QRegularExpressionMatch match;

    regex.setPattern("^BA\\_DEF\\_DEF\\_ \\\"*(\\w+)\\\"* \\\"*([#\\w]*)\\\"*");
    match = regex.match(line);
    //captured 1 is the name of the attribute
    //captured 2 is the default value for that attribute
    if (match.hasMatch())
    {
        qDebug() << "Found an attribute default value line, searching for an attribute named " << match.captured(1) << "with data " << match.captured(2);
        DBC_ATTRIBUTE *found = findAttributeByName(match.captured(1));
        if (found)
        {
            switch (found->valType)
            {
            case ATTR_STRING:
                found->defaultValue = match.captured(2);
                break;
            case ATTR_FLOAT:
                found->defaultValue = match.captured(2).toFloat();
                break;
            case ATTR_INT:
                found->defaultValue = match.captured(2).toInt();
                break;
            case ATTR_ENUM:
                QString temp = match.captured(2);
                found->defaultValue = 0;
                for (int x = 0; x < found->enumVals.count(); x++)
                {
                    if (!found->enumVals[x].compare(temp, Qt::CaseInsensitive))
                    {
                        found->defaultValue = x;
                        break;
                    }
                }
            }
            qDebug() << "Matched an attribute. Setting default value to " << found->defaultValue;
            return true;
        }
    }
    return false;
}

bool DBCFile::loadFile(QString fileName)
{
    QFile *inFile = new QFile(fileName);
    QString line, rawLine;
    QRegularExpression regex;
    QRegularExpressionMatch match;
    DBC_MESSAGE *currentMessage = nullptr;
    DBC_ATTRIBUTE attr;
    int numSigFaults = 0, numMsgFaults = 0;
    int linesSinceYield = 0;

    bool inMultilineBU = false;

    qDebug() << "DBC File: " << fileName;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        qDebug() << "Could not load the file!";
        return false;
    }

    qDebug() << "Starting DBC load";
    dbc_nodes.clear();
    messageHandler->removeAllMessages();
    messageHandler->setMatchingCriteria(EXACT);
    messageHandler->setFilterLabeling(false);

    DBC_NODE falseNode;
    falseNode.name = "Vector__XXX";
    falseNode.comment = "Default node if none specified";
    dbc_nodes.append(falseNode);

    while (!inFile->atEnd()) {
        rawLine = QString(inFile->readLine());
        line = rawLine.simplified();

        linesSinceYield++;
        if (linesSinceYield > 100)
        {
            qApp->processEvents();
        }

        if (inMultilineBU)
        {
            if (rawLine.startsWith("\t") || rawLine.startsWith("   "))
            {
                DBC_NODE node;
                node.name = line;
                dbc_nodes.append(node);
            }
            else inMultilineBU = false;
        }

        if (!inMultilineBU)
        {
            if (line.startsWith("BO_ ")) //defines a message
            {
                currentMessage = parseMessageLine(line);
                if (currentMessage == nullptr) numMsgFaults++;
            }
            if (line.startsWith("SG_ ")) //defines a signal
            {
                if (!parseSignalLine(line, currentMessage)) numSigFaults++;
            }

            if (line.startsWith("SG_MUL_VAL_ ")) //defines a signal multiplexing value definition
            {
                if (!parseSignalMultiplexValueLine(line)) numSigFaults++;
            }

            if (line.startsWith("BU_:")) //line specifies the nodes on this canbus
            {
                qDebug() << "Found a BU line";
                regex.setPattern("^BU\\_\\:(.*)");
                match = regex.match(line);
                //captured 1 = a list of node names separated by spaces. No idea how many yet
                if (match.hasMatch())
                {
                    QStringList nodeStrings = match.captured(1).split(' ');
                    qDebug() << "Found " << nodeStrings.count() << " node names";
                    for (int i = 0; i < nodeStrings.count(); i++)
                    {
                        //qDebug() << nodeStrings[i];
                        if (nodeStrings[i].length() > 1)
                        {
                            DBC_NODE node;
                            node.name = nodeStrings[i];
                            dbc_nodes.append(node);
                        }
                    }
                    inMultilineBU = true; //we might be... Need to check next line.
                }
            }
            if (line.startsWith("CM_ SG_ "))
            {
                qDebug() << "Found an SG comment line";
                regex.setPattern("^CM\\_ SG\\_ *(\\w+) *(\\w+) *\\\"(.*)\\\";");
                match = regex.match(line);
                //captured 1 is the ID to match against to get to the message
                //captured 2 is the signal name from that message
                //captured 3 is the comment itself
                if (match.hasMatch())
                {
                    //qDebug() << "Comment was: " << match.captured(3);
                    DBC_MESSAGE *msg = messageHandler->findMsgByID(match.captured(1).toUInt());
                    if (msg != nullptr)
                    {
                        DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(match.captured(2));
                        if (sig != nullptr)
                        {
                            sig->comment = match.captured(3);
                        }
                    }
                }
            }
            if (line.startsWith("CM_ BO_ "))
            {
                qDebug() << "Found a BO comment line";
                regex.setPattern("^CM\\_ BO\\_ *(\\w+) *\\\"(.*)\\\";");
                match = regex.match(line);
                //captured 1 is the ID to match against to get to the message
                //captured 2 is the comment itself
                if (match.hasMatch())
                {
                    //qDebug() << "Comment was: " << match.captured(2);
                    DBC_MESSAGE *msg = messageHandler->findMsgByID(match.captured(1).toUInt());
                    if (msg != nullptr)
                    {
                        msg->comment = match.captured(2);
                    }
                }
            }
            if (line.startsWith("CM_ BU_ "))
            {
                qDebug() << "Found a BU comment line";
                regex.setPattern("^CM\\_ BU\\_ *(\\w+) *\\\"(.*)\\\";");
                match = regex.match(line);
                //captured 1 is the Node name
                //captured 2 is the comment itself
                if (match.hasMatch())
                {
                    //qDebug() << "Comment was: " << match.captured(2);
                    DBC_NODE *node = findNodeByName(match.captured(1));
                    if (node != nullptr)
                    {
                        node->comment = match.captured(2);
                    }
                }
            }
            //VAL_ (1090) (VCUPresentParkLightOC) (1 "Error present" 0 "Error not present") ;
            if (line.startsWith("VAL_ "))
            {
                parseValueLine(line);
            }

            if (line.startsWith("BA_DEF_ SG_ "))
            {
                qDebug() << "Found a SG attribute line";

                if (parseAttribute(line.right(line.length() - 12), attr))
                {
                    //qDebug() << "Success";
                    attr.attrType = ATTR_TYPE_SIG;
                    dbc_attributes.append(attr);
                }
            }
            else if (line.startsWith("BA_DEF_ BO_ ")) //definition of a message attribute
            {
                qDebug() << "Found a BO attribute line";

                if (parseAttribute(line.right(line.length() - 12), attr))
                {
                    qDebug() << "Success";
                    attr.attrType = ATTR_TYPE_MESSAGE;
                    dbc_attributes.append(attr);
                }
            }
            else if (line.startsWith("BA_DEF_ BU_ ")) //definition of a node attribute
            {
                qDebug() << "Found a BU attribute line";

                if (parseAttribute(line.right(line.length() - 12), attr))
                {
                    //qDebug() << "Success";
                    attr.attrType = ATTR_TYPE_NODE;
                    dbc_attributes.append(attr);
                }
            }

            else if (line.startsWith("BA_DEF_ ")) //definition of a root attribute
            {
                qDebug() << "Found a BU attribute line";

                if (parseAttribute(line.right(line.length() - 9), attr))
                {
                    //qDebug() << "Success";
                    attr.attrType = ATTR_TYPE_GENERAL;
                    dbc_attributes.append(attr);
                }
            }


            if (line.startsWith("BA_DEF_DEF_ ")) //definition of default value for an attribute
            {
                parseDefaultAttrLine(line);
            }

            //BA_ "GenMsgCycleTime" BO_ 101 100;
            if (line.startsWith("BA_ ")) //set value of attribute
            {
                parseAttributeLine(line);
            }
        }
    }

    //upon loading the file add our custom foreground and background color attributes if they don't exist already
    DBC_ATTRIBUTE *bgAttr = findAttributeByName("GenMsgBackgroundColor");
    if (!bgAttr)
    {
        attr.attrType = ATTR_TYPE_MESSAGE;
        attr.defaultValue = QApplication::palette().color(QPalette::Base).name();
        attr.enumVals.clear();
        attr.lower = 0;
        attr.upper = 0;
        attr.name = "GenMsgBackgroundColor";
        attr.valType = ATTR_STRING;
        dbc_attributes.append(attr);
        bgAttr = findAttributeByName("GenMsgBackgroundColor");
    }

    DBC_ATTRIBUTE *fgAttr = findAttributeByName("GenMsgForegroundColor");
    if (!fgAttr)
    {
        attr.attrType = ATTR_TYPE_MESSAGE;
        attr.defaultValue = QApplication::palette().color(QPalette::WindowText).name();
        attr.enumVals.clear();
        attr.lower = 0;
        attr.upper = 0;
        attr.name = "GenMsgForegroundColor";
        attr.valType = ATTR_STRING;
        dbc_attributes.append(attr);
        fgAttr = findAttributeByName("GenMsgForegroundColor");
    }

    DBC_ATTRIBUTE *mc_attr = findAttributeByName("matchingcriteria");
    if (mc_attr)
    {
        messageHandler->setMatchingCriteria((MatchingCriteria_t)mc_attr->defaultValue.toInt());
    }
    else
    {
        messageHandler->setMatchingCriteria(EXACT);
    }

    DBC_ATTRIBUTE *fl_attr = findAttributeByName("filterlabeling");
    if (fl_attr)
    {
        messageHandler->setFilterLabeling(fl_attr->defaultValue.toInt());
    }
    else
    {
        messageHandler->setFilterLabeling(false);
    }

    QColor DefaultBG = QColor(bgAttr->defaultValue.toString());
    QColor DefaultFG = QColor(fgAttr->defaultValue.toString());

    DBC_ATTRIBUTE_VALUE *thisBG;
    DBC_ATTRIBUTE_VALUE *thisFG;

    for (int x = 0; x < messageHandler->getCount(); x++)
    {
        DBC_MESSAGE *msg = messageHandler->findMsgByIdx(x);
        msg->bgColor = DefaultBG;
        msg->fgColor = DefaultFG;
        thisBG = msg->findAttrValByName("GenMsgBackgroundColor");
        thisFG = msg->findAttrValByName("GenMsgForegroundColor");
        if (thisBG) msg->bgColor = QColor(thisBG->value.toString());
        if (thisFG) msg->fgColor = QColor(thisFG->value.toString());
        for (int y = 0; y < msg->sigHandler->getCount(); y++)
        {
            DBC_SIGNAL *sig = msg->sigHandler->findSignalByIdx(y);
            //if this doesn't have a multiplex parent set but is multiplexed then it must have used
            //simple multiplexing instead of any extended specification. So, fill in the multiplexor signal here
            //and also write the extended entry for it too.
            if (sig->isMultiplexed && (sig->multiplexParent == nullptr) && (msg->multiplexorSignal) )
            {
                sig->multiplexParent = msg->multiplexorSignal;
                msg->multiplexorSignal->multiplexedChildren.append(sig);
            }
            if ( sig->isMultiplexed && (!msg->multiplexorSignal) ) //marked multiplexed but there is no multiplexor.
            {
                sig->isMultiplexed = false; //can't multiplex if there is no multiplexor!
            }
        }
    }

    if (numSigFaults > 0 || numMsgFaults > 0)
    {
        QMessageBox msgBox;
        QString msg = "DBC file loaded with errors!\n";
        msg += "Number of faulty message entries: " + QString::number(numMsgFaults) + "\n";
        msg += "Number of faulty signal entries: " + QString::number(numSigFaults) + "\n\n";
        msg += "Faulty entries have not been loaded.\n\n";
        msg += "All other entries are, however, loaded.";
        msgBox.setText(msg);
        msgBox.exec();
    }
    inFile->close();
    delete inFile;
    QStringList fileList = fileName.split('/');
    this->fileName = fileList[fileList.length() - 1]; //whoops... same name as parameter in this function.
    filePath = fileName.left(fileName.length() - this->fileName.length());
    assocBuses = -1;
    isDirty = false;
    return true;
}

QVariant DBCFile::processAttributeVal(QString input, DBC_ATTRIBUTE_VAL_TYPE typ)
{
    QVariant out;
    switch (typ)
    {
    case ATTR_STRING:
        out = input;
        break;
    case ATTR_FLOAT:
        out = input.toFloat();
        break;
    case ATTR_INT:
    case ATTR_ENUM:
        out = input.toInt();
        break;
    }
    return out;
}

bool DBCFile::parseAttribute(QString inpString, DBC_ATTRIBUTE &attr)
{
    bool goodAttr = false;
    QRegularExpression regex;
    QRegularExpressionMatch match;

    regex.setPattern("\\\"*(\\w+)\\\"* \\\"*(\\w+)\\\"* (\\d+) (\\d+)");
    match = regex.match(inpString);
    //captured 1 is the name of the attribute to set up
    //captured 2 is the type of signal attribute to create.
    //captured 3 is the lower bound value for this attribute
    //captured 4 is the upper bound value for this attribute
    if (match.hasMatch())
    {
        qDebug() << "Parsing an attribute with low/high values";
        attr.name = match.captured(1);
        QString typ = match.captured(2);
        attr.attrType = ATTR_TYPE_SIG;
        attr.lower = 0;
        attr.upper = 0;
        attr.valType = ATTR_STRING;
        if (!typ.compare("INT", Qt::CaseInsensitive))
        {
            qDebug() << "INT attribute named " << attr.name;
            attr.valType = ATTR_INT;
            attr.lower = match.captured(3).toInt();
            attr.upper = match.captured(4).toInt();
            goodAttr = true;
        }
        if (!typ.compare("FLOAT", Qt::CaseInsensitive))
        {
            qDebug() << "FLOAT attribute named " << attr.name;
            attr.valType = ATTR_FLOAT;
            attr.lower = match.captured(3).toDouble();
            attr.upper = match.captured(4).toDouble();
            goodAttr = true;
        }
        if (!typ.compare("STRING", Qt::CaseInsensitive))
        {
            qDebug() << "STRING attribute named " << attr.name;
            attr.valType = ATTR_STRING;
            goodAttr = true;
        }
    }
    else
    {
        regex.setPattern("\\\"*(\\w+)\\\"* \\\"*(\\w+)\\\"* (.*)");
        match = regex.match(inpString);
        //Same as above but no upper/lower bound values.
        if (match.hasMatch())
        {
            qDebug() << "Parsing an attribute without boundaries";
            attr.name = match.captured(1);
            QString typ = match.captured(2);
            attr.lower = 0;
            attr.upper = 0;
            attr.attrType = ATTR_TYPE_SIG;

            if (!typ.compare("INT", Qt::CaseInsensitive))
            {
                qDebug() << "INT attribute named " << attr.name;
                attr.valType = ATTR_INT;
                goodAttr = true;
            }

            if (!typ.compare("FLOAT", Qt::CaseInsensitive))
            {
                qDebug() << "FLOAT attribute named " << attr.name;
                attr.valType = ATTR_FLOAT;
                goodAttr = true;
            }

            if (!typ.compare("STRING", Qt::CaseInsensitive))
            {
                qDebug() << "STRING attribute named " << attr.name;
                attr.valType = ATTR_STRING;
                goodAttr = true;
            }

            if (!typ.compare("ENUM", Qt::CaseInsensitive))
            {
                qDebug() << "ENUM attribute named " << attr.name;
                QStringList enumLst = match.captured(3).split(',');
                foreach (QString enumStr, enumLst)
                {
                    attr.enumVals.append(Utility::unQuote(enumStr));
                    qDebug() << "Enum value: " << enumStr;
                }
                attr.valType = ATTR_ENUM;
                goodAttr = true;
            }
        }
    }
    return goodAttr;
}

bool DBCFile::saveFile(QString fileName)
{
    int nodeNumber = 1;
    int msgNumber = 1;
    int sigNumber = 1;
    QFile *outFile = new QFile(fileName);
    QString nodesOutput, msgOutput, commentsOutput, valuesOutput;
    QString defaultsOutput, attrValOutput;
    bool hasExtendedMultiplexing = false;

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete outFile;
        return false;
    }

    //right now it outputs a standard hard coded boilerplate
    outFile->write("VERSION \"\"\n");
    outFile->write("\n");
    outFile->write("\n");
    outFile->write("NS_ :\n");
    outFile->write("    NS_DESC_\n");
    outFile->write("    CM_\n");
    outFile->write("    BA_DEF_\n");
    outFile->write("    BA_\n");
    outFile->write("    VAL_\n");
    outFile->write("    CAT_DEF_\n");
    outFile->write("    CAT_\n");
    outFile->write("    FILTER\n");
    outFile->write("    BA_DEF_DEF_\n");
    outFile->write("    EV_DATA_\n");
    outFile->write("    ENVVAR_DATA_\n");
    outFile->write("    SGTYPE_\n");
    outFile->write("    SGTYPE_VAL_\n");
    outFile->write("    BA_DEF_SGTYPE_\n");
    outFile->write("    BA_SGTYPE_\n");
    outFile->write("    SIG_TYPE_REF_\n");
    outFile->write("    VAL_TABLE_\n");
    outFile->write("    SIG_GROUP_\n");
    outFile->write("    SIG_VALTYPE_\n");
    outFile->write("    SIGTYPE_VALTYPE_\n");
    outFile->write("    BO_TX_BU_\n");
    outFile->write("    BA_DEF_REL_\n");
    outFile->write("    BA_REL_\n");
    outFile->write("    BA_DEF_DEF_REL_\n");
    outFile->write("    BU_SG_REL_\n");
    outFile->write("    BU_EV_REL_\n");
    outFile->write("    BU_BO_REL_\n");
    outFile->write("    SG_MUL_VAL_\n");
    outFile->write("\n");
    outFile->write("BS_: \n");

    //Build list of nodes line
    nodesOutput.append("BU_: ");
    for (int x = 0; x < dbc_nodes.count(); x++)
    {
        DBC_NODE node = dbc_nodes[x];
        if (node.name.compare("Vector__XXX", Qt::CaseInsensitive) != 0)
        {
            if (node.name.length() < 1) //detect an empty string and fill it out with something
            {
                node.name = "NODE" + QString::number(nodeNumber);
                nodeNumber++;
            }
            nodesOutput.append(node.name + " ");
            if (node.comment.length() > 0)
            {
                commentsOutput.append("CM_ BU_ " + node.name + " \"" + node.comment + "\";\n");
            }
            if (node.attributes.count() > 0)
            {
                foreach (DBC_ATTRIBUTE_VALUE val, node.attributes) {
                    attrValOutput.append("BA_ \"" + val.attrName + "\" BU_ ");
                    switch (val.value.type())
                    {
                    case QMetaType::QString:
                        attrValOutput.append("\"" + val.value.toString() + "\";\n");
                        break;
                    default:
                        attrValOutput.append(val.value.toString() + ";\n");
                        break;
                    }
                }
            }
        }
    }
    nodesOutput.append("\n");
    outFile->write(nodesOutput.toUtf8());

    //Go through all messages one at at time issuing the message line then all signals in there too.
    for (int x = 0; x < messageHandler->getCount(); x++)
    {
        DBC_MESSAGE *msg = messageHandler->findMsgByIdx(x);

        if (msg->name.length() < 1) //detect an empty string and fill it out with something
        {
            msg->name = "MSG" + QString::number(msgNumber);
            msgNumber++;
        }

        uint32_t ID = msg->ID;
        if (msg->ID > 0x7FF || msg->extendedID) msg->ID += 0x80000000ul; //set bit 31 if this ID is extended.

        msgOutput.append("BO_ " + QString::number(ID) + " " + msg->name + ": " + QString::number(msg->len) +
                         " " + msg->sender->name + "\n");
        if (msg->comment.length() > 0)
        {
            commentsOutput.append("CM_ BO_ " + QString::number(ID) + " \"" + msg->comment + "\";\n");
        }

        //If this message has attributes then compile them into attributes list to output later on.
        if (msg->attributes.count() > 0)
        {
            foreach (DBC_ATTRIBUTE_VALUE val, msg->attributes) {
                attrValOutput.append("BA_ \"" + val.attrName + "\" BO_ " + QString::number(ID) + " ");
                switch (val.value.type())
                {
                case QMetaType::QString:
                    attrValOutput.append("\"" + val.value.toString() + "\";\n");
                    break;
                default:
                    attrValOutput.append(val.value.toString() + ";\n");
                    break;
                }
            }
        }

        for (int s = 0; s < msg->sigHandler->getCount(); s++)
        {
            DBC_SIGNAL *sig = msg->sigHandler->findSignalByIdx(s);

            if (sig->name.length() < 1) //detect an empty string and fill it out with something
            {
                sig->name = "SIG" + QString::number(sigNumber);
                sigNumber++;
            }

            msgOutput.append("   SG_ " + sig->name);

            if (sig->isMultiplexed)
            {
                msgOutput.append(" m" + QString::number(sig->multiplexLowValue));
            }
            if (sig->isMultiplexor)
            {
                if (!sig->isMultiplexed) msgOutput.append(" ");
                msgOutput.append("M");
            }
            //check for the two telltale signs that we've got extended multiplexing going on.
            if (sig->isMultiplexed && sig->isMultiplexor) hasExtendedMultiplexing = true;
            if (sig->multiplexLowValue != sig->multiplexHighValue) hasExtendedMultiplexing = true;

            msgOutput.append(" : " + QString::number(sig->startBit) + "|" + QString::number(sig->signalSize) + "@");

            switch (sig->valType)
            {
            case UNSIGNED_INT:
                if (sig->intelByteOrder) msgOutput.append("1+");
                else msgOutput.append("0+");
                break;
            case SIGNED_INT:
                if (sig->intelByteOrder) msgOutput.append("1-");
                else msgOutput.append("0-");
                break;
            case SP_FLOAT:
                if (sig->intelByteOrder) msgOutput.append("5-");
                else msgOutput.append("2-");
                break;
            case DP_FLOAT:
                if (sig->intelByteOrder) msgOutput.append("6-");
                else msgOutput.append("3-");
                break;
            case STRING:
                msgOutput.append("4-");
                break;
            default:
                msgOutput.append("0-");
                break;
            }
            msgOutput.append(" (" + QString::number(sig->factor) + "," + QString::number(sig->bias) + ") [" +
                             QString::number(sig->min) + "|" + QString::number(sig->max) + "] \"" + sig->unitName
                             + "\" " + sig->receiver->name + "\n");
            if (sig->comment.length() > 0)
            {
                commentsOutput.append("CM_ SG_ " + QString::number(ID) + " " + sig->name + " \"" + sig->comment + "\";\n");
            }

            //if this signal has attributes then compile them in a special list of attributes
            if (sig->attributes.count() > 0)
            {
                foreach (DBC_ATTRIBUTE_VALUE val, sig->attributes) {
                    attrValOutput.append("BA_ \"" + val.attrName + "\" SG_ " + QString::number(ID) + " " + sig->name + " ");
                    switch (val.value.type())
                    {
                    case QMetaType::QString:
                        attrValOutput.append("\"" + val.value.toString() + "\";\n");
                        break;
                    default:
                        attrValOutput.append(val.value.toString() + ";\n");
                        break;
                    }
                }
            }

            if (sig->valList.count() > 0)
            {
                valuesOutput.append("VAL_ " + QString::number(ID) + " " + sig->name);
                for (int v = 0; v < sig->valList.count(); v++)
                {
                    DBC_VAL_ENUM_ENTRY val = sig->valList[v];
                    valuesOutput.append(" " + QString::number(val.value) + " \"" + val.descript +"\"");
                }
                valuesOutput.append(";\n");
            }
        }
        msgOutput.append("\n");
        //write it out every message so the string doesn't end up too huge
        outFile->write(msgOutput.toUtf8());
        msgOutput.clear(); //got to reset it after writing
    }

    //Now dump out all of the stored attributes
    for (int x = 0; x < dbc_attributes.count(); x++)
    {
        msgOutput.append("BA_DEF_ ");
        switch (dbc_attributes[x].attrType)
        {
        case ATTR_TYPE_GENERAL:
            break;
        case ATTR_TYPE_NODE:
            msgOutput.append("BU_ ");
            break;
        case ATTR_TYPE_MESSAGE:
            msgOutput.append("BO_ ");
            break;
        case ATTR_TYPE_SIG:
            msgOutput.append("SG_ ");
            break;
        }

        msgOutput.append("\"" + dbc_attributes[x].name + "\" ");

        switch (dbc_attributes[x].valType)
        {
        case ATTR_INT:
            msgOutput.append("INT " + QString::number(dbc_attributes[x].lower) + " " + QString::number(dbc_attributes[x].upper));
            break;
        case ATTR_FLOAT:
            msgOutput.append("FLOAT " + QString::number(dbc_attributes[x].lower) + " " + QString::number(dbc_attributes[x].upper));
            break;
        case ATTR_STRING:
            msgOutput.append("STRING ");
            break;
        case ATTR_ENUM:
            msgOutput.append("ENUM ");
            foreach (QString str, dbc_attributes[x].enumVals)
            {
                msgOutput.append("\"" + str + "\",");
            }
            msgOutput.truncate(msgOutput.length() - 1); //remove trailing ,
            break;
        }

        msgOutput.append(";\n");

        outFile->write(msgOutput.toUtf8());
        msgOutput.clear(); //got to reset it after writing

        if (dbc_attributes[x].defaultValue.isValid())
        {
            defaultsOutput.append("BA_DEF_DEF_ \"" + dbc_attributes[x].name + "\" ");
            switch (dbc_attributes[x].valType)
            {
            case ATTR_STRING:
                defaultsOutput.append("\"" + dbc_attributes[x].defaultValue.toString() + "\";\n");
                break;
            case ATTR_ENUM:
                defaultsOutput.append("\"" + dbc_attributes[x].enumVals[dbc_attributes[x].defaultValue.toInt()] + "\";\n");
                break;
            case ATTR_INT:
            case ATTR_FLOAT:
                defaultsOutput.append(dbc_attributes[x].defaultValue.toString() + ";\n");
                break;
            }
        }
    }

    //extended multiplexing uses SG_MUL_VAL_ to specify the relationships. If a signal is marked
    //as multiplexed then output a record for it. That's the most complete option. We've already
    //given the single value multiplex above for backward compatibility with things that don't support extended mode
    if (hasExtendedMultiplexing)
    {
        for (int x = 0; x < messageHandler->getCount(); x++)
        {
            DBC_MESSAGE *msg = messageHandler->findMsgByIdx(x);

            uint32_t ID = msg->ID;
            if (msg->ID > 0x7FF || msg->extendedID) msg->ID += 0x80000000ul; //set bit 31 if this ID is extended.

            for (int s = 0; s < msg->sigHandler->getCount(); s++)
            {
                DBC_SIGNAL *sig = msg->sigHandler->findSignalByIdx(s);

                if (sig->isMultiplexed)
                {
                    msgOutput.append("SG_MUL_VAL_ " + QString::number(ID) + " ");
                    msgOutput.append(sig->name + " " + sig->parentMessage->name + " ");
                    msgOutput.append(QString::number(sig->multiplexLowValue) + "-" + QString::number(sig->multiplexHighValue) + ";");
                    msgOutput.append("\n");
                    outFile->write(msgOutput.toUtf8());
                    msgOutput.clear(); //got to reset it after writing
                }
            }
        }
    }

    //now write out all of the accumulated comments and value tables from above
    outFile->write(attrValOutput.toUtf8());
    outFile->write(defaultsOutput.toUtf8());
    outFile->write(commentsOutput.toUtf8());
    outFile->write(valuesOutput.toUtf8());

    attrValOutput.clear();
    defaultsOutput.clear();
    commentsOutput.clear();
    valuesOutput.clear();

    outFile->close();
    delete outFile;

    isDirty = false;

    QStringList fileList = fileName.split('/');
    this->fileName = fileList[fileList.length() - 1]; //whoops... same name as parameter in this function.
    filePath = fileName.left(fileName.length() - this->fileName.length());
    return true;
}

void DBCHandler::saveDBCFile(int idx)
{
    QSettings settings;

    if (loadedFiles.count() == 0) return;
    if (idx < 0) return;
    if (idx >= loadedFiles.count()) return;

    QString filename;
    QFileDialog dialog;

    QStringList filters;
    filters.append(QString(tr("DBC File (*.dbc)")));

    dialog.setDirectory(settings.value("DBC/LoadSaveDirectory", dialog.directory().path()).toString());
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.selectFile(loadedFiles[idx].getFullFilename());

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        if (!filename.contains('.')) filename += ".dbc";
        loadedFiles[idx].saveFile(filename);
        settings.setValue("DBC/LoadSaveDirectory", dialog.directory().path());
    }
}

int DBCHandler::createBlankFile()
{
    DBCFile newFile;
    DBC_ATTRIBUTE attr;

    //add our custom attributes to the new file so that we know they're already there.
    attr.attrType = ATTR_TYPE_MESSAGE;
    attr.defaultValue = QApplication::palette().color(QPalette::Base).name();
    qDebug() << attr.defaultValue;
    attr.enumVals.clear();
    attr.lower = 0;
    attr.upper = 0;
    attr.name = "GenMsgBackgroundColor";
    attr.valType = ATTR_STRING;
    newFile.dbc_attributes.append(attr);

    attr.attrType = ATTR_TYPE_MESSAGE;
    attr.defaultValue = QApplication::palette().color(QPalette::WindowText).name();
    qDebug() << attr.defaultValue;
    attr.enumVals.clear();
    attr.lower = 0;
    attr.upper = 0;
    attr.name = "GenMsgForegroundColor";
    attr.valType = ATTR_STRING;
    newFile.dbc_attributes.append(attr);

    attr.attrType = ATTR_TYPE_MESSAGE;
    attr.defaultValue = 0;
    attr.enumVals.clear();
    attr.lower = 0;
    attr.upper = 0;
    attr.name = "matchingcriteria";
    attr.valType = ATTR_INT;
    newFile.dbc_attributes.append(attr);

    attr.attrType = ATTR_TYPE_MESSAGE;
    attr.defaultValue = 0;
    attr.enumVals.clear();
    attr.lower = 0;
    attr.upper = 0;
    attr.name = "filterlabeling";
    attr.valType = ATTR_INT;
    newFile.dbc_attributes.append(attr);

    DBC_NODE falseNode;
    falseNode.name = "Vector__XXX";
    falseNode.comment = "Default node if none specified";
    newFile.dbc_nodes.append(falseNode);
    newFile.setAssocBus(-1);

    loadedFiles.append(newFile);
    return loadedFiles.count();
}

DBCFile* DBCHandler::loadDBCFile(QString filename)
{
    DBCFile newFile;
    if (newFile.loadFile(filename))
    {
        loadedFiles.append(newFile);
    }
    else
    {
        //createBlankFile();
    }
    if (loadedFiles.count()> 0) return &loadedFiles.last();
    else return nullptr;
}

//the only reason to even bother sending the index is to see if
//the user wants to replace an already loaded DBC.
//Otherwise add a new one. Well, always add a new one.
//If a valid index is passed we'll remove that one and then commence
//adding. Otherwise, just go straight to adding.
DBCFile* DBCHandler::loadDBCFile(int idx)
{
   if (idx > -1 && idx < loadedFiles.count()) removeDBCFile(idx);

    QString filename;
    QFileDialog dialog;
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("DBC File (*.dbc)")));

    dialog.setDirectory(settings.value("DBC/LoadSaveDirectory", dialog.directory().path()).toString());
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        //right now there is only one file type that can be loaded here so just do it.
        settings.setValue("DBC/LoadSaveDirectory", dialog.directory().path());
        return loadDBCFile(filename);
    }

    return nullptr;
}

DBCFile* DBCHandler::loadSecretCSVFile(QString filename)
{
    DBCFile *thisFile;
    DBC_MESSAGE *pMsg;
    DBC_SIGNAL *pSig;
    QByteArray line;
    int lineCounter = 0;
    createBlankFile();
    thisFile = &loadedFiles.last();

    QFile *inFile = new QFile(filename);

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return nullptr;
    }

    //burn first two lines. contains header
    line = inFile->readLine().simplified().toUpper();
    line = inFile->readLine().simplified().toUpper();

    while (!inFile->atEnd())
    {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }
        line = inFile->readLine().simplified().toUpper();
        qDebug() << line;

        QList<QByteArray> tokens = line.split(',');
        if (tokens.length() == 10)
        {
            //Message,CAN ID,Signal,Short Name,Start Byte,Start Bit,Len,Data,Range,Conversion
            //Whl_Rotational_Stat_CheckVal_CE,$0C0,Wheel Rotational Status Check Data,WhlRotatStatChkData, 0, 7, 40,PKT,N/A,N/A
            //                0                1             2                                3            4  5  6  7    8   9
            if (tokens[0].length() > 2) //start of a signal def that starts a new message
            {
                DBC_MESSAGE msg;
                msg.name = tokens[0];
                msg.ID =   tokens[1].mid(1).toLong(nullptr, 16);
                msg.comment = "";
                msg.len = 8;
                msg.sender = thisFile->findNodeByIdx(0);
                msg.bgColor = QColor(thisFile->findAttributeByName("GenMsgBackgroundColor")->defaultValue.toString());
                msg.fgColor = QColor(thisFile->findAttributeByName("GenMsgForegroundColor")->defaultValue.toString());
                thisFile->messageHandler->addMessage(msg);
                pMsg = thisFile->messageHandler->findMsgByID(msg.ID);

                DBC_SIGNAL sig;
                sig.parentMessage = pMsg;
                sig.name = tokens[3];
                sig.comment = tokens[2];
                int startBit = (tokens[4].toInt() * 8) + tokens[5].toInt();
                sig.startBit = startBit;
                sig.signalSize = tokens[6].toInt();
                sig.intelByteOrder = false; //always for global-a?
                sig.receiver = thisFile->findNodeByIdx(0);
                QList<QByteArray> rangeToks = tokens[8].split('-');
                if (rangeToks.length() == 2)
                {
                    sig.min = rangeToks[0].simplified().toDouble();
                    sig.max = rangeToks[1].simplified().toDouble();
                }
                if (tokens[9].startsWith("E = N")) //not a value table, instead do scaling and bias
                {
                    QList<QByteArray> scalingToks = tokens[9].simplified().split(' ');
                    sig.factor = scalingToks[4].toDouble();
                    if (scalingToks.count() > 5)
                    {
                        if (scalingToks[5] == "+")
                        {
                            sig.bias = scalingToks[6].toDouble();
                        }
                        if (scalingToks[5] == "-")
                        {
                            sig.bias = scalingToks[6].toDouble() * 1.0;
                        }
                    }
                    else
                    {
                        sig.factor = 1;
                        sig.bias = 0;
                    }
                }
                else if (tokens[9].startsWith("$")) //one or more values table entries
                {
                    //$0=Inactive
                    QList<QByteArray> valToks = tokens[9].simplified().mid(1).split('=');
                    DBC_VAL_ENUM_ENTRY entry;
                    entry.value = valToks[0].toInt();
                    entry.descript = valToks[1];
                    sig.valList.append(entry);
                }
                pMsg->sigHandler->addSignal(sig);
                pSig = pMsg->sigHandler->findSignalByIdx(pMsg->sigHandler->getCount()-1);
            }
            //      0     1    2      3            4         5       6   7    8     9
            //Message,CAN ID,Signal,Short Name,Start Byte,Start Bit,Len,Data,Range,Conversion
            //,,Wheel Rotational Status Check Data : Left Driven Sequence Number,WRSCD_LftDrvnSqNm, 0, 7, 2,UNM,0 - 3 ,E = N * 1
            //    2                                                                3                4  5  6 7    8       9
            else if (tokens[2].length() > 2) //signal definition continuation of previous message
            {
                DBC_SIGNAL sig;
                sig.parentMessage = pMsg;
                sig.name = tokens[3];
                sig.comment = tokens[2];
                int startBit = (tokens[4].toInt() * 8) + tokens[5].toInt();
                sig.startBit = startBit;
                sig.signalSize = tokens[6].toInt();
                sig.intelByteOrder = false; //always for global-a?
                sig.receiver = thisFile->findNodeByIdx(0);
                QList<QByteArray> rangeToks = tokens[8].split('-');
                if (rangeToks.length() == 2)
                {
                    sig.min = rangeToks[0].simplified().toDouble();
                    sig.max = rangeToks[1].simplified().toDouble();
                }
                if (tokens[9].startsWith("E = N")) //not a value table, instead do scaling and bias
                {
                    QList<QByteArray> scalingToks = tokens[9].simplified().split(' ');
                    sig.factor = scalingToks[4].toDouble();
                    if (scalingToks.count() > 5)
                    {
                        if (scalingToks[5] == "+")
                        {
                            sig.bias = scalingToks[6].toDouble();
                        }
                        if (scalingToks[5] == "-")
                        {
                            sig.bias = scalingToks[6].toDouble() * 1.0;
                        }
                    }
                    else
                    {
                        sig.bias = 0;
                        sig.factor = 1;
                    }
                }
                else if (tokens[9].startsWith("$")) //one or more values table entries
                {
                    //$0=Inactive
                    QList<QByteArray> valToks = tokens[9].simplified().mid(1).split('=');
                    DBC_VAL_ENUM_ENTRY entry;
                    entry.value = valToks[0].toInt();
                    entry.descript = valToks[1];
                    sig.valList.append(entry);
                }
                pMsg->sigHandler->addSignal(sig);
                pSig = pMsg->sigHandler->findSignalByIdx(pMsg->sigHandler->getCount()-1);
            }
            else if (tokens[9].length() > 2) //additional values
            {
                //$0=Inactive
                QList<QByteArray> valToks = tokens[9].simplified().mid(1).split('=');
                DBC_VAL_ENUM_ENTRY entry;
                entry.value = valToks[0].toInt();
                entry.descript = valToks[1];
                pSig->valList.append(entry);
            }
        }
    }

    thisFile->setDirtyFlag();
    return thisFile;
}

DBCFile* DBCHandler::loadJSONFile(QString filename)
{
     QSettings settings;
     DBCFile *thisFile;
     DBC_MESSAGE *pMsg;

         createBlankFile();
         thisFile = &loadedFiles.last();

         QFile *inFile = new QFile(filename);
         if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
         {
             qDebug() << "Could not open JSON file for reading.";
             delete inFile;
             return nullptr;
         }
         QByteArray wholeFileData = inFile->readAll();
         inFile->close();
         delete inFile;
         //qDebug() << "Data Length: " << wholeFileData.length();
         QJsonDocument jsonDoc = QJsonDocument::fromJson(wholeFileData);
         if (jsonDoc.isNull())
         {
             qDebug() << "Couldn't load and parse the JSON file for some reason.";
             return nullptr;
         }
         qDebug() << "Loaded JSON";

         QJsonObject jsonObj = jsonDoc.object();
         QJsonObject jsonMessages = jsonObj["messages"].toObject();

         QJsonObject::iterator iter;
         for (iter = jsonMessages.begin(); iter != jsonMessages.end(); iter++)
         {
             qDebug() << iter.key();
             DBC_MESSAGE msg;
             msg.ID = static_cast<uint32_t>(iter->toObject().find("message_id").value().toInt());
             msg.name = QString(iter.key().toUtf8());
             msg.len = static_cast<unsigned int>(iter->toObject().find("length_bytes").value().toInt());
             msg.sender = thisFile->findNodeByIdx(0);
             QString nodeName = iter->toObject().find("originNode").value().toString();
             msg.sender = thisFile->findNodeByName(nodeName);
             msg.bgColor = QColor(thisFile->findAttributeByName("GenMsgBackgroundColor")->defaultValue.toString());
             msg.fgColor = QColor(thisFile->findAttributeByName("GenMsgForegroundColor")->defaultValue.toString());
             if (!msg.sender && nodeName.length() > 1)
             {
                 DBC_NODE node;
                 node.name = nodeName;
                 thisFile->dbc_nodes.append(node);
                 msg.sender = thisFile->findNodeByName(nodeName);
             }
             if (nodeName.length() < 2) msg.sender = thisFile->findNodeByIdx(0);
             thisFile->messageHandler->addMessage(msg);
             pMsg = thisFile->messageHandler->findMsgByID(msg.ID);
             if (!pMsg)
             {
                 qDebug() << "pMsg was null... I have no idea how that is possible. DEBUG ME";
                 return nullptr;
             }

             QJsonObject jsonSigs = iter->toObject().find("signals").value().toObject();
             QJsonObject::iterator sigIter;
             for (sigIter = jsonSigs.begin(); sigIter != jsonSigs.end(); sigIter++)
             {
                qDebug() << sigIter.key();
                DBC_SIGNAL sig;
                QJsonObject sigObj = sigIter->toObject();
                if (sigObj.isEmpty())
                {
                    qDebug() << "EMPTY!?";
                }
                sig.name = QString(sigIter.key().toUtf8());
                sig.factor = sigObj.find("scale").value().toDouble();
                sig.bias = sigObj.find("offset").value().toDouble();
                sig.max = sigObj.find("max").value().toDouble();
                sig.min = sigObj.find("min").value().toDouble();
                sig.startBit = sigObj.find("start_position").value().toInt();
                sig.unitName = sigObj.find("units").value().toString();
                sig.signalSize = sigObj.find("width").value().toInt();
                sig.isMultiplexed = false;
                sig.isMultiplexor = false;
                sig.parentMessage = pMsg;
                if (!sigObj.find("mux_id")->isUndefined())
                {
                    QJsonValue muxVal = sigObj.find("mux_id").value();
                    sig.multiplexLowValue = muxVal.toInt();
                    sig.multiplexHighValue = sig.multiplexLowValue;
                    sig.isMultiplexed = true;
                }
                else
                {
                    sig.multiplexLowValue = 0;
                    sig.multiplexHighValue = 0;
                }
                QJsonValue muxerVal = sigObj.find("is_muxer").value();
                if (!muxerVal.isNull())
                {
                    if (muxerVal.toBool())
                    {
                        sig.isMultiplexor = true;
                    }
                }

                QJsonObject valuesObj = sigObj.find("value_description")->toObject();
                if (!valuesObj.isEmpty())
                {
                    QJsonObject::iterator valIter;
                    for (valIter = valuesObj.begin(); valIter != valuesObj.end(); valIter++)
                    {
                        DBC_VAL_ENUM_ENTRY valEnum;
                        valEnum.value = valIter.value().toInt();
                        valEnum.descript = valIter.key().toUtf8();
                        sig.valList.append(valEnum);
                    }
                }

                QJsonArray rxArray = sigObj.find("receivers")->toArray();
                if (rxArray.size() < 1) sig.receiver = thisFile->findNodeByIdx(0);
                else
                {
                    qDebug() << rxArray[0].toString();
                    sig.receiver = thisFile->findNodeByName(rxArray[0].toString());
                    if (!sig.receiver && rxArray[0].toString().length() > 1)
                    {
                        DBC_NODE node;
                        node.name = rxArray[0].toString();
                        thisFile->dbc_nodes.append(node);
                        sig.receiver = thisFile->findNodeByName(rxArray[0].toString());
                    }
                    if (!sig.receiver) sig.receiver = thisFile->findNodeByIdx(0);
                }
                if (!sigObj.find("endianness").value().toString().compare("BIG"))
                {
                    sig.intelByteOrder = false;
                }
                else sig.intelByteOrder = true;

                if (!sigObj.find("signedness").value().toString().compare("UNSIGNED"))
                {
                    sig.valType = DBC_SIG_VAL_TYPE::UNSIGNED_INT;
                }
                else sig.valType = DBC_SIG_VAL_TYPE::SIGNED_INT;

                pMsg->sigHandler->addSignal(sig);
                if (sig.isMultiplexor) //if this signal was the multiplexor then store that info
                {
                    DBC_SIGNAL *pSig = pMsg->sigHandler->findSignalByName(sig.name);
                    if (pSig) pMsg->multiplexorSignal = pSig;
                }
             }
         }

         for (int x = 0; x < thisFile->messageHandler->getCount(); x++)
         {
             DBC_MESSAGE *msg = thisFile->messageHandler->findMsgByIdx(x);
             for (int y = 0; y < msg->sigHandler->getCount(); y++)
             {
                 DBC_SIGNAL *sig = msg->sigHandler->findSignalByIdx(y);
                 //if this doesn't have a multiplex parent set but is multiplexed then it must have used
                 //simple multiplexing instead of any extended specification. So, fill in the multiplexor signal here
                 //and also write the extended entry for it too.
                 if (sig->isMultiplexed && (sig->multiplexParent == nullptr) )
                 {
                     sig->multiplexParent = msg->multiplexorSignal;
                     msg->multiplexorSignal->multiplexedChildren.append(sig);
                 }
             }
         }

         thisFile->setDirtyFlag();
         return thisFile;
}

void DBCHandler::removeDBCFile(int idx)
{
    if (loadedFiles.count() == 0) return;
    if (idx < 0) return;
    if (idx >= loadedFiles.count()) return;
    loadedFiles.removeAt(idx);
}

void DBCHandler::removeAllFiles()
{
    loadedFiles.clear();
}

void DBCHandler::swapFiles(int pos1, int pos2)
{
    if (loadedFiles.count() < 2) return;
    if (pos1 < 0) return;
    if (pos1 >= loadedFiles.count()) return;
    if (pos2 < 0) return;
    if (pos2 >= loadedFiles.count()) return;

    loadedFiles.swapItemsAt(pos1, pos2);
}

/*
 * Convenience function that encapsulates a whole lot of the details.
 * You give it a canbus frame and it'll tell you whether there is a loaded DBC file that can
 * interpret that frame for you.
 * Returns nullptr if there is no message definition that matches.
*/
DBC_MESSAGE* DBCHandler::findMessage(const CANFrame &frame)
{
    for(int i = 0; i < loadedFiles.count(); i++)
    {
        if (loadedFiles[i].getAssocBus() == -1 || frame.bus == loadedFiles[i].getAssocBus())
        {
            DBC_MESSAGE* msg = loadedFiles[i].messageHandler->findMsgByID(frame.frameId());
            if (msg != nullptr) return msg;
        }
    }
    return nullptr;
}

DBC_MESSAGE* DBCHandler::findMessage(uint32_t id)
{
    for(int i = 0; i < loadedFiles.count(); i++)
    {
        DBC_MESSAGE* msg = loadedFiles[i].messageHandler->findMsgByID(id);
        if (msg != nullptr)
        {
            return msg;
        }
    }
    return nullptr;
}

// This function won't care which bus the DBC file is associated, but will return any message as long as ID matches and the file
// has filter labeling enabled.
// Returns the found message as well as the matching criteria (exact/J1939/GMLAN) 
// Used for quickly populating the Frame Filtering section with interpreted values
DBC_MESSAGE* DBCHandler::findMessageForFilter(uint32_t id, MatchingCriteria_t * matchingCriteria)
{
    for(int i = 0; i < loadedFiles.count(); i++)
    {
        if (loadedFiles[i].messageHandler->filterLabeling())
        {
            DBC_MESSAGE* msg = loadedFiles[i].messageHandler->findMsgByID(id);
            if (msg != nullptr) 
            {
                if (matchingCriteria) *matchingCriteria = loadedFiles[i].messageHandler->getMatchingCriteria();
                return msg;
            }
        }
    }
    return nullptr;
}


/*
 * As above, a real shortcut function that searches all files in order to try to find a message with the given name
*/
DBC_MESSAGE* DBCHandler::findMessage(const QString msgName)
{
    DBC_MESSAGE *msg = nullptr;
    for(int i = 0; i < loadedFiles.count(); i++)
    {
        DBCFile * file = getFileByIdx(i);
        msg = file->messageHandler->findMsgByName(msgName);
        if (msg) return msg; //if it's not null then we have a match so return it
    }
    return nullptr; //no match, tough luck, return null
}

int DBCHandler::getFileCount()
{
    return loadedFiles.count();
}

DBCFile* DBCHandler::getFileByIdx(int idx)
{
    if (loadedFiles.count() == 0) return nullptr;
    if (idx < 0) return nullptr;
    if (idx >= loadedFiles.count()) return nullptr;
    return &loadedFiles[idx];
}

DBCFile* DBCHandler::getFileByName(QString name)
{
    if (loadedFiles.count() == 0) return nullptr;
    for (int i = 0; i < loadedFiles.count(); i++)
    {
        if (loadedFiles[i].getFilename().compare(name, Qt::CaseInsensitive) == 0)
        {
            return &loadedFiles[i];
        }
    }
    return nullptr;
}

DBCHandler::DBCHandler()
{
    // Load previously saved DBC file settings
    QSettings settings;
    int filecount = settings.value("DBC/FileCount", 0).toInt();
    qDebug() << "Previously loaded DBC file count: " << filecount;
    for (int i=0; i<filecount; i++)
    {
        QString filename = settings.value("DBC/Filename_" + QString(i),"").toString();
        DBCFile * file = loadDBCFile(filename);
        if (file)
        {
            int bus = settings.value("DBC/AssocBus_" + QString(i),0).toInt();
            file->setAssocBus(bus);

            MatchingCriteria_t matchingCriteria = (MatchingCriteria_t)settings.value("DBC/MatchingCriteria_" + QString(i),0).toInt();

            DBC_ATTRIBUTE attr;

            attr.attrType = ATTR_TYPE_MESSAGE;
            attr.defaultValue = matchingCriteria;
            attr.enumVals.clear();
            attr.lower = 0;
            attr.upper = 0;
            attr.name = "matchingcriteria";
            attr.valType = ATTR_INT;
            file->dbc_attributes.append(attr);
            file->messageHandler->setMatchingCriteria(matchingCriteria);

            bool filterLabeling = settings.value("DBC/FilterLabeling_" + QString(i),0).toBool();
            attr.attrType = ATTR_TYPE_MESSAGE;
            attr.defaultValue = filterLabeling;
            attr.enumVals.clear();
            attr.lower = 0;
            attr.upper = 0;
            attr.name = "filterlabeling";
            attr.valType = ATTR_INT;
            file->dbc_attributes.append(attr);
            file->messageHandler->setFilterLabeling(filterLabeling);

            qInfo() << "Loaded DBC file" << filename << " (bus:" << bus
                << ", Matching Criteria:" << (int)matchingCriteria << "Filter labeling: " << (filterLabeling?"enabled":"disabled") << ")";
        }
    }
}

DBCHandler* DBCHandler::getReference()
{
    if (!instance) instance = new DBCHandler();
    return instance;
}
