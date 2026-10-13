#include "serialbusconnection.h"

#include "canconmanager.h"

#include <QCanBus>
#include <QCanBusFrame>
#include <QDateTime>
#include <QDebug>
#include <QLibrary>

namespace
{
constexpr quint8 PCAN_LISTEN_ONLY_PARAMETER = 0x08U;
constexpr quint32 PCAN_PARAMETER_OFF_VALUE = 0x00U;
constexpr quint32 PCAN_PARAMETER_ON_VALUE = 0x01U;
constexpr quint32 PCAN_ERROR_OK_VALUE = 0x00000U;

#ifdef Q_OS_WIN
using PcanSetValueFunction = quint32 (__stdcall *)(quint16, quint8, void *, quint32);
using PcanGetValueFunction = quint32 (__stdcall *)(quint16, quint8, void *, quint32);
using PcanGetErrorTextFunction = quint32 (__stdcall *)(quint32, quint16, char *);
#else
using PcanSetValueFunction = quint32 (*)(quint16, quint8, void *, quint32);
using PcanGetValueFunction = quint32 (*)(quint16, quint8, void *, quint32);
using PcanGetErrorTextFunction = quint32 (*)(quint32, quint16, char *);
#endif

bool parsePcanPortIndex(const QString &portName, const QString &prefix, int count, int *index)
{
    const QString trimmedPortName = portName.trimmed();
    if (!trimmedPortName.startsWith(prefix, Qt::CaseInsensitive))
        return false;

    bool conversionOk = false;
    const int parsedIndex = trimmedPortName.mid(prefix.size()).toInt(&conversionOk);
    if (!conversionOk || parsedIndex < 0 || parsedIndex >= count)
        return false;

    *index = parsedIndex;
    return true;
}

quint16 pcanChannelHandle(const QString &portName, bool *ok)
{
    int index = 0;
    *ok = true;

    if (parsePcanPortIndex(portName, QStringLiteral("usb"), 16, &index))
        return static_cast<quint16>(index < 8 ? 0x51U + index : 0x509U + (index - 8));

    if (parsePcanPortIndex(portName, QStringLiteral("pci"), 16, &index))
        return static_cast<quint16>(index < 8 ? 0x41U + index : 0x409U + (index - 8));

    *ok = false;
    return 0U;
}

QLibrary &pcanBasicLibrary()
{
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    static QLibrary library(QStringLiteral("PCBUSB"));
#else
    static QLibrary library(QStringLiteral("pcanbasic"));
#endif
    return library;
}

QString pcanStatusText(QLibrary &library, quint32 status)
{
    const auto getErrorText = reinterpret_cast<PcanGetErrorTextFunction>(library.resolve("CAN_GetErrorText"));
    if (getErrorText)
    {
        char buffer[256] = {};
        if (getErrorText(status, 0U, buffer) == PCAN_ERROR_OK_VALUE && buffer[0] != '\0')
            return QString::fromLocal8Bit(buffer);
    }

    return QStringLiteral("PCAN-Basic status 0x%1")
            .arg(status, 8, 16, QLatin1Char('0'));
}

bool configurePeakCanListenOnly(const QString &portName, bool enabled, QString *errorString)
{
    bool validChannel = false;
    const quint16 channel = pcanChannelHandle(portName, &validChannel);
    if (!validChannel)
    {
        if (errorString)
            *errorString = QStringLiteral("Unsupported PeakCAN channel name: %1").arg(portName);
        return false;
    }

    QLibrary &library = pcanBasicLibrary();
    if (!library.isLoaded() && !library.load())
    {
        if (errorString)
            *errorString = QStringLiteral("Could not load PCAN-Basic: %1").arg(library.errorString());
        return false;
    }

    const auto setValue = reinterpret_cast<PcanSetValueFunction>(library.resolve("CAN_SetValue"));
    if (!setValue)
    {
        if (errorString)
            *errorString = QStringLiteral("PCAN-Basic does not export CAN_SetValue");
        return false;
    }

    quint32 value = enabled ? PCAN_PARAMETER_ON_VALUE : PCAN_PARAMETER_OFF_VALUE;
    const quint32 status = setValue(channel, PCAN_LISTEN_ONLY_PARAMETER, &value, sizeof(value));
    if (status != PCAN_ERROR_OK_VALUE)
    {
        if (errorString)
            *errorString = pcanStatusText(library, status);
        return false;
    }

    return true;
}

bool verifyPeakCanListenOnly(const QString &portName, bool expected, QString *errorString)
{
    bool validChannel = false;
    const quint16 channel = pcanChannelHandle(portName, &validChannel);
    if (!validChannel)
    {
        if (errorString)
            *errorString = QStringLiteral("Unsupported PeakCAN channel name: %1").arg(portName);
        return false;
    }

    QLibrary &library = pcanBasicLibrary();
    if (!library.isLoaded() && !library.load())
    {
        if (errorString)
            *errorString = QStringLiteral("Could not load PCAN-Basic: %1").arg(library.errorString());
        return false;
    }

    const auto getValue = reinterpret_cast<PcanGetValueFunction>(library.resolve("CAN_GetValue"));
    if (!getValue)
    {
        if (errorString)
            *errorString = QStringLiteral("PCAN-Basic does not export CAN_GetValue");
        return false;
    }

    quint32 actualValue = PCAN_PARAMETER_OFF_VALUE;
    const quint32 status = getValue(channel, PCAN_LISTEN_ONLY_PARAMETER,
                                    &actualValue, sizeof(actualValue));
    if (status != PCAN_ERROR_OK_VALUE)
    {
        if (errorString)
            *errorString = pcanStatusText(library, status);
        return false;
    }

    const bool actual = actualValue == PCAN_PARAMETER_ON_VALUE;
    if (actual != expected)
    {
        if (errorString)
        {
            *errorString = QStringLiteral("PCAN-Basic reported listen-only %1 instead of %2")
                    .arg(actual ? QStringLiteral("on") : QStringLiteral("off"),
                         expected ? QStringLiteral("on") : QStringLiteral("off"));
        }
        return false;
    }

    return true;
}
}

/***********************************/
/****    class definition       ****/
/***********************************/

SerialBusConnection::SerialBusConnection(QString portName, QString driverName, int pBusSpeed, int pDataRate, bool pCanFd) :
    CANConnection(portName, driverName, CANCon::SERIALBUS,0 ,pBusSpeed, pCanFd, pDataRate ,1, 4000, true),
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

    const bool isPeakCan = getDriver().compare(QStringLiteral("peakcan"), Qt::CaseInsensitive) == 0;
    if (isPeakCan)
    {
        // Qt's PeakCAN backend does not consume SavvyCAN's UserKey bit mask. Set the
        // native PCAN-Basic parameter before QCanBusDevice initializes the channel.
        QString pcanError;
        if (!configurePeakCanListenOnly(getPort(), bus.isListenOnly(), &pcanError))
        {
            if (bus.isListenOnly())
            {
                qCritical() << "Refusing to open PeakCAN without verified listen-only setup:"
                            << pcanError;
                return;
            }

            qWarning() << "Could not explicitly disable PeakCAN listen-only mode:"
                       << pcanError;
        }
    }
    else
    {
        if(bus.isListenOnly())
            sbusconfig |= EN_SILENT_MODE;
        mDev_p->setConfigurationParameter(QCanBusDevice::UserKey, sbusconfig);
    }

    /* connect device */
    if (!mDev_p->connectDevice()) {
        disconnectDevice();
        qDebug() << "can't connect device";
        return;
    }

    if (isPeakCan && bus.isListenOnly())
    {
        QString pcanError;
        if (!verifyPeakCanListenOnly(getPort(), true, &pcanError))
        {
            qCritical() << "PeakCAN did not enter listen-only mode; disconnecting to avoid"
                           " acknowledging or disturbing the CAN bus:"
                        << pcanError;
            disconnectDevice();
        }
    }
}


bool SerialBusConnection::piSendFrame(const CANFrame& pFrame)
{
    /* sanity checks */
    if(0 != pFrame.bus /*|| pFrame.len>8*/)
        return false;
    if (!mDev_p) return false;

    CANBus bus;
    if (getBusConfig(0, bus) && bus.isListenOnly())
        return false;

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
                    frame_p->setTimeStamp(QCanBusFrame::TimeStamp::fromMicroSeconds(QDateTime::currentMSecsSinceEpoch() * 1000ul));
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
            if (mDev_p && mDev_p->state() == QCanBusDevice::ConnectedState) {
                setStatus(CANCon::CONNECTED);
                stats.conStatus = getStatus();
                stats.numHardwareBuses = mNumBuses;
                emit status(stats);
            }
            else if (mDev_p && mDev_p->state() == QCanBusDevice::UnconnectedState) {
                /* try to reconnect */
                CANBus bus;
                if(getBusConfig(0, bus))
                {
                    bus.setActive(true);
                    setBusSettings(0, bus);
                }

                if (mDev_p->state() == QCanBusDevice::ConnectedState)
                {
                    setStatus(CANCon::CONNECTED);
                    stats.conStatus = getStatus();
                    stats.numHardwareBuses = mNumBuses;
                    emit status(stats);
                }
            }
            break;
        default: {}
    }
}
