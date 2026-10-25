#include <QtTest/QtTest>
#include "isotp_handler.h"
#include "uds_handler.h"

class TestUdsHandler : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void testReadDataByIdentifier();
    void testMultiFrameResponse();
    void testNegativeResponse();
    void testSendUDSFrame();
signals:
    void framesReceived(void*, const QVector<CommFrame>& frames);
    void forwardedISOTPMessage(const ISOTP_MESSAGE& msg);
private:
    void sendCanFramesToHandler(void*, const QVector<CommFrame>& frames);
    void sendIsoTpFramesToHandler(const ISOTP_MESSAGE& msg);
    ISOTP_HANDLER* initAndGetIsoTpHandler();
    std::vector<ISOTP_MESSAGE> capturedIsoTpMessages;
    std::vector<UDS_MESSAGE> capturedUDSMessages;
    std::vector<CommFrame> sentFrames;
    QVector<CommFrame> dummyFrames;
    UDS_HANDLER* uds_handler;
};

void TestUdsHandler::sendCanFramesToHandler(void*, const QVector<CommFrame>& frames)
{
    emit framesReceived(nullptr, frames);
}

void TestUdsHandler::sendIsoTpFramesToHandler(const ISOTP_MESSAGE& msg)
{
    emit forwardedISOTPMessage(msg);
}

ISOTP_HANDLER* TestUdsHandler::initAndGetIsoTpHandler()
{
    capturedIsoTpMessages.clear();

    ISOTP_HANDLER* isotp_handler;
    // Create pendingConnection to later send can frames into the handler
    auto pendingConn = [this](ISOTP_HANDLER* handler) -> QMetaObject::Connection {
        return QObject::connect(
            this,
            &TestUdsHandler::framesReceived,
            handler,
            [handler](void*, const QVector<CommFrame>& frames){
                handler->rapidFrames(frames);
            }
        );
    };

    // Construct ISOTP_HANDLER with proper callbacks
    isotp_handler = new ISOTP_HANDLER(
        dummyFrames,
        [this](const CommFrame& frame) { sentFrames.push_back(frame); },
        []() -> int { return 1; }, // getNoOfBuses callback
        pendingConn
    );

    // capture the sent isotp messages
    QObject::connect(isotp_handler, &ISOTP_HANDLER::newISOMessage,
            this, [this](const ISOTP_MESSAGE& msg) {
            emit forwardedISOTPMessage(msg);
            capturedIsoTpMessages.push_back(msg);
            });

    return isotp_handler;
}

void TestUdsHandler::init()
{
    capturedUDSMessages.clear();
    sentFrames.clear();
    dummyFrames.clear();


    ///////////////// UDS handler setup //////////////
    // Create UDS_HANDLER with proper callbacks
    auto udsPendingConn = [this](UDS_HANDLER *handler) -> QMetaObject::Connection
    {
        return QObject::connect(
            this,
            &TestUdsHandler::forwardedISOTPMessage,
            handler,
            [handler](const ISOTP_MESSAGE& msg)
            {
                handler->gotISOTPFrame(msg);
            });
    };

    uds_handler = new UDS_HANDLER(
        initAndGetIsoTpHandler(),
        []() -> int { return 1; }, // getNoOfBuses callback
        udsPendingConn
    );

    uds_handler->setProcessAllIDs(true);
    uds_handler->setReception(true);

    // capture the sent uds messages
    QObject::connect(uds_handler, &UDS_HANDLER::newUDSMessage,
            this, [this](UDS_MESSAGE msg) {
            capturedUDSMessages.push_back(msg);
            });
}

void TestUdsHandler::cleanup()
{
    delete uds_handler;
    capturedIsoTpMessages.clear();
    capturedUDSMessages.clear();
    sentFrames.clear();
}

void TestUdsHandler::testReadDataByIdentifier()
{
    ISOTP_MESSAGE msg;

    msg.setFrameType(CommFrame::CANDataFrame);
    msg.setExtendedFrameFormat(false);
    msg.setFrameId(0x7E0);
    msg.reportedLength = 3;
    msg.setPayload(QByteArray::fromHex("22F190")); // ReadDataByIdentifier, DID=F190

    // Feed the ISO-TP message via QT signal -> slot
    sendIsoTpFramesToHandler(msg);

    QCOMPARE(capturedUDSMessages.size(), 1u);

    const UDS_MESSAGE& uds = capturedUDSMessages[0];
    qDebug() << uds_handler->getDetailedMessageAnalysis(uds);
    QCOMPARE(uds.service, 0x22);
    QCOMPARE(uds.subFunc, 0xF1);   // first byte after the service
    QCOMPARE(uds.payload(), QByteArray::fromHex("22F190")); // the whole ISO-TP payload, service included
}

void TestUdsHandler::testMultiFrameResponse()
{
    // Prepare a multi-frame ISO-TP message: positive response 0x62, DID F190, VIN starts with "WVW1234"
    ISOTP_MESSAGE msg;
    msg.setFrameId(0x7E8);
    msg.reportedLength = 20;
    msg.setPayload(QByteArray::fromHex("62F1905756575731323334")); // 0x62 = positive response, F190 = DID

    // Feed the ISO-TP message via QT signal -> slot
    sendIsoTpFramesToHandler(msg);

    // Verify that one UDS message was captured
    QCOMPARE(capturedUDSMessages.size(), 1u);

    const UDS_MESSAGE& uds = capturedUDSMessages[0];
    qDebug() << uds_handler->getDetailedMessageAnalysis(uds);
    QCOMPARE(uds.service, 0x62);                         // service byte
    QVERIFY(uds.payload().startsWith(QByteArray::fromHex("62F190"))); // service byte, then the DID
    QVERIFY(uds.payload().contains("WVW"));             // VIN prefix check
}

void TestUdsHandler::testNegativeResponse()
{
    // ISO-TP negative response: service 0x7F, original service 0x22, NRC = 0x13
    ISOTP_MESSAGE msg;
    msg.setFrameType(CommFrame::CANDataFrame);
    msg.setExtendedFrameFormat(false);
    msg.setFrameId(0x7E8);
    msg.reportedLength = 3;
    msg.setPayload(QByteArray::fromHex("7F2213")); // 0x7F = NACK, 0x22 = service, 0x13 = NRC

    // Feed the ISO-TP message via QT signal -> slot
    sendIsoTpFramesToHandler(msg);

    QCOMPARE(capturedUDSMessages.size(), 1u);

    const UDS_MESSAGE& uds = capturedUDSMessages[0];
    QCOMPARE(uds.service, 0x22);      // original requested service
    QVERIFY(uds.isErrorReply);        // should be flagged as negative response
    QCOMPARE(uds.subFunc, 0x13);      // NRC
    QCOMPARE(uds.payload(), QByteArray::fromHex("7F2213")); // the whole ISO-TP payload
}

void TestUdsHandler::testSendUDSFrame()
{
    // OBD-II Mode 01 PID 0x0C (engine speed) to all ECUs: service, then the one byte sub function, no data
    UDS_MESSAGE msg;
    msg.setBus(0);
    msg.setFrameId(0x7DF);
    msg.service = 0x01;
    msg.subFuncLen = 1;
    msg.subFunc = 0x0C;

    uds_handler->sendUDSFrame(msg);

    QCOMPARE(sentFrames.size(), 1u);
    QCOMPARE(sentFrames[0].frameId(), 0x7DFu);
    QVERIFY(!sentFrames[0].hasExtendedFrameFormat());
    QCOMPARE(sentFrames[0].payload(), QByteArray::fromHex("02010CAAAAAAAAAA")); // single frame, padded

    // a two byte sub function (UDS ReadDataByIdentifier F190) goes out high byte first
    msg.setFrameId(0x7E0);
    msg.service = 0x22;
    msg.subFuncLen = 2;
    msg.subFunc = 0xF190;
    uds_handler->sendUDSFrame(msg);

    QCOMPARE(sentFrames.size(), 2u);
    QCOMPARE(sentFrames[1].payload(), QByteArray::fromHex("0322F190AAAAAAAA"));
}
QTEST_MAIN(TestUdsHandler)
#include "test_uds_handler.moc"
