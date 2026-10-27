#include <QtTest>
#include <QTemporaryDir>
#include "framefileio.h"

class TestNativeCSV : public QObject
{
    Q_OBJECT

    QString writeCSV(QTemporaryDir &directory, const QByteArray &contents)
    {
        QString path = directory.filePath("frames.csv");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return QString();
        if (file.write(contents) != contents.size()) return QString();
        return path;
    }

private slots:
    void timestamps_data()
    {
        QTest::addColumn<QByteArray>("timestamp");
        QTest::addColumn<qint64>("expected");
        QTest::addColumn<bool>("versionTwo");
        const QList<QByteArray> times = {"0", "1", "973", "999", "1000",
            "2009", "2988", "1790000000123456", "-1000"};
        for (bool versionTwo : {false, true})
        {
            for (const QByteArray &time : times)
            {
                QByteArray name = (versionTwo ? "v2-" : "v1-") + time;
                QTest::newRow(name.constData()) << time << time.toLongLong() << versionTwo;
            }
        }
    }

    void timestamps()
    {
        QFETCH(QByteArray, timestamp);
        QFETCH(qint64, expected);
        QFETCH(bool, versionTwo);
        QByteArray header = versionTwo
            ? "Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n"
            : "Time Stamp,ID,Extended,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n";
        QByteArray row = timestamp + ",0000013B,false,"
            + (versionTwo ? "Tx," : "") + "2,8,00,00,08,00,00,00,00,4C\n";
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString path = writeCSV(directory, header + row);
        QVERIFY(!path.isEmpty());

        // The same log must retain the same time on every import.
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            QVector<CANFrame> frames;
            QVERIFY(FrameFileIO::loadNativeCSVFile(path, &frames));
            QCOMPARE(frames.size(), 1);
            QCOMPARE(frames.at(0).timeStamp().microSeconds(), expected);
            QCOMPARE(frames.at(0).frameId(), quint32(0x13B));
            QCOMPARE(frames.at(0).bus, 2);
            QCOMPARE(frames.at(0).isReceived, !versionTwo);
            QVERIFY(!frames.at(0).hasExtendedFrameFormat());
            QCOMPARE(frames.at(0).payload(), QByteArray::fromHex("000008000000004C"));
        }
    }

    void reportedMixedTimestamps()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString path = writeCSV(directory,
            "Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n"
            "973,0000013B,false,Rx,0,8,00,00,08,00,00,00,00,4C\n"
            "2009,00000030,false,Rx,0,8,00,00,00,00,17,00,01,50\n"
            "2988,00000090,false,Rx,0,8,02,02,02,02,02,00,00,A2\n");
        QVERIFY(!path.isEmpty());
        QVector<CANFrame> frames;
        QVERIFY(FrameFileIO::loadNativeCSVFile(path, &frames));
        QCOMPARE(frames.size(), 3);
        QCOMPARE(frames.at(0).timeStamp().microSeconds(), qint64(973));
        QCOMPARE(frames.at(1).timeStamp().microSeconds(), qint64(2009));
        QCOMPARE(frames.at(2).timeStamp().microSeconds(), qint64(2988));
    }

    void missingTimestampsKeepLegacySpacing()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString path = writeCSV(directory,
            "Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n"
            ",0000013B,false,Rx,0,1,AA\n"
            ",0000013C,false,Rx,0,1,BB\n");
        QVERIFY(!path.isEmpty());
        QVector<CANFrame> frames;
        QVERIFY(FrameFileIO::loadNativeCSVFile(path, &frames));
        QCOMPARE(frames.size(), 2);
        QCOMPARE(frames.at(1).timeStamp().microSeconds()
            - frames.at(0).timeStamp().microSeconds(), qint64(5));
    }
};

QTEST_MAIN(TestNativeCSV)
#include "tst_nativecsv.moc"
