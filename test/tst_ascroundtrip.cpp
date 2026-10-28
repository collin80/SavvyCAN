#include <QtTest>
#include <QTemporaryDir>
#include <limits>
#include "framefileio.h"

class TestASCRoundTrip : public QObject
{
    Q_OBJECT

    QByteArray header() const
    {
        return "date Mon Jan 01 00:00:00.000 am 2024\n"
            "base hex  timestamps absolute\n"
            "no internal event logging\n"
            "// version 11.0.0\n";
    }

private slots:
    void channelNumbers_data()
    {
        QTest::addColumn<int>("channel");
        QTest::addColumn<int>("expectedBus");
        QTest::addColumn<bool>("fdSyntax");
        for (bool fdSyntax : {false, true})
        {
            QTest::newRow(fdSyntax ? "fd-first" : "classic-first") << 1 << 0 << fdSyntax;
            QTest::newRow(fdSyntax ? "fd-second" : "classic-second") << 2 << 1 << fdSyntax;
            QTest::newRow(fdSyntax ? "fd-legacy-zero" : "classic-legacy-zero") << 0 << 0 << fdSyntax;
            QTest::newRow(fdSyntax ? "fd-lowest" : "classic-lowest")
                << std::numeric_limits<int>::min() << 0 << fdSyntax;
            QTest::newRow(fdSyntax ? "fd-highest" : "classic-highest")
                << std::numeric_limits<int>::max()
                << std::numeric_limits<int>::max() - 1 << fdSyntax;
        }
    }

    void channelNumbers()
    {
        QFETCH(int, channel);
        QFETCH(int, expectedBus);
        QFETCH(bool, fdSyntax);
        QByteArray number = QByteArray::number(channel);
        QByteArray row = fdSyntax
            ? "0.0721 CANFD " + number + " Rx 132 0 0 8 8 64 97 F8 FF 27 40 11 22\n"
            : "0.0721 " + number + " 132 Rx d 6 64 97 F8 FF 27 40\n";
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString path = directory.filePath("input.asc");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(header() + row), qint64(header().size() + row.size()));
        file.close();

        QVector<CANFrame> frames;
        QVERIFY(FrameFileIO::loadCanalyzerASC(path, &frames));
        QCOMPARE(frames.size(), 1);
        QCOMPARE(frames.at(0).bus, expectedBus);
        QCOMPARE(frames.at(0).frameId(), quint32(0x132));
        QCOMPARE(frames.at(0).timeStamp().microSeconds(), qint64(72100));
        QVERIFY(frames.at(0).isReceived);
        QCOMPARE(frames.at(0).payload(), QByteArray::fromHex(
            fdSyntax ? "6497F8FF27401122" : "6497F8FF2740"));
    }

    void savedChannelsRemainStable()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVector<CANFrame> frames;
        for (int bus : {0, 1})
        {
            CANFrame frame;
            frame.setFrameId(0x132 + bus);
            frame.setFrameType(QCanBusFrame::DataFrame);
            frame.setTimeStamp(QCanBusFrame::TimeStamp(0, 100000 + bus * 25000));
            frame.setPayload(QByteArray::fromHex("010203"));
            frame.bus = bus;
            frame.isReceived = (bus == 0);
            frames.append(frame);
        }
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            QString path = directory.filePath(QString("roundtrip-%1.asc").arg(cycle));
            QVERIFY(FrameFileIO::saveCanalyzerASC(path, &frames));
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QByteArray text = file.readAll();
            QVERIFY(text.contains(" 1  "));
            QVERIFY(text.contains(" 2  "));

            QVector<CANFrame> loaded;
            QVERIFY(FrameFileIO::loadCanalyzerASC(path, &loaded));
            QCOMPARE(loaded.size(), 2);
            for (int i = 0; i < loaded.size(); ++i)
            {
                QCOMPARE(loaded.at(i).bus, i);
                QCOMPARE(loaded.at(i).frameId(), frames.at(i).frameId());
                QCOMPARE(loaded.at(i).payload(), frames.at(i).payload());
                QCOMPARE(loaded.at(i).isReceived, frames.at(i).isReceived);
            }
            QCOMPARE(loaded.at(1).timeStamp().microSeconds()
                - loaded.at(0).timeStamp().microSeconds(), qint64(25000));
            frames = loaded;
        }
    }
};

QTEST_MAIN(TestASCRoundTrip)
#include "tst_ascroundtrip.moc"
