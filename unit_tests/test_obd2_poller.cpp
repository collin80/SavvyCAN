#include <QtTest/QtTest>
#include "isotp_handler.h"
#include "obd2_poller.h"

class TestObd2Poller : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testRequestIds();
    void testOnlyReadOnlyServices();
    void testDiscoveryOneEcu();
    void testDiscoverySeveralEcus();
    void testDiscoveryNoEcu();
    void testDiscovery29Bit();
    void testOneRequestOutstanding();
    void testTimeout();
    void testResponsePending();
    void testNegativeReplyMarksPidUnsupported();
    void testConditionsNotCorrectKeepsPolling();
    void testRateLimit();
    void testIntervals();
    void testPollAllEcus();
    void testLateReply();
    void testReadVin();
    void testIgnoresOtherFrames();
    void testStop();
    void testRepliesLastSecond();

signals:
    void framesReceived(void*, const QVector<CommFrame>& frames);

private:
    void receive(quint32 id, const char *hex, int bus = 0, bool received = true, bool extended = false);
    QByteArray sentPayload(int i) const { return sentFrames[i].payload(); }
    void advance(qint64 ms) { now += ms; poller->service(); }

    qint64 now = 0;
    std::vector<CommFrame> sentFrames;
    QVector<CommFrame> dummyFrames;
    std::unique_ptr<ISOTP_HANDLER> isotp;
    std::unique_ptr<Obd2Poller> poller;
};

void TestObd2Poller::init()
{
    now = 1000;
    sentFrames.clear();

    auto pendingConn = [this](ISOTP_HANDLER* handler) -> QMetaObject::Connection {
        return QObject::connect(this, &TestObd2Poller::framesReceived, handler,
            [handler](void*, const QVector<CommFrame>& frames) { handler->rapidFrames(frames); });
    };
    isotp = std::make_unique<ISOTP_HANDLER>(
        dummyFrames,
        [this](const CommFrame& frame) { sentFrames.push_back(frame); },
        []() -> int { return 2; },
        pendingConn);

    poller = std::make_unique<Obd2Poller>(isotp.get(), [this]() { return now; });
    poller->setMaxRequestRate(1000); //1 ms apart, so most tests only care about replies and timeouts
}

void TestObd2Poller::cleanup()
{
    poller.reset();
    isotp.reset();
}

// A single CAN frame from an ECU; hex is the whole data field, PCI byte included
void TestObd2Poller::receive(quint32 id, const char *hex, int bus, bool received, bool extended)
{
    CommFrame frame;
    frame.setBus(bus);
    frame.setFrameId(id);
    frame.setExtendedFrameFormat(extended || id > 0x7FF);
    frame.setReceived(received);
    QByteArray data = QByteArray::fromHex(hex);
    while (data.size() < 8) data.append(char(0x55)); //padding the ECU might use
    frame.setPayload(data);
    emit framesReceived(nullptr, {frame});
}

void TestObd2Poller::testRequestIds()
{
    QCOMPARE(poller->functionalRequestId(), 0x7DFu);
    QCOMPARE(poller->requestIdFor(0), 0x7DFu);
    QCOMPARE(poller->requestIdFor(0x7E8), 0x7E0u);
    QCOMPARE(poller->requestIdFor(0x7EF), 0x7E7u);
    QVERIFY(poller->isReplyId(0x7E8));
    QVERIFY(poller->isReplyId(0x7EF));
    QVERIFY(!poller->isReplyId(0x7E0));
    QVERIFY(!poller->isReplyId(0x7DF));

    poller->setAddressing(Obd2Poller::Addressing::Extended29Bit);
    QCOMPARE(poller->functionalRequestId(), 0x18DB33F1u);
    QCOMPARE(poller->requestIdFor(0x18DAF110), 0x18DA10F1u);
    QVERIFY(poller->isReplyId(0x18DAF110));
    QVERIFY(!poller->isReplyId(0x18DA10F1));
    QVERIFY(!poller->isReplyId(0x7E8));
}

// The safety list: nothing but Mode 01 and Mode 09 ever goes out
void TestObd2Poller::testOnlyReadOnlyServices()
{
    QVERIFY(!poller->request(0x7E8, QByteArray::fromHex("04")));     //clear DTCs
    QVERIFY(!poller->request(0, QByteArray::fromHex("04")));
    QVERIFY(!poller->request(0x7E8, QByteArray::fromHex("0801")));   //control on-board devices
    QVERIFY(!poller->request(0x7E8, QByteArray::fromHex("1003")));   //UDS session control
    QVERIFY(!poller->request(0x7E8, QByteArray::fromHex("2EF19000"))); //UDS write by identifier
    QVERIFY(!poller->request(0x7E8, QByteArray::fromHex("3101FF00"))); //UDS routine control
    QVERIFY(!poller->request(0x7E8, QByteArray()));
    QVERIFY(!poller->request(0x123, QByteArray::fromHex("010C")));   //not an ECU reply ID
    QVERIFY(sentFrames.empty());

    for (int s = 0; s < 256; s++)
        QCOMPARE(Obd2Poller::isServiceAllowed(quint8(s)), s == 0x01 || s == 0x09);

    QVERIFY(poller->request(0x7E8, QByteArray::fromHex("010C")));
    QCOMPARE(sentFrames.size(), 1u);
    QCOMPARE(sentFrames[0].frameId(), 0x7E0u);
    QCOMPARE(sentPayload(0), QByteArray::fromHex("02010CAAAAAAAAAA"));
}

void TestObd2Poller::testDiscoveryOneEcu()
{
    QList<quint32> found;
    int finished = 0;
    connect(poller.get(), &Obd2Poller::ecuFound, this, [&](quint32 id) { found.append(id); });
    connect(poller.get(), &Obd2Poller::discoveryFinished, this, [&]() { finished++; });

    poller->discover();
    QVERIFY(poller->isDiscovering());
    QCOMPARE(sentFrames.size(), 1u);
    QCOMPARE(sentFrames[0].frameId(), 0x7DFu);
    QCOMPARE(sentPayload(0), QByteArray::fromHex("020100AAAAAAAAAA"));

    receive(0x7E8, "064100BE1FA813"); //PIDs 01-1F, and 0x20 supported
    QCOMPARE(found, QList<quint32>({0x7E8}));
    QCOMPARE(sentFrames.size(), 1u); //a request to all ECUs waits the whole time for the others

    advance(49);
    QCOMPARE(sentFrames.size(), 1u);
    advance(1);
    QCOMPARE(sentFrames.size(), 2u); //then the next bitmap, to that ECU alone
    QCOMPARE(sentFrames[1].frameId(), 0x7E0u);
    QCOMPARE(sentPayload(1), QByteArray::fromHex("020120AAAAAAAAAA"));
    QCOMPARE(finished, 0);

    receive(0x7E8, "06412080000000"); //0x21 only, no next bitmap
    QCOMPARE(finished, 1);
    QVERIFY(!poller->isDiscovering());
    QCOMPARE(poller->ecus().size(), 1);
    QList<quint8> expected = {0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x13, 0x15,
                              0x1C, 0x1F, 0x21};
    QCOMPARE(poller->ecus()[0].supportedPids, expected);
    QCOMPARE(poller->stats().timeouts, 0);
}

void TestObd2Poller::testDiscoverySeveralEcus()
{
    int finished = 0;
    connect(poller.get(), &Obd2Poller::discoveryFinished, this, [&]() { finished++; });

    poller->discover();
    receive(0x7E8, "06410080000001"); //engine: 0x01, and 0x20
    receive(0x7E9, "06410080000000"); //gearbox: 0x01 only
    advance(50);

    QCOMPARE(poller->ecus().size(), 2);
    QCOMPARE(sentFrames.size(), 2u);
    QCOMPARE(sentFrames[1].frameId(), 0x7E0u); //only the engine has more bitmaps
    QCOMPARE(sentPayload(1), QByteArray::fromHex("020120AAAAAAAAAA"));

    receive(0x7E8, "06412020000001"); //0x23, and 0x40 next
    QCOMPARE(sentFrames.size(), 2u); //rate limit: 1 ms apart
    advance(1);
    QCOMPARE(sentFrames.size(), 3u);
    QCOMPARE(sentPayload(2), QByteArray::fromHex("020140AAAAAAAAAA"));
    receive(0x7E8, "06414040000000"); //0x42

    QCOMPARE(finished, 1);
    QCOMPARE(poller->ecus()[0].supportedPids, QList<quint8>({0x01, 0x23, 0x42}));
    QCOMPARE(poller->ecus()[1].supportedPids, QList<quint8>({0x01}));
}

void TestObd2Poller::testDiscoveryNoEcu()
{
    int finished = 0;
    connect(poller.get(), &Obd2Poller::discoveryFinished, this, [&]() { finished++; });

    poller->discover();
    advance(50);
    QCOMPARE(finished, 1);
    QVERIFY(poller->ecus().isEmpty());
    QCOMPARE(poller->stats().timeouts, 1);
    QCOMPARE(sentFrames.size(), 1u);
}

void TestObd2Poller::testDiscovery29Bit()
{
    poller->setAddressing(Obd2Poller::Addressing::Extended29Bit);
    poller->discover();
    QCOMPARE(sentFrames[0].frameId(), 0x18DB33F1u);
    QVERIFY(sentFrames[0].hasExtendedFrameFormat());

    receive(0x7E8, "06410080000001"); //an 11-bit reply isn't one now
    QVERIFY(poller->ecus().isEmpty());

    receive(0x18DAF110, "06410080000001");
    QCOMPARE(poller->ecus().size(), 1);
    advance(50);
    QCOMPARE(sentFrames.size(), 2u);
    QCOMPARE(sentFrames[1].frameId(), 0x18DA10F1u);
    QVERIFY(sentFrames[1].hasExtendedFrameFormat());
}

void TestObd2Poller::testOneRequestOutstanding()
{
    QList<QPair<quint8, double>> updates;
    connect(poller.get(), &Obd2Poller::pidUpdated, this,
            [&](quint32 ecu, quint8 pid, const QByteArray &, const QList<OBD2::Value> &values) {
                QCOMPARE(ecu, 0x7E8u);
                updates.append({pid, values.isEmpty() ? -1.0 : values[0].value});
            });

    poller->setPollList({{0x7E8, 0x0C, 0}, {0x7E8, 0x0D, 0}});
    poller->start();
    QCOMPARE(sentFrames.size(), 1u);
    QCOMPARE(sentPayload(0), QByteArray::fromHex("02010CAAAAAAAAAA"));

    advance(10);
    QCOMPARE(sentFrames.size(), 1u); //still waiting for the reply

    receive(0x7E8, "04410C1AF8");
    QCOMPARE(updates.size(), 1);
    QCOMPARE(updates[0].first, quint8(0x0C));
    QCOMPARE(updates[0].second, 1726.0);
    QCOMPARE(sentFrames.size(), 2u); //next one straight away (10 ms since the last send)
    QCOMPARE(sentPayload(1), QByteArray::fromHex("02010DAAAAAAAAAA"));

    receive(0x7E8, "03410D32");
    QCOMPARE(updates[1].second, 50.0);
    advance(1);
    QCOMPARE(sentFrames.size(), 3u);
    QCOMPARE(sentPayload(2), QByteArray::fromHex("02010CAAAAAAAAAA")); //in turn
    QCOMPARE(poller->stats().replies, 2);
    QCOMPARE(poller->stats().requests, 3);
}

void TestObd2Poller::testTimeout()
{
    QList<QByteArray> timedOut;
    connect(poller.get(), &Obd2Poller::requestTimedOut, this,
            [&](quint32 ecu, const QByteArray &req) { QCOMPARE(ecu, 0x7E8u); timedOut.append(req); });

    poller->setPollList({{0x7E8, 0x0C, 0}, {0x7E8, 0x0D, 0}});
    poller->start();
    advance(49);
    QVERIFY(timedOut.isEmpty());
    QCOMPARE(sentFrames.size(), 1u);

    advance(1);
    QCOMPARE(timedOut, QList<QByteArray>({QByteArray::fromHex("010C")}));
    QCOMPARE(poller->stats().timeouts, 1);
    QCOMPARE(sentFrames.size(), 2u); //and the next one goes out
    QCOMPARE(sentPayload(1), QByteArray::fromHex("02010DAAAAAAAAAA"));

    poller->setReplyTimeout(100);
    advance(49);
    QCOMPARE(timedOut.size(), 1); //the request already out keeps its 50 ms
    advance(1);
    QCOMPARE(timedOut.size(), 2);
    QCOMPARE(sentFrames.size(), 3u);
    advance(99);
    QCOMPARE(timedOut.size(), 2); //the new one waits 100 ms
    advance(1);
    QCOMPARE(timedOut.size(), 3);
}

void TestObd2Poller::testResponsePending()
{
    QList<double> values;
    connect(poller.get(), &Obd2Poller::pidUpdated, this,
            [&](quint32, quint8, const QByteArray &, const QList<OBD2::Value> &v) { values.append(v[0].value); });

    poller->setPollList({{0x7E8, 0x05, 0}});
    poller->start();
    receive(0x7E8, "037F0178"); //response pending
    advance(1000);
    QCOMPARE(poller->stats().timeouts, 0);
    QCOMPARE(poller->stats().negativeReplies, 0);
    QCOMPARE(sentFrames.size(), 1u);

    receive(0x7E8, "0341057B");
    QCOMPARE(values, QList<double>({83.0}));
    QCOMPARE(sentFrames.size(), 2u);

    receive(0x7E8, "037F0178");
    advance(4999);
    QCOMPARE(poller->stats().timeouts, 0);
    advance(1);
    QCOMPARE(poller->stats().timeouts, 1); //P2* is up

    //"response pending" for another service doesn't stretch the wait
    receive(0x7E8, "037F0978");
    advance(50);
    QCOMPARE(poller->stats().timeouts, 2);
}

void TestObd2Poller::testNegativeReplyMarksPidUnsupported()
{
    QList<quint8> unsupported;
    connect(poller.get(), &Obd2Poller::pidUnsupported, this, [&](quint32, quint8 pid) { unsupported.append(pid); });

    poller->setPollList({{0x7E8, 0x5C, 0}, {0x7E8, 0x0C, 0}});
    poller->start();
    receive(0x7E8, "037F0112"); //sub function (PID) not supported
    QCOMPARE(unsupported, QList<quint8>({0x5C}));
    QCOMPARE(poller->stats().negativeReplies, 1);
    QVERIFY(poller->ecus()[0].unsupportedPids.contains(0x5C));
    advance(1);
    QCOMPARE(sentPayload(1), QByteArray::fromHex("02010CAAAAAAAAAA"));

    for (int i = 0; i < 3; i++)
    {
        receive(0x7E8, "04410C1AF8");
        advance(1);
    }
    for (size_t i = 1; i < sentFrames.size(); i++)
        QCOMPARE(sentPayload(int(i)), QByteArray::fromHex("02010CAAAAAAAAAA")); //0x5C isn't asked for again
}

void TestObd2Poller::testConditionsNotCorrectKeepsPolling()
{
    poller->setPollList({{0x7E8, 0x0C, 0}});
    poller->start();
    receive(0x7E8, "037F0122"); //conditions not correct
    QCOMPARE(poller->stats().negativeReplies, 1);
    QVERIFY(poller->ecus()[0].unsupportedPids.isEmpty());
    advance(1);
    QCOMPARE(sentFrames.size(), 2u);
    QCOMPARE(sentPayload(1), QByteArray::fromHex("02010CAAAAAAAAAA"));
}

void TestObd2Poller::testRateLimit()
{
    poller->setMaxRequestRate(10); //100 ms apart
    poller->setPollList({{0x7E8, 0x0C, 0}});
    poller->start();
    receive(0x7E8, "04410C1AF8");
    QCOMPARE(sentFrames.size(), 1u);
    advance(99);
    QCOMPARE(sentFrames.size(), 1u);
    advance(1);
    QCOMPARE(sentFrames.size(), 2u);
}

void TestObd2Poller::testIntervals()
{
    poller->setPollList({{0x7E8, 0x0C, 0}, {0x7E8, 0x05, 1000}});
    poller->start();

    int coolant = 0;
    for (int i = 0; i < 20; i++)
    {
        QByteArray last = sentFrames.back().payload();
        if (last[2] == char(0x05))
        {
            coolant++;
            receive(0x7E8, "0341057B");
        }
        else receive(0x7E8, "04410C1AF8");
        advance(10);
    }
    QCOMPARE(coolant, 1); //once in the first 200 ms, then not before 1000 ms

    for (int i = 0; i < 100; i++)
    {
        QByteArray last = sentFrames.back().payload();
        if (last[2] == char(0x05))
        {
            coolant++;
            receive(0x7E8, "0341057B");
        }
        else receive(0x7E8, "04410C1AF8");
        advance(10);
    }
    QCOMPARE(coolant, 2);
}

// A poll item for all ECUs waits the whole time and takes every answer
void TestObd2Poller::testPollAllEcus()
{
    QList<quint32> from;
    connect(poller.get(), &Obd2Poller::pidUpdated, this,
            [&](quint32 ecu, quint8, const QByteArray &, const QList<OBD2::Value> &) { from.append(ecu); });

    poller->setPollList({{0, 0x0D, 0}});
    poller->start();
    QCOMPARE(sentFrames[0].frameId(), 0x7DFu);
    receive(0x7E8, "03410D32");
    receive(0x7E9, "03410D33");
    QCOMPARE(sentFrames.size(), 1u);
    advance(50);
    QCOMPARE(sentFrames.size(), 2u);
    QCOMPARE(from, QList<quint32>({0x7E8, 0x7E9}));
    QCOMPARE(poller->stats().timeouts, 0);
}

// A reply after its request timed out is still a value, but doesn't end the request now outstanding
void TestObd2Poller::testLateReply()
{
    QList<quint8> pids;
    connect(poller.get(), &Obd2Poller::pidUpdated, this,
            [&](quint32, quint8 pid, const QByteArray &, const QList<OBD2::Value> &) { pids.append(pid); });

    poller->setPollList({{0x7E8, 0x0C, 0}, {0x7E8, 0x0D, 0}});
    poller->start();
    advance(50);  //0x0C times out, 0x0D goes out
    receive(0x7E8, "04410C1AF8");
    QCOMPARE(pids, QList<quint8>({0x0C}));
    advance(10);
    QCOMPARE(sentFrames.size(), 2u); //still waiting for 0x0D
    receive(0x7E8, "03410D32");
    QCOMPARE(sentFrames.size(), 3u);
}

// VIN: a request to one ECU and a reply over three frames, with flow control
void TestObd2Poller::testReadVin()
{
    QString vin;
    connect(poller.get(), &Obd2Poller::vehicleInfoRead, this, [&](quint32 ecu, quint8 pid, const QString &text) {
        QCOMPARE(ecu, 0x7E8u);
        QCOMPARE(pid, quint8(0x02));
        vin = text;
    });

    QVERIFY(!poller->readVin(0)); //not to all ECUs
    QVERIFY(poller->readVin(0x7E8));
    QCOMPARE(sentFrames.size(), 1u);
    QCOMPARE(sentFrames[0].frameId(), 0x7E0u);
    QCOMPARE(sentPayload(0), QByteArray::fromHex("020902AAAAAAAAAA"));

    receive(0x7E8, "1014490201314731"); //first frame: 20 bytes, "49 02 01 1G1"
    QCOMPARE(sentFrames.size(), 2u);
    QCOMPARE(sentFrames[1].frameId(), 0x7E0u); //flow control to the same ECU
    QCOMPARE(sentPayload(1).left(3), QByteArray::fromHex("300003"));

    receive(0x7E8, "214A433534343452"); //JC5444R
    QVERIFY(vin.isEmpty());
    receive(0x7E8, "2237323532333637"); //7252367
    QCOMPARE(vin, QString("1G1JC5444R7252367"));
    QCOMPARE(poller->stats().replies, 1);
}

void TestObd2Poller::testIgnoresOtherFrames()
{
    int replies = 0;
    connect(poller.get(), &Obd2Poller::replyReceived, this, [&](quint32, const QByteArray &) { replies++; });

    poller->setPollList({{0x7E8, 0x0C, 0}});
    poller->start();
    receive(0x123, "04410C1AF8");                //not an ECU
    receive(0x7E8, "04410C1AF8", 1);             //another bus
    receive(0x7E8, "04410C1AF8", 0, false);      //sent by us (local echo)
    QCOMPARE(replies, 0);
    QVERIFY(poller->ecus().isEmpty());
    QCOMPARE(sentFrames.size(), 1u);

    receive(0x7E8, "04410C1AF8");
    QCOMPARE(replies, 1);
}

void TestObd2Poller::testStop()
{
    poller->setPollList({{0x7E8, 0x0C, 0}});
    poller->start();
    QVERIFY(poller->isRunning());
    poller->stop();
    receive(0x7E8, "04410C1AF8");
    advance(100);
    QCOMPARE(sentFrames.size(), 1u);
    QVERIFY(!poller->isRunning());

    poller->start();
    QCOMPARE(sentFrames.size(), 2u);
}

void TestObd2Poller::testRepliesLastSecond()
{
    poller->setPollList({{0x7E8, 0x0C, 0}});
    poller->start();
    for (int i = 0; i < 10; i++)
    {
        receive(0x7E8, "04410C1AF8");
        advance(100);
    }
    QCOMPARE(poller->stats().repliesLastSecond, 10);
    advance(1000);
    receive(0x7E8, "04410C1AF8");
    QCOMPARE(poller->stats().repliesLastSecond, 1);
}

QTEST_MAIN(TestObd2Poller)
#include "test_obd2_poller.moc"
