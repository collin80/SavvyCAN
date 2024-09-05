#include "serialbusconnection.h"

#include "canconmanager.h"

#include <QCanBus>
#include <QCanBusFrame>
#include <QDateTime>
#include <QDebug>

/***********************************/
/****    class definition       ****/
/***********************************/

SerialBusConnection::SerialBusConnection(QString portName, QString driverName) :
    CANConnection(portName, driverName, CANCon::SERIALBUS,0 ,0, 0, false, 0 ,1, 4000, true),
    mTimer(this) /*NB: set connection as parent of timer to manage it from working thread */
{
}


SerialBusConnection::~SerialBusConnection()
{
    stop();
}


void SerialBusConnection::piStarted()
{
    qDebug() << "piStarted()";
    /* create device */
    QString errorString;
    qDebug() << "Creating device instance";
    mDev_p = QCanBus::instance()->createDevice(getDriver(), getPort(), &errorString);
    if (!mDev_p) {
        disconnectDevice();
        qDebug() << "Error: createDevice(" << getType() << getDriver() << getPort() << "):" << errorString;
        return;
    }

    /* connect slots */
    connect(mDev_p, &QCanBusDevice::errorOccurred, this, &SerialBusConnection::errorReceived);
    connect(mDev_p, &QCanBusDevice::framesWritten, this, &SerialBusConnection::framesWritten);
    connect(mDev_p, &QCanBusDevice::framesReceived, this, &SerialBusConnection::framesReceived);

    connect(&mTimer, SIGNAL(timeout()), this, SLOT(testConnection()));
    mTimer.setInterval(1000);
    mTimer.setSingleShot(false); //keep ticking
    mTimer.start();
    mBusData[0].mBus.setActive(true);
    mBusData[0].mBus.setCanFD(false);
    mBusData[0].mConfigured = true;
}


void SerialBusConnection::piSuspend(bool pSuspend)
{
    /* update capSuspended */
    setCapSuspended(pSuspend);

    /* flush queue if we are suspended */
    if(isCapSuspended())
        getQueue().flush();
}


void SerialBusConnection::piStop() {
    qDebug() << "piStop()";
    mTimer.stop();
    disconnectDevice();
}


bool SerialBusConnection::piGetBusSettings(int pBusIdx, CANBus& pBus)
{
    return getBusConfig(pBusIdx, pBus);
}


void SerialBusConnection::piSetBusSettings(int pBusIdx, CANBus bus)
{
    quint32 sbusconfig = 0;

    //CANConStatus stats;
    /* sanity checks */
    if(0 != pBusIdx)
        return;

    if (!mDev_p) return;

    /* disconnect device if we have one connected */
    disconnectDevice();

    /* copy bus config */
    setBusConfig(0, bus);

    /* if bus is not active we are done */
    if(!bus.isActive())
        return;

    /* set configuration */
    /*if (p.useConfigurationEnabled) {
     foreach (const SettingsDialog::ConfigurationItem &item, p.configurations)
         mDev->setConfigurationParameter(item.first, item.second);
    }*/

    //You cannot set the speed of a socketcan interface, it has to be set with console commands.
    //But, you can probabaly set the speed of many of the other serialbus devices so go ahead and try
    mDev_p->setConfigurationParameter(QCanBusDevice::BitRateKey, bus.getSpeed());
    mDev_p->setConfigurationParameter(QCanBusDevice::CanFdKey, bus.isCanFD());

    if(bus.isListenOnly())
        sbusconfig |= EN_SILENT_MODE;
    mDev_p->setConfigurationParameter(QCanBusDevice::UserKey, sbusconfig);

    /* connect device */
    if (!mDev_p->connectDevice()) {
        disconnectDevice();
        qDebug() << "can't connect device";
    }
}


bool SerialBusConnection::piSendFrame(const CANFrame& pFrame)
{
    /* sanity checks */
    if(0 != pFrame.bus /*|| pFrame.len>8*/)
        return false;
    if (!mDev_p) return false;

    return mDev_p->writeFrame(pFrame);
}


/***********************************/
/****   private methods         ****/
/***********************************/


/* disconnect device */
void SerialBusConnection::disconnectDevice() {
    if(mDev_p) {
        mDev_p->disconnectDevice();
    }
}


void SerialBusConnection::errorReceived(QCanBusDevice::CanBusError error) const
{
    switch (error) {
        case QCanBusDevice::ReadError:
        case QCanBusDevice::WriteError:
        case QCanBusDevice::ConnectionError:
        case QCanBusDevice::ConfigurationError:
        case QCanBusDevice::UnknownError:
        qWarning() << mDev_p->errorString();
        break;
    default:
        break;
    }
}

void SerialBusConnection::framesWritten(qint64 count)
{
    Q_UNUSED(count);
    //qDebug() << "Number of frames written:" << count;
}

void SerialBusConnection::framesReceived()
{
    uint64_t timeBasis = CANConManager::getInstance()->getTimeBasis();

    /* sanity checks */
    if(!mDev_p)
        return;

    /* read frame */
    while(true)
    {
        const QCanBusFrame recFrame = mDev_p->readFrame();

        /* exit case */
        if(!recFrame.isValid())
            break;

        /* drop frame if capture is suspended */
        if(isCapSuspended())
            continue;

        /* check frame */
        //if (recFrame.payload().length() <= 8) {
        if (true) {
            CANFrame* frame_p = getQueue().get();
            if(frame_p) {
                frame_p->setPayload(recFrame.payload());
                frame_p->bus = 0;
                if (recFrame.frameType() == recFrame.ErrorFrame)
                {
                    frame_p->setExtendedFrameFormat(recFrame.hasExtendedFrameFormat());
                    frame_p->setFrameId(recFrame.frameId() + 0x20000000ull);
	            frame_p->isReceived = true;
                }
                else
                {
                    frame_p->setExtendedFrameFormat(recFrame.hasExtendedFrameFormat());
                    frame_p->setFrameId(recFrame.frameId());
                }
                frame_p->setTimeStamp(recFrame.timeStamp());
                frame_p->setFrameType(recFrame.frameType());
                frame_p->setError(recFrame.error());
                /* If recorded frame has a local echo, it is a Tx message, and thus should not be marked as Rx */
                frame_p->isReceived = !recFrame.hasLocalEcho();

                if (useSystemTime) {
                    frame_p->setTimeStamp(QCanBusFrame::TimeStamp(0, QDateTime::currentMSecsSinceEpoch() * 1000ul));
                }
                else frame_p->setTimeStamp(QCanBusFrame::TimeStamp(0, (recFrame.timeStamp().seconds() * 1000000ul + recFrame.timeStamp().microSeconds()) - timeBasis));

                checkTargettedFrame(*frame_p);

                /* enqueue frame */
                getQueue().queue();
            }
            else
                qDebug() << "can't get a frame, ERROR";
        }
    }
}


void SerialBusConnection::testConnection() {
    CANConStatus stats;

    switch(getStatus())
    {
        case CANCon::CONNECTED:
            if (!mDev_p || mDev_p->state() == QCanBusDevice::UnconnectedState) {
                /* we have lost connectivity */
                disconnectDevice();

                setStatus(CANCon::NOT_CONNECTED);
                stats.conStatus = getStatus();
                stats.numHardwareBuses = mNumBuses;
                emit status(stats);
                piStop();
            }
            break;
        case CANCon::NOT_CONNECTED:
            if (mDev_p && mDev_p->state() == QCanBusDevice::UnconnectedState) {
                /* try to reconnect */
                CANBus bus;
                if(getBusConfig(0, bus))
                {
                    bus.setActive(true);
                    setBusSettings(0, bus);
                }

                setStatus(CANCon::CONNECTED);
                stats.conStatus = getStatus();
                stats.numHardwareBuses = mNumBuses;
                emit status(stats);
            }
            break;
        default: {}
    }
}
