#include <QObject>
#include <QDebug>
#include <QCanBusFrame>


#include "canframemodel.h"
#include "canconnection.h"
#include "socketcanconnection.h"

/***********************************/
/****   nested class            ****/
/***********************************/


BUSConfig::BUSConfig():isConfigured(false){}

void BUSConfig::reset() {
    isConfigured = false;
}

bool BUSConfig::operator==(CANBus& bus) {
    return isConfigured && speed == bus.speed &&
            listenOnly == bus.listenOnly && active == bus.active;
}

void BUSConfig::operator=(CANBus& bus) {
    isConfigured = true;
    speed = bus.speed;
    listenOnly = bus.listenOnly;
    active = bus.active;
}



/***********************************/
/****    class definition       ****/
/***********************************/

SocketCanConnection::SocketCanConnection(CANFrameModel *model, int base) : CANConnection(model, base), mDev_p(NULL)
{
    qDebug() << "SocketCanConnection()";
    qRegisterMetaType<CANFrame>("CANFrame");
}

SocketCanConnection::~SocketCanConnection()
{
    qDebug() << "~SocketCanConnection()";
    /* stop device */
}

void SocketCanConnection::updateBusSettings(CANBus bus)
{
    qDebug()<<"updateBusSettings";

    if(mConf == bus) return;

    /* disconnect device if we have one connected */
    if(mDev_p) disconnect();

    /* if bus is not active we are done */
    if(!bus.active) return;

    /* create device */
    mDev_p = QCanBus::instance()->createDevice("socketcan", portName);
    if (!mDev_p) {
     qDebug() << "can't create device";
     return;
    }

    /* connect slots */
    connect(mDev_p, &QCanBusDevice::errorOccurred, this, &SocketCanConnection::errorReceived);
    connect(mDev_p, &QCanBusDevice::framesReceived, this, &SocketCanConnection::framesReceived);
    connect(mDev_p, &QCanBusDevice::framesWritten, this, &SocketCanConnection::framesWritten);

    /* set configuration */
    /*if (p.useConfigurationEnabled) {
     foreach (const SettingsDialog::ConfigurationItem &item, p.configurations)
         mDev->setConfigurationParameter(item.first, item.second);
    }*/

    /* connect device */
    if (!mDev_p->connectDevice()) {
        disconnect();
        qDebug() << "can't connect device";
    }
}


/***********************************/
/****   private methods         ****/
/***********************************/


/* connect device */
void SocketCanConnection::disconnect() {
    if(mDev_p) {
        mDev_p->disconnectDevice();
        delete mDev_p;
        mDev_p = Q_NULLPTR;
    }
}


void SocketCanConnection::errorReceived(QCanBusDevice::CanBusError error) const
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

void SocketCanConnection::framesWritten(qint64 count)
{
    qDebug() << "Number of frames written:" << count;
}

void SocketCanConnection::framesReceived()
{
    /* sanity checks */
    if(!mDev_p) return;

    /* read frame */
    while(true)
    {
        const QCanBusFrame recFrame = mDev_p->readFrame();
        /* exit case */
        if(!recFrame.isValid()) return;

        if(!recFrame.payload().isEmpty() &&
           recFrame.payload().length()<=8)
        {
            CANFrame frame;
            frame.len           = recFrame.payload().length();
            frame.bus           = 0;
            memcpy(frame.data, recFrame.payload().data(), frame.len);
            frame.extended      = false;
            frame.ID            = recFrame.frameId();
            frame.isReceived    = true;
            frame.timestamp     = recFrame.timeStamp().microSeconds();

            /* send frame */
            QMetaObject::invokeMethod(model, "addFrame",
                                      Qt::QueuedConnection,
                                      Q_ARG(CANFrame, frame),
                                      Q_ARG(bool, false));
        }
        else {
            qDebug() << "invalid frame";
        }
    }
}
