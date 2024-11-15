#include <QObject>
#include <QDebug>
#include <QCanBusFrame>
#include <QSettings>
#include <QStringBuilder>
#include <QtNetwork>

#include "utility.h"
#include "mqtt_bus.h"

MQTT_BUS::MQTT_BUS(QString topicName) :
    CANConnection(topicName, "mqtt_client", CANCon::MQTT, 0, 0, 0, false, 0, 1, 4000, true),
    mTimer(this) /*NB: set this as parent of timer to manage it from working thread */
{

    sendDebug("MQTT_BUS()");

    crypto = new SimpleCrypt(Q_UINT64_C(0xdeadbeefface6285));

    isAutoRestart = false;
    this->topicName = topicName;

    timeBasis = 0;
    lastSystemTimeBasis = 0;

    readSettings();
}


MQTT_BUS::~MQTT_BUS()
{
    delete crypto;
    stop();
    sendDebug("~MQTT_BUS");
}

void MQTT_BUS::sendDebug(const QString debugText)
{
    qDebug() << debugText;
    debugOutput(debugText);
}

void MQTT_BUS::piStarted()
{
    QSettings settings;
    QString userName = settings.value("Remote/User", "Anonymous").toString();
    QString host = settings.value("Remote/Host", "api.savvycan.com").toString();
    int port = settings.value("Remote/Port", 8883).toInt();
    QByteArray encPass = settings.value("Remote/Pass", "").toByteArray();
    QByteArray password;
    if (encPass.length() > 0)
        password = crypto->decryptToByteArray(encPass);

    if (port == 8883)
    {
        QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
        mqttClient = new QMQTT::Client(host, port, sslConfig);
    }
    else
    {
        QHostAddress hAddr = QHostInfo::fromName(host).addresses()[0];
        sendDebug("IP Address of Host: " + hAddr.toString());
        mqttClient = new QMQTT::Client(hAddr, port);
    }

    connect(mqttClient, &QMQTT::Client::connected, this, &MQTT_BUS::clientConnected);
    connect(mqttClient, &QMQTT::Client::error, this, &MQTT_BUS::clientErrored);

    //qDebug() << "User: " << userName << "  Pass: " << password;
    mqttClient->setClientId(genRandomClientID());
    if (userName.length() > 0) mqttClient->setUsername(userName);
    if (password.length() > 0) mqttClient->setPassword(password);
    sendDebug("Attempting to connect to MQTT.");
    mqttClient->connectToHost();
}

//MQTT required a client ID and they cannot be the same for any two clients. But, they really aren't super exciting
//or important to be named explicitly. Perhaps it might be nice to be able to see it for debugging though.
QString MQTT_BUS::genRandomClientID()
{
    QString output;
    output.reserve(12);
    QRandomGenerator gen = QRandomGenerator::securelySeeded();
    for (int i = 0; i < 12; i++)
    {
        int val = gen.bounded(0, 62);
        if (val < 26) output.append(QChar('A'+val));
        else if (val < 52) output.append(QChar('a'+val-26));
        else output.append(QChar('0'+val-52));
    }
    qDebug() << "Client ID: " << output;
    return output;
}

void MQTT_BUS::clientErrored(const QMQTT::ClientError error)
{
    switch (error)
    {
    case QMQTT::UnknownError:
        sendDebug("MQTT Unknown Error");
        break;
    case QMQTT::SocketConnectionRefusedError:
        sendDebug("MQTT Connection Refused");
        break;
    case QMQTT::SocketRemoteHostClosedError:
        sendDebug("MQTT Remote Host Closed The Connection");
        break;
    case QMQTT::SocketHostNotFoundError:
        sendDebug("MQTT Remote Host Not Found");
        break;
    case QMQTT::SocketAccessError:
        sendDebug("MQTT Socket Access Error");
        break;
    case QMQTT::SocketResourceError:
        sendDebug("MQTT Resource Error");
        break;
    case QMQTT::SocketTimeoutError:
        sendDebug("MQTT Timeout Error");
        break;
    case QMQTT::SocketDatagramTooLargeError:
        sendDebug("MQTT Datagram too large");
        break;
    case QMQTT::SocketNetworkError:
        sendDebug("MQTT Network Error");
        break;
    case QMQTT::SocketAddressInUseError:
        sendDebug("MQTT Address In Use ERROR");
        break;
    case QMQTT::SocketAddressNotAvailableError:
        sendDebug("MQTT Address Not Available");
        break;
    case QMQTT::SocketUnsupportedSocketOperationError:
        sendDebug("MQTT Unsupported Operation");
        break;
    case QMQTT::SocketUnfinishedSocketOperationError:
        sendDebug("MQTT Unfinished Socket Operation");
        break;
    case QMQTT::SocketProxyAuthenticationRequiredError:
        sendDebug("MQTT Proxy Auth Required");
        break;
    case QMQTT::SocketSslHandshakeFailedError:
        sendDebug("MQTT SSL Handshake Failure");
        break;
    case QMQTT::SocketProxyConnectionRefusedError:
        sendDebug("MQTT Proxy Connection Refused");
        break;
    case QMQTT::SocketProxyConnectionClosedError:
        sendDebug("MQTT Proxy Connection Closed");
        break;
    case QMQTT::SocketProxyConnectionTimeoutError:
        sendDebug("MQTT Proxy Connection Timeout");
        break;
    case QMQTT::SocketProxyNotFoundError:
        sendDebug("MQTT Proxy Not Found");
        break;
    case QMQTT::SocketProxyProtocolError:
        sendDebug("MQTT Proxy Protocol Error");
        break;
    case QMQTT::SocketOperationError:
        sendDebug("MQTT Socket Operation Error");
        break;
    case QMQTT::SocketSslInternalError:
        sendDebug("MQTT SSL Internal Error");
        break;
    case QMQTT::SocketSslInvalidUserDataError:
        sendDebug("MQTT SSL Invalid User Data");
        break;
    case QMQTT::SocketTemporaryError:
        sendDebug("MQTT Temporary Error");
        break;
    case QMQTT::MqttUnacceptableProtocolVersionError:
        sendDebug("MQTT Unaccepted Protocol Version");
        break;
    case QMQTT::MqttIdentifierRejectedError:
        sendDebug("MQTT Identifier Rejected");
        break;
    case QMQTT::MqttServerUnavailableError:
        sendDebug("MQTT Server Unavailable");
        break;
    case QMQTT::MqttBadUserNameOrPasswordError:
        sendDebug("MQTT Bad Access Credentials");
        break;
    case QMQTT::MqttNotAuthorizedError:
        sendDebug("MQTT Not Authorized");
        break;
    case QMQTT::MqttNoPingResponse:
        sendDebug("MQTT No PING Response");
        break;
    }
}

void MQTT_BUS::piSuspend(bool pSuspend)
{
    /* update capSuspended */
    setCapSuspended(pSuspend);

    /* flush queue if we are suspended */
    if(isCapSuspended())
        getQueue().flush();
}


void MQTT_BUS::piStop()
{
    mTimer.stop();
    disconnectDevice();
}


bool MQTT_BUS::piGetBusSettings(int pBusIdx, CANBus& pBus)
{
    return getBusConfig(pBusIdx, pBus);
}


void MQTT_BUS::piSetBusSettings(int pBusIdx, CANBus bus)
{
    /* sanity checks */
    if( (pBusIdx < 0) || pBusIdx >= getNumBuses())
        return;

    /* copy bus config */
    setBusConfig(pBusIdx, bus);
    //we don't really update anything. We're just here to listen and perhaps send frames.
}


bool MQTT_BUS::piSendFrame(const CANFrame& frame)
{
    QByteArray buffer;
    //int c;
    //quint32 ID;

    //qDebug() << "Sending out GVRET frame with id " << frame.ID << " on bus " << frame.bus;

    framesRapid++;

    // Doesn't make sense to send an error frame
    // to an adapter
    if (frame.frameId() & 0x20000000) {
        return true;
    }

    QMQTT::Message msg;
    QByteArray bytes;

    msg.setTopic(topicName + "/s/" + QString::number(frame.frameId()));
    uint8_t flags = 0;
    if (frame.hasExtendedFrameFormat()) flags += 1;
    if (frame.frameType() == QCanBusFrame::RemoteRequestFrame) flags += 2;
    if (frame.hasFlexibleDataRateFormat()) flags += 4;
    if (frame.frameType() == QCanBusFrame::ErrorFrame) flags += 8;

    uint64_t micros = QDateTime::currentMSecsSinceEpoch() * 1000ull;
    for (int x = 0; x < 8; x++)
    {
        bytes.append(micros & 0xFF);
        micros = micros / 256;
    }
    bytes.append(flags);
    bytes.append(frame.payload());

    msg.setPayload(bytes);
    mqttClient->publish(msg);

    return true;
}



/****************************************************************/

void MQTT_BUS::readSettings()
{
    QSettings settings;

}

void MQTT_BUS::clientMessageReceived(const QMQTT::Message& message)
{
    //uint64_t timeBasis = CANConManager::getInstance()->getTimeBasis();


    /* drop frame if capture is suspended */
    if(isCapSuspended())
        return;

    CANFrame* frame_p = getQueue().get();
    if(frame_p)
    {
        uint32_t frameID = message.topic().split("/")[1].toInt();

        QByteArray timeStampBytes = message.payload().left(8);
        uint64_t timeStamp = qFromLittleEndian<uint64_t>(timeStampBytes.data());

        int flags = message.payload()[8];
        frame_p->setPayload(message.payload().right(message.payload().count() - 9));
        frame_p->bus = 0;
        frame_p->setExtendedFrameFormat(flags & 1);
        frame_p->setFrameId(frameID);
        frame_p->setFrameType(QCanBusFrame::DataFrame);
        frame_p->isReceived = true;
        if (useSystemTime)
        {
            frame_p->setTimeStamp(QCanBusFrame::TimeStamp(0, QDateTime::currentMSecsSinceEpoch() * 1000ul));
        }
        else frame_p->setTimeStamp(QCanBusFrame::TimeStamp(0, timeStamp));

        checkTargettedFrame(*frame_p);

        /* enqueue frame */
        getQueue().queue();
    }
}

void MQTT_BUS::clientConnected()
{
    sendDebug("Connected to MQTT Broker!");

    mqttClient->subscribe(topicName + "/+", 0); //subscribe to all sub topics to grab the frames.
    connect(mqttClient, &QMQTT::Client::received, this, &MQTT_BUS::clientMessageReceived);

    setStatus(CANCon::CONNECTED);
    CANConStatus stats;
    stats.conStatus = getStatus();
    stats.numHardwareBuses = 1;//mNumBuses;
    emit status(stats);
}

void MQTT_BUS::disconnectDevice() {

    setStatus(CANCon::NOT_CONNECTED);
    CANConStatus stats;
    stats.conStatus = getStatus();
    stats.numHardwareBuses = mNumBuses;
    emit status(stats);
}

void MQTT_BUS::rebuildLocalTimeBasis()
{

    //qDebug() << "Rebuilding GVRET time base. GVRET local base = " << buildTimeBasis;

    /*
      our time basis is the value we have to modulate the main system basis by in order
      to sync the GVRET timestamps to the rest of the system.
      The rest of the system uses CANConManager::getInstance()->getTimeBasis as the basis.
      GVRET returns to us the current time since boot up in microseconds.
      timeAtGVRETSync stores the "system" timestamp when the GVRET timestamp was retrieved.
    */
    /*
    lastSystemTimeBasis = CANConManager::getInstance()->getTimeBasis();
    int64_t systemDelta = timeAtGVRETSync - lastSystemTimeBasis;
    int32_t localDelta = buildTimeBasis - systemDelta;
    timeBasis = -localDelta;
    */
}
