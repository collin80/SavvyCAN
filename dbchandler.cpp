#include "dbchandler.h"

#include <QFile>
#include <QRegularExpression>
#include <QDebug>

DBCHandler::DBCHandler(QObject *parent) : QObject(parent)
{

}

void DBCHandler::loadDBCFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QString line;
    QRegularExpression regex;
    QRegularExpressionMatch match;
    DBC_MESSAGE *currentMessage;

    qDebug() << "DBC File: " << filename;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    qDebug() << "Starting DBC load";
    dbc_nodes.clear();
    dbc_messages.clear();

    DBC_NODE falseNode;
    falseNode.name = "Vector__XXX";
    falseNode.comment = "Default node if none specified";
    dbc_nodes.append(falseNode);

    while (!inFile->atEnd()) {
        line = QString(inFile->readLine().simplified());
        if (line.startsWith("BO_ "))
        {
            //qDebug() << "Found a BO line";
            regex.setPattern("^BO\\_ (\\w+) (\\w+) *: (\\w+) (\\w+)");
            match = regex.match(line);
            //captured 1 = the ID in decimal
            //captured 2 = The message name
            //captured 3 = the message length
            //captured 4 = the NODE responsible for this message
            if (match.hasMatch())
            {
                DBC_MESSAGE msg;
                msg.ID = match.captured(1).toInt(); //the ID is always stored in decimal format
                msg.name = match.captured(2);
                msg.len = match.captured(3).toInt();
                msg.sender = findNodeByName(match.captured(4));
                dbc_messages.append(msg);
                currentMessage = &dbc_messages.last();
            }
        }
        if (line.startsWith("SG_ "))
        {
            //qDebug() << "Found a SG line";
            regex.setPattern("^SG\\_ (\\w+) : (\\d+)\\|(\\d+)@(\\d+)([\\+|\\-]) \\(([0-9.+\\-eE]+),([0-9.+\\-eE]+)\\) \\[([0-9.+\\-eE]+)\\|([0-9.+\\-eE]+)\\] \\\"(.*)\\\" (.*)");
            match = regex.match(line);
            //captured 1 is the signal name
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
                DBC_SIGNAL sig;
                sig.name = match.captured(1);
                sig.startBit = match.captured(2).toInt();
                sig.signalSize = match.captured(3).toInt();
                int val = match.captured(4).toInt();
                if (val < 2)
                {
                    if (match.captured(5) == "+") sig.valType = UNSIGNED_INT;
                    else sig.valType = SIGNED_INT;
                }
                switch (val)
                {
                case 0:
                    sig.intelByteOrder = true;
                    break;
                case 1:
                    sig.intelByteOrder = false;
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
                }
                sig.factor = match.captured(6).toDouble();
                sig.bias = match.captured(7).toDouble();
                sig.min = match.captured(8).toDouble();
                sig.max = match.captured(9).toDouble();
                sig.unitName = match.captured(10);
                sig.receiver = findNodeByName(match.captured(11));
                currentMessage->msgSignals.append(sig);
            }
        }
        if (line.startsWith("BU_:"))
        {
            //qDebug() << "Found a BU line";
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
            }
        }
        if (line.startsWith("CM_ SG_ "))
        {
            //qDebug() << "Found an SG comment line";
            regex.setPattern("^CM\\_ SG\\_ *(\\w+) *(\\w+) *\\\"(.*)\\\";");
            match = regex.match(line);
            //captured 1 is the ID to match against to get to the message
            //captured 2 is the signal name from that message
            //captured 3 is the comment itself
            if (match.hasMatch())
            {
                //qDebug() << "Comment was: " << match.captured(3);
                DBC_MESSAGE *msg = findMsgByID(match.captured(1).toInt());
                if (msg != NULL)
                {
                    DBC_SIGNAL *sig = findSignalByName(msg, match.captured(2));
                    if (sig != NULL)
                    {
                        sig->comment = match.captured(3);
                    }
                }
            }
        }
        if (line.startsWith("CM_ BO_ "))
        {
            //qDebug() << "Found a BO comment line";
            regex.setPattern("^CM\\_ BO\\_ *(\\w+) *\\\"(.*)\\\";");
            match = regex.match(line);
            //captured 1 is the ID to match against to get to the message
            //captured 2 is the comment itself
            if (match.hasMatch())
            {
                //qDebug() << "Comment was: " << match.captured(2);
                DBC_MESSAGE *msg = findMsgByID(match.captured(1).toInt());
                if (msg != NULL)
                {
                    msg->comment = match.captured(2);
                }
            }
        }
        if (line.startsWith("CM_ BU_ "))
        {
            //qDebug() << "Found a BU comment line";
            regex.setPattern("^CM\\_ BU\\_ *(\\w+) *\\\"(.*)\\\";");
            match = regex.match(line);
            //captured 1 is the Node name
            //captured 2 is the comment itself
            if (match.hasMatch())
            {
                //qDebug() << "Comment was: " << match.captured(2);
                DBC_NODE *node = findNodeByName(match.captured(1));
                if (node != NULL)
                {
                    node->comment = match.captured(2);
                }
            }
        }
        //VAL_ (1090) (VCUPresentParkLightOC) (1 "Error present" 0 "Error not present") ;
        if (line.startsWith("VAL_ "))
        {
            //qDebug() << "Found a value definition line";
            regex.setPattern("^VAL\\_ (\\w+) (\\w+) (.*);");
            match = regex.match(line);
            //captured 1 is the ID to match against
            //captured 2 is the signal name to match against
            //captured 3 is a series of values in the form (number "text") that is, all sep'd by spaces
            if (match.hasMatch())
            {
                //qDebug() << "Data was: " << match.captured(3);
                DBC_MESSAGE *msg = findMsgByID(match.captured(1).toInt());
                if (msg != NULL)
                {
                    DBC_SIGNAL *sig = findSignalByName(msg, match.captured(2));
                    if (sig != NULL)
                    {
                        QString tokenString = match.captured(3);
                        DBC_VAL val;
                        while (tokenString.length() > 2)
                        {
                            regex.setPattern("(\\d+) \\\"(.*?)\\\" (.*)");
                            match = regex.match(tokenString);
                            if (match.hasMatch())
                            {
                                val.value = match.captured(1).toInt();
                                val.descript = match.captured(2);
                                //qDebug() << "sig val " << val.value << " desc " <<val.descript;
                                sig->valList.append(val);
                                tokenString = tokenString.right(tokenString.length() - match.captured(1).length() - match.captured(2).length() - 4);
                                //qDebug() << "New token string: " << tokenString;
                            }
                            else tokenString = "";
                        }
                    }
                }
            }
        }

        /*
        if (line.startsWith("BA_DEF_ SG_ "))
        {
            qDebug() << "Found a SG attribute line";
            regex.setPattern("^BA\\_DEF\\_ SG\\_ +\\\"([A-Za-z0-9\-_]+)\\\" +(.+);");
            match = regex.match(line);
            //captured 1 is the Node name
            //captured 2 is the comment itself
            if (match.hasMatch())
            {
                qDebug() << "Comment was: " << match.captured(2);
            }
        }
        if (line.startsWith("BA_DEF_ BO_ "))
        {

        }
        if (line.startsWith("BA_DEF_ BU_ "))
        {

        }
*/
    }
    inFile->close();
}

DBC_NODE *DBCHandler::findNodeByName(QString name)
{
    if (dbc_nodes.length() == 0) return NULL;
    for (int i = 0; i < dbc_nodes.length(); i++)
    {
        if (dbc_nodes[i].name == name)
        {
            return &dbc_nodes[i];
        }
    }
    return NULL;
}

DBC_NODE *DBCHandler::findNodeByIdx(int idx)
{
    if (idx < 0) return NULL;
    if (idx >= dbc_nodes.count()) return NULL;
    return &dbc_nodes[idx];
}

DBC_MESSAGE *DBCHandler::findMsgByID(int id)
{
    if (dbc_messages.length() == 0) return NULL;
    for (int i = 0; i < dbc_messages.length(); i++)
    {
        if (dbc_messages[i].ID == id)
        {
            return &dbc_messages[i];
        }
    }
    return NULL;
}

DBC_MESSAGE *DBCHandler::findMsgByIdx(int idx)
{
    if (dbc_messages.length() == 0) return NULL;
    if (idx < 0) return NULL;
    if (idx >= dbc_messages.count()) return NULL;
    return &dbc_messages[idx];
}

DBC_MESSAGE *DBCHandler::findMsgByName(QString name)
{
    if (dbc_messages.length() == 0) return NULL;
    for (int i = 0; i < dbc_messages.length(); i++)
    {
        if (dbc_messages[i].name.compare(name, Qt::CaseInsensitive))
        {
            return &dbc_messages[i];
        }
    }
    return NULL;
}

DBC_SIGNAL *DBCHandler::findSignalByName(DBC_MESSAGE *msg, QString name)
{
    if (msg == NULL) return NULL;
    if (msg->msgSignals.length() == 0) return NULL;
    for (int i = 0; i < msg->msgSignals.length(); i++)
    {
        if (msg->msgSignals[i].name == name)
        {
            return &msg->msgSignals[i];
        }
    }
    return NULL;
}

DBC_SIGNAL *DBCHandler::findSignalByIdx(DBC_MESSAGE *msg, int idx)
{
    if (msg == NULL) return NULL;
    if (msg->msgSignals.length() == 0) return NULL;
    if (idx < 0) return NULL;
    if (idx >= msg->msgSignals.count()) return NULL;
    return &msg->msgSignals[idx];
}

//Dumps the messages, signals, values structs out in order to debugging console. Used only for debugging
//not really meant for general consumption.
void DBCHandler::listDebugging()
{
    for (int i = 0; i < dbc_messages.length(); i++)
    {
        DBC_MESSAGE msg = dbc_messages.at(i);
        qDebug() << " ";
        qDebug() << "Msg ID: " << msg.ID << " Name: " << msg.name;

        for (int j = 0; j < msg.msgSignals.length(); j++)
        {
            DBC_SIGNAL sig;
            sig = msg.msgSignals.at(j);
            qDebug() << "    Signal Name: " << sig.name;
            qDebug() << "    Start bit: " << sig.startBit;
            qDebug() << "    Bit Length: " << sig.signalSize;
            if (sig.valList.length() > 1) qDebug() << "      Values: ";
            for (int k = 0; k < sig.valList.length(); k++)
            {
                DBC_VAL val = sig.valList.at(k);
                qDebug() << "            " << val.value << " Description: " << val.descript;
            }
        }
    }
}


//Vector uses a special format for bit ordering. It pretends that the bits are numbered
//0 to 63 in ascending order of bits as if a 64 bit integer were stored lowest first
//and highest bit last.
//0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31    Vector bit ordering
//7 6 5 4 3 2 1 0 7 6 5  4  3  2  1  0  7  6  5  4  3  2  1  0  7  6  5  4  3  2  1  0     Normal bitwise ordering within bytes
//0 1 2 3 4 5 6 7 0 1 2  3  4  5  6  7  0  1  2  3  4  5  6  7  0  1  2  3  4  5  6  7     Reversed bit order used by Vector
//0               1                     2                       3                          Byte ordering (same either way)
//A 16 bit integer would be stored Low first high second for intel format and high first, low second for motorola

//For intel format invert the starting bit within a byte.
//Otherwise, iterate over the bytes that it encompasses
//For intel format this works nicely as it means you can just go through the list getting higher and higher
//values for each bit as you go.
//For motorola it is backwards but only partially. For each byte you can go through and it's higher as you go
//but, at each byte boundary the next byte is lower than the multiplier for the last.
QString DBCHandler::processSignal(const CANFrame &frame, const DBC_SIGNAL &sig)
{
    int startBit, endBit, startByte, endByte, bitWithinByteStart, bitWithinByteEnd;
    int result = 0;
    int multiplier;
    int bitsToGo;

    startBit = sig.startBit;
    startByte = startBit / 8;
    bitWithinByteStart = startBit % 8;
    if (sig.intelByteOrder)
    {
        bitWithinByteStart = 7 - bitWithinByteStart;
        startBit = (startByte * 8) + bitWithinByteStart;
    }

    if (sig.valType == STRING)
    {
        QString buildString;
        int bytes = sig.signalSize / 8;
        for (int x = 0; x < bytes; x++) buildString.append(frame.data[startByte + x]);
        return buildString;
    }

    endBit = startBit + sig.signalSize - 1;
    endByte = endBit / 8;
    bitWithinByteEnd = endBit % 8;
    bitsToGo = sig.signalSize - 1;

    multiplier = 1;
    if (sig.intelByteOrder)
    {
        for (int y = startByte; y < endByte; y++) multiplier *= 256;
    }

    //qDebug() << "Signal Name: " << sig.name;
    //qDebug() << "Intel Order: " << sig.intelByteOrder;
    //qDebug() << "start byte: " << startByte;
    //qDebug() << "End Byte: " << endByte;

    int sBit, eBit;
    sBit = bitWithinByteStart;
    eBit = sBit + bitsToGo;
    if (eBit > 7) eBit = 7;
    bitsToGo -= (eBit - sBit + 1);
    for (int b = startByte; b <= endByte; b++)
    {
        //qDebug() << "Byte: " << frame.data[b];
        //qDebug() << "S: " << sBit;
        //qDebug() << "E: " << eBit;
        //process this byte
        result += processByte(frame.data[b], sBit, eBit) * multiplier;

        //add to multiplier
        if (!sig.intelByteOrder)
            multiplier = multiplier << 8;
        else
            multiplier = multiplier >> 8;

        //Prepare sBit and eBit for next byte
        sBit = 0; //fresh byte so we start at the beginning now
        eBit = sBit + bitsToGo;
        if (eBit > 7) eBit = 7;
        bitsToGo -= (eBit - sBit + 1);
    }

    if (sig.valType == SIGNED_INT)
    {
        int mask = (1 << (sig.signalSize - 1));
        if ((result & mask) == mask) //is the highest bit possible for this signal size set?
        {
            /*
             * if so we need to also set every bit higher in the result int too.
             * This leads to the below two lines that are nasty. Here's the theory behind that...
             * If the value is signed and the highest bit is set then it is negative. To create
             * a negative value out of this even though the variable result is 64 bit we have to
             * run 1's all of the way up to bit 63 in result. -1 is all ones for whatever size integer
             * you have. So, it's 64 1's in this case.
             * signedMask is done this way:
             * first you take the signal size and shift 1 up that far. Then subtract one. Lets
             * see that for a 16 bit signal:
             * (1 << 16) - 1 = the first 16 bits set as 1's. So far so good. We then negate the whole
             * thing which flips all bits. Thus signedMask ends up with 1's everwhere that the signal
             * doesn't take up in the 64 bit signed integer result. Then, result has an OR operation on
             * it with the old value and -1 masked so that the the 1 bits from -1 don't overwrite bits from the
             * actual signal. This extends the sign bits out so that the integer result reads as the proper negative
             * value. We dont need to do any of this if the sign bit wasn't set.
            */
            int signedMask = ~((1 << sig.signalSize) - 1);
            result = (-1 & signedMask) | result;
        }
    }

    double endResult = ((double)result * sig.factor) + sig.bias;
    result = (int) endResult;

    //qDebug() << "Result: " << result;

    QString outputString;

    outputString = sig.name + ": ";

    if (sig.valList.count() > 0) //if this is a value list type then look it up and display the proper string
    {
        for (int x = 0; x < sig.valList.count(); x++)
        {
            if (sig.valList.at(x).value == result) outputString += sig.valList.at(x).descript;
        }
    }
    else //otherwise display the actual number and unit (if it exists)
    {
       outputString += QString::number(endResult) + sig.unitName;
    }

    return outputString;
}

//given a byte it will reverse the bit order in that byte
unsigned char DBCHandler::reverseBits(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

unsigned char DBCHandler::processByte(unsigned char input, int start, int end)
{
    unsigned char output = 0, size = end - start + 1;
    //first knock it down so that bottom is is start
    output = input >> start;
    //then mask off all bits above the proper ending
    output &= ((1 << size) - 1);
    return output;
}

/*
 SG_ NLG5_E_B_P : 15|1@0+ (1,0) [0|1] ""  Control
 SG_ NLG5_S_MC_M_PI : 23|8@0+ (0.1,0) [0|20] "A"  Control
 SG_ NLG5_S_MC_M_CP : 7|16@0+ (0.1,0) [0|100] "A"  Control
*/
