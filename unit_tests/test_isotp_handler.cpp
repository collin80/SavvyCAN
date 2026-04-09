#include <QtTest/QtTest>
#include "isotp_handler.h"

class TestIsoTpHandler : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testSingleFrame();
    void testFirstAndConsecutiveFramesNoCf();
    void testFirstAndConsecutiveFramesWithCf();
    void testFlowControlFrame();
    void testExtendedAddressing();
    void testFirstAndConsecutiveFramesAndExtedingAddressingNoCf();
    void testFirstAndConsecutiveFramesAndExtedingAddressingWithCf();
signals:
    void framesReceived(void*, const QVector<CANFrame>& frames);
private:
    void sendCanFramesToHandler(void*, const QVector<CANFrame>& frames);
    void helperFirstAndConsecutiveFrames(bool sendCF);
    void helperFirstAndConsecutiveFramesAndExtedingAddressing(bool sendCF);
    std::vector<ISOTP_MESSAGE> capturedIsoTpMessages;
    std::vector<CANFrame> sentFrames;
    QVector<CANFrame> dummyFrames;
    std::unique_ptr<ISOTP_HANDLER> isotp_handler;
};

void TestIsoTpHandler::sendCanFramesToHandler(void*, const QVector<CANFrame>& frames)
{
    emit framesReceived(nullptr, frames);
}

void TestIsoTpHandler::init()
{
    capturedIsoTpMessages.clear();
    sentFrames.clear();
    dummyFrames.clear();

    // Create pendingConnection to later send can frames into the handler
    auto pendingConn = [this](ISOTP_HANDLER* handler) -> QMetaObject::Connection {
        return QObject::connect(
            this,
            &TestIsoTpHandler::framesReceived,
            handler,
            [handler](void*, const QVector<CANFrame>& frames){
                handler->rapidFrames(frames);
            }
        );
    };

    // Construct ISOTP_HANDLER with proper callbacks
    isotp_handler = std::make_unique<ISOTP_HANDLER>(
        dummyFrames,
        [this](const CANFrame& frame) { sentFrames.push_back(frame); },
        []() -> int { return 1; }, // getNoOfBuses callback
        pendingConn
    );

    // capture the sent isotp messages
    QObject::connect(isotp_handler.get(), &ISOTP_HANDLER::newISOMessage,
            this, [this](const ISOTP_MESSAGE& msg) {
            capturedIsoTpMessages.push_back(msg);
            });

    // Process all message IDs, not just filtered ones
    isotp_handler->setProcessAll(true);
    // enable pendingConn
    isotp_handler->setReception(true);
}

void TestIsoTpHandler::cleanup()
{
    isotp_handler.reset();
    capturedIsoTpMessages.clear();
    sentFrames.clear();
}

// --- Test Single Frame (SF) ---
void TestIsoTpHandler::testSingleFrame()
{
    CANFrame frame;
    frame.bus = 0;
    frame.setFrameId(0x123);
    frame.setExtendedFrameFormat(false);

    QByteArray data(8, 0);
    data[0] = 0x02; // SF, length = 2
    data[1] = 0xDE;
    data[2] = 0xAD;
    frame.setPayload(data);

    sendCanFramesToHandler(nullptr, {frame});

    QCOMPARE(capturedIsoTpMessages.size(), 1u);
    QCOMPARE(capturedIsoTpMessages[0].frameId(), 0x123u);
    QCOMPARE(capturedIsoTpMessages[0].payload(), QByteArray::fromHex("dead"));
}

void TestIsoTpHandler::testFirstAndConsecutiveFramesWithCf()
{
    helperFirstAndConsecutiveFrames(true);
}

void TestIsoTpHandler::testFirstAndConsecutiveFramesNoCf()
{
    helperFirstAndConsecutiveFrames(false);
}

void TestIsoTpHandler::helperFirstAndConsecutiveFrames(bool sendCF)
{
    int senderId = 0x451;
    int receiverId = 0x456;
    if (sendCF)
    {
        isotp_handler->setFlowCtrl(sendCF);
        // send SF Message to set private members lastSenderID and lastSenderBus
        // that are needed for later FC frame
        isotp_handler->sendISOTPFrame(/* bus */ 0, senderId, QByteArray::fromHex("AFFEBABE"));
    }

    // FF – total length = 8 bytes
    CANFrame ff;
    ff.bus = 0;
    ff.setFrameId(receiverId);
    ff.setExtendedFrameFormat(false);
    QByteArray ffData(8, 0);
    ffData[0] = 0x10; // FF type
    ffData[1] = 0x08; // total length = 8
    ffData[2] = 0xCA;
    ffData[3] = 0xFE;
    ffData[4] = 0xBA;
    ffData[5] = 0xBE;
    ffData[6] = 0xAF;
    ffData[7] = 0xFE;
    ff.setPayload(ffData);

    sendCanFramesToHandler(nullptr, {ff});

    if (sendCF)
    {
        auto &msg = sentFrames[0];
        QCOMPARE(msg.payload(), QByteArray::fromHex("04AFFEBABEAAAAAA"));
        QCOMPARE(msg.frameId(), senderId);
        // FC - did we sent it?
        QCOMPARE(sentFrames.size(), 2u);
        QByteArray fcData(8, 0);
        fcData[0] = 0x30; // Flow Control: Continue
        fcData[1] = 0x00; // Block size
        fcData[2] = 0x03; // STmin
        QCOMPARE(sentFrames[1].payload(), fcData);
    }

    // CF – remaining 2 bytes
    CANFrame cf;
    cf.bus = 0;
    cf.setFrameId(receiverId);
    cf.setExtendedFrameFormat(false);
    QByteArray cfData(8, 0);
    cfData[0] = 0x21; // sequence 1
    cfData[1] = 0xEF;
    cfData[2] = 0xAC;
    cfData[3] = 0x00;
    cfData[4] = 0x00;
    cfData[5] = 0x00;
    cfData[6] = 0x00;
    cfData[7] = 0x00;
    cf.setPayload(cfData);

    sendCanFramesToHandler(nullptr, {cf});

    QCOMPARE(capturedIsoTpMessages.size(), 1u);
    QCOMPARE(capturedIsoTpMessages[0].frameId(), 0x456u);
    auto &msg = capturedIsoTpMessages[0];
    QCOMPARE(msg.reportedLength, /* expectedLength */ 8);
    QCOMPARE(msg.payload(), QByteArray::fromHex("CAFEBABEAFFEEFAC"));
}

void TestIsoTpHandler::testFirstAndConsecutiveFramesAndExtedingAddressingNoCf()
{
    helperFirstAndConsecutiveFramesAndExtedingAddressing(false);
}

void TestIsoTpHandler::testFirstAndConsecutiveFramesAndExtedingAddressingWithCf()
{
    helperFirstAndConsecutiveFramesAndExtedingAddressing(true);
}

void TestIsoTpHandler::helperFirstAndConsecutiveFramesAndExtedingAddressing(bool sendCF)
{
    int senderId = 0x451;
    int receiverId = 0x456;
    uint8_t targetAddress = 0xfb; // TA (extended addressing)

    isotp_handler->setExtendedAddressing(true);

    if (sendCF)
    {
        isotp_handler->setFlowCtrl(sendCF);
        // send SF Message to set private members lastSenderID and lastSenderBus
        // that are needed for later FC frame
        isotp_handler->sendISOTPFrame(/* bus */ 0, senderId, QByteArray::fromHex("AFFEBABE"));
    }

    // FF – total length = 8 bytes
    CANFrame ff;
    ff.bus = 0;
    ff.setFrameId(receiverId);
    ff.setExtendedFrameFormat(false);
    QByteArray ffData(8, 0);
    ffData[0] = targetAddress;
    ffData[1] = 0x10; // FF type
    ffData[2] = 0x08; // total length = 8
    ffData[3] = 0xCA;
    ffData[4] = 0xFE;
    ffData[5] = 0xBA;
    ffData[6] = 0xBE;
    ffData[7] = 0xAF;
    ff.setPayload(ffData);

    sendCanFramesToHandler(nullptr, {ff});

    if (sendCF)
    {
        auto &msg = sentFrames[0];
        QCOMPARE(msg.payload(), QByteArray::fromHex("04AFFEBABEAAAAAA"));
        QCOMPARE(msg.frameId(), senderId);
        // FC - did we sent it?
        QCOMPARE(sentFrames.size(), 2u);
        QByteArray fcData(8, 0);
        fcData[0] = 0x30; // Flow Control: Continue
        fcData[1] = 0x00; // Block size
        fcData[2] = 0x03; // STmin
        QCOMPARE(sentFrames[1].payload(), fcData);
    }

    // CF – remaining 2 bytes
    CANFrame cf;
    cf.bus = 0;
    cf.setFrameId(receiverId);
    cf.setExtendedFrameFormat(false);
    QByteArray cfData(8, 0);
    cfData[0] = targetAddress;
    cfData[1] = 0x21; // sequence 1
    cfData[2] = 0xFE;
    cfData[3] = 0xEF;
    cfData[4] = 0xAC;
    cfData[5] = 0x00;
    cfData[6] = 0x00;
    cfData[7] = 0x00;
    cf.setPayload(cfData);

    sendCanFramesToHandler(nullptr, {cf});

    QCOMPARE(capturedIsoTpMessages.size(), 1u);
    QCOMPARE(capturedIsoTpMessages[0].frameId() >> 8 ,  receiverId); // the CAN id
    QCOMPARE(capturedIsoTpMessages[0].frameId() & 0xFF,  targetAddress); // the TA
    auto &msg = capturedIsoTpMessages[0];
    QCOMPARE(msg.reportedLength, /* expectedLength */ 8);
    QCOMPARE(msg.payload(), QByteArray::fromHex("CAFEBABEAFFEEFAC"));
}

// --- Test Flow Control Frame (FC) ---
void TestIsoTpHandler::testFlowControlFrame()
{
    CANFrame fc;
    fc.bus = 0;
    fc.setFrameId(0x321);
    fc.setExtendedFrameFormat(false);
    QByteArray fcData(8, 0);
    fcData[0] = 0x30; // Flow Control: Continue To Send
    fcData[1] = 0x00; // block size
    fcData[2] = 0x05; // separation time
    fc.setPayload(fcData);

    sendCanFramesToHandler(nullptr, {fc});

    // FC frame should not generate ISO messages
    QVERIFY(capturedIsoTpMessages.empty());
    QVERIFY(sentFrames.empty());
}

// --- Extended Addressing Test ---
void TestIsoTpHandler::testExtendedAddressing()
{
    isotp_handler->setExtendedAddressing(true);
    int receiverId = 0x200;

    CANFrame frame;
    frame.bus = 0;
    frame.setFrameId(receiverId);
    frame.setExtendedFrameFormat(false);
    QByteArray data(8, 0);

    uint8_t targetAddress = 0xfb; // TA (extended addressing)
    data[0] = targetAddress;
    data[1] = 0x02; // SF, length=2
    data[2] = 0xAA;
    data[3] = 0xBB;
    frame.setPayload(data);

    sendCanFramesToHandler(nullptr, {frame});

    QCOMPARE(capturedIsoTpMessages.size(), 1u);
    QCOMPARE(capturedIsoTpMessages[0].frameId() >> 8 ,  receiverId); // the CAN id
    QCOMPARE(capturedIsoTpMessages[0].frameId() & 0xFF,  targetAddress); // the TA
    QCOMPARE(capturedIsoTpMessages[0].payload(), QByteArray::fromHex("AABB")); // payload
}

QTEST_MAIN(TestIsoTpHandler)
#include "test_isotp_handler.moc"
