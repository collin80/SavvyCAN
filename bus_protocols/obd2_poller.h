#ifndef OBD2_POLLER_H
#define OBD2_POLLER_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVector>
#include <functional>

#include "isotp_handler.h"
#include "obd2_pids.h"

/*
 * Polls OBD-II PIDs from the ECUs on one bus, through an ISO-TP handler.
 *
 * - Discovery asks every ECU for PID 0x00 (a functional request), keeps every ECU that answers, then follows each
 *   ECU's supported-PID bitmaps (0x20, 0x40 ...) with requests to that ECU alone.
 * - Polling takes the poll list in turn, weighted by each item's interval, with one request outstanding at a time:
 *   the next goes out when the reply arrives or the wait for it times out, and never faster than the request rate
 *   limit allows.
 * - Only read-only services (Mode 01 and Mode 09) are ever sent; anything else is refused.
 *
 * The poller sets the ISO-TP handler's filters, flow control and reception, so give it a handler of its own. It
 * doesn't take ownership of the handler. Timing comes from the clock passed in (milliseconds, only differences
 * matter); an internal timer calls service() while there's work, and tests can call it themselves instead.
 */
class Obd2Poller : public QObject
{
    Q_OBJECT

public:
    using Clock = std::function<qint64()>;

    enum class Addressing
    {
        Standard11Bit,  //requests to 0x7DF or 0x7E0-0x7E7, replies from 0x7E8-0x7EF
        Extended29Bit   //requests to 0x18DB33F1 or 0x18DAxxF1, replies from 0x18DAF1xx
    };

    struct PollItem
    {
        quint32 ecuReplyId;     //0 asks all ECUs (a functional request)
        quint8 pid;
        int intervalMs;         //0 polls as often as the others and the rate limit allow
    };

    struct Ecu
    {
        quint32 replyId;
        QList<quint8> supportedPids;    //from the bitmaps, without the bitmap PIDs themselves
        QSet<quint8> unsupportedPids;   //PIDs the ECU refused with a negative reply
    };

    struct Stats
    {
        int requests = 0;
        int replies = 0;            //positive replies
        int timeouts = 0;
        int negativeReplies = 0;    //not counting "response pending"
        int repliesLastSecond = 0;
    };

    static const int DEFAULT_REPLY_TIMEOUT_MS = 50;     //P2 of ISO 15765-4
    static const int DEFAULT_PENDING_TIMEOUT_MS = 5000; //P2* after a "response pending" reply
    static const int DEFAULT_MAX_REQUEST_RATE = 20;     //requests a second

    explicit Obd2Poller(ISOTP_HANDLER *isotp, Clock clock = Clock(), QObject *parent = nullptr);
    ~Obd2Poller();

    //Changing the bus or addressing forgets the ECUs found and anything outstanding
    void setBus(int bus);
    int bus() const { return busNum; }
    void setAddressing(Addressing mode);
    Addressing addressing() const { return addressMode; }
    void setReplyTimeout(int ms);
    void setPendingTimeout(int ms);
    void setMaxRequestRate(int perSecond);
    void setPollList(const QList<PollItem> &items);

    const QList<Ecu> &ecus() const { return ecuList; }
    const Stats &stats() const { return statistics; }
    bool isRunning() const { return running; }
    bool isDiscovering() const { return discovering; }

    static bool isServiceAllowed(quint8 service);
    quint32 functionalRequestId() const;
    quint32 requestIdFor(quint32 ecuReplyId) const;
    bool isReplyId(quint32 id) const;

    void discover();
    void start();
    void stop();
    //Mode 09 VIN from one ECU (its reply spans several frames, so it can't go to all ECUs at once)
    bool readVin(quint32 ecuReplyId);
    //One request outside the poll list, sent before the next poll; false for a service that isn't read-only
    bool request(quint32 ecuReplyId, const QByteArray &payload);

public slots:
    void service();

signals:
    void ecuFound(quint32 ecuReplyId);
    void discoveryFinished();
    void pidUpdated(quint32 ecuReplyId, quint8 pid, const QByteArray &data, const QList<OBD2::Value> &values);
    void pidUnsupported(quint32 ecuReplyId, quint8 pid);
    void vehicleInfoRead(quint32 ecuReplyId, quint8 pid, const QString &text);
    void requestTimedOut(quint32 ecuReplyId, const QByteArray &request);
    void negativeReply(quint32 ecuReplyId, quint8 service, quint8 code);
    void replyReceived(quint32 ecuReplyId, const QByteArray &payload); //every reply, positive or negative
    void statsChanged();

private:
    enum class Kind { Discovery, Bitmap, Poll, OneOff };

    struct Request
    {
        quint32 ecuReplyId;
        QByteArray payload;
        Kind kind;
    };

    struct Outstanding
    {
        bool active = false;
        Request request{0, QByteArray(), Kind::OneOff};
        qint64 deadline = 0;
        bool gotReply = false;
    };

    void configureHandler();
    void reset();
    void gotISOTPMessage(const ISOTP_MESSAGE &msg);
    void handleMode01(quint32 ecuReplyId, const QByteArray &payload);
    void handleNegative(quint32 ecuReplyId, const QByteArray &payload, qint64 now);
    bool replyMatchesOutstanding(quint32 ecuReplyId, const QByteArray &payload) const;
    bool sendRequest(const Request &req, qint64 now);
    void finishOutstanding();
    void checkDiscoveryDone();
    int nextDuePollItem(qint64 now) const;
    bool rateAllows(qint64 now) const;
    void countReply(qint64 now);
    Ecu &ecuFor(quint32 replyId);
    void startTimer();

    ISOTP_HANDLER *isotp;
    Clock clock;
    QElapsedTimer elapsed;
    QTimer timer;
    QMetaObject::Connection isotpConnection;

    int busNum = 0;
    Addressing addressMode = Addressing::Standard11Bit;
    int replyTimeoutMs = DEFAULT_REPLY_TIMEOUT_MS;
    int pendingTimeoutMs = DEFAULT_PENDING_TIMEOUT_MS;
    int maxRequestRate = DEFAULT_MAX_REQUEST_RATE;

    QList<PollItem> pollList;
    QVector<qint64> nextDue;    //per poll item: when it may be sent again
    QVector<quint64> lastSent;  //per poll item: send order, so items due together take turns
    quint64 sendCounter = 0;

    QList<Request> queue;       //discovery and one-off requests, sent before polling
    QList<Ecu> ecuList;
    Outstanding outstanding;
    bool running = false;
    bool discovering = false;
    bool hasSent = false;
    qint64 lastSendTime = 0;

    Stats statistics;
    QList<qint64> replyTimes;   //positive replies in the last second
};

#endif // OBD2_POLLER_H
