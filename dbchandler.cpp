#include "dbchandler.h"

#include <QFile>
#include <QRegularExpression>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include "utility.h"

DBC_SIGNAL* DBCSignalHandler::findSignalByIdx(int idx)
{
    if (sigs.count() == 0) return NULL;
    if (idx < 0) return NULL;
    if (idx >= sigs.count()) return NULL;

    return &sigs[idx];
}

DBC_SIGNAL* DBCSignalHandler::findSignalByName(QString name)
{
    if (sigs.count() == 0) return NULL;
    for (int i = 0; i < sigs.count(); i++)
    {
        if (sigs[i].name.compare(name, Qt::CaseInsensitive) == 0)
        {
            return &sigs[i];
        }
    }
    return NULL;
}

bool DBCSignalHandler::addSignal(DBC_SIGNAL &sig)
{
    sigs.append(sig);
    return true;
}

bool DBCSignalHandler::removeSignal(DBC_SIGNAL *sig)
{
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

DBC_MESSAGE* DBCMessageHandler::findMsgByID(int id)
{
    if (messages.count() == 0) return NULL;
    for (int i = 0; i < messages.count(); i++)
    {
        if (messages[i].ID == id)
        {
            return &messages[i];
        }
    }
    return NULL;
}

DBC_MESSAGE* DBCMessageHandler::findMsgByIdx(int idx)
{
    if (messages.count() == 0) return NULL;
    if (idx < 0) return NULL;
    if (idx >= messages.count()) return NULL;
    return &messages[idx];
}

DBC_MESSAGE* DBCMessageHandler::findMsgByName(QString name)
{
    if (messages.count() == 0) return NULL;
    for (int i = 0; i < messages.count(); i++)
    {
        if (messages[i].name.compare(name, Qt::CaseInsensitive) == 0)
        {
            return &messages[i];
        }
    }
    return NULL;
}

bool DBCMessageHandler::addMessage(DBC_MESSAGE &msg)
{
    messages.append(msg);
    return true;
}

bool DBCMessageHandler::removeMessage(DBC_MESSAGE *msg)
{
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

bool DBCMessageHandler::removeMessage(int ID)
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

DBCFile::DBCFile()
{
    messageHandler = new DBCMessageHandler;
}

DBCFile::DBCFile(const DBCFile& cpy)
{
    messageHandler = new DBCMessageHandler;
    for (int i = 0 ; i < cpy.messageHandler->getCount() ; i++)
        messageHandler->addMessage(*cpy.messageHandler->findMsgByIdx(i));

    fileName = cpy.fileName;
    filePath = cpy.filePath;
    assocBuses = cpy.assocBuses;
    dbc_nodes.clear();
    dbc_nodes.append(cpy.dbc_nodes);
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
    }
    return *this;
}

DBC_NODE* DBCFile::findNodeByIdx(int idx)
{
    if (idx < 0) return NULL;
    if (idx >= dbc_nodes.count()) return NULL;
    return &dbc_nodes[idx];
}

DBC_NODE* DBCFile::findNodeByName(QString name)
{
    if (dbc_nodes.length() == 0) return NULL;
    for (int i = 0; i < dbc_nodes.length(); i++)
    {
        if (dbc_nodes[i].name.compare(name, Qt::CaseInsensitive) == 0)
        {
            return &dbc_nodes[i];
        }
    }
    return NULL;
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
    if (bus > 1) return;
    assocBuses = bus;
}

void DBCFile::loadFile(QString fileName)
{
    QFile *inFile = new QFile(fileName);
    QString line;
    QRegularExpression regex;
    QRegularExpressionMatch match;
    DBC_MESSAGE *currentMessage = NULL;
    int numSigFaults = 0, numMsgFaults = 0;

    qDebug() << "DBC File: " << fileName;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return;
    }

    qDebug() << "Starting DBC load";
    dbc_nodes.clear();
    messageHandler->removeAllMessages();

    DBC_NODE falseNode;
    falseNode.name = "Vector__XXX";
    falseNode.comment = "Default node if none specified";
    dbc_nodes.append(falseNode);

    while (!inFile->atEnd()) {
        line = QString(inFile->readLine().simplified());
        if (line.startsWith("BO_ ")) //defines a message
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
                messageHandler->addMessage(msg);
                currentMessage = messageHandler->findMsgByID(msg.ID);
            }
            else numMsgFaults++;
        }
        if (line.startsWith("SG_ ")) //defines a signal
        {
            int offset = 0;
            bool isMultiplexor = false;
            bool isMultiplexed = false;
            DBC_SIGNAL sig;

            sig.multiplexValue = 0;
            sig.isMultiplexed = false;
            sig.isMultiplexor = false;

            qDebug() << "Found a SG line";
            regex.setPattern("^SG\\_ *(\\w+) *M *: *(\\d+)\\|(\\d+)@(\\d+)([\\+|\\-]) \\(([0-9.+\\-eE]+),([0-9.+\\-eE]+)\\) \\[([0-9.+\\-eE]+)\\|([0-9.+\\-eE]+)\\] \\\"(.*)\\\" (.*)");

            match = regex.match(line);
            if (match.hasMatch())
            {
                qDebug() << "Multiplexor signal";
                isMultiplexor = true;
                sig.isMultiplexor = true;
            }
            else
            {
                regex.setPattern("^SG\\_ *(\\w+) *m(\\d+) *: *(\\d+)\\|(\\d+)@(\\d+)([\\+|\\-]) \\(([0-9.+\\-eE]+),([0-9.+\\-eE]+)\\) \\[([0-9.+\\-eE]+)\\|([0-9.+\\-eE]+)\\] \\\"(.*)\\\" (.*)");
                match = regex.match(line);
                if (match.hasMatch())
                {
                    qDebug() << "Multiplexed signal";
                    isMultiplexed = true;
                    sig.isMultiplexed = true;
                    sig.multiplexValue = match.captured(2).toInt();
                    offset = 1;
                }
                else
                {
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
                }
                sig.factor = match.captured(6 + offset).toDouble();
                sig.bias = match.captured(7 + offset).toDouble();
                sig.min = match.captured(8 + offset).toDouble();
                sig.max = match.captured(9 + offset).toDouble();
                sig.unitName = match.captured(10 + offset);
                if (match.captured(11 + offset).contains(','))
                {
                    QString tmp = match.captured(11).split(',')[0];
                    sig.receiver = findNodeByName(tmp);
                }
                else sig.receiver = findNodeByName(match.captured(11 + offset));
                sig.parentMessage = currentMessage;
                currentMessage->sigHandler->addSignal(sig);
                if (isMultiplexor) currentMessage->multiplexorSignal = currentMessage->sigHandler->findSignalByName(sig.name);
            }
            else numSigFaults++;
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
                DBC_MESSAGE *msg = messageHandler->findMsgByID(match.captured(1).toInt());
                if (msg != NULL)
                {
                    DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(match.captured(2));
                    if (sig != NULL)
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
                DBC_MESSAGE *msg = messageHandler->findMsgByID(match.captured(1).toInt());
                if (msg != NULL)
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
                if (node != NULL)
                {
                    node->comment = match.captured(2);
                }
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
                //qDebug() << "Data was: " << match.captured(3);
                DBC_MESSAGE *msg = messageHandler->findMsgByID(match.captured(1).toInt());
                if (msg != NULL)
                {
                    DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(match.captured(2));
                    if (sig != NULL)
                    {
                        QString tokenString = match.captured(3);
                        DBC_VAL val;
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
    if (numSigFaults > 0 || numMsgFaults > 0)
    {
        QMessageBox msgBox;
        QString msg = "DBC file loaded with errors!\n";
        msg += "Number of faulty message entries: " + QString::number(numMsgFaults) + "\n";
        msg += "Number of faulty signal entries: " + QString::number(numSigFaults) + "\n\n";
        msg += "Faulty entries have not been loaded.";
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

void DBCFile::saveFile(QString fileName)
{
    QFile *outFile = new QFile(fileName);
    QString nodesOutput, msgOutput, commentsOutput, valuesOutput;

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

    nodesOutput.append("BU_: ");
    for (int x = 0; x < dbc_nodes.count(); x++)
    {
        DBC_NODE node = dbc_nodes[x];
        if (node.name.compare("Vector__XXX", Qt::CaseInsensitive) != 0)
        {
            nodesOutput.append(node.name + " ");
            if (node.comment.length() > 0)
            {
                commentsOutput.append("CM_ BU_ " + node.name + " \"" + node.comment + "\";\n");
            }
        }
    }
    nodesOutput.append("\n");
    outFile->write(nodesOutput.toUtf8());

    for (int x = 0; x < messageHandler->getCount(); x++)
    {
        DBC_MESSAGE *msg = messageHandler->findMsgByIdx(x);
        msgOutput.append("BO_ " + QString::number(msg->ID) + " " + msg->name + ": " + QString::number(msg->len) +
                         " " + msg->sender->name + "\n");
        if (msg->comment.length() > 0)
        {
            commentsOutput.append("CM_ BO_ " + QString::number(msg->ID) + " \"" + msg->comment + "\";\n");
        }
        for (int s = 0; s < msg->sigHandler->getCount(); s++)
        {
            DBC_SIGNAL *sig = msg->sigHandler->findSignalByIdx(s);
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
                msgOutput.append("2-");
                break;
            case DP_FLOAT:
                msgOutput.append("3-");
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
            if (sig->valList.count() > 0)
            {
                valuesOutput.append("VAL_ " + QString::number(msg->ID) + " " + sig->name);
                for (int v = 0; v < sig->valList.count(); v++)
                {
                    DBC_VAL val = sig->valList[v];
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

    //now write out all of the accumulated comments and value tables from above
    outFile->write(commentsOutput.toUtf8());
    outFile->write(valuesOutput.toUtf8());

    outFile->close();
    delete outFile;

    QStringList fileList = fileName.split('/');
    this->fileName = fileList[fileList.length() - 1]; //whoops... same name as parameter in this function.
    filePath = fileName.left(fileName.length() - this->fileName.length());
}

void DBCHandler::saveDBCFile(int idx)
{
    if (loadedFiles.count() == 0) return;
    if (idx < 0) return;
    if (idx >= loadedFiles.count()) return;

    QString filename;
    QFileDialog dialog;

    QStringList filters;
    filters.append(QString(tr("DBC File (*.dbc)")));

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
    }
}

int DBCHandler::createBlankFile()
{
    DBCFile newFile;
    loadedFiles.append(newFile);
    return loadedFiles.count();
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

    QStringList filters;
    filters.append(QString(tr("DBC File (*.dbc)")));

    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        //right now there is only one file type that can be loaded here so just do it.
        DBCFile newFile;
        newFile.loadFile(filename);
        loadedFiles.append(newFile);

        return &loadedFiles.last();
    }

    return NULL;
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
 * Returns NULL if there is no message definition that matches.
*/
DBC_MESSAGE* DBCHandler::findMessage(const CANFrame &frame)
{
    for(int i = 0; i < loadedFiles.count(); i++)
    {
        if (loadedFiles[i].getAssocBus() == -1 || frame.bus == loadedFiles[i].getAssocBus())
        {
            DBC_MESSAGE* msg = loadedFiles[i].messageHandler->findMsgByID(frame.ID);
            if (msg != NULL) return msg;
        }
    }
    return NULL;
}

int DBCHandler::getFileCount()
{
    return loadedFiles.count();
}

DBCFile* DBCHandler::getFileByIdx(int idx)
{
    if (loadedFiles.count() == 0) return NULL;
    if (idx < 0) return NULL;
    if (idx >= loadedFiles.count()) return NULL;
    return &loadedFiles[idx];
}

DBCFile* DBCHandler::getFileByName(QString name)
{
    if (loadedFiles.count() == 0) return NULL;
    for (int i = 0; i < loadedFiles.count(); i++)
    {
        if (loadedFiles[i].getFilename().compare(name, Qt::CaseInsensitive) == 0)
        {
            return &loadedFiles[i];
        }
    }
    return NULL;
}
