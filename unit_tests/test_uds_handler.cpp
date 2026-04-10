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
signals:
    void framesReceived(void*, const QVector<CANFrame>& frames);
    void forwardedISOTPMessage(const ISOTP_MESSAGE& msg);
private:
    void sendCanFramesToHandler(void*, const QVector<CANFrame>& frames);
    void sendIsoTpFramesToHandler(const ISOTP_MESSAGE& msg);
    ISOTP_HANDLER* initAndGetIsoTpHandler();
    std::vector<ISOTP_MESSAGE> capturedIsoTpMessages;
    std::vector<UDS_MESSAGE> capturedUDSMessages;
    std::vector<CANFrame> sentFrames;
    QVector<CANFrame> dummyFrames;
    UDS_HANDLER* uds_handler;
};

void TestUdsHandler::sendCanFramesToHandler(void*, const QVector<CANFrame>& frames)
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
            [handler](void*, const QVector<CANFrame>& frames){
                handler->rapidFrames(frames);
            }
        );
    };

    // Construct ISOTP_HANDLER with proper callbacks
    isotp_handler = new ISOTP_HANDLER(
        dummyFrames,
        [this](const CANFrame& frame) { sentFrames.push_back(frame); },
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

    msg.setFrameType(QCanBusFrame::FrameType::DataFrame);
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
    QCOMPARE(uds.payload(), QByteArray::fromHex("F190"));
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
    QVERIFY(uds.payload().startsWith(QByteArray::fromHex("F190"))); // DID should be first bytes
    QVERIFY(uds.payload().contains("WVW"));             // VIN prefix check
}

void TestUdsHandler::testNegativeResponse()
{
    // ISO-TP negative response: service 0x7F, original service 0x22, NRC = 0x13
    ISOTP_MESSAGE msg;
    msg.setFrameType(QCanBusFrame::FrameType::DataFrame);
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
    QCOMPARE(uds.payload(), QByteArray()); // payload should be empty after removing 2 bytes
}
QTEST_MAIN(TestUdsHandler)
#include "test_uds_handler.moc"
