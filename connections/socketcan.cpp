#include <QObject>
#include <QDebug>
#include <QCanBusFrame>

#include "socketcan.h"



/***********************************/
/****    class definition       ****/
/***********************************/

SocketCan::SocketCan(QString portName) :
    CANConnection(portName, CANCon::SOCKETCAN, 1, 4000, true),
    mDev_p(NULL),
    mTimer(this) /*NB: set connection as parent of timer to manage it from working thread */
{
    qDebug() << "SocketCan()";
}


SocketCan::~SocketCan()
{
    stop();
    qDebug() << "~SocketCan()";
}


void SocketCan::piStarted()
{
    connect(&mTimer, SIGNAL(timeout()), this, SLOT(testConnection()));
    mTimer.setInterval(1000);
    mTimer.setSingleShot(false); //keep ticking
    mTimer.start();
}


void SocketCan::piSuspend(bool pSuspend)
{
    /* update capSuspended */
    setCapSuspended(pSuspend);

    /* flush queue if we are suspended */
    if(isCapSuspended())
        getQueue().flush();
}


void SocketCan::piStop() {
    mTimer.stop();
    disconnectDevice();
}


bool SocketCan::piGetBusSettings(int pBusIdx, CANBus& pBus)
{
    return getBusConfig(pBusIdx, pBus);
}


void SocketCan::piSetBusSettings(int pBusIdx, CANBus bus)
{
    /* sanity checks */
    if(0 != pBusIdx)
        return;

    /* disconnect device if we have one connected */
    if(mDev_p)
        disconnectDevice();

    /* copy bus config */
    setBusConfig(0, bus);

    /* if bus is not active we are done */
    if(!bus.active)
        return;

    /* create device */
    mDev_p = QCanBus::instance()->createDevice("socketcan", getPort());
    if (!mDev_p) {
        disconnectDevice();
        qDebug() << "can't create device";
        return;
    }

    /* connect slots */
    connect(mDev_p, &QCanBusDevice::errorOccurred, this, &SocketCan::errorReceived);
    connect(mDev_p, &QCanBusDevice::framesWritten, this, &SocketCan::framesWritten);
    connect(mDev_p, &QCanBusDevice::framesReceived, this, &SocketCan::framesReceived);

    /* set configuration */
    /*if (p.useConfigurationEnabled) {
     foreach (const SettingsDialog::ConfigurationItem &item, p.configurations)
         mDev->setConfigurationParameter(item.first, item.second);
    }*/

    /* connect device */
    if (!mDev_p->connectDevice()) {
        disconnectDevice();
        qDebug() << "can't connect device";
    }
}


void SocketCan::piSendFrame(const CANFrame&) {}
void SocketCan::piSendFrameBatch(const QList<CANFrame>&){}


/***********************************/
/****   private methods         ****/
/***********************************/


/* disconnect device */
void SocketCan::disconnectDevice() {
    if(mDev_p) {
        mDev_p->disconnectDevice();
        delete mDev_p;
        mDev_p = Q_NULLPTR;
    }
}


void SocketCan::errorReceived(QCanBusDevice::CanBusError error) const
{
    switch (error) {
        case QCanBusDevice::ReadError:
        case QCanBusDevice::WriteError:
        case QCanBusDevice::ConnectionError:
        case QCanBusDevice::ConfigurationError:
        case QCanBusDevice::UnknownError:
        qWarning() << mDev_p->errorString();
    default:
        break;
    }
}

void SocketCan::framesWritten(qint64 count)
{
    qDebug() << "Number of frames written:" << count;
}

void SocketCan::framesReceived()
{
    /* sanity checks */
    if(!mDev_p)
        return;

    /* read frame */
    while(true)
    {
        const QCanBusFrame recFrame = mDev_p->readFrame();

        /* exit case */
        if(!recFrame.isValid())
            return;

        /* drop frame if capture is suspended */
        if(isCapSuspended())
            continue;

        /* check frame */
        if(!recFrame.payload().isEmpty() &&
           recFrame.payload().length()<=8)
        {
            CANFrame* frame_p = getQueue().get();
            if(frame_p) {
                frame_p->len           = recFrame.payload().length();
                frame_p->bus           = 0;
                memcpy(frame_p->data, recFrame.payload().data(), frame_p->len);
                frame_p->extended      = false;
                frame_p->ID            = recFrame.frameId();
                frame_p->isReceived    = true;
                frame_p->timestamp     = recFrame.timeStamp().seconds()*1000000 + recFrame.timeStamp().microSeconds();

                /* enqueue frame */
                getQueue().queue();
            }
            else
                qDebug() << "can't get a frame, ERROR";
        }
        else {
            qDebug() << "invalid frame";
        }
    }
}


void SocketCan::testConnection() {
    QCanBusDevice*  dev_p = QCanBus::instance()->createDevice("socketcan", getPort());

    switch(getStatus())
    {
        case CANCon::CONNECTED:
            if (!dev_p || !dev_p->connectDevice()) {
                /* we have lost connectivity */
                disconnectDevice();

                setStatus(CANCon::NOT_CONNECTED);
                emit status(getStatus());
            }
            break;
        case CANCon::NOT_CONNECTED:
            if (dev_p && dev_p->connectDevice()) {
                if(!mDev_p) {
                    /* try to reconnect */
                    CANBus bus;
                    if(getBusConfig(0, bus))
                        setBusSettings(0, bus);
                }
                /* disconnect test instance */
                dev_p->disconnectDevice();

                setStatus(CANCon::CONNECTED);
                emit status(getStatus());
            }
            break;
        default: {}
    }

    if(dev_p)
        delete dev_p;
}
