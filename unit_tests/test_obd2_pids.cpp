#include <QtTest/QtTest>
#include <QRegularExpression>
#include <QSet>
#include "obd2_pids.h"

class TestObd2Pids : public QObject
{
    Q_OBJECT

private slots:
    void testTableConsistent();
    void testDecode_data();
    void testDecode();
    void testValueNames();
    void testShortData();
    void testFieldRanges();
    void testSupportedPids();
    void testSupportedPidsLastBitmap();
    void testRequests();
    void testParseSingleReply();
    void testParseSeveralPids();
    void testParseUnknownPid();
    void testParseNotAReply();
    void testVin();
    void testEcuName();
};

namespace
{
    double decodeOne(quint8 pid, const char *hex, int fieldIndex)
    {
        const OBD2::PidInfo *info = OBD2::findMode01Pid(pid);
        if (!info) return qQNaN();
        QList<OBD2::Value> values;
        if (!OBD2::decode(*info, QByteArray::fromHex(hex), values)) return qQNaN();
        if (fieldIndex >= values.size()) return qQNaN();
        return values[fieldIndex].value;
    }
}

// Every PID once, in order; fields inside their data bytes; names usable as DBC signal names and unique, since the
// generated DBC puts every PID's fields in one message
void TestObd2Pids::testTableConsistent()
{
    const auto &pids = OBD2::mode01Pids();
    QVERIFY(pids.size() > 80);

    QRegularExpression identifier("^[A-Za-z][A-Za-z0-9_]*$");
    QSet<QString> fieldNames;
    int previous = -1;
    for (const OBD2::PidInfo &info : pids)
    {
        QVERIFY2(info.pid > previous, qPrintable(QString("PID 0x%1 out of order or twice").arg(info.pid, 0, 16)));
        previous = info.pid;
        QVERIFY(!info.name.isEmpty());
        QVERIFY(info.dataBytes >= 1 && info.dataBytes <= 5);
        QCOMPARE(OBD2::isSupportBitmapPid(info.pid), info.fields.isEmpty());
        QCOMPARE(OBD2::findMode01Pid(info.pid), &info);

        for (const OBD2::Field &field : info.fields)
        {
            QVERIFY2(identifier.match(field.name).hasMatch(), qPrintable(field.name));
            QVERIFY2(!fieldNames.contains(field.name), qPrintable(field.name + " used twice"));
            fieldNames.insert(field.name);
            QVERIFY(field.bitLength >= 1);
            QVERIFY2(field.startBit + field.bitLength <= info.dataBytes * 8, qPrintable(field.name));
            QVERIFY(field.scale > 0.0);
        }
    }
    QVERIFY(OBD2::findMode01Pid(0x02) == nullptr); //freeze frame DTC: Mode 02 only
}

void TestObd2Pids::testDecode_data()
{
    QTest::addColumn<int>("pid");
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<int>("field");
    QTest::addColumn<double>("expected");

    QTest::newRow("engine speed (256A+B)/4") << 0x0C << QByteArray("1AF8") << 0 << 1726.0;
    QTest::newRow("vehicle speed") << 0x0D << QByteArray("32") << 0 << 50.0;
    QTest::newRow("coolant A-40") << 0x05 << QByteArray("7B") << 0 << 83.0;
    QTest::newRow("coolant minimum") << 0x05 << QByteArray("00") << 0 << -40.0;
    QTest::newRow("engine load 100%") << 0x04 << QByteArray("FF") << 0 << 100.0;
    QTest::newRow("fuel trim zero") << 0x06 << QByteArray("80") << 0 << 0.0;
    QTest::newRow("fuel trim -100") << 0x07 << QByteArray("00") << 0 << -100.0;
    QTest::newRow("timing advance") << 0x0E << QByteArray("94") << 0 << 10.0;
    QTest::newRow("MAF") << 0x10 << QByteArray("01F4") << 0 << 5.0;
    QTest::newRow("fuel pressure 3A") << 0x0A << QByteArray("64") << 0 << 300.0;
    QTest::newRow("O2 sensor 1 voltage") << 0x14 << QByteArray("B480") << 0 << 0.9;
    QTest::newRow("O2 sensor 1 trim") << 0x14 << QByteArray("B480") << 1 << 0.0;
    QTest::newRow("wide O2 sensor 2 lambda") << 0x25 << QByteArray("80000000") << 0 << 1.0;
    QTest::newRow("wide O2 sensor 2 voltage") << 0x25 << QByteArray("00008000") << 1 << 4.0;
    QTest::newRow("current O2 sensor 1 current") << 0x34 << QByteArray("80008000") << 1 << 0.0;
    QTest::newRow("evap pressure, signed") << 0x32 << QByteArray("FFFC") << 0 << -1.0;
    QTest::newRow("evap pressure, positive") << 0x32 << QByteArray("0010") << 0 << 4.0;
    QTest::newRow("catalyst temperature") << 0x3C << QByteArray("1194") << 0 << 410.0;
    QTest::newRow("control module voltage") << 0x42 << QByteArray("3A98") << 0 << 15.0;
    QTest::newRow("hybrid battery remaining") << 0x5B << QByteArray("80") << 0 << 128.0 * 100.0 / 255.0;
    QTest::newRow("fuel rate") << 0x5E << QByteArray("00C8") << 0 << 10.0;
    QTest::newRow("injection timing") << 0x5D << QByteArray("6900") << 0 << 0.0;
    QTest::newRow("actual torque") << 0x62 << QByteArray("AF") << 0 << 50.0;
    QTest::newRow("torque data point 4") << 0x64 << QByteArray("7D7D7D7DE1") << 4 << 100.0;
    QTest::newRow("odometer 32 bit") << 0xA6 << QByteArray("0001E240") << 0 << 12345.6;
    QTest::newRow("monitor status DTC count") << 0x01 << QByteArray("83076504") << 1 << 3.0;
    QTest::newRow("run time") << 0x1F << QByteArray("0E10") << 0 << 3600.0;
}

void TestObd2Pids::testDecode()
{
    QFETCH(int, pid);
    QFETCH(QByteArray, data);
    QFETCH(int, field);
    QFETCH(double, expected);

    double value = decodeOne(static_cast<quint8>(pid), data.constData(), field);
    QVERIFY2(qAbs(value - expected) < 1e-9, qPrintable(QString("got %1").arg(value, 0, 'g', 12)));
}

void TestObd2Pids::testValueNames()
{
    QList<OBD2::Value> values;

    QVERIFY(OBD2::decode(*OBD2::findMode01Pid(0x01), QByteArray::fromHex("83076504"), values));
    QCOMPARE(values.size(), 3);
    QCOMPARE(values[0].field->name, QString("MIL"));
    QCOMPARE(values[0].raw, 1);
    QCOMPARE(values[0].text, QString("On"));
    QCOMPARE(values[2].text, QString("Spark")); //B3 clear

    QVERIFY(OBD2::decode(*OBD2::findMode01Pid(0x01), QByteArray::fromHex("000F0000"), values));
    QCOMPARE(values[0].text, QString("Off"));
    QCOMPARE(values[2].text, QString("Compression")); //B3 set

    QVERIFY(OBD2::decode(*OBD2::findMode01Pid(0x03), QByteArray::fromHex("0200"), values));
    QCOMPARE(values[0].text, QString("Closed loop, using oxygen sensor feedback"));
    QCOMPARE(values[1].text, QString("Off"));

    QVERIFY(OBD2::decode(*OBD2::findMode01Pid(0x51), QByteArray::fromHex("08"), values));
    QCOMPARE(values[0].text, QString("Electric"));

    QVERIFY(OBD2::decode(*OBD2::findMode01Pid(0x1C), QByteArray::fromHex("63"), values));
    QCOMPARE(values[0].raw, 0x63);
    QVERIFY(values[0].text.isEmpty()); //no name for that value: the number is all there is

    QVERIFY(OBD2::decode(*OBD2::findMode01Pid(0x0C), QByteArray::fromHex("1AF8"), values));
    QVERIFY(values[0].text.isEmpty());
    QCOMPARE(values[0].field->unit, QString("rpm"));
}

void TestObd2Pids::testShortData()
{
    QList<OBD2::Value> values;
    QVERIFY(!OBD2::decode(*OBD2::findMode01Pid(0x0C), QByteArray::fromHex("1A"), values));
    QVERIFY(values.isEmpty());
    QVERIFY(!OBD2::decode(*OBD2::findMode01Pid(0xA6), QByteArray::fromHex("000102"), values));

    //longer is fine: the extra bytes are ignored
    QVERIFY(OBD2::decode(*OBD2::findMode01Pid(0x0D), QByteArray::fromHex("3200AA"), values));
    QCOMPARE(values[0].value, 50.0);

    qint64 raw;
    QVERIFY(!OBD2::extractBits(QByteArray::fromHex("FF"), 4, 8, false, raw));
    QVERIFY(OBD2::extractBits(QByteArray::fromHex("ABCD"), 4, 8, false, raw));
    QCOMPARE(raw, 0xBC);
    QVERIFY(OBD2::extractBits(QByteArray::fromHex("80"), 0, 8, true, raw));
    QCOMPARE(raw, -128);
}

void TestObd2Pids::testFieldRanges()
{
    const OBD2::Field &speed = OBD2::findMode01Pid(0x0C)->fields[0];
    QCOMPARE(speed.minimum(), 0.0);
    QCOMPARE(speed.maximum(), 16383.75);

    const OBD2::Field &coolant = OBD2::findMode01Pid(0x05)->fields[0];
    QCOMPARE(coolant.minimum(), -40.0);
    QCOMPARE(coolant.maximum(), 215.0);

    const OBD2::Field &evap = OBD2::findMode01Pid(0x32)->fields[0];
    QCOMPARE(evap.minimum(), -8192.0);
    QCOMPARE(evap.maximum(), 8191.75);
}

// The usual worked example: BE 1F A8 13
void TestObd2Pids::testSupportedPids()
{
    QList<quint8> pids = OBD2::supportedPids(0x00, QByteArray::fromHex("BE1FA813"));
    QList<quint8> expected = {0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x13, 0x15,
                              0x1C, 0x1F, 0x20};
    QCOMPARE(pids, expected);

    QCOMPARE(OBD2::supportedPids(0x40, QByteArray::fromHex("40000001")), QList<quint8>({0x42, 0x60}));
    QVERIFY(OBD2::supportedPids(0x00, QByteArray::fromHex("BE1FA8")).isEmpty()); //short
    QVERIFY(OBD2::supportedPids(0x0C, QByteArray::fromHex("FFFFFFFF")).isEmpty()); //not a bitmap PID

    QVERIFY(OBD2::isSupportBitmapPid(0x00));
    QVERIFY(OBD2::isSupportBitmapPid(0xC0));
    QVERIFY(!OBD2::isSupportBitmapPid(0x21));
}

void TestObd2Pids::testSupportedPidsLastBitmap()
{
    QList<quint8> pids = OBD2::supportedPids(0xE0, QByteArray::fromHex("80000001"));
    QCOMPARE(pids, QList<quint8>({0xE1})); //the last bit would be PID 0x100
}

void TestObd2Pids::testRequests()
{
    QCOMPARE(OBD2::mode01Request(0x0C), QByteArray::fromHex("010C"));
    QCOMPARE(OBD2::mode01Request(0x00), QByteArray::fromHex("0100"));
    QCOMPARE(OBD2::mode09Request(OBD2::VIN_PID), QByteArray::fromHex("0902"));
}

void TestObd2Pids::testParseSingleReply()
{
    QList<OBD2::Reply> replies;
    QVERIFY(OBD2::parseMode01Reply(QByteArray::fromHex("410C1AF8"), replies));
    QCOMPARE(replies.size(), 1);
    QCOMPARE(replies[0].pid, quint8(0x0C));
    QCOMPARE(replies[0].data, QByteArray::fromHex("1AF8"));

    QVERIFY(OBD2::parseMode01Reply(QByteArray::fromHex("4100BE1FA813"), replies));
    QCOMPARE(replies[0].pid, quint8(0x00));
    QCOMPARE(replies[0].data, QByteArray::fromHex("BE1FA813"));

    QVERIFY(!OBD2::parseMode01Reply(QByteArray::fromHex("410C1A"), replies)); //data cut short
}

// A request for several PIDs (01 0C 0D 05) gets them all in one reply
void TestObd2Pids::testParseSeveralPids()
{
    QList<OBD2::Reply> replies;
    QVERIFY(OBD2::parseMode01Reply(QByteArray::fromHex("410C1AF80D32057B"), replies));
    QCOMPARE(replies.size(), 3);
    QCOMPARE(replies[0].pid, quint8(0x0C));
    QCOMPARE(replies[1].pid, quint8(0x0D));
    QCOMPARE(replies[1].data, QByteArray::fromHex("32"));
    QCOMPARE(replies[2].pid, quint8(0x05));
    QCOMPARE(replies[2].data, QByteArray::fromHex("7B"));

    //an unknown PID after the first ends it: its length isn't known
    QVERIFY(OBD2::parseMode01Reply(QByteArray::fromHex("410D32FF0102"), replies));
    QCOMPARE(replies.size(), 1);
}

void TestObd2Pids::testParseUnknownPid()
{
    QList<OBD2::Reply> replies;
    QVERIFY(OBD2::parseMode01Reply(QByteArray::fromHex("41FF0102"), replies));
    QCOMPARE(replies.size(), 1);
    QCOMPARE(replies[0].pid, quint8(0xFF));
    QCOMPARE(replies[0].data, QByteArray::fromHex("0102"));
}

void TestObd2Pids::testParseNotAReply()
{
    QList<OBD2::Reply> replies;
    QVERIFY(!OBD2::parseMode01Reply(QByteArray::fromHex("7F0112"), replies)); //negative reply
    QVERIFY(!OBD2::parseMode01Reply(QByteArray::fromHex("010C"), replies));   //a request
    QVERIFY(!OBD2::parseMode01Reply(QByteArray::fromHex("41"), replies));
    QVERIFY(!OBD2::parseMode01Reply(QByteArray(), replies));
}

void TestObd2Pids::testVin()
{
    QString text;
    QByteArray payload = QByteArray::fromHex("490201") + QByteArray("1G1JC5444R7252367");
    QVERIFY(OBD2::decodeMode09Text(payload, OBD2::VIN_PID, text));
    QCOMPARE(text, QString("1G1JC5444R7252367"));

    //some ECUs pad the front with NULs
    payload = QByteArray::fromHex("490201000000") + QByteArray("WVWZZZ1KZAW000001");
    QVERIFY(OBD2::decodeMode09Text(payload, OBD2::VIN_PID, text));
    QCOMPARE(text, QString("WVWZZZ1KZAW000001"));

    QVERIFY(!OBD2::decodeMode09Text(payload, OBD2::ECU_NAME_PID, text)); //asked for another PID
    QVERIFY(!OBD2::decodeMode09Text(QByteArray::fromHex("7F0911"), OBD2::VIN_PID, text));
}

void TestObd2Pids::testEcuName()
{
    QString text;
    QByteArray name("ECM\0-EngineControl\0\0", 21);
    QVERIFY(OBD2::decodeMode09Text(QByteArray::fromHex("490A01") + name, OBD2::ECU_NAME_PID, text));
    QCOMPARE(text, QString("ECM-EngineControl"));
}

QTEST_MAIN(TestObd2Pids)
#include "test_obd2_pids.moc"
