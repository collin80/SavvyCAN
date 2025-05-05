#include <QObject>
#include <QDebug>
#include <QCanBusFrame>
#include <QSerialPortInfo>
#include <QSettings>
#include <QStringBuilder>
#include <QtNetwork>

#include "lawicel_serial.h"
#include "utility.h"

LAWICELSerial::LAWICELSerial(QString portName, int serialSpeed, int lawicelSpeed, bool canFd, int dataRate) :
    CANConnection(portName, "LAWICEL", CANCon::LAWICEL,serialSpeed, lawicelSpeed, 0, canFd, dataRate, 3, 4000, true),
    mTimer(this) /*NB: set this as parent of timer to manage it from working thread */
{
    sendDebug("LAWICELSerial()");

    serial = nullptr;
    isAutoRestart = false;

    readSettings();
}


LAWICELSerial::~LAWICELSerial()
{
    stop();
    sendDebug("~LAWICELSerial()");
}

void LAWICELSerial::sendDebug(const QString debugText)
{
    qDebug() << debugText;
    debugOutput(debugText);
}

void LAWICELSerial::sendToSerial(const QByteArray &bytes)
{
    if (serial == nullptr)
    {
        sendDebug("Attempt to write to serial port when it has not been initialized!");
        return;
    }

    if (serial && !serial->isOpen())
    {
        sendDebug("Attempt to write to serial port when it is not open!");
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
}

void LAWICELSerial::piStarted()
{    
    connectDevice();
}


void LAWICELSerial::piSuspend(bool pSuspend)
{
    /* update capSuspended */
    setCapSuspended(pSuspend);

    /* flush queue if we are suspended */
    if(isCapSuspended())
        getQueue().flush();
}


void LAWICELSerial::piStop()
{
    mTimer.stop();
    disconnectDevice();
}


bool LAWICELSerial::piGetBusSettings(int pBusIdx, CANBus& pBus)
{
    return getBusConfig(pBusIdx, pBus);
}


void LAWICELSerial::piSetBusSettings(int pBusIdx, CANBus bus)
{
    /* sanity checks */
    if( (pBusIdx < 0) || pBusIdx >= getNumBuses())
        return;

    /* copy bus config */
    setBusConfig(pBusIdx, bus);
/*
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
*/
    if (pBusIdx < 2) {
        /* update baud rates */
        QByteArray buffer;
        //sendDebug("Got signal to update bauds. 1: " + QString::number((can0Baud & 0xFFFFFFF)));
        buffer[0] = (char)0xF1; //start of a command over serial
        //sendToSerial(buffer);
    }

}


bool LAWICELSerial::piSendFrame(const CANFrame& frame)
{
    QByteArray buffer;
    int c;
    quint32 ID;

    //qDebug() << "Sending out lawicel frame with id " << frame.ID << " on bus " << frame.bus;

    framesRapid++;

    if (serial == nullptr) return false;
    if (serial && !serial->isOpen()) return false;
    //if (!isConnected) return false;

    // Doesn't make sense to send an error frame
    // to an adapter
    if (frame.frameId() & 0x20000000) {
        return true;
    }

    ID = frame.frameId();
    if (frame.hasExtendedFrameFormat()) ID |= 1u << 31;

    int idx = 0;
    QString buildStr;
    if(frame.hasFlexibleDataRateFormat()){
        if (frame.hasExtendedFrameFormat())
        {
            if (frame.hasBitrateSwitch())
                buildStr = QString::asprintf("B%08X%u", ID, LAWICELSerial::bytes_to_dlc_code(frame.payload().length()));
            else
                buildStr = QString::asprintf("D%08X%u", ID, LAWICELSerial::bytes_to_dlc_code(frame.payload().length()));
        }
        else
        {
            if (frame.hasBitrateSwitch())
                buildStr = QString::asprintf("b%03X%u", ID, LAWICELSerial::bytes_to_dlc_code(frame.payload().length()));
            else
                buildStr = QString::asprintf("d%03X%u", ID, LAWICELSerial::bytes_to_dlc_code(frame.payload().length()));
        }
    }
    else {
        if (frame.hasExtendedFrameFormat())
        {
            buildStr = QString::asprintf("T%08X%u", ID, frame.payload().length());
        }
        else
        {
            buildStr = QString::asprintf("t%03X%u", ID, frame.payload().length());
        }
    }
    foreach (QChar chr, buildStr)
    {
        buffer[idx] = chr.toLatin1();
        idx++;
    }

    for (c = 0; c < frame.payload().length(); c++)
    {
        QString byt = Utility::formatByteAsHex(frame.payload()[c]);
        buffer[idx + (c * 2)] = byt[0].toLatin1();
        buffer[idx + (c * 2) + 1] = byt[1].toLatin1();
    }
    buffer[idx + (frame.payload().length() * 2)] = 13; //CR

    sendToSerial(buffer);

    return true;
}



/****************************************************************/

void LAWICELSerial::readSettings()
{
    QSettings settings;
}


void LAWICELSerial::connectDevice()
{
    QSettings settings;

    /* disconnect device */
    if(serial)
        disconnectDevice();

    /* open new device */

    qDebug() << "Serial port: " << getPort();

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
    serial->setBaudRate(mSerialSpeed);
    serial->setDataBits(serial->Data8);

    serial->setFlowControl(serial->HardwareControl);
    //serial->setFlowControl(serial->NoFlowControl);
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

void LAWICELSerial::deviceConnected()
{
    sendDebug("Connecting to LAWICEL Device!");

    QByteArray output;

    output.clear();
    output.append('C'); //close the bus in case it was already up
    output.append(13);
    sendToSerial(output);

    output.clear();
    output.append('S'); //configure speed of bus
        switch (this->mBusData[0].mBus.getSpeed())
        {
        case 10000:
            output.append('0');
            break;
        case 20000:
            output.append('1');
            break;
        case 50000:
            output.append('2');
            break;
        case 100000:
            output.append('3');
            break;
        case 125000:
            output.append('4');
            break;
        case 250000:
            output.append('5');
            break;
        case 500000:
            output.append('6');
            break;
        case 800000:
            output.append('7');
            break;
        case 1000000:
            output.append('8');
            break;
        default:
            output.append('6');
            break;
        }

    output.append('\x0D');

    sendToSerial(output);

    output.clear();

    if (this->canFd){
        switch (this->dataRate)
        {
        case 1000000:
            output.append("Y1");
            break;
        case 2000000:
            output.append("Y2");
            break;
        case 4000000:
            output.append("Y4");
            break;
        case 5000000:
            output.append("Y5");
            break;
        default:
            output.append("Y2");
            break;
        }

        output.append('\x0D');
        sendToSerial(output);
        output.clear();
    }

    output.append('O'); //open bus now that we set the speed
    output.append(13);

    sendToSerial(output);

    mNumBuses = 1;
    setStatus(CANCon::CONNECTED);
    CANConStatus stats;
    stats.conStatus = getStatus();
    stats.numHardwareBuses = mNumBuses;
    mBusData[0].mConfigured = true;
    mBusData[0].mBus.setActive(true);
    //mBusData[0].mBus.setSpeed();
    emit status(stats);
}

void LAWICELSerial::disconnectDevice() {
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

    setStatus(CANCon::NOT_CONNECTED);
    CANConStatus stats;
    stats.conStatus = getStatus();
    stats.numHardwareBuses = mNumBuses;
    emit status(stats);
}

void LAWICELSerial::serialError(QSerialPort::SerialPortError err)
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
    case QSerialPort::ParityError:
        errMessage = "Parity error on serial port";
        break;
    case QSerialPort::FramingError:
        errMessage = "Framing error on serial port";
        break;
    case QSerialPort::BreakConditionError:
        errMessage = "Break error on serial port";
        break;
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


void LAWICELSerial::connectionTimeout()
{
    //one second after trying to connect are we actually connected?
    if (CANCon::NOT_CONNECTED==getStatus()) //no?
    {
        //then emit the the failure signal and see if anyone cares
        sendDebug("Failed to connect to LAWICEL at that com port");

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

void LAWICELSerial::readSerialData()
{
    QByteArray data;
    unsigned char c;
    QString debugBuild;
    CANFrame buildFrame;
    QByteArray buildData;

    if (serial) data = serial->readAll();

    sendDebug("Got data from serial. Len = " % QString::number(data.length()));
    for (int i = 0; i < data.length(); i++)
    {
        c = data.at(i);
        //qDebug() << c << "    " << QString::number(c, 16) << "     " << QString(c);
        debugBuild = debugBuild % QString::number(c, 16).rightJustified(2,'0') % " ";
        //procRXChar(c);
        mBuildLine.append(c);
        if (c == 13) //all lawicel commands end in CR
        {
            qDebug() << "Got CR!";

            if (useSystemTime)
            {
                buildTimestamp = QDateTime::currentMSecsSinceEpoch() * 1000l;
            }
            else
            {
                //If total length is greater than command, header and data, timestamps must be enabled.
                if (data.length() > (5 + mBuildLine.mid(4, 1).toInt() * 2 + 1))
                {
                    //Four bytes after the end of the data bytes.
                    buildTimestamp = mBuildLine.mid(5 + mBuildLine.mid(4, 1).toInt() * 2, 4).toInt(nullptr, 16) * 1000l;
                }
                else
                {
                    //Default to system time if timestamps are disabled.
                    buildTimestamp = QDateTime::currentMSecsSinceEpoch() * 1000l;
                }
            }
            buildFrame.setTimeStamp(QCanBusFrame::TimeStamp(0, buildTimestamp));

            switch (mBuildLine[0].toLatin1())
            {
            case 't': //standard frame
                //tIIILDD
                buildFrame.setFrameId(mBuildLine.mid(1, 3).toInt(nullptr, 16));
                buildFrame.isReceived = true;
                buildFrame.setFrameType(QCanBusFrame::FrameType::DataFrame);
                buildData.resize(mBuildLine.mid(4, 1).toInt());
                for (int c = 0; c < buildData.size(); c++)
                {
                    buildData[c] = mBuildLine.mid(5 + (c*2), 2).toInt(nullptr, 16);
                }
                buildFrame.setPayload(buildData);
                if (!isCapSuspended())
                {
                    /* get frame from queue */
                    CANFrame* frame_p = getQueue().get();
                    if(frame_p) {
                        //qDebug() << "Lawicel got frame on bus " << frame_p->bus;
                        /* copy frame */
                        *frame_p = buildFrame;
                        checkTargettedFrame(buildFrame);
                        /* enqueue frame */
                        getQueue().queue();
                    }
                    else
                        qDebug() << "can't get a frame, ERROR";
                }
                break;
            case 'T': //extended frame
                //TIIIIIIIILDD.
                buildFrame.setFrameId(mBuildLine.mid(1, 8).toInt(nullptr, 16));
                buildFrame.isReceived = true;
                buildFrame.setFrameType(QCanBusFrame::FrameType::DataFrame);
                buildFrame.setExtendedFrameFormat(true);
                buildData.resize(mBuildLine.mid(9, 1).toInt());
                for (int c = 0; c < buildData.size(); c++)
                {
                    buildData[c] = mBuildLine.mid(10 + (c*2), 2).toInt(nullptr, 16);
                }
                buildFrame.setPayload(buildData);
                if (!isCapSuspended())
                {
                    /* get frame from queue */
                    CANFrame* frame_p = getQueue().get();
                    if(frame_p) {
                        //qDebug() << "Lawicel got frame on bus " << frame_p->bus;
                        /* copy frame */
                        *frame_p = buildFrame;
                        checkTargettedFrame(buildFrame);
                        /* enqueue frame */
                        getQueue().queue();
                    }
                    else
                        qDebug() << "can't get a frame, ERROR";
                }
                break;
            case 'b':
                buildFrame.setBitrateSwitch(true); //BRS enabled
                [[fallthrough]];
            case 'd': //standard fd frame, BRS disabled
                //tIIILDD
                buildFrame.setFlexibleDataRateFormat(true);
                buildFrame.setFrameId(mBuildLine.mid(1, 3).toInt(nullptr, 16));
                buildFrame.isReceived = true;
                buildFrame.setFrameType(QCanBusFrame::FrameType::DataFrame);
                buildData.resize(LAWICELSerial::dlc_code_to_bytes(mBuildLine.mid(4, 1).toInt(nullptr, 16)));
                for (int c = 0; c < buildData.size(); c++)
                {
                    buildData[c] = mBuildLine.mid(5 + (c*2), 2).toInt(nullptr, 16);
                }
                buildFrame.setPayload(buildData);
                if (!isCapSuspended())
                {
                    /* get frame from queue */
                    CANFrame* frame_p = getQueue().get();
                    if(frame_p) {
                        //qDebug() << "Lawicel got frame on bus " << frame_p->bus;
                        /* copy frame */
                        *frame_p = buildFrame;
                        checkTargettedFrame(buildFrame);
                        /* enqueue frame */
                        getQueue().queue();
                    }
                    else
                        qDebug() << "can't get a frame, ERROR";
                }
                break;
            case 'B':
                buildFrame.setBitrateSwitch(true); //BRS enabled
                [[fallthrough]];
            case 'D': //extended fd frame
                //TIIIIIIIILDD.
                buildFrame.setFlexibleDataRateFormat(true);
                buildFrame.setBitrateSwitch(true);
                buildFrame.setFrameId(mBuildLine.mid(1, 8).toInt(nullptr, 16));
                buildFrame.isReceived = true;
                buildFrame.setFrameType(QCanBusFrame::FrameType::DataFrame);
                buildFrame.setExtendedFrameFormat(true);
                buildData.resize(LAWICELSerial::dlc_code_to_bytes(mBuildLine.mid(4, 1).toInt(nullptr, 16)));
                for (int c = 0; c < buildData.size(); c++)
                {
                    buildData[c] = mBuildLine.mid(10 + (c*2), 2).toInt(nullptr, 16);
                }
                buildFrame.setPayload(buildData);
                if (!isCapSuspended())
                {
                    /* get frame from queue */
                    CANFrame* frame_p = getQueue().get();
                    if(frame_p) {
                        //qDebug() << "Lawicel got frame on bus " << frame_p->bus;
                        /* copy frame */
                        *frame_p = buildFrame;
                        checkTargettedFrame(buildFrame);
                        /* enqueue frame */
                        getQueue().queue();
                    }
                    else
                        qDebug() << "can't get a frame, ERROR";
                }
                break;
            }
            mBuildLine.clear();
        }
    }
    debugOutput(debugBuild);
    //qDebug() << debugBuild;
}

//Debugging data sent from connection window. Inject it into Comm traffic.
void LAWICELSerial::debugInput(QByteArray bytes) {
   sendToSerial(bytes);
}

void LAWICELSerial::handleTick()
{
    //qDebug() << "Tick!";
}


// Convert a FDCAN_data_length_code to number of bytes in a message
uint8_t LAWICELSerial::dlc_code_to_bytes(int dlc_code)
{
    if (dlc_code<=8)
        return dlc_code;
    else{
        switch(dlc_code)
        {
            case 9:
                return 12;
            case 10:
                return 16;
            case 11:
                return 20;
            case 12:
                return 24;
            case 13:
                return 32;
            case 14:
                return 48;
            case 15:
                return 64;
            default:
                return 0;
        }
    }
}

uint8_t LAWICELSerial::bytes_to_dlc_code(uint8_t bytes)
{
    if (bytes<=8)
        return bytes;
    else{
        switch(bytes)
        {
            case 12:
                return 9;
            case 16:
                return 10;
            case 20:
                return 11;
            case 24:
                return 12;
            case 32:
                return 13;
            case 48:
                return 14;
            case 64:
                return 15;
            default:
                return 0;
        }
    }
}


