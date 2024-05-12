#include <QObject>
#include <QDebug>
#include <QCanBusFrame>
#include <QSettings>
#include <QStringBuilder>
#include <QtNetwork>

#include "utility.h"
#include "canlogserver.h"

CanLogServer::CanLogServer(QString serverAddressString) :
    CANConnection(serverAddressString, "CanLogserver", CANCon::CANLOGSERVER, 0, 0, 0, false, 0, 1, 4000, true),
    m_ptcpSocket(new QTcpSocket(this))
{

    qDebug() << "Canlogserver: " << "Constructing new Connection...";

    CANBus bus_info;
    bus_info.setActive(true);
    bus_info.setListenOnly(true);
    bus_info.setSpeed(500000);

    setBusConfig(0, bus_info);

    // Connect data ready signal
    connect(m_ptcpSocket, SIGNAL(readyRead()), this, SLOT(readNetworkData()));
    // Connect network connected signal
    connect(m_ptcpSocket, SIGNAL(connected()),this, SLOT(networkConnected()));
    // Connect network disconnected signal
    connect(m_ptcpSocket, SIGNAL(disconnected()),this, SLOT(networkDisconnected()));
    // Save address
    this->m_qsAddress = serverAddressString;
}


CanLogServer::~CanLogServer()
{
    qDebug() << "Canlogserver: " << "Deconstructing Connection...";

    stop();

    m_ptcpSocket->close();
    delete m_ptcpSocket;
    m_ptcpSocket = NULL;
}

void CanLogServer::readNetworkData()
{
    // If capture is suspended bail out after reading the bytes from the network.  No need to parse them
    if(isCapSuspended())
        return;

    while (m_ptcpSocket->canReadLine()) {
        // Get a complete line and remove whitespace at the start and at the end of the string
        QString data = QString(m_ptcpSocket->readLine()).trimmed();
        // Split to space (obtain "<(time)> <canID> <msgId#data>")
        QStringList lstData = data.split(" ");
        // Expect 3 item in list
        if(lstData.size() == 3){
            // Obtain the timestamp. Remove '(', ')' and '.' (logserver send as "(time.fraction)").
            QString qstrTs = lstData[0].remove(QChar('(')).remove(QChar('.')).remove(QChar(')'));
            // One bus only. TODO: Use another parameter in connection conf for spcify can bus id to capture.
            QString qstrCanId = "0";
            // Split frame
            QStringList lstMsg = lstData[2].split("#");
            // Expect 2 item (<id>#<payload>)
            if(lstMsg.size() == 2){
                // Extract id
                QString qstrId = lstMsg[0];
                // Extract payload
                QString qstrPayload = lstMsg[1];
                // Support only normal can message. Extended CAN not supported.
                if(qstrId.size() <= 4){
                    // Prepare the frame
                    CANFrame* frame_p = getQueue().get();
                    // Check for frame existence
                    if(frame_p){
                        // Set frame ID
                        frame_p->setFrameId(qstrId.toInt(nullptr, 16));
                        // Extended frame NOT SUPPORTED
                        frame_p->setExtendedFrameFormat(0);
                        // Set bus id
                        frame_p->bus = qstrCanId.toInt();
                        // Set frame type
                        frame_p->setFrameType(QCanBusFrame::DataFrame);
                        // Frame is recived
                        frame_p->isReceived = true;
                        // Set timestamp
                        frame_p->setTimeStamp(QCanBusFrame::TimeStamp(0, qstrTs.toULongLong()));
                        // Set payload
                        frame_p->setPayload(QByteArray::fromHex(qstrPayload.toUtf8()));
                        // Elaborate frame
                        checkTargettedFrame(*frame_p);
                        /* enqueue frame */
                        getQueue().queue();
                    }
                    qDebug() << data << "---" << qstrTs << " - " << qstrId << " + " << qstrPayload;
                }
            }
        }
    }
}

void CanLogServer::networkConnected()
{
    CANConStatus stats;
    qDebug() << "networkConnected";

    setStatus(CANCon::CONNECTED);
    stats.conStatus = getStatus();
    stats.numHardwareBuses = 1;
    emit status(stats);
}

void CanLogServer::networkDisconnected()
{
    qDebug() << "networkDisconnected";
}

void CanLogServer::piStarted()
{
    this->connectToDevice();
}

void CanLogServer::piSuspend(bool pSuspend)
{
    /* update capSuspended */
    setCapSuspended(pSuspend);

    /* flush queue if we are suspended */
    if(isCapSuspended())
        getQueue().flush();
}


void CanLogServer::piStop()
{
    this->disconnectFromDevice();
}


bool CanLogServer::piGetBusSettings(int pBusIdx, CANBus& pBus)
{
    return getBusConfig(pBusIdx, pBus);
}


void CanLogServer::piSetBusSettings(int pBusIdx, CANBus bus)
{
    /* sanity checks */
    if( (pBusIdx < 0) || pBusIdx >= getNumBuses())
        return;

    /* copy bus config */
    setBusConfig(pBusIdx, bus);
}


bool CanLogServer::piSendFrame(const CANFrame& )
{
    //We don't support sending frames right now
    return true;
}

void CanLogServer::connectToDevice()
{
    qDebug() << "Canlogserver: " << "Establishing connection to a Canlogserver device...";

    QUrl url("http://" + m_qsAddress);
    qDebug() << "address:" << m_qsAddress;
    qDebug() << "Host:" << url.host();
    qDebug() << "Port:" << url.port();
    // Set status at not connected
    setStatus(CANCon::NOT_CONNECTED);
    // No proxy for connection
    m_ptcpSocket->setProxy(QNetworkProxy::NoProxy);
    // Connect to log server
    m_ptcpSocket->connectToHost(url.host(), url.port());
}

void CanLogServer::disconnectFromDevice()
{
    qDebug() << "Canlogserver: " << "Disconnecting...";
    // Close socket
    m_ptcpSocket->close();
}
