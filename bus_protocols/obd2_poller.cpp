#include "obd2_poller.h"

#include <QDebug>
#include <algorithm>

namespace
{
    const quint32 FUNCTIONAL_11BIT = 0x7DF;
    const quint32 REPLY_BASE_11BIT = 0x7E8;
    const quint32 REPLY_MASK_11BIT = 0x7F8;
    const quint32 FUNCTIONAL_29BIT = 0x18DB33F1;
    const quint32 PHYSICAL_29BIT = 0x18DA00F1;  //target address in bits 8-15
    const quint32 REPLY_BASE_29BIT = 0x18DAF100; //source address in bits 0-7
    const quint32 REPLY_MASK_29BIT = 0x1FFFFF00;

    const quint8 NEGATIVE_REPLY = 0x7F;
    const quint8 NRC_SERVICE_NOT_SUPPORTED = 0x11;
    const quint8 NRC_SUB_FUNCTION_NOT_SUPPORTED = 0x12;
    const quint8 NRC_REQUEST_OUT_OF_RANGE = 0x31;
    const quint8 NRC_RESPONSE_PENDING = 0x78;

    const int SERVICE_INTERVAL_MS = 2;
}

Obd2Poller::Obd2Poller(ISOTP_HANDLER *isotp, Clock clock, QObject *parent)
    : QObject(parent), isotp(isotp), clock(clock)
{
    elapsed.start();
    if (!this->clock) this->clock = [this]() { return elapsed.elapsed(); };

    timer.setTimerType(Qt::PreciseTimer);
    timer.setInterval(SERVICE_INTERVAL_MS);
    connect(&timer, &QTimer::timeout, this, &Obd2Poller::service);

    isotpConnection = connect(isotp, &ISOTP_HANDLER::newISOMessage, this, &Obd2Poller::gotISOTPMessage);
    configureHandler();
}

Obd2Poller::~Obd2Poller()
{
    disconnect(isotpConnection);
}

void Obd2Poller::configureHandler()
{
    isotp->setReception(false);
    isotp->clearAllFilters();
    isotp->setProcessAll(false);
    isotp->setExtendedAddressing(false);
    if (addressMode == Addressing::Standard11Bit) isotp->addFilter(busNum, REPLY_BASE_11BIT, REPLY_MASK_11BIT);
    else isotp->addFilter(busNum, REPLY_BASE_29BIT, REPLY_MASK_29BIT);
    isotp->setFlowCtrl(true); //Mode 09 replies span several frames
    isotp->setReception(true);
}

void Obd2Poller::reset()
{
    queue.clear();
    ecuList.clear();
    outstanding = Outstanding();
    discovering = false;
}

void Obd2Poller::setBus(int bus)
{
    if (bus == busNum) return;
    busNum = bus;
    reset();
    configureHandler();
}

void Obd2Poller::setAddressing(Addressing mode)
{
    if (mode == addressMode) return;
    addressMode = mode;
    reset();
    configureHandler();
}

void Obd2Poller::setReplyTimeout(int ms)
{
    replyTimeoutMs = qMax(1, ms);
}

void Obd2Poller::setPendingTimeout(int ms)
{
    pendingTimeoutMs = qMax(1, ms);
}

void Obd2Poller::setMaxRequestRate(int perSecond)
{
    maxRequestRate = qBound(1, perSecond, 1000);
}

void Obd2Poller::setPollList(const QList<PollItem> &items)
{
    pollList = items;
    qint64 now = clock();
    nextDue = QVector<qint64>(pollList.size(), now);
    lastSent = QVector<quint64>(pollList.size(), 0);
}

bool Obd2Poller::isServiceAllowed(quint8 service)
{
    //Read-only services only: never clear DTCs (04), control devices (08) or anything UDS
    return service == OBD2::SHOW_CURRENT_DATA || service == OBD2::VEHICLE_INFO;
}

quint32 Obd2Poller::functionalRequestId() const
{
    return (addressMode == Addressing::Standard11Bit) ? FUNCTIONAL_11BIT : FUNCTIONAL_29BIT;
}

quint32 Obd2Poller::requestIdFor(quint32 ecuReplyId) const
{
    if (ecuReplyId == 0) return functionalRequestId();
    if (addressMode == Addressing::Standard11Bit) return ecuReplyId - 8;
    return PHYSICAL_29BIT | ((ecuReplyId & 0xFF) << 8);
}

bool Obd2Poller::isReplyId(quint32 id) const
{
    if (addressMode == Addressing::Standard11Bit) return (id & REPLY_MASK_11BIT) == REPLY_BASE_11BIT;
    return (id & REPLY_MASK_29BIT) == REPLY_BASE_29BIT;
}

void Obd2Poller::discover()
{
    ecuList.clear();
    //drop discovery requests from an earlier discovery, keep one-off requests
    for (int i = queue.size() - 1; i >= 0; i--)
    {
        if (queue[i].kind == Kind::Discovery || queue[i].kind == Kind::Bitmap) queue.removeAt(i);
    }
    queue.prepend({0, OBD2::mode01Request(0x00), Kind::Discovery});
    discovering = true;
    service();
}

void Obd2Poller::start()
{
    running = true;
    service();
}

void Obd2Poller::stop()
{
    running = false;
}

bool Obd2Poller::readVin(quint32 ecuReplyId)
{
    if (ecuReplyId == 0) return false; //flow control can only go to one ECU
    return request(ecuReplyId, OBD2::mode09Request(OBD2::VIN_PID));
}

bool Obd2Poller::request(quint32 ecuReplyId, const QByteArray &payload)
{
    if (payload.isEmpty() || !isServiceAllowed(static_cast<quint8>(payload[0])))
    {
        qWarning() << "OBD-II poller: refusing to send service" << (payload.isEmpty() ? -1 : static_cast<quint8>(payload[0]));
        return false;
    }
    if (ecuReplyId != 0 && !isReplyId(ecuReplyId)) return false;
    queue.append({ecuReplyId, payload, Kind::OneOff});
    service();
    return true;
}

void Obd2Poller::service()
{
    qint64 now = clock();

    if (outstanding.active && now >= outstanding.deadline)
    {
        //a request to all ECUs always waits the whole time; it only timed out if nobody answered
        if (outstanding.request.ecuReplyId != 0 || !outstanding.gotReply)
        {
            statistics.timeouts++;
            emit requestTimedOut(outstanding.request.ecuReplyId, outstanding.request.payload);
            emit statsChanged();
        }
        finishOutstanding();
    }

    if (!outstanding.active && rateAllows(now))
    {
        if (!queue.isEmpty())
        {
            sendRequest(queue.takeFirst(), now);
        }
        else if (running)
        {
            int idx = nextDuePollItem(now);
            if (idx >= 0)
            {
                const PollItem &item = pollList[idx];
                nextDue[idx] = now + item.intervalMs;
                lastSent[idx] = ++sendCounter;
                sendRequest({item.ecuReplyId, OBD2::mode01Request(item.pid), Kind::Poll}, now);
            }
        }
    }

    if (outstanding.active || !queue.isEmpty() || running) startTimer();
    else timer.stop();
}

void Obd2Poller::startTimer()
{
    if (!timer.isActive()) timer.start();
}

bool Obd2Poller::rateAllows(qint64 now) const
{
    if (!hasSent) return true;
    return (now - lastSendTime) * maxRequestRate >= 1000;
}

int Obd2Poller::nextDuePollItem(qint64 now) const
{
    int best = -1;
    for (int i = 0; i < pollList.size(); i++)
    {
        const PollItem &item = pollList[i];
        if (nextDue[i] > now) continue;
        if (item.ecuReplyId != 0)
        {
            bool refused = false;
            for (const Ecu &ecu : ecuList)
            {
                if (ecu.replyId == item.ecuReplyId && ecu.unsupportedPids.contains(item.pid)) refused = true;
            }
            if (refused) continue;
        }
        if (best < 0 || nextDue[i] < nextDue[best] || (nextDue[i] == nextDue[best] && lastSent[i] < lastSent[best]))
            best = i;
    }
    return best;
}

bool Obd2Poller::sendRequest(const Request &req, qint64 now)
{
    //checked again here so nothing that bypasses request() can send a writing service
    if (req.payload.isEmpty() || !isServiceAllowed(static_cast<quint8>(req.payload[0]))) return false;

    outstanding.active = true;
    outstanding.request = req;
    outstanding.deadline = now + replyTimeoutMs;
    outstanding.gotReply = false;
    hasSent = true;
    lastSendTime = now;
    statistics.requests++;

    isotp->sendISOTPFrame(busNum, static_cast<int>(requestIdFor(req.ecuReplyId)), req.payload);
    emit statsChanged();
    return true;
}

void Obd2Poller::finishOutstanding()
{
    outstanding = Outstanding();
    checkDiscoveryDone();
}

void Obd2Poller::checkDiscoveryDone()
{
    if (!discovering) return;
    if (outstanding.active && (outstanding.request.kind == Kind::Discovery || outstanding.request.kind == Kind::Bitmap))
        return;
    for (const Request &req : queue)
    {
        if (req.kind == Kind::Discovery || req.kind == Kind::Bitmap) return;
    }
    discovering = false;
    emit discoveryFinished();
}

Obd2Poller::Ecu &Obd2Poller::ecuFor(quint32 replyId)
{
    for (Ecu &ecu : ecuList)
    {
        if (ecu.replyId == replyId) return ecu;
    }
    ecuList.append({replyId, {}, {}});
    emit ecuFound(replyId);
    return ecuList.last();
}

void Obd2Poller::countReply(qint64 now)
{
    statistics.replies++;
    replyTimes.append(now);
    while (!replyTimes.isEmpty() && now - replyTimes.first() >= 1000) replyTimes.removeFirst();
    statistics.repliesLastSecond = replyTimes.size();
    emit statsChanged();
}

bool Obd2Poller::replyMatchesOutstanding(quint32 ecuReplyId, const QByteArray &payload) const
{
    if (!outstanding.active) return false;
    const QByteArray &req = outstanding.request.payload;
    if (outstanding.request.ecuReplyId != 0 && outstanding.request.ecuReplyId != ecuReplyId) return false;
    if (static_cast<quint8>(payload[0]) != static_cast<quint8>(req[0]) + OBD2::POSITIVE_REPLY_OFFSET) return false;
    //the first PID of the reply is the first one asked for
    if (req.size() >= 2 && (payload.size() < 2 || payload[1] != req[1])) return false;
    return true;
}

void Obd2Poller::gotISOTPMessage(const ISOTP_MESSAGE &msg)
{
    if (!msg.isReceived()) return; //our own requests coming back
    if (msg.getBus() != busNum) return;
    quint32 id = msg.frameId();
    if (!isReplyId(id)) return;
    QByteArray payload = msg.payload();
    if (payload.isEmpty()) return;

    qint64 now = clock();
    ecuFor(id);
    emit replyReceived(id, payload);

    quint8 replyService = static_cast<quint8>(payload[0]);
    if (replyService == NEGATIVE_REPLY)
    {
        handleNegative(id, payload, now);
    }
    else
    {
        bool matches = replyMatchesOutstanding(id, payload);
        if (matches) outstanding.gotReply = true;

        if (replyService == OBD2::SHOW_CURRENT_DATA + OBD2::POSITIVE_REPLY_OFFSET)
        {
            handleMode01(id, payload);
        }
        else if (replyService == OBD2::VEHICLE_INFO + OBD2::POSITIVE_REPLY_OFFSET && payload.size() >= 2)
        {
            quint8 pid = static_cast<quint8>(payload[1]);
            QString text;
            if ((pid == OBD2::VIN_PID || pid == OBD2::ECU_NAME_PID) && OBD2::decodeMode09Text(payload, pid, text))
                emit vehicleInfoRead(id, pid, text);
        }
        countReply(now);

        //a request to one ECU is done when it answers; a request to all ECUs waits for the others too
        if (matches && outstanding.request.ecuReplyId != 0) finishOutstanding();
    }

    service();
}

void Obd2Poller::handleMode01(quint32 ecuReplyId, const QByteArray &payload)
{
    QList<OBD2::Reply> replies;
    if (!OBD2::parseMode01Reply(payload, replies)) return;

    for (const OBD2::Reply &reply : replies)
    {
        if (OBD2::isSupportBitmapPid(reply.pid))
        {
            QList<quint8> supported = OBD2::supportedPids(reply.pid, reply.data);
            Ecu &ecu = ecuFor(ecuReplyId);
            for (quint8 pid : supported)
            {
                if (!OBD2::isSupportBitmapPid(pid) && !ecu.supportedPids.contains(pid)) ecu.supportedPids.append(pid);
            }
            std::sort(ecu.supportedPids.begin(), ecu.supportedPids.end());

            int next = reply.pid + 0x20;
            if (discovering && next <= 0xE0 && supported.contains(static_cast<quint8>(next)))
                queue.append({ecuReplyId, OBD2::mode01Request(static_cast<quint8>(next)), Kind::Bitmap});
        }

        QList<OBD2::Value> values;
        const OBD2::PidInfo *info = OBD2::findMode01Pid(reply.pid);
        if (info) OBD2::decode(*info, reply.data, values);
        emit pidUpdated(ecuReplyId, reply.pid, reply.data, values);
    }
}

void Obd2Poller::handleNegative(quint32 ecuReplyId, const QByteArray &payload, qint64 now)
{
    if (payload.size() < 3) return;
    quint8 service = static_cast<quint8>(payload[1]);
    quint8 code = static_cast<quint8>(payload[2]);

    bool forOutstanding = outstanding.active && !outstanding.request.payload.isEmpty()
                          && static_cast<quint8>(outstanding.request.payload[0]) == service
                          && (outstanding.request.ecuReplyId == 0 || outstanding.request.ecuReplyId == ecuReplyId);

    if (code == NRC_RESPONSE_PENDING)
    {
        if (forOutstanding) outstanding.deadline = now + pendingTimeoutMs;
        return;
    }

    statistics.negativeReplies++;
    emit negativeReply(ecuReplyId, service, code);
    emit statsChanged();
    if (!forOutstanding) return;

    const QByteArray &req = outstanding.request.payload;
    if (service == OBD2::SHOW_CURRENT_DATA && req.size() >= 2
        && (code == NRC_SERVICE_NOT_SUPPORTED || code == NRC_SUB_FUNCTION_NOT_SUPPORTED || code == NRC_REQUEST_OUT_OF_RANGE))
    {
        quint8 pid = static_cast<quint8>(req[1]);
        Ecu &ecu = ecuFor(ecuReplyId);
        if (!ecu.unsupportedPids.contains(pid))
        {
            ecu.unsupportedPids.insert(pid);
            emit pidUnsupported(ecuReplyId, pid);
        }
    }

    outstanding.gotReply = true;
    if (outstanding.request.ecuReplyId != 0) finishOutstanding();
}
