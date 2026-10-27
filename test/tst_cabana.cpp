#include <QtTest>
#include <QTemporaryDir>
#include "framefileio.h"

class TestCabana : public QObject
{
    Q_OBJECT
    QString writeLog(QTemporaryDir &directory, const QByteArray &row)
    {
        QString path = directory.filePath("cabana.csv");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return QString();
        QByteArray contents = "time,addr,bus,data\n" + row + "\n";
        if (file.write(contents) != contents.size()) return QString();
        return path;
    }
private slots:
    void identifiers_data()
    {
        QTest::addColumn<QByteArray>("idText");
        QTest::addColumn<quint32>("expectedId");
        QTest::newRow("decimal") << QByteArray("521") << quint32(521);
        QTest::newRow("decimal-leading-zero") << QByteArray("0521") << quint32(521);
        QTest::newRow("hex-lower") << QByteArray("0x209") << quint32(0x209);
        QTest::newRow("hex-upper") << QByteArray("0X20A") << quint32(0x20A);
        QTest::newRow("extended") << QByteArray("0x1FFFFFFF") << quint32(0x1FFFFFFF);
        QTest::newRow("zero") << QByteArray("0") << quint32(0);
    }
    void identifiers()
    {
        QFETCH(QByteArray, idText);
        QFETCH(quint32, expectedId);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString path = writeLog(directory, "0.000," + idText + ",2,3A5B00000000A000");
        QVERIFY(!path.isEmpty());
        QVector<CANFrame> frames;
        QVERIFY(FrameFileIO::loadCabanaFile(path, &frames));
        QCOMPARE(frames.size(), 1);
        QCOMPARE(frames.at(0).frameId(), expectedId);
        QCOMPARE(frames.at(0).hasExtendedFrameFormat(), expectedId > 0x7FF);
        QCOMPARE(frames.at(0).payload(), QByteArray::fromHex("3A5B00000000A000"));
        QCOMPARE(frames.at(0).bus, 2);
        QCOMPARE(frames.at(0).timeStamp().microSeconds(), qint64(0));
        QVERIFY(frames.at(0).isReceived);
    }
    void recognizesIdentifiers_data() { identifiers_data(); }
    void recognizesIdentifiers()
    {
        QFETCH(QByteArray, idText);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString path = writeLog(directory, "0.000," + idText + ",2,AA");
        QVERIFY(!path.isEmpty());
        QVERIFY(FrameFileIO::isCabanaFile(path));
    }
    void prefixedPayloads_data()
    {
        QTest::addColumn<QByteArray>("encoded");
        QTest::addColumn<QByteArray>("expected");
        QTest::newRow("lower") << QByteArray("0x3A5B00000000A000")
            << QByteArray::fromHex("3A5B00000000A000");
        QTest::newRow("upper") << QByteArray("0X955B00000000A000")
            << QByteArray::fromHex("955B00000000A000");
        QTest::newRow("empty") << QByteArray("0x") << QByteArray();
    }
    void prefixedPayloads()
    {
        QFETCH(QByteArray, encoded);
        QFETCH(QByteArray, expected);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString path = writeLog(directory, "0.000,521,2," + encoded);
        QVERIFY(!path.isEmpty());
        QVector<CANFrame> frames;
        QVERIFY(FrameFileIO::loadCabanaFile(path, &frames));
        QCOMPARE(frames.size(), 1);
        QCOMPARE(frames.at(0).payload(), expected);
    }
    void malformedRows_data()
    {
        QTest::addColumn<QByteArray>("row");
        QTest::newRow("missing-payload") << QByteArray("0.000,521,2");
        QTest::newRow("odd-payload") << QByteArray("0.000,521,2,ABC");
        QTest::newRow("nonhex-payload") << QByteArray("0.000,521,2,0xGG");
        QTest::newRow("oversized-payload") << QByteArray("0.000,521,2,001122334455667788");
        QTest::newRow("nonhex-id") << QByteArray("0.000,0xGG,2,AA");
        QTest::newRow("out-of-range-id") << QByteArray("0.000,536870912,2,AA");
        QTest::newRow("negative-id") << QByteArray("0.000,-1,2,AA");
    }
    void malformedRows()
    {
        QFETCH(QByteArray, row);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString path = writeLog(directory, row);
        QVERIFY(!path.isEmpty());
        QVERIFY(!FrameFileIO::isCabanaFile(path));
        QVector<CANFrame> frames;
        QVERIFY(!FrameFileIO::loadCabanaFile(path, &frames));
        QVERIFY(frames.isEmpty());
    }
    void reportedLogAutoDetects()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString path = writeLog(directory,
            "0.000,0x209,2,0x3A5B00000000A000\n"
            "0.001,0x20a,2,0x955B00000000A000\n"
            "0.002,0x29b,1,0xD3030000000000FF\n"
            "0.003,0x29c,1,0x3103000000000000");
        QVERIFY(!path.isEmpty());
        QVERIFY(FrameFileIO::isCabanaFile(path));
        QVector<CANFrame> frames;
        QVERIFY(FrameFileIO::autoDetectLoadFile(path, &frames));
        QCOMPARE(frames.size(), 4);
        const quint32 expectedIds[] = {0x209, 0x20A, 0x29B, 0x29C};
        const char *payloads[] = {"3A5B00000000A000", "955B00000000A000",
            "D3030000000000FF", "3103000000000000"};
        for (int i = 0; i < frames.size(); ++i)
        {
            QCOMPARE(frames.at(i).frameId(), expectedIds[i]);
            QCOMPARE(frames.at(i).payload(), QByteArray::fromHex(payloads[i]));
            QCOMPARE(frames.at(i).timeStamp().microSeconds(), qint64(i * 1000));
            QCOMPARE(frames.at(i).bus, i < 2 ? 2 : 1);
        }
    }
};

QTEST_MAIN(TestCabana)
#include "tst_cabana.moc"
