//
//  canserver.cpp
//  SavvyCAN
//
//  Created by Chris Whiteford on 2022-01-21.
//

#include <QObject>
#include <QDebug>
#include <QCanBusFrame>
#include <QSettings>
#include <QStringBuilder>
#include <QtNetwork>

#include "utility.h"
#include "canserver.h"

CANserver::CANserver(QString serverAddressString) :
    CANConnection(serverAddressString, "CANserver", CANCon::CANSERVER, 0, 0, 0, false, 0, 3, 4000, true),
    _udpClient(new QUdpSocket(this)),
    _heartbeatTimer(new QTimer(this))
{
    
    qDebug() << "CANserver: " << "Constructing new Connection...";
    
    CANBus bus_info;
    bus_info.setActive(true);
    bus_info.setListenOnly(true);
    bus_info.setSpeed(500000);

    setBusConfig(0, bus_info);
    setBusConfig(1, bus_info);
    setBusConfig(2, bus_info);
    
    // setup udp client
    this->_canserverAddress = QHostAddress(serverAddressString);
    connect(_udpClient, SIGNAL(readyRead()), this, SLOT(readNetworkData()));

    // setup signal and slot
    connect(_heartbeatTimer, SIGNAL(timeout()), this, SLOT(heartbeatTimerSlot()));
    _heartbeatTimer->stop();
}


CANserver::~CANserver()
{
    qDebug() << "CANserver: " << "Deconstructing Connection...";
    
    stop();
    
    delete _heartbeatTimer; _heartbeatTimer = NULL;
    delete _udpClient; _udpClient = NULL;
}

void CANserver::piStarted()
{
    this->connectToDevice();
}

void CANserver::piSuspend(bool pSuspend)
{
    /* update capSuspended */
    setCapSuspended(pSuspend);

    /* flush queue if we are suspended */
    if(isCapSuspended())
        getQueue().flush();
}


void CANserver::piStop()
{
    this->disconnectFromDevice();
}


bool CANserver::piGetBusSettings(int pBusIdx, CANBus& pBus)
{
    return getBusConfig(pBusIdx, pBus);
}


void CANserver::piSetBusSettings(int pBusIdx, CANBus bus)
{
    /* sanity checks */
    if( (pBusIdx < 0) || pBusIdx >= getNumBuses())
        return;

    /* copy bus config */
    setBusConfig(pBusIdx, bus);
}


bool CANserver::piSendFrame(const CANFrame& )
{
    //We don't support sending frames right now
    return true;
}

void CANserver::connectToDevice()
{
    qDebug() << "CANserver: " << "Establishing UDP connection to a CANserver device...";
    
    bool bindResult = _udpClient->bind(1338);
    qDebug() << "CANserver: " << "UDP Bind result: " << bindResult;
    
    heartbeat();
    
    qDebug() << "CANserver: " << "Scheduling additional heartbeats...";
    _heartbeatTimer->start(3500);

    setStatus(CANCon::CONNECTED);

    CANConStatus stats;
    stats.conStatus = getStatus();
    stats.numHardwareBuses = 3;
    emit status(stats);
}

void CANserver::disconnectFromDevice()
{
    _heartbeatTimer->stop();
    
    QByteArray Data;
    Data.append("bye");
    QNetworkDatagram datagram(Data, _canserverAddress, 1338);
    
    qDebug() << "CANserver: " << "Sending bye...";
    _udpClient->writeDatagram(datagram);

    
    _udpClient->close();
}

void CANserver::heartbeat()
{
    //Send the PANDA hello packet (in this case we are using the v1 version of the protocol because we just want to get ALL the data)
    QByteArray Data;
    Data.append("hello");
    QNetworkDatagram datagram(Data, _canserverAddress, 1338);

    qDebug() << "CANserver: " << "Sending a heartbeat...";
    _udpClient->writeDatagram(datagram);
}

void CANserver::readNetworkData()
{
    QByteArray datagram;
    do {
        datagram.resize(_udpClient->pendingDatagramSize());
        _udpClient->readDatagram(datagram.data(), datagram.size());
    } while (_udpClient->hasPendingDatagrams());

    
    // If capture is suspended bail out after reading the bytes from the network.  No need to parse them
    if(isCapSuspended())
        return;

    
    uint16_t packetCount = datagram.length() / 16;
    //qDebug() << "Processing " << packetCount << " packets";

    //Process this chunk of data
    for (int i = 0 ; i < packetCount; i++)
    {
        uint16_t headerByteLocation = (i*16);
        uint16_t dataByteLocation = (i*16) + 8;

        
        uint32_t headerData1Byte1 = datagram.at(headerByteLocation + 0) & 0xFF;
        uint32_t headerData1Byte2 = datagram.at(headerByteLocation + 1) & 0xFF;
        uint32_t headerData1Byte3 = datagram.at(headerByteLocation + 2) & 0xFF;
        uint32_t headerData1Byte4 = datagram.at(headerByteLocation + 3) & 0xFF;
        
        uint32_t headerData2Byte1 = datagram.at(headerByteLocation + 4) & 0xFF;
        uint32_t headerData2Byte2 = datagram.at(headerByteLocation + 5) & 0xFF;
        uint32_t headerData2Byte3 = datagram.at(headerByteLocation + 6) & 0xFF;
        uint32_t headerData2Byte4 = datagram.at(headerByteLocation + 7) & 0xFF;
        
        
        //printf("HB1: %02X, %02X, %02X, %02X\n", headerData1Byte1, headerData1Byte2, headerData1Byte3, headerData1Byte4);
        //printf("HB2: %02X, %02X, %02X, %02X\n", headerData2Byte1, headerData2Byte2, headerData2Byte3, headerData2Byte4);

        uint32_t headerData1 = headerData1Byte1;
        headerData1 += headerData1Byte2 << 8;
        headerData1 += headerData1Byte3 << 16;
        headerData1 += headerData1Byte4 << 24;

        uint32_t headerData2 = headerData2Byte1;
        headerData2 += headerData2Byte2 << 8;
        headerData2 += headerData2Byte3 << 16;
        headerData2 += headerData2Byte4 << 24;

        uint32_t frameId = headerData1 >> 21;
        uint32_t length = (headerData2 & 0x0F);
        uint32_t busId = (headerData2 >> 4);

        //printf("frameId: %02X, busId: %d, length: %d\n", frameId, busId, length);
        
        CANFrame* frame_p = getQueue().get();
        if(frame_p)
        {
            frame_p->setFrameId(frameId);
            frame_p->setExtendedFrameFormat(0);

            // We need to change the bus id if it is the special CANserver bus id.
            // This keeps us from needing to define 15 busses just to get access to our special one
            if (busId == 15)
            {
                busId = 2;
            }
            frame_p->bus = busId;
            
            frame_p->setFrameType(QCanBusFrame::DataFrame);
            frame_p->isReceived = true;
        
            frame_p->setTimeStamp(QCanBusFrame::TimeStamp(0, QDateTime::currentMSecsSinceEpoch() * 1000ul));

            frame_p->setPayload(datagram.mid(dataByteLocation, length));
        
            checkTargettedFrame(*frame_p);

            /* enqueue frame */
            getQueue().queue();
        }
    }

}

void CANserver::heartbeatTimerSlot()
{
    heartbeat();
}

void CANserver::readSettings()
{
    QSettings settings;
}
