#include "serialworker.h"

#include <QSerialPort>
#include <QDebug>
#include <QTimer>
#include <QSettings>

SerialWorker::SerialWorker(CANFrameModel *model, QObject *parent) : QObject(parent)
{    
    serial = NULL;
    rx_state = IDLE;
    rx_step = 0;
    buildFrame = new CANFrame;
    canModel = model;
    gotFrames = 0;
    ticker = NULL;
    elapsedTime = NULL;
    framesPerSec = 0;
    capturing = true;
    gotValidated = true;
    isAutoRestart = false;
    targetID = -1;

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
    //serial->setBaudRate(10000000); //more speed! probably does nothing for USB serial
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

    serial->write(output);
    if (doValidation) connected = false;
        else connected = true;
    connect(serial, SIGNAL(readyRead()), this, SLOT(readSerialData()));
    if (doValidation) QTimer::singleShot(1000, this, SLOT(connectionTimeout()));
    if (ticker == NULL)
    {
        ticker = new QTimer;
        connect(ticker, SIGNAL(timeout()), this, SLOT(handleTick()));
    }
    if (elapsedTime == NULL)
    {
        elapsedTime = new QTime;
        elapsedTime->start();
    }
    ticker->setInterval(250); //tick four times per second
    ticker->setSingleShot(false); //keep ticking
    ticker->start();
}

void SerialWorker::connectionTimeout()
{
    //one second after trying to connect are we actually connected?
    if (!connected) //no?
    {
        //then emit the the failure signal and see if anyone cares
        qDebug() << "Failed to connect to GVRET at that com port";
        ticker->stop();
        closeSerialPort(); //make sure it's properly closed anyway
        emit connectionFailure();
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
}

void SerialWorker::sendFrame(const CANFrame *frame, int bus = 0)
{
    QByteArray buffer;
    int c;
    int ID;

    //qDebug() << "Sending out frame with id " << frame->ID;

    if (!connected) return;

    ID = frame->ID;
    if (frame->extended) ID |= 1 << 31;

    buffer[0] = (char)0xF1; //start of a command over serial
    buffer[1] = 0; //command ID for sending a CANBUS frame
    buffer[2] = (unsigned char)(ID & 0xFF); //four bytes of ID LSB first
    buffer[3] = (unsigned char)(ID >> 8);
    buffer[4] = (unsigned char)(ID >> 16);
    buffer[5] = (unsigned char)(ID >> 24);
    buffer[6] = (unsigned char)(bus & 1);
    buffer[7] = (unsigned char)frame->len;
    for (c = 0; c < frame->len; c++)
    {
        buffer[8 + c] = frame->data[c];
    }
    buffer[8 + frame->len] = 0;

    if (serial == NULL) return;
    if (!serial->isOpen()) return;
    //qDebug() << "writing " << buffer.length() << " bytes to serial port";
    serial->write(buffer);

    //show our sent frames in the list too.
    canModel->addFrame(frame, false);
    gotFrames++;
}

//a simple way for another thread to pass us a bunch of frames to send.
//Don't get carried away here. The GVRET firmware only has finite
//buffers and besides, the other end will get buried in traffic.
void SerialWorker::sendFrameBatch(const QList<CANFrame> *frames)
{
    for (int i = 0; i < frames->length(); i++) sendFrame(&frames->at(i), frames->at(i).bus);
}

void SerialWorker::updateBaudRates(int Speed1, int Speed2)
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
        case 1: //we don't accept time sync commands from the firmware
            rx_state = IDLE;
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
            buildFrame->timestamp = c;
            break;
        case 1:
            buildFrame->timestamp |= (uint)(c << 8);
            break;
        case 2:
            buildFrame->timestamp |= (uint)c << 16;
            break;
        case 3:
            buildFrame->timestamp |= (uint)c << 24;
            break;
        case 4:
            buildFrame->ID = c;
            break;
        case 5:
            buildFrame->ID |= c << 8;
            break;
        case 6:
            buildFrame->ID |= c << 16;
            break;
        case 7:
            buildFrame->ID |= c << 24;
            if ((buildFrame->ID & 1 << 31) == 1 << 31)
            {
                buildFrame->ID &= 0x7FFFFFFF;
                buildFrame->extended = true;
            }
            else buildFrame->extended = false;
            break;
        case 8:
            buildFrame->len = c & 0xF;
            if (buildFrame->len > 8) buildFrame->len = 8;
            buildFrame->bus = (c & 0xF0) >> 4;
            break;
        default:
            if (rx_step < buildFrame->len + 9)
            {
                buildFrame->data[rx_step - 9] = c;
            }
            else
            {
                rx_state = IDLE;
                rx_step = 0;
                //qDebug() << "emit from serial handler to main form id: " << buildFrame->ID;
                if (capturing)
                {
                    canModel->addFrame(*buildFrame, false);
                    gotFrames++;
                    if (buildFrame->ID == targetID) emit gotTargettedFrame(canModel->rowCount() - 1);
                }
            }
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
            can0Enabled = c;
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
            can1Enabled = c;
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
            if (!can1Enabled) can1Baud = 0;
            if (!can0Enabled) can0Baud = 0;
            connected = true;
            emit connectionSuccess(can0Baud, can1Baud);
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
    case TIME_SYNC:
        rx_state = IDLE;
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

    if (!gotValidated)
    {
        if (serial->isOpen()) //if it's still false we have a problem...
        {
            qDebug() << "Comm validation failed. ";
            closeSerialPort(); //start by stopping everything.
            QTimer::singleShot(500, this, SLOT(handleReconnect()));
            return;
        }
    }

    framesPerSec += gotFrames * 1000 / elapsedTime->elapsed() - (framesPerSec / 4);
    elapsedTime->restart();
    emit frameUpdateTick(framesPerSec / 4, gotFrames); //sends stats to interested parties
    canModel->sendBulkRefresh(gotFrames);
    gotFrames = 0;    
    if (doValidation) sendCommValidation();
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
    ticker->stop();
    delete serial;
    serial = NULL;
}

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

void SerialWorker::targetFrameID(int target)
{
    targetID = target;
}
