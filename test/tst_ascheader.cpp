#include <QtTest>
#include <QTemporaryDir>
#include "framefileio.h"

class TestASCHeader : public QObject
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
    void records_data()
    {
        QTest::addColumn<int>("recordCount");
        QTest::addColumn<bool>("separator");
        QTest::newRow("single") << 1 << false;
        QTest::newRow("multiple") << 2 << false;
        QTest::newRow("empty") << 0 << false;
        QTest::newRow("blank-separator-control") << 1 << true;
    }
    void records()
    {
        QFETCH(int, recordCount);
        QFETCH(bool, separator);
        QByteArray text = header();
        if (separator) text += "\n";
        for (int i = 0; i < recordCount; ++i)
            text += "0.125 1 " + QByteArray::number(0x132 + i, 16) + " Rx d 1 AA\n";
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString path = directory.filePath("input.asc");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(text), qint64(text.size()));
        file.close();
        QVector<CANFrame> frames;
        QVERIFY(FrameFileIO::loadCanalyzerASC(path, &frames));
        QCOMPARE(frames.size(), recordCount);
        for (int i = 0; i < frames.size(); ++i)
        {
            QCOMPARE(frames.at(i).frameId(), quint32(0x132 + i));
            QCOMPARE(frames.at(i).payload(), QByteArray::fromHex("AA"));
            QCOMPARE(frames.at(i).timeStamp().microSeconds(), qint64(125000));
        }
    }
    void writerOutputKeepsFirstRecord()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CANFrame frame;
        frame.setFrameId(0x132);
        frame.setFrameType(QCanBusFrame::DataFrame);
        frame.setTimeStamp(QCanBusFrame::TimeStamp(0, 125000));
        frame.setPayload(QByteArray::fromHex("AA"));
        frame.bus = 0;
        frame.isReceived = true;
        QVector<CANFrame> original{frame};
        QString path = directory.filePath("saved.asc");
        QVERIFY(FrameFileIO::saveCanalyzerASC(path, &original));
        QVector<CANFrame> loaded;
        QVERIFY(FrameFileIO::loadCanalyzerASC(path, &loaded));
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.at(0).frameId(), frame.frameId());
        QCOMPARE(loaded.at(0).payload(), frame.payload());
    }
};
QTEST_MAIN(TestASCHeader)
#include "tst_ascheader.moc"
