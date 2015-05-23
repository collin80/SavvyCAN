#include "serialworker.h"

#include <QSerialPort>
#include <QDebug>
#include <QTimer>

SerialWorker::SerialWorker(CANFrameModel *model, QObject *parent) : QObject(parent)
{
    serial = NULL;
    rx_state = IDLE;
    rx_step = 0;
    buildFrame = new CANFrame;
    canModel = model;
    ticker.setInterval(250); //tick four times per second
    ticker.setSingleShot(false); //keep ticking
    ticker.start();
    gotFrames = 0;

    connect(&ticker, SIGNAL(timeout()), this, SLOT(handleTick()));
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
    ticker.stop();
}

void SerialWorker::setSerialPort(QSerialPortInfo *port)
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

    serial = new QSerialPort(*port);

    qDebug() << "Serial port name is " << port->portName();
    serial->setBaudRate(serial->Baud115200);
    serial->setDataBits(serial->Data8);
    serial->setFlowControl(serial->HardwareControl);
    serial->open(QIODevice::ReadWrite);
    serial->setDataTerminalReady(true);
    serial->setRequestToSend(true);
    QByteArray output;
    output.append(0xE7); //this puts the device into binary comm mode
    output.append(0xE7);
    output.append(0xF1); //signal we want to issue a command
    output.append(0x06); //request canbus stats from the board
    output.append(0xF1); //another command to the GVRET
    output.append(0x07); //request device information
    serial->write(output);
    connected = false;
    connect(serial, SIGNAL(readyRead()), this, SLOT(readSerialData()));
    QTimer::singleShot(1000, this, SLOT(connectionTimeout()));
}

void SerialWorker::connectionTimeout()
{
    //one second after trying to connect are we actually connected?
    if (!connected) //no?
    {
        //then emit the the failure signal and see if anyone cares
        qDebug() << "Failed to connect to GVRET at that com port";
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

    if (!connected) return;

    //qDebug() << "Sending out frame with id " << frame->ID;

    ID = frame->ID;
    if (frame->extended) ID |= 1 << 31;

    buffer[0] = 0xF1; //start of a command over serial
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
    qDebug() << "writing " << buffer.length() << " bytes to serial port";
    serial->write(buffer);
}

void SerialWorker::updateBaudRates(int Speed1, int Speed2)
{
    QByteArray buffer;
    qDebug() << "Got signal to update bauds. 1: " << Speed1 <<" 2: " << Speed2;
    buffer[0] = 0xF1; //start of a command over serial
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
                //emit receivedFrame(buildFrame);
                canModel->addFrame(*buildFrame, false);
                gotFrames++;
                //buildFrame = new CANFrame;
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
    }
}

void SerialWorker::handleTick()
{
    if (gotFrames > 0)
    {
        emit receivedFrames(); //notify interested parties that there are new frames
        canModel->sendBulkRefresh(gotFrames);
        gotFrames = 0;
    }
}
