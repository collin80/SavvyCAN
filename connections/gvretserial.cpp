#include <QObject>
#include <QDebug>
#include <QCanBusFrame>
#include <QSerialPortInfo>
#include <QSettings>
#include <QStringBuilder>
#include <QtNetwork>

#include "gvretserial.h"

GVRetSerial::GVRetSerial(QString portName, bool useTcp) :
    CANConnection(portName, "gvret", CANCon::GVRET_SERIAL, 0, 0, 0, false, 0, 3, 4000, true),
    mTimer(this), /*NB: set this as parent of timer to manage it from working thread */
    useTcp(useTcp)
{
    sendDebug("GVRetSerial()");

    serial = nullptr;
    tcpClient = nullptr;
    udpClient = nullptr;
    rx_state = IDLE;
    rx_step = 0;
    validationCounter = 10; //how many times we can miss validation before we die
    isAutoRestart = false;
    espSerialMode = true;

    timeBasis = 0;
    lastSystemTimeBasis = 0;
    timeAtGVRETSync = 0;

    readSettings();
}


GVRetSerial::~GVRetSerial()
{
    stop();
    sendDebug("~GVRetSerial()");
}

void GVRetSerial::sendDebug(const QString debugText)
{
    qDebug() << debugText;
    debugOutput(debugText);
}

void GVRetSerial::sendToSerial(const QByteArray &bytes)
{
    if (serial == nullptr && tcpClient == nullptr && udpClient == nullptr)
    {
        sendDebug("Attempt to write to serial port when it has not been initialized!");
        return;
    }

    if (serial && !serial->isOpen())
    {
        sendDebug("Attempt to write to serial port when it is not open!");
        return;
    }

    if (tcpClient && !tcpClient->isOpen())
    {
        sendDebug("Attempt to write to TCP/IP port when it is not open!");
        return;
    }

    if (udpClient && !udpClient->isOpen())
    {
        sendDebug("Attempt to write to UDP Socket when it is not open!");
        return;
    }

    QString buildDebug;
    buildDebug = "Write to serial -> ";
    foreach (int byt, bytes) {
        byt = (unsigned char)byt;
        buildDebug = buildDebug % QString::number(byt, 16) % " ";
    }
    sendDebug(buildDebug);

    if (serial) serial->write(bytes);
    if (tcpClient) tcpClient->write(bytes);
    if (udpClient) udpClient->write(bytes);
}

void GVRetSerial::piStarted()
{    
    connectDevice();
}


void GVRetSerial::piSuspend(bool pSuspend)
{
    /* update capSuspended */
    setCapSuspended(pSuspend);

    /* flush queue if we are suspended */
    if(isCapSuspended())
        getQueue().flush();
}


void GVRetSerial::piStop()
{
    mTimer.stop();
    disconnectDevice();
}


bool GVRetSerial::piGetBusSettings(int pBusIdx, CANBus& pBus)
{
    return getBusConfig(pBusIdx, pBus);
}


void GVRetSerial::piSetBusSettings(int pBusIdx, CANBus bus)
{
    /* sanity checks */
    if( (pBusIdx < 0) || pBusIdx >= getNumBuses())
        return;

    /* copy bus config */
    setBusConfig(pBusIdx, bus);

    qDebug() << "About to update bus " << pBusIdx << " on GVRET";
    if (pBusIdx == 0)
    {
        can0Baud = bus.getSpeed();
        can0Baud |= 0x80000000;
        if (bus.isActive())
        {
            can0Baud |= 0x40000000;
            can0Enabled = true;
        }
        else can0Enabled = false;

        if (bus.isListenOnly())
        {
            can0Baud |= 0x20000000;
            can0ListenOnly = true;
        }
        else can0ListenOnly = false;
    }
    else if (pBusIdx == 1)
    {
        can1Baud = bus.getSpeed();
        can1Baud |= 0x80000000;
        if (bus.isActive())
        {
            can1Baud |= 0x40000000;
            can1Enabled = true;
        }
        else can1Enabled = false;

        if (bus.isListenOnly())
        {
            can1Baud |= 0x20000000;
            can1ListenOnly = true;
        }
        else can1ListenOnly = false;

        if (bus.isSingleWire())
        {
            can1Baud |= 0x10000000;
            deviceSingleWireMode = 1;
        }
        else deviceSingleWireMode = 0;
    }
    else if (pBusIdx == 2)
    {
        swcanBaud = bus.getSpeed();
        swcanBaud |= 0x80000000;
        if (bus.isActive())
        {
            swcanBaud |= 0x40000000;
            swcanEnabled = true;
        }
        else swcanEnabled = false;

        if (bus.isListenOnly())
        {
            swcanBaud |= 0x20000000;
            swcanListenOnly = true;
        }
        else swcanListenOnly = false;

    }

    if (pBusIdx < 2) {
        /* update baud rates */
        QByteArray buffer;
        sendDebug("Got signal to update bauds. 1: " + QString::number((can0Baud & 0xFFFFFFF)) + " 2: " + QString::number((can1Baud & 0xFFFFFFF)));
        buffer[0] = (char)0xF1; //start of a command over serial
        buffer[1] = 5; //setup canbus
        buffer[2] = (char)(can0Baud & 0xFF); //four bytes of ID LSB first
        buffer[3] = (char)(can0Baud >> 8);
        buffer[4] = (char)(can0Baud >> 16);
        buffer[5] = (char)(can0Baud >> 24);
        buffer[6] = (char)(can1Baud & 0xFF); //four bytes of ID LSB first
        buffer[7] = (char)(can1Baud >> 8);
        buffer[8] = (char)(can1Baud >> 16);
        buffer[9] = (char)(can1Baud >> 24);
        buffer[10] = 0;
        sendToSerial(buffer);
    }
    else
    {
        /* update baud rates */
        QByteArray buffer;
        sendDebug("Got signal to update extended bus speeds SWCAN: " + QString::number(swcanBaud) + " LIN1: " + QString::number(lin1Baud) + " LIN2: " + QString::number(lin2Baud));
        buffer[0] = (char)0xF1; //start of a command over serial
        buffer[1] = 14; //setup extended buses
        buffer[2] = (char)(swcanBaud & 0xFF); //four bytes of ID LSB first
        buffer[3] = (char)(swcanBaud >> 8);
        buffer[4] = (char)(swcanBaud >> 16);
        buffer[5] = (char)(swcanBaud >> 24);
        buffer[6] = (char)(lin1Baud & 0xFF); //four bytes of ID LSB first
        buffer[7] = (char)(lin1Baud >> 8);
        buffer[8] = (char)(lin1Baud >> 16);
        buffer[9] = (char)(lin1Baud >> 24);
        buffer[10] = (char)(lin2Baud & 0xFF); //four bytes of ID LSB first
        buffer[11] = (char)(lin2Baud >> 8);
        buffer[12] = (char)(lin2Baud >> 16);
        buffer[13] = (char)(lin2Baud >> 24);
        buffer[14] = 0;
        sendToSerial(buffer);
    }
}


bool GVRetSerial::piSendFrame(const CANFrame& frame)
{
    QByteArray buffer;
    int c;
    quint32 ID;

    //qDebug() << "Sending out GVRET frame with id " << frame.ID << " on bus " << frame.bus;

    framesRapid++;

    if (serial == nullptr && tcpClient == nullptr && udpClient == nullptr) return false;
    if (serial && !serial->isOpen()) return false;
    if (tcpClient && !tcpClient->isOpen()) return false;
    if (udpClient && !udpClient->isOpen()) return false;
    //if (!isConnected) return false;

    // Doesn't make sense to send an error frame
    // to an adapter
    if (frame.frameId() & 0x20000000) {
        return true;
    }
    ID = frame.frameId();
    if (frame.hasExtendedFrameFormat()) ID |= 1u << 31;

    buffer[0] = (char)0xF1; //start of a command over serial
    buffer[1] = 0; //command ID for sending a CANBUS frame
    buffer[2] = (char)(ID & 0xFF); //four bytes of ID LSB first
    buffer[3] = (char)(ID >> 8);
    buffer[4] = (char)(ID >> 16);
    buffer[5] = (char)(ID >> 24);
    buffer[6] = (char)((frame.bus) & 3);
    buffer[7] = (char)frame.payload().length();
    for (c = 0; c < frame.payload().length(); c++)
    {
        buffer[8 + c] = frame.payload()[c];
    }
    buffer[8 + frame.payload().length()] = 0;

    sendToSerial(buffer);

    return true;
}



/****************************************************************/

void GVRetSerial::readSettings()
{
    QSettings settings;

    if (settings.value("Main/ValidateComm", true).toBool())
    {
        doValidation = true;
    }
    else doValidation = false;
}


void GVRetSerial::connectDevice()
{
    QSettings settings;

    /* disconnect device */
    if(serial)
        disconnectDevice();
    if(tcpClient)
        disconnectDevice();
    if (udpClient)
        disconnectDevice();

    /* open new device */

    if (useTcp)
    {
        // /*
        sendDebug("TCP Connection to a GVRET device");
        tcpClient = new QTcpSocket();
        tcpClient->connectToHost(getPort(), 23);
        connect(tcpClient, SIGNAL(readyRead()), this, SLOT(readSerialData()));
        connect(tcpClient, SIGNAL(connected()), this, SLOT(deviceConnected()));
        sendDebug("Created TCP Socket");
        // */
        /*
        qDebug() << "UDP Connection to a GVRET device";
        udpClient = new QUdpSocket();
        udpClient->connectToHost(getPort(), 17222);
        connect(udpClient, SIGNAL(readyRead()), this, SLOT(readSerialData()));
        //connect(udpClient, SIGNAL(connected()), this, SLOT(tcpConnected()));
        debugOutput("Created UDP Socket");
        tcpConnected();
        */
    }
    else {
        sendDebug("Serial connection to a GVRET device");
        serial = new QSerialPort(QSerialPortInfo(getPort()));
        if(!serial) {
            sendDebug("can't open serial port " + getPort());
            return;
        }
        sendDebug("Created Serial Port Object");

        /* connect reading event */
        connect(serial, SIGNAL(readyRead()), this, SLOT(readSerialData()));
        connect(serial, SIGNAL(error(QSerialPort::SerialPortError)), this, SLOT(serialError(QSerialPort::SerialPortError)));

        /* configure */
        serial->setBaudRate(1000000); //most GVRET devices ignore baud, ESP32 needs it set explicitly to the proper value
        serial->setDataBits(serial->Data8);

        if (espSerialMode)
        {
            sendDebug("Trying ESP32 Serial Mode");
            serial->setFlowControl(serial->NoFlowControl);
            if (!serial->open(QIODevice::ReadWrite))
            {
                //sendDebug("Error returned during port opening: " + serial->errorString());
            }
            else
            {
                serial->setDataTerminalReady(false); //ESP32 uses these for bootloader selection and reset so turn them off
                serial->setRequestToSend(false);                
                QTimer::singleShot(3000, this, SLOT(deviceConnected())); //give ESP32 some time as it could have rebooted
            }
        }
        else
        {            
            sendDebug("Trying Standard Serial Mode");
            serial->setFlowControl(serial->HardwareControl); //Most GVRET style devices use hardware flow control
            if (!serial->open(QIODevice::ReadWrite))
            {
                //sendDebug("Error returned during port opening: " + serial->errorString());
            }
            else
            {
                //serial->setDataTerminalReady(true); //Seemingly these two lines used to be needed
                //serial->setRequestToSend(true);     //But, really both ends should automatically handle these
                deviceConnected();
            }
        }
    }
}

void GVRetSerial::deviceConnected()
{
    sendDebug("Connecting to GVRET Device!");
    QByteArray output;
    output.append((char)0xE7); //this puts the device into binary comm mode
    output.append((char)0xE7);

    output.append((char)0xF1);
    output.append((char)0x0C); //get number of actually implemented buses. Not implemented except on M2RET
    mNumBuses = 2; //the proper number if C/12 is not implemented

    output.append((char)0xF1); //signal we want to issue a command
    output.append((char)0x06); //request canbus stats from the board

    output.append((char)0xF1); //another command to the GVRET
    output.append((char)0x07); //request device information

    /*output.append((char)0xF1);
    output.append((char)0x08); //setting singlewire mode
    if (settings.value("Main/SingleWireMode", false).toBool())
    {
        output.append((char)0x10); //signal that we do want single wire mode
    }
    else
    {
        output.append((char)0xFF); //signal we don't want single wire mode
    }*/

    output.append((char)0xF1); //and another command
    output.append((char)0x01); //Time Sync - Not implemented until 333 but we can try

    output.append((char)0xF1); //yet another command
    output.append((char)0x09); //comm validation command

    continuousTimeSync = true;

    sendToSerial(output);

    if(doValidation) {
        QTimer::singleShot(5000, this, SLOT(connectionTimeout()));
    }
    else {
        setStatus(CANCon::CONNECTED);
        CANConStatus stats;
        stats.conStatus = getStatus();
        stats.numHardwareBuses = mNumBuses;
        emit status(stats);
    }
}

void GVRetSerial::disconnectDevice() {
    if (serial != nullptr)
    {
        if (serial->isOpen())
        {
            //serial->clear();
            serial->close();

        }
        serial->disconnect(); //disconnect all signals
        delete serial;
        serial = nullptr;
    }
    if (tcpClient != nullptr)
    {
        if (tcpClient->isOpen())
        {
            tcpClient->close();
        }
        tcpClient->disconnect();
        delete tcpClient;
        tcpClient = nullptr;
    }
    if (udpClient != nullptr)
    {
        if (udpClient->isOpen())
        {
            udpClient->close();
        }
        udpClient->disconnect();
        delete udpClient;
        udpClient = nullptr;
    }

    setStatus(CANCon::NOT_CONNECTED);
    CANConStatus stats;
    stats.conStatus = getStatus();
    stats.numHardwareBuses = mNumBuses;
    emit status(stats);
}

void GVRetSerial::serialError(QSerialPort::SerialPortError err)
{
    QString errMessage;
    bool killConnection = false;
    switch (err)
    {
    case QSerialPort::NoError:
        return;
    case QSerialPort::DeviceNotFoundError:
        errMessage = "Device not found error on serial";
        killConnection = true;
        piStop();
        break;
    case QSerialPort::PermissionError:
        errMessage =  "Permission error on serial port";
        killConnection = true;
        piStop();
        break;
    case QSerialPort::OpenError:
        errMessage =  "Open error on serial port";
        killConnection = true;
        piStop();
        break;
#if QT_VERSION <= QT_VERSION_CHECK( 6, 0, 0 )
    case QSerialPort::ParityError:
        errMessage = "Parity error on serial port";
        break;
    case QSerialPort::FramingError:
        errMessage = "Framing error on serial port";
        break;
    case QSerialPort::BreakConditionError:
        errMessage = "Break error on serial port";
        break;
#endif
    case QSerialPort::WriteError:
        errMessage = "Write error on serial port";
        piStop();
        break;
    case QSerialPort::ReadError:
        errMessage = "Read error on serial port";
        piStop();
        break;
    case QSerialPort::ResourceError:
        errMessage = "Serial port seems to have disappeared.";
        killConnection = true;
        piStop();
        break;
    case QSerialPort::UnsupportedOperationError:
        errMessage = "Unsupported operation on serial port";
        killConnection = true;
        break;
    case QSerialPort::UnknownError:
        errMessage = "Beats me what happened to the serial port.";
        killConnection = true;
        piStop();
        break;
    case QSerialPort::TimeoutError:
        errMessage = "Timeout error on serial port";
        killConnection = true;
        break;
    case QSerialPort::NotOpenError:
        errMessage = "The serial port isn't open";
        killConnection = true;
        piStop();
        break;
    }
    /*
    if (serial)
    {
        serial->clearError();
        serial->flush();
        serial->close();
    }*/
    if (errMessage.length() > 1)
    {
        sendDebug(errMessage);
    }
    if (killConnection)
    {
        qDebug() << "Shooting the serial object in the head. It deserves it.";
        disconnectDevice();
    }
}


void GVRetSerial::connectionTimeout()
{
    //one second after trying to connect are we actually connected?
    if (CANCon::NOT_CONNECTED==getStatus()) //no?
    {
        //then emit the the failure signal and see if anyone cares
        sendDebug("Failed to connect to GVRET at that com port");

        //toggle the serial mode and try again
        espSerialMode = !espSerialMode;
        disconnectDevice();
        connectDevice();
    }
    else
    {
        /* start timer */
        connect(&mTimer, SIGNAL(timeout()), this, SLOT(handleTick()));
        mTimer.setInterval(250); //tick four times per second
        mTimer.setSingleShot(false); //keep ticking
        mTimer.start();
    }
}


void GVRetSerial::readSerialData()
{
    QByteArray data;
    unsigned char c;
    QString debugBuild;

    if (serial) data = serial->readAll();
    if (tcpClient) data = tcpClient->readAll();
    if (udpClient) data = udpClient->readAll();

    sendDebug("Got data from serial. Len = " % QString::number(data.length()));
    for (int i = 0; i < data.length(); i++)
    {
        c = data.at(i);
        //qDebug() << c << "    " << QString::number(c, 16) << "     " << QString(c);
        debugBuild = debugBuild % QString::number(c, 16).rightJustified(2,'0') % " ";
        procRXChar(c);
    }
    debugOutput(debugBuild);
    //qDebug() << debugBuild;
}

//Debugging data sent from connection window. Inject it into Comm traffic.
void GVRetSerial::debugInput(QByteArray bytes) {
   sendToSerial(bytes);
}

void GVRetSerial::procRXChar(unsigned char c)
{
    CANConStatus stats;
    int oldBuses;
    QByteArray output;

    switch (rx_state)
    {
    case IDLE:
        if (c == 0xF1) rx_state = GET_COMMAND;
        break;
    case GET_COMMAND:
        switch (c)
        {
        case 0: //receiving a can frame
            rx_state = BUILD_CAN_FRAME;
            rx_step = 0;
            break;
        case 1: //time sync
            rx_state = TIME_SYNC;
            rx_step = 0;
            break;
        case 2: //process a return reply for digital input states.
            rx_state = GET_DIG_INPUTS;
            rx_step = 0;
            break;
        case 3: //process a return reply for analog inputs
            rx_state = GET_ANALOG_INPUTS;
            break;
        case 4: //we set digital outputs we don't accept replies so nothing here.
            rx_state = IDLE;
            break;
        case 5: //we set canbus specs we don't accept replies.
            rx_state = IDLE;
            break;
        case 6: //get canbus parameters from GVRET
            rx_state = GET_CANBUS_PARAMS;
            rx_step = 0;
            break;
        case 7: //get device info
            rx_state = GET_DEVICE_INFO;
            rx_step = 0;
            break;
        case 9:
            validationCounter = 10;
            qDebug() << "Got validated";
            rx_state = IDLE;
            break;
        case 12:
            rx_state = GET_NUM_BUSES;
            qDebug() << "Got num buses reply";
            rx_step = 0;
            break;
        case 13:
            rx_state = GET_EXT_BUSES;
            qDebug() << "Got extended buses info reply";
            rx_step = 0;
            break;
        case 20:
            rx_state = BUILD_FD_FRAME;
            rx_step = 0;
            break;
        case 22:
            rx_state = GET_FD_SETTINGS;
            rx_step = 0;
            qDebug() << "Got FD settings reply";
            break;
        }
        break;
    case BUILD_CAN_FRAME:
        switch (rx_step)
        {
        case 0:
            buildTimestamp = c;
            break;
        case 1:
            buildTimestamp |= (uint)(c << 8);
            break;
        case 2:
            buildTimestamp |= (uint)c << 16;
            break;
        case 3:
            buildTimestamp |= (uint)c << 24;

            buildTimestamp += timeBasis;
            if (useSystemTime)
            {
                buildTimestamp = QDateTime::currentMSecsSinceEpoch() * 1000l;
            }
            buildFrame.setTimeStamp(QCanBusFrame::TimeStamp(0, buildTimestamp));
            break;
        case 4:
            buildId = c;
            break;
        case 5:
            buildId |= c << 8;
            break;
        case 6:
            buildId |= c << 16;
            break;
        case 7:
            buildId |= c << 24;
            if ((buildId & 1 << 31) == 1u << 31)
            {
                buildId &= 0x7FFFFFFF;
                buildFrame.setExtendedFrameFormat(true);
            }
            else buildFrame.setExtendedFrameFormat(false);
            buildFrame.setFrameId(buildId);
            break;
        case 8:
            buildData.resize(c & 0xF);
            buildFrame.bus = (c & 0xF0) >> 4;
            break;
        default:
            if (rx_step < buildData.length() + 9)
            {
                buildData[rx_step - 9] = c;
                if (rx_step == buildData.length() + 8) //it's the last data byte so immediately process the frame
                {
                    rx_state = IDLE;
                    rx_step = 0;
                    buildFrame.isReceived = true;
                    buildFrame.setPayload(buildData);
                    buildFrame.setFrameType(QCanBusFrame::FrameType::DataFrame);
                    if (!isCapSuspended())
                    {
                        /* get frame from queue */
                        CANFrame* frame_p = getQueue().get();
                        if(frame_p) {
                            //qDebug() << "GVRET got frame on bus " << frame_p->bus;
                            /* copy frame */
                            *frame_p = buildFrame;
                            checkTargettedFrame(buildFrame);
                            /* enqueue frame */
                            getQueue().queue();
                        }
                        else
                            qDebug() << "can't get a frame, ERROR";

                        //take the time the frame came in and try to resync the time base.
                        //if (continuousTimeSync) txTimestampBasis = QDateTime::currentMSecsSinceEpoch() - (buildFrame.timestamp / 1000);
                    }
                }
            }
            else //should never get here! But, just in case, reset the comm
            {
                rx_state = IDLE;
                rx_step = 0;
            }
            break;
        }
        rx_step++;
        break;
    case BUILD_FD_FRAME:
        switch (rx_step)
        {
        case 0:
            buildTimestamp = c;
            break;
        case 1:
            buildTimestamp |= (uint)(c << 8);
            break;
        case 2:
            buildTimestamp |= (uint)c << 16;
            break;
        case 3:
            buildTimestamp |= (uint)c << 24;

            buildTimestamp += timeBasis;
            if (useSystemTime)
            {
                buildTimestamp = QDateTime::currentMSecsSinceEpoch() * 1000l;
            }
            buildFrame.setTimeStamp(QCanBusFrame::TimeStamp(0, buildTimestamp));
            break;
        case 4:
            buildId = c;
            break;
        case 5:
            buildId |= c << 8;
            break;
        case 6:
            buildId |= c << 16;
            break;
        case 7:
            buildId |= c << 24;
            if ((buildId & 1 << 31) == 1u << 31)
            {
                buildId &= 0x7FFFFFFF;
                buildFrame.setExtendedFrameFormat(true);
            }
            else buildFrame.setExtendedFrameFormat(false);
            buildFrame.setFrameId(buildId);
            break;
        case 8:
            buildData.resize(c & 0x3F);
            break;
        case 9:
            buildFrame.bus = c;
            break;
        default:
            if (rx_step < buildData.length() + 10)
            {
                buildData[rx_step - 9] = c;
            }
            else
            {
                rx_state = IDLE;
                rx_step = 0;
                buildFrame.isReceived = true;
                buildFrame.setPayload(buildData);
                buildFrame.setFrameType(QCanBusFrame::FrameType::DataFrame);
                if (!isCapSuspended())
                {
                    /* get frame from queue */
                    CANFrame* frame_p = getQueue().get();
                    if(frame_p) {
                        //qDebug() << "GVRET got frame on bus " << frame_p->bus;
                        /* copy frame */
                        *frame_p = buildFrame;
                        checkTargettedFrame(buildFrame);
                        /* enqueue frame */
                        getQueue().queue();
                    }
                    else
                        qDebug() << "can't get a frame, ERROR";

                    //take the time the frame came in and try to resync the time base.
                    //if (continuousTimeSync) txTimestampBasis = QDateTime::currentMSecsSinceEpoch() - (buildFrame.timestamp / 1000);
                }
            }
            break;
        }
        rx_step++;
        break;
    case TIME_SYNC: //gives a pretty good base guess for the proper timestamp. Can be refined when traffic starts to flow (if wanted)
        switch (rx_step)
        {
        case 0:
            buildTimeBasis = c;
            break;
        case 1:
            buildTimeBasis += ((uint32_t)c << 8);
            break;
        case 2:
            buildTimeBasis += ((uint32_t)c << 16);
            break;
        case 3:
            buildTimeBasis += ((uint32_t)c << 24);
            qDebug() << "GVRET firmware reports timestamp of " << buildTimeBasis;
            timeAtGVRETSync = QDateTime::currentMSecsSinceEpoch() * 1000;

            rebuildLocalTimeBasis();

            continuousTimeSync = false;
            rx_state = IDLE;
            break;
        }
        rx_step++;
        break;

    case GET_ANALOG_INPUTS: //get 9 bytes - 2 per analog input plus checksum
        switch (rx_step)
        {
        case 0:
            break;
        }
        rx_step++;
        break;
    case GET_DIG_INPUTS: //get two bytes. One for digital in status and one for checksum.
        switch (rx_step)
        {
        case 0:
            break;
        case 1:
            rx_state = IDLE;
            break;
        }
        rx_step++;
        break;
    case GET_CANBUS_PARAMS:
        switch (rx_step)
        {
        case 0:
            can0Enabled = (c & 0xF);
            can0ListenOnly = (c >> 4);
            break;
        case 1:
            can0Baud = c;
            break;
        case 2:
            can0Baud |= c << 8;
            break;
        case 3:
            can0Baud |= c << 16;
            break;
        case 4:
            can0Baud |= c << 24;
            break;
        case 5:
            can1Enabled = (c & 0xF);
            can1ListenOnly = (c >> 4);
            deviceSingleWireMode = (c >> 6);
            break;
        case 6:
            can1Baud = c;
            break;
        case 7:
            can1Baud |= c << 8;
            break;
        case 8:
            can1Baud |= c << 16;
            break;
        case 9:
            can1Baud |= c << 24;
            rx_state = IDLE;
            qDebug() << "Baud 0 = " << can0Baud;
            qDebug() << "Baud 1 = " << can1Baud;
            mBusData[0].mBus.setSpeed(can0Baud);
            mBusData[0].mBus.setActive(can0Enabled);
            mBusData[0].mConfigured = true;
            if (mBusData.count() > 1)
            {
                mBusData[1].mBus.setSpeed(can1Baud);
                mBusData[1].mBus.setActive(can1Enabled);
                mBusData[1].mConfigured = true;
            }

            can0Baud |= 0x80000000;
            if (can0Enabled) can0Baud |= 0x40000000;
            if (can0ListenOnly) can0Baud |= 0x20000000;

            can1Baud |= 0x80000000;
            if (can1Enabled) can1Baud |= 0x40000000;
            if (can1ListenOnly) can1Baud |= 0x20000000;
            if (deviceSingleWireMode > 0) can1Baud |= 0x10000000;

            setStatus(CANCon::CONNECTED);
            stats.conStatus = getStatus();
            stats.numHardwareBuses = mNumBuses;
            emit status(stats);

            int can0Status = 0x78; //updating everything we can update
            int can1Status = 0x78;
            if (can0Enabled) can0Status +=1;
            if (can0ListenOnly) can0Status += 4;
            if (can1Enabled) can1Status += 1;
            if (deviceSingleWireMode > 0) can1Status += 2;
            if (can1ListenOnly) can1Status += 4;
            //emit busStatus(busBase, can0Baud & 0xFFFFF, can0Status);
            //emit busStatus(busBase + 1, can1Baud & 0xFFFFF, can1Status);
            break;
        }
        rx_step++;
        break;
    case GET_DEVICE_INFO:
        switch (rx_step)
        {
        case 0:
            deviceBuildNum = c;
            break;
        case 1:
            deviceBuildNum |= c << 8;
            break;
        case 2:
            break; //don't care about eeprom version
        case 3:
            break; //don't care about file type
        case 4:
            break; //don't care about whether it auto logs or not
        case 5:
            deviceSingleWireMode = c;
            rx_state = IDLE;
            qDebug() << "build num: " << deviceBuildNum;
            qDebug() << "single wire can: " << deviceSingleWireMode;
            emit deviceInfo(deviceBuildNum, deviceSingleWireMode);
            break;
        }
        rx_step++;
        break;
    case SET_DIG_OUTPUTS:
        rx_state = IDLE;
        break;
    case SETUP_CANBUS:
        rx_state = IDLE;
        break;
    case SET_SINGLEWIRE_MODE:
        rx_state = IDLE;
        break;
    case GET_NUM_BUSES:
        oldBuses = mNumBuses;
        mNumBuses = c;
        rx_state = IDLE;
        qDebug() << "Get number of buses = " << mNumBuses;
        stats.conStatus = getStatus();
        stats.numHardwareBuses = mNumBuses;
        mBusData.resize(mNumBuses);
        if (mNumBuses > oldBuses)
        {
            for (int i = oldBuses; i < mNumBuses; i++)
            {
                mBusData[i].mConfigured = true;
                mBusData[i].mBus = mBusData[0].mBus;
            }
        }

        output.append((unsigned char)0xF1); //start a new command
        output.append((unsigned char)13); //get extended buses
        sendToSerial(output);

        emit status(stats);
        break;
    case GET_FD_SETTINGS:
        break;
    case GET_EXT_BUSES:
        switch (rx_step)
        {
        case 0:
            swcanEnabled = (c & 0xF);
            swcanListenOnly = (c >> 4);
            break;
        case 1:
            swcanBaud = c;
            break;
        case 2:
            swcanBaud |= c << 8;
            break;
        case 3:
            swcanBaud |= c << 16;
            break;
        case 4:
            swcanBaud |= c << 24;
            break;
        case 5:
            lin1Enabled = (c & 0xF);
            break;
        case 6:
            lin1Baud = c;
            break;
        case 7:
            lin1Baud |= c << 8;
            break;
        case 8:
            lin1Baud |= c << 16;
            break;
        case 9:
            lin1Baud |= c << 24;
            break;
        case 10:
            lin2Enabled = (c & 0xF);
            break;
        case 11:
            lin2Baud = c;
            break;
        case 12:
            lin2Baud |= c << 8;
            break;
        case 13:
            lin2Baud |= c << 16;
            break;
        case 14:
            lin2Baud |= c << 24;
            rx_state = IDLE;
            qDebug() << "SWCAN Baud = " << swcanBaud;
            qDebug() << "LIN1 Baud = " << lin1Baud;
            qDebug() << "LIN2 Baud = " << lin2Baud;
            if (getNumBuses() > 2)
            {
                mBusData[2].mBus.setSpeed(swcanBaud);
                mBusData[2].mBus.setActive(swcanEnabled);
            }

            setStatus(CANCon::CONNECTED);
            stats.conStatus = getStatus();
            stats.numHardwareBuses = mNumBuses;
            emit status(stats);
            break;
        }
        rx_step++;
        break;
    }
}

void GVRetSerial::rebuildLocalTimeBasis()
{
    qDebug() << "Rebuilding GVRET time base. GVRET local base = " << buildTimeBasis;

    /*
      our time basis is the value we have to modulate the main system basis by in order
      to sync the GVRET timestamps to the rest of the system.
      The rest of the system uses CANConManager::getInstance()->getTimeBasis as the basis.
      GVRET returns to us the current time since boot up in microseconds.
      timeAtGVRETSync stores the "system" timestamp when the GVRET timestamp was retrieved.
    */
    lastSystemTimeBasis = CANConManager::getInstance()->getTimeBasis();
    int64_t systemDelta = timeAtGVRETSync - lastSystemTimeBasis;
    int32_t localDelta = buildTimeBasis - systemDelta;
    timeBasis = -localDelta;
}

void GVRetSerial::handleTick()
{
    if (lastSystemTimeBasis != CANConManager::getInstance()->getTimeBasis()) rebuildLocalTimeBasis();
    //qDebug() << "Tick!";

    if( CANCon::CONNECTED == getStatus() )
    {
        if (doValidation) validationCounter--;
        //qDebug() << validationCounter;
        if (validationCounter == 0 && doValidation)
        {
            if (serial == nullptr && tcpClient == nullptr) return;
            if ( (serial && serial->isOpen()) || (tcpClient && tcpClient->isOpen()) || (udpClient && udpClient->isOpen())) //if it's still false we have a problem...
            {
                sendDebug("Comm validation failed.");

                setStatus(CANCon::NOT_CONNECTED);
                //emit status(getStatus());

                disconnectDevice(); //start by stopping everything.
                //Then wait 500ms and restart the connection automatically
                //QTimer::singleShot(500, this, SLOT(connectDevice()));
                return;
            }
        }
        else if (doValidation)
        {
            //qDebug()  << "Comm connection validated";
        }
    }
    if (doValidation && serial && serial->isOpen()) sendCommValidation();
    if (doValidation && tcpClient && tcpClient->isOpen()) sendCommValidation();
    if (doValidation && udpClient && udpClient->isOpen()) sendCommValidation();
}


void GVRetSerial::sendCommValidation()
{
    QByteArray output;

    output.append((unsigned char)0xF1); //another command to the GVRET
    output.append((unsigned char)0x09); //request a reply to get validation

    sendToSerial(output);
}



