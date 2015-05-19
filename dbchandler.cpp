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

    while (!inFile->atEnd()) {
        line = QString(inFile->readLine().toUpper().simplified());
        if (line.startsWith("BO_ "))
        {
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
            qDebug() << "Found a SG line";
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
                qDebug() << "Comment was: " << match.captured(3);
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
                qDebug() << "Comment was: " << match.captured(2);
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
                qDebug() << "Comment was: " << match.captured(2);
            }
        }
        //VAL_ (1090) (VCUPresentParkLightOC) (1 "Error present" 0 "Error not present") ;
        if (line.startsWith("VAL_ "))
        {
            qDebug() << "Found a value definition line";
            regex.setPattern("^VAL\\_ (\\w+) (\\w+) (.*);");
            match = regex.match(line);
            //captured 1 is the ID to match against
            //captured 2 is the signal name to match against
            //captured 3 is a series of values in the form (number "text") that is, all sep'd by spaces
            if (match.hasMatch())
            {
                qDebug() << "Data was: " << match.captured(3);
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
    for (int i = 0; i < dbc_nodes.length(); i++)
    {
        if (dbc_nodes[i].name == name)
        {
            return &dbc_nodes[i];
        }
    }
    return NULL;
}

DBC_MESSAGE *DBCHandler::findMsgByID(int id)
{
    for (int i = 0; i < dbc_messages.length(); i++)
    {
        if (dbc_messages[i].ID == id)
        {
            return &dbc_messages[i];
        }
    }
    return NULL;
}

DBC_SIGNAL *DBCHandler::findSignalByName(DBC_MESSAGE *msg, QString name)
{
    for (int i = 0; i < msg->msgSignals.length(); i++)
    {
        if (msg->msgSignals[i].name == name)
        {
            return &msg->msgSignals[i];
        }
    }
    return NULL;
}

