#include <QObject>
#include <QDebug>
#include <QCanBusFrame>
#include <QSerialPortInfo>
#include <QSettings>
#include <QStringBuilder>
#include <QtNetwork>
#include <QMetaObject>

#include "socketcand.h"

SocketCANd::SocketCANd(QString portName) :
    CANConnection(portName, "kayak", CANCon::KAYAK, 0, 0, 0, false, 0, 1, 4000, true),
    mTimer(this) /*NB: set this as parent of timer to manage it from working thread */
{

    mTimer.setInterval(10000); //tick every 10 seconds
    mTimer.setSingleShot(false); //keep ticking
    connect(&mTimer, SIGNAL(timeout()), this, SLOT(checkConnection()));

    sendDebug("SocketCANd()");
    //tcpClient = nullptr;
    hostCanIDs = portName.left(portName.indexOf("@")).split(',');
    QString hostIPandPort = portName.mid(portName.indexOf("can://")+6, portName.length() - portName.indexOf("can://") - 7); //7 is lenght of 'can://' and ')'
    hostIP = QHostAddress(hostIPandPort.left(hostIPandPort.indexOf(":")));
    hostPort = (hostIPandPort.right(hostIPandPort.length() - hostIPandPort.lastIndexOf(":") -1)).toInt();
    QVarLengthArray<QTcpSocket*> tcpClient(0);
    mNumBuses = hostCanIDs.length();
    mBusData.resize(mNumBuses);
    reconnecting = false;

    for (int i = 0; i < mNumBuses; i++)
    {
        rx_state.append(IDLE);
        unprocessedData.append("");
    }

}


SocketCANd::~SocketCANd()
{
    stop();
    sendDebug("~SocketCANd()");
}

void SocketCANd::sendDebug(const QString debugText)
{
    qDebug() << debugText;
    debugOutput(debugText);
}

void SocketCANd::sendBytesToTCP(const QByteArray &bytes, int busNum)
{
    if (tcpClient[busNum] && !tcpClient[busNum]->isOpen())
    {
        sendDebug("Attempt to write to TCP/IP port when it is not open!");
        return;
    }

    QString buildDebug;
    buildDebug = "Send data to " + hostIP.toString() + ":" + QString::number(hostPort) + " -> ";
    foreach (int byt, bytes) {
        byt = (unsigned char)byt;
        buildDebug = buildDebug % QString::number(byt, 16) % " ";
    }
    //sendDebug(buildDebug);

    if (tcpClient[busNum]) tcpClient[busNum]->write(bytes);
}

void SocketCANd::sendStringToTCP(const char* data, int busNum)
{
    if (tcpClient[busNum] && !tcpClient[busNum]->isOpen())
    {
        sendDebug("Attempt to write to TCP/IP port when it is not open!");
        return;
    }

    //QString buildDebug;
    //buildDebug = "Send data to " + hostIP.toString() + ":" + QString::number(hostPort) + " -> " + data;
    //sendDebug(buildDebug);
    //qInfo() << buildDebug;

    if (tcpClient[busNum]) tcpClient[busNum]->write(data);
}

void SocketCANd::piStarted()
{
    connectDevice();
}


void SocketCANd::piSuspend(bool pSuspend)
{
    /* update capSuspended */
    setCapSuspended(pSuspend);

    /* flush queue if we are suspended */
    if(isCapSuspended())
        getQueue().flush();
}


void SocketCANd::piStop()
{
    mTimer.stop();
    disconnectDevice();
}


bool SocketCANd::piGetBusSettings(int pBusIdx, CANBus& pBus)
{
    return getBusConfig(pBusIdx, pBus);
}


void SocketCANd::piSetBusSettings(int pBusIdx, CANBus bus)
{
    setBusConfig(pBusIdx, bus);
    bus.setSpeed(250000);
}


bool SocketCANd::piSendFrame(const CANFrame& frame)
{
    QByteArray buffer;
    int c;
    int ID;

//    //calculate bus number offset (in case of multiple connections)
//    //useless since SavvyCAN already delivers the right index in frame.bus
//    QList<CANConnection*> connList = CANConManager::getInstance()->getConnections();
//    int currentConnPos = connList.indexOf(this);
//    int busOffset = 0;
//    for (int i = 0; i < currentConnPos; i++)
//    {
//        busOffset += connList[i]->getNumBuses();
//    }
//    int busNum = frame.bus - busOffset;

    int busNum = frame.bus;

    framesRapid++;

    if (tcpClient[busNum] && !tcpClient[busNum]->isOpen()) return false;
    //if (!isConnected) return false;

    // Doesn't make sense to send an error frame
    // to an adapter
    if (frame.frameId() & 0x20000000) {
        return true;
    }
    ID = frame.frameId();
    if (frame.hasExtendedFrameFormat()) ID |= 1 << 31;

    QString sendStr = "< send " + QString::number(ID, 16) + " " + QString::number(frame.payload().length()) + " ";
    for (c = 0; c < frame.payload().length(); c++)
    {
       sendStr.append(QString::number(frame.payload()[c], 16) + " ");
    }
    sendStr.append(">");
    std::string str = sendStr.toStdString();
    const char* sendCmd = str.c_str();
    sendStringToTCP(sendCmd, busNum);

    return true;
}



/****************************************************************/

void SocketCANd::connectDevice()
{
    QSettings settings;

    /* disconnect device */
    /* avoiding this solution since it removes connection selection in case of connection check */
    //if(tcpClient)
    //    disconnectDevice();

    // only resetting tcpClient
    for (int i = 0; i < tcpClient.length(); i++)
    {
        if (tcpClient[i])
        {
            if (tcpClient[i]->isOpen())
            {
                tcpClient[i]->close();
            }
            tcpClient[i]->disconnect();
            delete tcpClient[i];
            tcpClient[i] = nullptr;
        }
    }
    tcpClient.clear();

    mTimer.start();

    //sendDebug("TCP Connection to a Kayak device");
    for (int i = 0; i < mNumBuses; i++)
    {
        rx_state[i] = IDLE;
        tcpClient.append(new QTcpSocket());
        tcpClient[i]->connectToHost(hostIP, hostPort);
        //connect(tcpClient[i], SIGNAL(readyRead()), this, SLOT(readTCPData()));
        connect(tcpClient[i], SIGNAL(readyRead()), this, SLOT(invokeReadTCPData()));
        sendDebug("Created TCP Socket to Kayak device " + hostCanIDs.at(i));
    }
    setStatus(CANCon::CONNECTED);
    CANConStatus stats;
    stats.conStatus = getStatus();
    stats.numHardwareBuses = mNumBuses;
    //signalSender = qobject_cast<QTcpSocket*>(QObject::sender());
    if (!reconnecting) emit status(stats);
}

void SocketCANd::deviceConnected(int busNum)
{
    sendDebug("Opening CAN on Kayak Device!");
    QString openCanCmd("< open " % hostCanIDs[busNum] % " >");
    sendStringToTCP(openCanCmd.toUtf8().data(), busNum);

    QCoreApplication::processEvents();
}

void SocketCANd::checkConnection()
{
    int bufSize = 0;
    for (int i = 0; i < tcpClient.length(); i++)
    {
        bufSize += tcpClient[i]->readBufferSize();
        //sendDebug("Buffer size: " + QString::number(bufSize));
    }

    if (bufSize == 0)
    {
        reconnecting = true;
        connectDevice();
        sendDebug("Reconnecting to TCP Host " + hostIP.toString());
    }
}

void SocketCANd::switchToRawMode(int busNum)
{
    sendDebug("Switching to rawmode...");
    const char* rawmodeCmd = "< rawmode >";
    sendStringToTCP(rawmodeCmd, busNum);
    QCoreApplication::processEvents();
}

QString SocketCANd::decodeFrames(QString data, int busNum)
{
    if (data.indexOf("<") == -1)
        return "";
    else if(data.length() >= 8 && data.indexOf("< frame ") == -1)
        return "";

    int firstIndex = data.indexOf("< frame ");
    if(firstIndex > 0)
    {
        QString framePartial = data.left(firstIndex);
        qDebug() << "Received datagramm that starts with fragment (missing '< frame'), this should only occur on startup, removing...: " << framePartial;
    }
    QString framePart = data.mid(firstIndex); //remove starting beginning of payload if not < frame >
    const QString frameStrConst = framePart.left(framePart.indexOf(">")+1);
    QString frameStr = frameStrConst;
    QStringList frameParsed = (frameStr.remove(QRegularExpression("^<")).remove(QRegularExpression(">$"))).simplified().split(' ');

    if(frameParsed.length() < 3)
    {
        //qDebug() << "Received datagramm is an incomplete frame: " << data;

        //ok great, need to leave it in the buffer in case it can be combined with what comes next
        //but if there was a fragment that did not have a starting token then we don't want it so only return
        //known good data...again this should only happen on startup, but just in case we need to remove it
        //so the data buffer doesn't grow uncontrolled.
        return framePart;
    }

    buildFrame.setFrameId(frameParsed[1].toUInt(nullptr, 16));
    buildFrame.bus = busNum;

    if (buildFrame.frameId() > 0x7FF) buildFrame.setExtendedFrameFormat(true);
    else buildFrame.setExtendedFrameFormat(false);

    buildFrame.setTimeStamp(QCanBusFrame::TimeStamp(0, frameParsed[2].toDouble() * 1000000l));
    //buildFrame.len =  frameParsed[3].length() * 0.5;

    int framelength = 0;

    if(frameParsed.length() == 4)
    {
        framelength = frameParsed[3].length() * 0.5;
    }

    QByteArray buildData;
    buildData.resize(framelength);

    int c;
    for (c = 0; c < framelength; c++)
    {
        bool ok;
        unsigned char byteVal = frameParsed[3].mid(c*2, 2).toUInt(&ok, 16);
        buildData[c] = byteVal;
    }
    buildFrame.setPayload(buildData);
//        buildFrame.isReceived = true;

    if (!isCapSuspended())
    {
        /* get frame from queue */
        CANFrame* frame_p = getQueue().get();
        if(frame_p) {
            /* copy frame */
            *frame_p = buildFrame;
            //frame_p->remote = false;
            frame_p->setFrameType(QCanBusFrame::DataFrame);
            checkTargettedFrame(buildFrame);
            /* enqueue frame */
            getQueue().queue();
        }
    }
    //else
    //    qDebug() << "can't get a frame, capture suspended";

    //take out the data that we just processed and anything that is in front of it
    //this should keep broken frames from accumulating at in the data buffer
    if (framePart.length() > frameStrConst.length())
    {
        return decodeFrames(framePart.right(framePart.length() - frameStrConst.length()), busNum);
    }

    return "";
}

void SocketCANd::disconnectDevice() {
    for (int i = 0; i < tcpClient.length(); i++)
    {
        if (tcpClient[i])
        {
            if (tcpClient[i]->isOpen())
            {
                tcpClient[i]->close();
            }
            tcpClient[i]->disconnect();
            delete tcpClient[i];
            tcpClient[i] = nullptr;
        }
    }
    tcpClient.clear();

    setStatus(CANCon::NOT_CONNECTED);
    CANConStatus stats;
    stats.conStatus = getStatus();
    stats.numHardwareBuses = mNumBuses;
    emit status(stats);
}

void SocketCANd::invokeReadTCPData()
{
    QTcpSocket* signalSender = qobject_cast<QTcpSocket*>(QObject::sender());
    int busNum = tcpClient.indexOf(signalSender);
    QMetaObject::invokeMethod(this, "readTCPData", Qt::QueuedConnection, Q_ARG(int, busNum));
}

void SocketCANd::readTCPData(int busNum)
{
    QString data;

    if (QTcpSocket* socket = tcpClient.value(busNum))
        data = QString(socket->readAll());
    //sendDebug("Got data from TCP. Len = " % QString::number(data.length()));
    //qDebug() << "Received datagramm: " << data;
    procRXData(data, busNum);
}

void SocketCANd::procRXData(QString data, int busNum)
{
    if (data != "")
    {
        mTimer.stop();
        mTimer.start();
    }
    switch (rx_state.at(busNum))
    {
    case IDLE:
        qDebug() << "Received datagramm: " << data;
        if (data == "< hi >")
        {
            deviceConnected(busNum);
            rx_state[busNum] = BCM;
        }
        else qInfo() << hostCanIDs[busNum] << ": Could not open bus. Host did not greet with ""< hi >"": " << data;
        break;
    case BCM:
        qDebug() << "Received datagramm: " << data;
        if (data == "< ok >")
        {
            switchToRawMode(busNum);
            rx_state[busNum] = SWITCHING2RAW;
            unprocessedData[busNum].clear();
        }
        else qInfo() << hostCanIDs[busNum] << ": Could not open bus. Host did not respond with ""< ok >"": " << data;
        break;
    case SWITCHING2RAW:
        qDebug() << "Received datagramm: " << data;
        if (data == "< ok >")
        {
            rx_state[busNum] = RAWMODE;
        }
        else if(data.indexOf("< ok >", 0, Qt::CaseSensitivity::CaseInsensitive) == 0)
        {
            qDebug() << "Ok found at start of compound message, switching to RAW and decoding immediately";
            rx_state[busNum] = RAWMODE;
            unprocessedData[busNum] = decodeFrames(data, busNum);
        }
        else if(data.indexOf("< ok >", 0, Qt::CaseSensitivity::CaseInsensitive) > 0)
        {
            qDebug() << "Ok found at in middle of compound message, switching to RAW and decoding immediately";
            rx_state[busNum] = RAWMODE;
            unprocessedData[busNum] = decodeFrames(data, busNum);
        }
        break;
    case RAWMODE:
        unprocessedData[busNum] = decodeFrames(unprocessedData[busNum] + data, busNum);

        if(unprocessedData[busNum].length() > 128)
        {
            //the buffer has grown too much we need to clear it out, but what is good logic for that?
            //the decodeFrames function strips out datat that doesn't have a '< frame' starting token, and in its
            //recursive calling of itself it strips out data that preceedes valid frames, so this should never happen
            qDebug() << "busNum: " << busNum << "- " << unprocessedData[busNum].length() << " bytes in unprocessedData, something is wrong, clearing...";
            unprocessedData[busNum].clear();
        }
        break;
    case ISOTP:
        break;
    }
}
