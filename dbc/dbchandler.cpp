#include "dbchandler.h"

#include <QFile>
#include <QRegularExpression>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QPalette>
#include <QSettings>
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
    Q_UNUSED(sig);
    //if (sigs.removeAll(*sig) > 0) return true;
    return false;
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

bool DBCMessageHandler::addMessage(DBC_MESSAGE &msg)
{
    messages.append(msg);
    return true;
}

bool DBCMessageHandler::removeMessage(DBC_MESSAGE *msg)
{
    Q_UNUSED(msg);
    //if (messages.removeAll(*msg) > 0) return true;
    return false;
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

DBC_ATTRIBUTE *DBCFile::findAttributeByName(QString name)
{
    if (dbc_attributes.length() == 0) return nullptr;
    for (int i = 0; i < dbc_attributes.length(); i++)
    {
        if (dbc_attributes[i].name.compare(name, Qt::CaseInsensitive) == 0)
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
        msg.ID = match.captured(1).toULong() & 0x7FFFFFFFul; //the ID is always stored in decimal format
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
    bool isMultiplexor = false;
    //bool isMultiplexed = false;
    DBC_SIGNAL sig;

    sig.multiplexValue = 0;
    sig.isMultiplexed = false;
    sig.isMultiplexor = false;

    qDebug() << "Found a SG line";
    regex.setPattern("^SG\\_ *(\\w+) +M *: *(\\d+)\\|(\\d+)@(\\d+)([\\+|\\-]) \\(([0-9.+\\-eE]+),([0-9.+\\-eE]+)\\) \\[([0-9.+\\-eE]+)\\|([0-9.+\\-eE]+)\\] \\\"(.*)\\\" (.*)");

    match = regex.match(line);
    if (match.hasMatch())
    {
        qDebug() << "Multiplexor signal";
        isMultiplexor = true;
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
            sig.multiplexValue = match.captured(2).toInt();
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
            if (isMultiplexor) msg->multiplexorSignal = msg->sigHandler->findSignalByName(sig.name);
            return msg->sigHandler->findSignalByName(sig.name);
        }
        else return nullptr;
    }

    return nullptr;
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
        DBC_MESSAGE *msg = messageHandler->findMsgByID(match.captured(1).toUInt());
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
                        val.value = match.captured(1).toInt();
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
            DBC_MESSAGE *foundMsg = messageHandler->findMsgByID(match.captured(2).toUInt());
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
            DBC_MESSAGE *foundMsg = messageHandler->findMsgByID(match.captured(2).toUInt());
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
            case QSTRING:
                found->defaultValue = match.captured(2);
                break;
            case QFLOAT:
                found->defaultValue = match.captured(2).toFloat();
                break;
            case QINT:
                found->defaultValue = match.captured(2).toInt();
                break;
            case ENUM:
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

void DBCFile::loadFile(QString fileName)
{
    QFile *inFile = new QFile(fileName);
    QString line, rawLine;
    QRegularExpression regex;
    QRegularExpressionMatch match;
    DBC_MESSAGE *currentMessage = nullptr;
    DBC_ATTRIBUTE attr;
    int numSigFaults = 0, numMsgFaults = 0;

    bool inMultilineBU = false;

    qDebug() << "DBC File: " << fileName;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return;
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
                    attr.attrType = SIG;
                    dbc_attributes.append(attr);
                }
            }
            if (line.startsWith("BA_DEF_ BO_ ")) //definition of a message attribute
            {
                qDebug() << "Found a BO attribute line";

                if (parseAttribute(line.right(line.length() - 12), attr))
                {
                    qDebug() << "Success";
                    attr.attrType = MESSAGE;
                    dbc_attributes.append(attr);
                }
            }
            if (line.startsWith("BA_DEF_ BU_ ")) //definition of a node attribute
            {
                qDebug() << "Found a BU attribute line";

                if (parseAttribute(line.right(line.length() - 12), attr))
                {
                    //qDebug() << "Success";
                    attr.attrType = NODE;
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
        attr.attrType = MESSAGE;
        attr.defaultValue = QApplication::palette().color(QPalette::Base).name();
        attr.enumVals.clear();
        attr.lower = 0;
        attr.upper = 0;
        attr.name = "GenMsgBackgroundColor";
        attr.valType = QSTRING;
        dbc_attributes.append(attr);
        bgAttr = findAttributeByName("GenMsgBackgroundColor");
    }

    DBC_ATTRIBUTE *fgAttr = findAttributeByName("GenMsgForegroundColor");
    if (!fgAttr)
    {
        attr.attrType = MESSAGE;
        attr.defaultValue = QApplication::palette().color(QPalette::WindowText).name();
        attr.enumVals.clear();
        attr.lower = 0;
        attr.upper = 0;
        attr.name = "GenMsgForegroundColor";
        attr.valType = QSTRING;
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
}

QVariant DBCFile::processAttributeVal(QString input, DBC_ATTRIBUTE_VAL_TYPE typ)
{
    QVariant out;
    switch (typ)
    {
    case QSTRING:
        out = input;
        break;
    case QFLOAT:
        out = input.toFloat();
        break;
    case QINT:
    case ENUM:
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
        attr.attrType = SIG;
        attr.lower = 0;
        attr.upper = 0;
        attr.valType = QSTRING;
        if (!typ.compare("INT", Qt::CaseInsensitive))
        {
            qDebug() << "INT attribute named " << attr.name;
            attr.valType = QINT;
            attr.lower = match.captured(3).toInt();
            attr.upper = match.captured(4).toInt();
            goodAttr = true;
        }
        if (!typ.compare("FLOAT", Qt::CaseInsensitive))
        {
            qDebug() << "FLOAT attribute named " << attr.name;
            attr.valType = QFLOAT;
            attr.lower = match.captured(3).toDouble();
            attr.upper = match.captured(4).toDouble();
            goodAttr = true;
        }
        if (!typ.compare("STRING", Qt::CaseInsensitive))
        {
            qDebug() << "STRING attribute named " << attr.name;
            attr.valType = QSTRING;
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
            attr.attrType = SIG;

            if (!typ.compare("INT", Qt::CaseInsensitive))
            {
                qDebug() << "INT attribute named " << attr.name;
                attr.valType = QINT;
                goodAttr = true;
            }

            if (!typ.compare("FLOAT", Qt::CaseInsensitive))
            {
                qDebug() << "FLOAT attribute named " << attr.name;
                attr.valType = QFLOAT;
                goodAttr = true;
            }

            if (!typ.compare("STRING", Qt::CaseInsensitive))
            {
                qDebug() << "STRING attribute named " << attr.name;
                attr.valType = QSTRING;
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
                attr.valType = ENUM;
                goodAttr = true;
            }
        }
    }
    return goodAttr;
}

void DBCFile::saveFile(QString fileName)
{
    int nodeNumber = 1;
    int msgNumber = 1;
    int sigNumber = 1;
    QFile *outFile = new QFile(fileName);
    QString nodesOutput, msgOutput, commentsOutput, valuesOutput;
    QString defaultsOutput, attrValOutput;

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete outFile;
        return;
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

        msgOutput.append("BO_ " + QString::number(msg->ID) + " " + msg->name + ": " + QString::number(msg->len) +
                         " " + msg->sender->name + "\n");
        if (msg->comment.length() > 0)
        {
            commentsOutput.append("CM_ BO_ " + QString::number(msg->ID) + " \"" + msg->comment + "\";\n");
        }

        //If this message has attributes then compile them into attributes list to output later on.
        if (msg->attributes.count() > 0)
        {
            foreach (DBC_ATTRIBUTE_VALUE val, msg->attributes) {
                attrValOutput.append("BA_ \"" + val.attrName + "\" BO_ " + QString::number(msg->ID) + " ");
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

            if (sig->isMultiplexor) msgOutput.append(" M");
            if (sig->isMultiplexed)
            {
                msgOutput.append(" m" + QString::number(sig->multiplexValue));
            }

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
                commentsOutput.append("CM_ SG_ " + QString::number(msg->ID) + " " + sig->name + " \"" + sig->comment + "\";\n");
            }

            //if this signal has attributes then compile them in a special list of attributes
            if (sig->attributes.count() > 0)
            {
                foreach (DBC_ATTRIBUTE_VALUE val, sig->attributes) {
                    attrValOutput.append("BA_ \"" + val.attrName + "\" SG_ " + QString::number(msg->ID) + " " + sig->name + " ");
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
                valuesOutput.append("VAL_ " + QString::number(msg->ID) + " " + sig->name);
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
        case GENERAL:
            break;
        case NODE:
            msgOutput.append("BU_ ");
            break;
        case MESSAGE:
            msgOutput.append("BO_ ");
            break;
        case SIG:
            msgOutput.append("SG_ ");
            break;
        }

        msgOutput.append("\"" + dbc_attributes[x].name + "\" ");

        switch (dbc_attributes[x].valType)
        {
        case QINT:
            msgOutput.append("INT " + QString::number(dbc_attributes[x].lower) + " " + QString::number(dbc_attributes[x].upper));
            break;
        case QFLOAT:
            msgOutput.append("FLOAT " + QString::number(dbc_attributes[x].lower) + " " + QString::number(dbc_attributes[x].upper));
            break;
        case QSTRING:
            msgOutput.append("STRING ");
            break;
        case ENUM:
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
            case QSTRING:
                defaultsOutput.append("\"" + dbc_attributes[x].defaultValue.toString() + "\";\n");
                break;
            case ENUM:
                defaultsOutput.append("\"" + dbc_attributes[x].enumVals[dbc_attributes[x].defaultValue.toInt()] + "\";\n");
                break;
            case QINT:
            case QFLOAT:
                defaultsOutput.append(dbc_attributes[x].defaultValue.toString() + ";\n");
                break;
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

    QStringList fileList = fileName.split('/');
    this->fileName = fileList[fileList.length() - 1]; //whoops... same name as parameter in this function.
    filePath = fileName.left(fileName.length() - this->fileName.length());
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
    attr.attrType = MESSAGE;
    attr.defaultValue = QApplication::palette().color(QPalette::Base).name();
    attr.enumVals.clear();
    attr.lower = 0;
    attr.upper = 0;
    attr.name = "GenMsgBackgroundColor";
    attr.valType = QSTRING;
    newFile.dbc_attributes.append(attr);

    attr.attrType = MESSAGE;
    attr.defaultValue = QApplication::palette().color(QPalette::WindowText).name();
    attr.enumVals.clear();
    attr.lower = 0;
    attr.upper = 0;
    attr.name = "GenMsgForegroundColor";
    attr.valType = QSTRING;
    newFile.dbc_attributes.append(attr);

    attr.attrType = MESSAGE;
    attr.defaultValue = 0;
    attr.enumVals.clear();
    attr.lower = 0;
    attr.upper = 0;
    attr.name = "matchingcriteria";
    attr.valType = QINT;
    newFile.dbc_attributes.append(attr);

    attr.attrType = MESSAGE;
    attr.defaultValue = 0;
    attr.enumVals.clear();
    attr.lower = 0;
    attr.upper = 0;
    attr.name = "filterlabeling";
    attr.valType = QINT;
    newFile.dbc_attributes.append(attr);

    loadedFiles.append(newFile);
    return loadedFiles.count();
}

DBCFile* DBCHandler::loadDBCFile(QString filename)
{
    DBCFile newFile;
    newFile.loadFile(filename);
    loadedFiles.append(newFile);
    return &loadedFiles.last();
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

    loadedFiles.swap(pos1, pos2);
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
        if (loadedFiles[i].getAssocBus() == -1 || frame.bus == (unsigned int)loadedFiles[i].getAssocBus())
        {
            DBC_MESSAGE* msg = loadedFiles[i].messageHandler->findMsgByID(frame.ID);
            if (msg != nullptr) return msg;
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
                *matchingCriteria = loadedFiles[i].messageHandler->getMatchingCriteria();
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
    for (int i=0; i<filecount; i++)
    {
        QString filename = settings.value("DBC/Filename_" + QString(i),"").toString();
        DBCFile * file = loadDBCFile(filename);
        int bus = settings.value("DBC/AssocBus_" + QString(i),0).toInt();
        file->setAssocBus(bus);

        MatchingCriteria_t matchingCriteria = (MatchingCriteria_t)settings.value("DBC/MatchingCriteria_" + QString(i),0).toInt();

        DBC_ATTRIBUTE attr;

        attr.attrType = MESSAGE;
        attr.defaultValue = matchingCriteria;
        attr.enumVals.clear();
        attr.lower = 0;
        attr.upper = 0;
        attr.name = "matchingcriteria";
        attr.valType = QINT;
        file->dbc_attributes.append(attr);
        file->messageHandler->setMatchingCriteria(matchingCriteria);

        bool filterLabeling = settings.value("DBC/FilterLabeling_" + QString(i),0).toBool();
        attr.attrType = MESSAGE;
        attr.defaultValue = filterLabeling;
        attr.enumVals.clear();
        attr.lower = 0;
        attr.upper = 0;
        attr.name = "filterlabeling";
        attr.valType = QINT;
        file->dbc_attributes.append(attr);
        file->messageHandler->setFilterLabeling(filterLabeling);

        qInfo() << "Loaded DBC file" << filename << " (bus:" << bus 
            << ", Matching Criteria:" << (int)matchingCriteria << "Filter labeling: " << (filterLabeling?"enabled":"disabled") << ")";
    }
}

DBCHandler* DBCHandler::getReference()
{
    if (!instance) instance = new DBCHandler();
    return instance;
}
