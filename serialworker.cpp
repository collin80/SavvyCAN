#include "serialworker.h"

#include <QSerialPort>
#include <QDebug>
#include <QTimer>
#include <QSettings>

#if 0
SerialWorker::SerialWorker(CANFrameModel *model, int base) : CANConnection(model, base)
{    
    qDebug() << "Serial Worker constructor";
    serial = NULL;
    rx_state = IDLE;
    rx_step = 0;
    buildFrame;
    ticker = NULL;
    framesRapid = 0;
    gotValidated = true;
    isAutoRestart = false;

    txTimestampBasis = QDateTime::currentMSecsSinceEpoch();

    readSettings();
}

SerialWorker::~SerialWorker()
{
    if (serial != NULL)
    {
        if (serial->isOpen())
        {
            serial->clear();
            serial->close();

        }
        serial->disconnect(); //disconnect all signals
        delete serial;
    }
    if (ticker != NULL) ticker->stop();
}

void SerialWorker::run()
{
    qDebug() << "Serial worker thread starting";
    ticker = new QTimer;
    connect(ticker, SIGNAL(timeout()), this, SLOT(handleTick()));

    ticker->setInterval(250); //tick four times per second
    ticker->setSingleShot(false); //keep ticking
    ticker->start();
}

void SerialWorker::readSettings()
{
    QSettings settings;

    if (settings.value("Main/ValidateComm", true).toBool())
    {
        doValidation = true;
    }
    else doValidation = false;
    //doValidation=false;
}

void SerialWorker::setSerialPort(QSerialPortInfo *port)
{
    QSettings settings;

    currentPort = port;

    if (serial != NULL)
    {
        if (serial->isOpen())
        {
            serial->clear();
            serial->close();
        }
        serial->disconnect(); //disconnect all signals
        delete serial;
    }

    serial = new QSerialPort(*port);

    qDebug() << "Serial port name is " << port->portName();
    serial->setDataBits(serial->Data8);
    serial->setFlowControl(serial->HardwareControl); //this is important though
    if (!serial->open(QIODevice::ReadWrite))
    {
        qDebug() << serial->errorString();
    }
    serial->setDataTerminalReady(true); //you do need to set these or the fan gets dirty
    serial->setRequestToSend(true);
    QByteArray output;
    output.append((char)0xE7); //this puts the device into binary comm mode
    output.append((char)0xE7);

    output.append((char)0xF1); //signal we want to issue a command
    output.append((char)0x06); //request canbus stats from the board

    output.append((char)0xF1); //another command to the GVRET
    output.append((char)0x07); //request device information

    output.append((char)0xF1);
    output.append((char)0x08); //setting singlewire mode
    if (settings.value("Main/SingleWireMode", false).toBool())
    {
        output.append((char)0x10); //signal that we do want single wire mode
    }
    else
    {
        output.append((char)0xFF); //signal we don't want single wire mode
    }

    output.append((char)0xF1); //yet another command
    output.append((char)0x09); //comm validation command

    output.append((char)0xF1); //and another command
    output.append((char)0x01); //Time Sync - Not implemented until 333 but we can try

    continuousTimeSync = true;

    serial->write(output);
    if (doValidation) isConnected = false;
        else isConnected = true;
    connect(serial, SIGNAL(readyRead()), this, SLOT(readSerialData()));
    if (doValidation) QTimer::singleShot(1000, this, SLOT(connectionTimeout()));
}

void SerialWorker::connectionTimeout()
{
    //one second after trying to connect are we actually connected?
    if (!isConnected) //no?
    {
        //then emit the the failure signal and see if anyone cares
        qDebug() << "Failed to connect to GVRET at that com port";
        //ticker->stop();
        closeSerialPort(); //make sure it's properly closed anyway
        emit connectionFailure(this);
    }
}

void SerialWorker::readSerialData()
{
    QByteArray data = serial->readAll();
    unsigned char c;
    //qDebug() << (tr("Got data from serial. Len = %0").arg(data.length()));
    for (int i = 0; i < data.length(); i++)
    {
        c = data.at(i);
        procRXChar(c);
    }
    if (framesRapid > 0)
    {
        emit frameUpdateRapid(framesRapid);
        framesRapid = 0;
    }
}

void SerialWorker::sendFrame(const CANFrame *frame)
{
    QByteArray buffer;
    int c;
    int ID;
    CANFrame tempFrame = *frame;
    tempFrame.isReceived = false;
    tempFrame.timestamp = ((QDateTime::currentMSecsSinceEpoch() - txTimestampBasis) * 1000);

    //qDebug() << "Sending out frame with id " << frame->ID;

    //show our sent frames in the list too. This happens even if we're not connected.
    /* model lives in UI thread, we need to call invokeMethod */
    QMetaObject::invokeMethod(model, "addFrame",
                              Qt::QueuedConnection,
                              Q_ARG(CANFrame, tempFrame),
                              Q_ARG(bool, false));

    framesRapid++;

    if (serial == NULL) return;
    if (!serial->isOpen()) return;
    if (!isConnected) return;

    ID = frame->ID;
    if (frame->extended) ID |= 1 << 31;

    buffer[0] = (char)0xF1; //start of a command over serial
    buffer[1] = 0; //command ID for sending a CANBUS frame
    buffer[2] = (unsigned char)(ID & 0xFF); //four bytes of ID LSB first
    buffer[3] = (unsigned char)(ID >> 8);
    buffer[4] = (unsigned char)(ID >> 16);
    buffer[5] = (unsigned char)(ID >> 24);
    buffer[6] = (unsigned char)((frame->bus - this->getBusBase()) & 1);
    buffer[7] = (unsigned char)frame->len;
    for (c = 0; c < frame->len; c++)
    {
        buffer[8 + c] = frame->data[c];
    }
    buffer[8 + frame->len] = 0;

    //qDebug() << "writing " << buffer.length() << " bytes to serial port";
    serial->write(buffer);
}

//a simple way for another thread to pass us a bunch of frames to send.
//Don't get carried away here. The GVRET firmware only has finite
//buffers and besides, the other end will get buried in traffic.
void SerialWorker::sendFrameBatch(const QList<CANFrame> *frames)
{
    sendBulkMutex.lock();
    for (int i = 0; i < frames->length(); i++) sendFrame(&frames->at(i));
    sendBulkMutex.unlock();
}

void SerialWorker::updateBaudRates(unsigned int Speed1, unsigned int Speed2)
{
    QByteArray buffer;
    qDebug() << "Got signal to update bauds. 1: " << Speed1 <<" 2: " << Speed2;
    buffer[0] = (char)0xF1; //start of a command over serial
    buffer[1] = 5; //setup canbus
    buffer[2] = (unsigned char)(Speed1 & 0xFF); //four bytes of ID LSB first
    buffer[3] = (unsigned char)(Speed1 >> 8);
    buffer[4] = (unsigned char)(Speed1 >> 16);
    buffer[5] = (unsigned char)(Speed1 >> 24);
    buffer[6] = (unsigned char)(Speed2 & 0xFF); //four bytes of ID LSB first
    buffer[7] = (unsigned char)(Speed2 >> 8);
    buffer[8] = (unsigned char)(Speed2 >> 16);
    buffer[9] = (unsigned char)(Speed2 >> 24);
    buffer[10] = 0;
    if (serial == NULL) return;
    if (!serial->isOpen()) return;
    serial->write(buffer);
}

void SerialWorker::procRXChar(unsigned char c)
{
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
            gotValidated = true;
            //qDebug() << "Got validated";
            rx_state = IDLE;
            break;
        }
        break;
    case BUILD_CAN_FRAME:
        switch (rx_step)
        {
        case 0:
            buildFrame.timestamp = c;
            break;
        case 1:
            buildFrame.timestamp |= (uint)(c << 8);
            break;
        case 2:
            buildFrame.timestamp |= (uint)c << 16;
            break;
        case 3:
            buildFrame.timestamp |= (uint)c << 24;
            break;
        case 4:
            buildFrame.ID = c;
            break;
        case 5:
            buildFrame.ID |= c << 8;
            break;
        case 6:
            buildFrame.ID |= c << 16;
            break;
        case 7:
            buildFrame.ID |= c << 24;
            if ((buildFrame.ID & 1 << 31) == 1 << 31)
            {
                buildFrame.ID &= 0x7FFFFFFF;
                buildFrame.extended = true;
            }
            else buildFrame.extended = false;
            break;
        case 8:
            buildFrame.len = c & 0xF;
            if (buildFrame.len > 8) buildFrame.len = 8;
            buildFrame.bus = (c & 0xF0) >> 4;
            break;
        default:
            if (rx_step < buildFrame.len + 9)
            {
                buildFrame.data[rx_step - 9] = c;
            }
            else
            {
                rx_state = IDLE;
                rx_step = 0;
                //qDebug() << "emit from serial handler to main form id: " << buildFrame.ID;
                //if (capturing)
                //{
                    buildFrame.isReceived = true;
                    /* model lives in UI thread, we need to call invokeMethod */
                    QMetaObject::invokeMethod(model, "addFrame",
                                              Qt::QueuedConnection,
                                              Q_ARG(CANFrame, buildFrame),
                                              Q_ARG(bool, false));

                    //take the time the frame came in and try to resync the time base.
                    if (continuousTimeSync) txTimestampBasis = QDateTime::currentMSecsSinceEpoch() - (buildFrame.timestamp / 1000);
                    framesRapid++;                    
                //}
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
            txTimestampBasis = QDateTime::currentMSecsSinceEpoch() - ((uint64_t)buildTimeBasis / (uint64_t)1000ull);
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

            can0Baud |= 0x80000000;
            if (can0Enabled) can0Baud |= 0x40000000;
            if (can0ListenOnly) can0Baud |= 0x20000000;

            can1Baud |= 0x80000000;
            if (can1Enabled) can1Baud |= 0x40000000;
            if (can1ListenOnly) can1Baud |= 0x20000000;
            if (deviceSingleWireMode > 0) can1Baud |= 0x10000000;

            isConnected = true;
            emit connectionSuccess(this);
            int can0Status = 0x78; //updating everything we can update
            int can1Status = 0x78;
            if (can0Enabled) can0Status +=1;
            if (can0ListenOnly) can0Status += 4;
            if (can1Enabled) can1Status += 1;
            if (deviceSingleWireMode > 0) can1Status += 2;
            if (can1ListenOnly) can1Status += 4;
            emit busStatus(busBase, can0Baud & 0xFFFFF, can0Status);
            emit busStatus(busBase + 1, can1Baud & 0xFFFFF, can1Status);
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

    }
}

void SerialWorker::handleTick()
{
    //qDebug() << "Tick!";

    if (isConnected)
    {
        if (!gotValidated && doValidation)
        {
            if (serial == NULL) return;
            if (serial->isOpen()) //if it's still false we have a problem...
            {
                qDebug() << "Comm validation failed. ";
                closeSerialPort(); //start by stopping everything.
                //Then wait 500ms and restart the connection automatically
                QTimer::singleShot(500, this, SLOT(handleReconnect()));
                return;
            }
        }
    }

    if (doValidation && serial && serial->isOpen()) sendCommValidation();
}

void SerialWorker::handleReconnect()
{
    qDebug() << "Automatically reopening the connection";
    setSerialPort(currentPort); //then go back through the re-init
}

void SerialWorker::sendCommValidation()
{
    QByteArray output;

    gotValidated = false;
    output.append((char)0xF1); //another command to the GVRET
    output.append((char)0x09); //request a reply to get validation
    //send it twice for good measure.
    output.append((char)0xF1); //another command to the GVRET
    output.append((char)0x09); //request a reply to get validation

    serial->write(output);
}

//totally shuts down the whole thing
void SerialWorker::closeSerialPort()
{
    if (serial == NULL) return;
    if (serial->isOpen())
    {
        serial->clear();
        serial->close();
    }
    serial->disconnect();
    //do not stop the ticker here. It always stays running now.
    //ticker->stop();
    delete serial;
    serial = NULL;
}

/*
void SerialWorker::stopFrameCapture()
{
    qDebug() << "Stopping frame capture";
    capturing = false;
}

void SerialWorker::startFrameCapture()
{
    qDebug() << "Starting up frame capture";
    capturing = true;
}
*/

void SerialWorker::updatePortName(QString portName)
{
    QList<QSerialPortInfo> ports;

    CANConnection::updatePortName(portName);

    ports = QSerialPortInfo::availablePorts();

    for (int i = 0; i < ports.count(); i++)
    {
        if (portName == ports[i].portName())
        {
            setSerialPort(&ports[i]);
            return;
        }
    }
}



void SerialWorker::updateBusSettings(CANBus bus)
{
    int busNum = bus.busNum - busBase;
    if (busNum < 0) return;
    if (busNum >= numBuses) return;
    qDebug() << "About to update bus " << busNum << " on GVRET";
    if (busNum == 0)
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
    if (busNum == 1)
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

    updateBaudRates(can0Baud,can1Baud);
}


/************************************/

int SerialWorker::getNumBuses()
{
    return 2;
}

QString SerialWorker::getConnTypeName()
{
    return QString("GVRET");
}

#endif
