#include <QObject>
#include <QDebug>
#include <QCanBusFrame>
#include <QSettings>
#include <QStringBuilder>
#include <QtNetwork>

#include "mqtt_bus.h"

MQTT_BUS::MQTT_BUS(QString topicName) :
    CANConnection(topicName, "mqtt_client", CANCon::MQTT, 1, 4000, true),
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
    int port = settings.value("Remote/Port", 8333).toInt();
    QByteArray encPass = settings.value("Remote/Pass", "").toByteArray();
    QByteArray password = crypto->decryptToByteArray(encPass);

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    mqttClient = new QMQTT::Client(host, port, sslConfig);

    connect(mqttClient, &QMQTT::Client::connected, this, &MQTT_BUS::clientConnected);
    connect(mqttClient, &QMQTT::Client::error, this, &MQTT_BUS::clientErrored);

    //qDebug() << "User: " << userName << "  Pass: " << password;
    mqttClient->setClientId(genRandomClientID());
    mqttClient->setUsername(userName);
    mqttClient->setPassword(password);
    mqttClient->connectToHost();
}

//MQTT required a client ID and they cannot be the same for any two clients. But, they really aren't super exciting
//or important to be named explicitly. Perhaps it might be nice to be able to see it for debugging though.
QString MQTT_BUS::genRandomClientID()
{
    QString output;
    output.reserve(12);
    QRandomGenerator gen;
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
    qDebug() << QString::number(error);
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
    int c;
    quint32 ID;

    //qDebug() << "Sending out GVRET frame with id " << frame.ID << " on bus " << frame.bus;

    framesRapid++;

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

    return true;
}



/****************************************************************/

void MQTT_BUS::readSettings()
{
    QSettings settings;

}

void MQTT_BUS::clientConnected()
{
    sendDebug("Connecting to MQTT Broker!");

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
