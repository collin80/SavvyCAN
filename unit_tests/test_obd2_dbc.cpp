#include <QtTest/QtTest>
#include <QTemporaryDir>
#include "dbc/dbchandler.h"
#include "obd2_dbc.h"

class TestObd2Dbc : public QObject
{
    Q_OBJECT

private slots:
    void testNames();
    void testStartBits();
    void testLoads();
    void testEveryFieldDecodesLikeTheTable();
    void testOtherPidsDontMatch();
    void testValueNames();
    void test29BitIds();
    void testSaveAndReload();
    void testSignalsInTheLastBytes();

private:
    std::unique_ptr<DBCFile> load(const QString &text);
    static CommFrame reply(quint32 id, quint8 pid, const QByteArray &data);
    static QByteArray pattern(int bytes, quint8 seed);

    QTemporaryDir dir;
    int fileCount = 0;
};

std::unique_ptr<DBCFile> TestObd2Dbc::load(const QString &text)
{
    QString path = dir.filePath(QString("obd2_%1.dbc").arg(fileCount++));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return nullptr;
    file.write(text.toUtf8());
    file.close();
    auto dbc = std::make_unique<DBCFile>();
    if (!dbc->loadFile(path)) return nullptr;
    return dbc;
}

// A single-frame Mode 01 reply, padded to 8 bytes as ECUs send it
CommFrame TestObd2Dbc::reply(quint32 id, quint8 pid, const QByteArray &data)
{
    QByteArray payload;
    payload.append(char(2 + data.size()));
    payload.append(char(0x41));
    payload.append(char(pid));
    payload.append(data);
    while (payload.size() < 8) payload.append(char(0x55));
    CommFrame frame;
    frame.setFrameId(id);
    frame.setExtendedFrameFormat(id > 0x7FF);
    frame.setPayload(payload);
    return frame;
}

QByteArray TestObd2Dbc::pattern(int bytes, quint8 seed)
{
    QByteArray data;
    quint8 v = seed;
    for (int i = 0; i < bytes; i++)
    {
        v = quint8(v * 37 + 11);
        data.append(char(v));
    }
    return data;
}

void TestObd2Dbc::testNames()
{
    const OBD2::PidInfo *speed = OBD2::findMode01Pid(0x0C);
    QCOMPARE(OBD2::dbcSignalName(0x0C, speed->fields[0]), QString("PID_0C_EngineSpeed"));
    QCOMPARE(OBD2::dbcSignalName(0xA6, OBD2::findMode01Pid(0xA6)->fields[0]), QString("PID_A6_Odometer"));
    QCOMPARE(OBD2::dbcMessageName(0x7E8), QString("OBD2_ECU_7E8"));
    QCOMPARE(OBD2::dbcMessageName(0x18DAF110), QString("OBD2_ECU_18DAF110"));
}

// Data byte A is frame byte 3, whose top bit is DBC bit 31
void TestObd2Dbc::testStartBits()
{
    QCOMPARE(OBD2::dbcStartBit(OBD2::findMode01Pid(0x0D)->fields[0]), 31);     //A
    QCOMPARE(OBD2::dbcStartBit(OBD2::findMode01Pid(0x14)->fields[1]), 39);     //B
    QCOMPARE(OBD2::dbcStartBit(OBD2::findMode01Pid(0x01)->fields[1]), 30);     //A6..A0
    QCOMPARE(OBD2::dbcStartBit(OBD2::findMode01Pid(0x01)->fields[2]), 35);     //B3
    QCOMPARE(OBD2::dbcStartBit(OBD2::findMode01Pid(0x64)->fields[4]), 63);     //E, the last byte
}

void TestObd2Dbc::testLoads()
{
    //with SAVVYCAN_OBD2_DBC set to a file name, the DBC is written there too (to check it with other tools)
    const QString out = qEnvironmentVariable("SAVVYCAN_OBD2_DBC");
    if (!out.isEmpty())
    {
        QFile file(out);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(OBD2::makeDbc({0x7E8, 0x18DAF110}).toUtf8());
    }

    auto dbc = load(OBD2::makeDbc({0x7E8, 0x7E9}));
    QVERIFY(dbc);
    QCOMPARE(dbc->messageHandler->getCount(), 2);

    DBC_MESSAGE *msg = dbc->messageHandler->findMsgByID(0x7E8);
    QVERIFY(msg);
    QCOMPARE(msg->name, QString("OBD2_ECU_7E8"));
    QVERIFY(!msg->extendedID);
    QCOMPARE(msg->len, 8u);
    QVERIFY(msg->multiplexorSignal);
    QCOMPARE(msg->multiplexorSignal->name, QString("Service"));

    int fields = 0;
    for (const OBD2::PidInfo &info : OBD2::mode01Pids()) fields += info.fields.size();
    QCOMPARE(msg->sigHandler->getCount(), fields + 2); //plus Service and PID

    DBC_SIGNAL *pid = msg->sigHandler->findSignalByName("PID");
    QVERIFY(pid->isMultiplexor);
    QCOMPARE(pid->multiplexParent, msg->multiplexorSignal);
    DBC_SIGNAL *speed = msg->sigHandler->findSignalByName("PID_0C_EngineSpeed");
    QVERIFY(speed);
    QCOMPARE(speed->multiplexParent, pid);
    QCOMPARE(speed->unitName, QString("rpm"));
    QCOMPARE(speed->comment, QString("Engine speed"));
    QVERIFY(!speed->intelByteOrder);
    QCOMPARE(speed->max, 16383.75);
}

// The DBC and the PID table must decode every field the same way
void TestObd2Dbc::testEveryFieldDecodesLikeTheTable()
{
    auto dbc = load(OBD2::makeDbc({0x7E8}));
    QVERIFY(dbc);
    DBC_MESSAGE *msg = dbc->messageHandler->findMsgByID(0x7E8);

    int checked = 0;
    for (const OBD2::PidInfo &info : OBD2::mode01Pids())
    {
        for (quint8 seed : {quint8(0x00), quint8(0x5A), quint8(0xC3)})
        {
            QByteArray data = pattern(info.dataBytes, seed);
            CommFrame frame = reply(0x7E8, info.pid, data);
            QList<OBD2::Value> values;
            QVERIFY(OBD2::decode(info, data, values));
            for (const OBD2::Value &v : values)
            {
                QString name = OBD2::dbcSignalName(info.pid, *v.field);
                DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(name);
                QVERIFY2(sig, qPrintable(name));
                QVERIFY2(sig->isSignalInMessage(frame), qPrintable(name));
                double y = 0;
                QVERIFY2(sig->processAsDouble(frame, y), qPrintable(name));
                QVERIFY2(qAbs(y - v.value) < 1e-6 * qMax(1.0, qAbs(v.value)),
                         qPrintable(QString("%1: DBC %2, table %3").arg(name).arg(y).arg(v.value)));
                checked++;
            }
        }
    }
    QVERIFY(checked > 300);
}

void TestObd2Dbc::testOtherPidsDontMatch()
{
    auto dbc = load(OBD2::makeDbc({0x7E8}));
    DBC_MESSAGE *msg = dbc->messageHandler->findMsgByID(0x7E8);
    CommFrame speed = reply(0x7E8, 0x0D, QByteArray::fromHex("32"));

    QVERIFY(msg->sigHandler->findSignalByName("PID_0D_VehicleSpeed")->isSignalInMessage(speed));
    QVERIFY(!msg->sigHandler->findSignalByName("PID_0C_EngineSpeed")->isSignalInMessage(speed));
    QVERIFY(!msg->sigHandler->findSignalByName("PID_05_CoolantTemp")->isSignalInMessage(speed));

    //a negative reply or a Mode 09 reply isn't a Mode 01 value
    CommFrame negative;
    negative.setFrameId(0x7E8);
    negative.setPayload(QByteArray::fromHex("037F011255555555"));
    QVERIFY(!msg->sigHandler->findSignalByName("PID_12_SecondaryAirStatus")->isSignalInMessage(negative));
    CommFrame vin;
    vin.setFrameId(0x7E8);
    vin.setPayload(QByteArray::fromHex("1014490201314731"));
    for (DBC_SIGNAL *sig : msg->sigHandler->getSignalsAsList())
    {
        if (sig->name == "Service") continue;
        QVERIFY2(!sig->isSignalInMessage(vin), qPrintable(sig->name));
    }
}

void TestObd2Dbc::testValueNames()
{
    auto dbc = load(OBD2::makeDbc({0x7E8}));
    DBC_MESSAGE *msg = dbc->messageHandler->findMsgByID(0x7E8);
    DBC_SIGNAL *fuel = msg->sigHandler->findSignalByName("PID_51_FuelType");
    QString text;
    QVERIFY(fuel->getValueString(8, text));
    QVERIFY2(text.contains("Electric"), qPrintable(text));
    DBC_SIGNAL *mil = msg->sigHandler->findSignalByName("PID_01_MIL");
    QVERIFY(mil->getValueString(1, text));
    QVERIFY2(text.contains("On"), qPrintable(text));
}

void TestObd2Dbc::test29BitIds()
{
    auto dbc = load(OBD2::makeDbc({0x18DAF110, 0x18DAF118}));
    QVERIFY(dbc);
    DBC_MESSAGE *msg = dbc->messageHandler->findMsgByID(0x18DAF110);
    QVERIFY(msg);
    QVERIFY(msg->extendedID);
    QCOMPARE(msg->name, QString("OBD2_ECU_18DAF110"));
    CommFrame frame = reply(0x18DAF110, 0x0C, QByteArray::fromHex("1AF8"));
    double y = 0;
    DBC_SIGNAL *speed = msg->sigHandler->findSignalByName("PID_0C_EngineSpeed");
    QVERIFY(speed->isSignalInMessage(frame));
    QVERIFY(speed->processAsDouble(frame, y));
    QCOMPARE(y, 1726.0);
}

// SavvyCAN's own saver keeps the multiplexing, so a saved copy decodes the same
void TestObd2Dbc::testSaveAndReload()
{
    auto dbc = load(OBD2::makeDbc({0x7E8}));
    QString saved = dir.filePath("saved.dbc");
    QVERIFY(dbc->saveFile(saved));

    DBCFile reloaded;
    QVERIFY(reloaded.loadFile(saved));
    DBC_MESSAGE *a = dbc->messageHandler->findMsgByID(0x7E8);
    DBC_MESSAGE *b = reloaded.messageHandler->findMsgByID(0x7E8);
    QVERIFY(b);
    QCOMPARE(b->sigHandler->getCount(), a->sigHandler->getCount());

    for (quint8 pid : {quint8(0x0C), quint8(0x32), quint8(0x42), quint8(0xA6)})
    {
        const OBD2::PidInfo *info = OBD2::findMode01Pid(pid);
        CommFrame frame = reply(0x7E8, pid, pattern(info->dataBytes, 0x77));
        QString name = OBD2::dbcSignalName(pid, info->fields[0]);
        double ya = 0, yb = 0;
        QVERIFY(b->sigHandler->findSignalByName(name)->isSignalInMessage(frame));
        QVERIFY(a->sigHandler->findSignalByName(name)->processAsDouble(frame, ya));
        QVERIFY(b->sigHandler->findSignalByName(name)->processAsDouble(frame, yb));
        QCOMPARE(yb, ya);
    }
}

// Not OBD-specific: processAsDouble used to refuse a Motorola signal ending in the frame's last bytes, because it
// checked startBit + size against the frame as it does for Intel signals (a Motorola start bit is the top bit)
void TestObd2Dbc::testSignalsInTheLastBytes()
{
    CommFrame frame;
    frame.setFrameId(0x123);
    frame.setPayload(QByteArray::fromHex("0001020304051234"));
    double y = 0;

    DBC_SIGNAL motorola;
    motorola.startBit = 55; //bytes 6 and 7, big-endian
    motorola.signalSize = 16;
    motorola.intelByteOrder = false;
    QVERIFY(motorola.processAsDouble(frame, y));
    QCOMPARE(y, double(0x1234));

    motorola.startBit = 63; //byte 7 alone
    motorola.signalSize = 8;
    QVERIFY(motorola.processAsDouble(frame, y));
    QCOMPARE(y, double(0x34));

    DBC_SIGNAL intel;
    intel.startBit = 48; //bytes 6 and 7, little-endian: unchanged
    intel.signalSize = 16;
    intel.intelByteOrder = true;
    QVERIFY(intel.processAsDouble(frame, y));
    QCOMPARE(y, double(0x3412));

    CommFrame shortFrame;
    shortFrame.setFrameId(0x123);
    shortFrame.setPayload(QByteArray::fromHex("00010203040512"));
    motorola.startBit = 55;
    motorola.signalSize = 16;
    QVERIFY(!motorola.processAsDouble(shortFrame, y)); //byte 7 is missing
    QVERIFY(!intel.processAsDouble(shortFrame, y));
    motorola.startBit = 49; //bits 49..48 then byte 7's top bits: still needs byte 7
    motorola.signalSize = 4;
    QVERIFY(!motorola.processAsDouble(shortFrame, y));
    motorola.startBit = 51; //bits 51..48, all in byte 6
    QVERIFY(motorola.processAsDouble(shortFrame, y));
}

QTEST_MAIN(TestObd2Dbc)
#include "test_obd2_dbc.moc"
