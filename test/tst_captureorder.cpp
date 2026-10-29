#include <QtTest>
#include <QTemporaryDir>
#include "canframemodel.h"
#include <algorithm>

class TestCaptureOrder : public QObject
{
    Q_OBJECT
    QTemporaryDir settingsDirectory;
private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SavvyCANTests");
        QCoreApplication::setApplicationName("CaptureOrder");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
        QSettings().setValue("Main/MaximumFrames", 100);
        qInfo() << "CANFrame bytes" << sizeof(CANFrame);
    }
    void restoreCaptureOrder_data()
    {
        QTest::addColumn<bool>("descending");
        QTest::addColumn<bool>("filter");
        QTest::newRow("descending-all") << true << false;
        QTest::newRow("ascending-all") << false << false;
        QTest::newRow("descending-filtered") << true << true;
        QTest::newRow("ascending-filtered") << false << true;
    }
    void restoreCaptureOrder()
    {
        QFETCH(bool, descending);
        QFETCH(bool, filter);
        CANFrameModel model;
        QVector<CANFrame> capture;
        const uint ids[] = {0x300, 0x100, 0x200, 0x300, 0x100, 0x200};
        const qint64 stamps[] = {1000, 1000, 1000, 2000, 2000, 2000};
        for (int i = 0; i < 6; ++i)
        {
            CANFrame frame;
            frame.setFrameId(ids[i]);
            frame.setTimeStamp(QCanBusFrame::TimeStamp(0, stamps[i]));
            frame.setPayload(QByteArray(1, char(i)));
            capture.append(frame);
        }
        model.insertFrames(capture);
        if (filter) model.setFilterState(0x100, false);
        model.sortByColumn(int(Column::FrameId));
        model.sortByColumn(int(Column::TimeStamp));
        if (!descending) model.sortByColumn(int(Column::TimeStamp));

        QVector<CANFrame> expected;
        for (const CANFrame &frame : capture)
            if (!filter || frame.frameId() != 0x100) expected.append(frame);
        std::stable_sort(expected.begin(), expected.end(), [descending](const CANFrame &a, const CANFrame &b) {
            return descending ? a.timeStamp().microSeconds() > b.timeStamp().microSeconds()
                              : a.timeStamp().microSeconds() < b.timeStamp().microSeconds();
        });
        const QVector<CANFrame> actual = *model.getFilteredListReference();
        QCOMPARE(actual.count(), expected.count());
        for (int i = 0; i < actual.count(); ++i)
            QCOMPARE(actual[i].payload(), expected[i].payload());
        QCOMPARE(model.getListReference()->count(), capture.count());
        for (int i = 0; i < capture.count(); ++i)
            QCOMPARE(model.getListReference()->at(i).payload(), capture[i].payload());
    }
    void captureStorageUnaffectedBySort()
    {
        CANFrameModel model;
        QVector<CANFrame> capture;
        for (int i = 0; i < 4; ++i)
        {
            CANFrame frame;
            frame.setFrameId(4-i);
            frame.setTimeStamp(QCanBusFrame::TimeStamp(0, 1000));
            frame.setPayload(QByteArray(1, char(i)));
            capture.append(frame);
        }
        model.insertFrames(capture);
        model.sortByColumn(int(Column::FrameId));
        model.sortByColumn(int(Column::TimeStamp));
        for (int i = 0; i < capture.count(); ++i)
            QCOMPARE(model.getListReference()->at(i).payload(), capture[i].payload());
    }
    void distinctTimestampsRemainOrdered()
    {
        CANFrameModel model;
        QVector<CANFrame> capture;
        for (int stamp : {3000, 1000, 2000})
        {
            CANFrame frame;
            frame.setFrameId(uint(stamp));
            frame.setTimeStamp(QCanBusFrame::TimeStamp(0, stamp));
            capture.append(frame);
        }
        model.insertFrames(capture);
        model.sortByColumn(int(Column::FrameId));
        model.sortByColumn(int(Column::TimeStamp));
        QCOMPARE(model.getFilteredListReference()->at(0).timeStamp().microSeconds(), qint64(3000));
        QCOMPARE(model.getFilteredListReference()->at(2).timeStamp().microSeconds(), qint64(1000));
        model.sortByColumn(int(Column::TimeStamp));
        QCOMPARE(model.getFilteredListReference()->at(0).timeStamp().microSeconds(), qint64(1000));
        QCOMPARE(model.getFilteredListReference()->at(2).timeStamp().microSeconds(), qint64(3000));
    }
    void appendAfterIdSort()
    {
        CANFrameModel model;
        for (int i=0; i<3; ++i)
        {
            CANFrame frame;
            frame.setFrameId(0x300-i);
            frame.setTimeStamp(QCanBusFrame::TimeStamp(0, 1000));
            frame.setPayload(QByteArray(1, char(i)));
            model.addFrame(frame, false);
        }
        model.sortByColumn(int(Column::FrameId));
        CANFrame appended;
        appended.setFrameId(0x100);
        appended.setTimeStamp(QCanBusFrame::TimeStamp(0, 1000));
        appended.setPayload(QByteArray(1, char(3)));
        model.addFrame(appended, false);
        model.sortByColumn(int(Column::TimeStamp));
        for (int i=0; i<4; ++i)
            QCOMPARE(model.getFilteredListReference()->at(i).payload(), QByteArray(1,char(i)));
    }
    void clearImportAndBusFilter()
    {
        CANFrameModel model;
        QVector<CANFrame> capture;
        for (int i=0; i<6; ++i)
        {
            CANFrame frame;
            frame.setFrameId(0x300-i);
            frame.bus=i%2;
            frame.setTimeStamp(QCanBusFrame::TimeStamp(0, 1000));
            frame.setPayload(QByteArray(1,char(i)));
            capture.append(frame);
        }
        model.insertFrames(capture);
        model.sortByColumn(int(Column::FrameId));
        model.clearFrames();
        model.insertFrames(capture);
        model.setBusFilterState(1,false);
        model.sortByColumn(int(Column::FrameId));
        model.sortByColumn(int(Column::TimeStamp));
        QCOMPARE(model.rowCount(),3);
        for (int i=0; i<3; ++i)
            QCOMPARE(model.getFilteredListReference()->at(i).payload(),QByteArray(1,char(i*2)));
    }
    void overwriteStillUsesIntervals()
    {
        CANFrameModel model;
        QVector<CANFrame> capture;
        const int ids[]={0x300,0x100,0x300,0x100};
        const int stamps[]={1000,1000,1400,1100};
        for (int i=0; i<4; ++i)
        {
            CANFrame frame;
            frame.setFrameId(ids[i]);
            frame.setTimeStamp(QCanBusFrame::TimeStamp(0,stamps[i]));
            frame.setPayload(QByteArray(1,char(i)));
            capture.append(frame);
        }
        model.insertFrames(capture);
        model.setOverwriteMode(true);
        model.sortByColumn(int(Column::TimeStamp));
        QCOMPARE(model.rowCount(),2);
        QCOMPARE(model.getFilteredListReference()->at(0).frameId(),quint32(0x100));
        QCOMPARE(model.getFilteredListReference()->at(0).timedelta,uint64_t(100));
        QCOMPARE(model.getFilteredListReference()->at(0).frameCount,quint32(2));
        model.sortByColumn(int(Column::TimeStamp));
        QCOMPARE(model.getFilteredListReference()->at(0).frameId(),quint32(0x300));
        QCOMPARE(model.totalFrameCount(),4);
    }
    void evictionPreservesVisibleMembership()
    {
        CANFrameModel model;
        QVector<CANFrame> capture;
        for (int i=0; i<100; ++i)
        {
            CANFrame frame;
            frame.setFrameId(200-i);
            frame.setTimeStamp(QCanBusFrame::TimeStamp(0,1000));
            frame.setPayload(QByteArray(1,char(i)));
            capture.append(frame);
        }
        model.insertFrames(capture);
        model.sortByColumn(int(Column::FrameId));
        CANFrame next=capture.first();
        next.setPayload(QByteArray(1,char(100)));
        model.addFrames(nullptr,QVector<CANFrame>{next});
        QVERIFY(model.getListReference()->count() < 101);
        QVector<int> expected;
        for (const CANFrame &frame : *model.getFilteredListReference())
            expected.append(static_cast<unsigned char>(frame.payload()[0]));
        std::sort(expected.begin(),expected.end());
        const int rows=model.rowCount();
        model.sortByColumn(int(Column::TimeStamp));
        QCOMPARE(model.rowCount(),rows);
        for (int i=0; i<rows; ++i)
            QCOMPARE(int(static_cast<unsigned char>(model.getFilteredListReference()->at(i).payload()[0])),expected[i]);
    }
    void primaryTimestampConversionIsUnchanged()
    {
        CANFrameModel model;
        QVector<CANFrame> capture;
        for (qint64 stamp : {-1,0,1})
        {
            CANFrame frame;
            frame.setTimeStamp(QCanBusFrame::TimeStamp(0,stamp));
            capture.append(frame);
        }
        model.insertFrames(capture);
        model.sortByColumn(int(Column::TimeStamp));
        // Existing numeric column sorting uses an unsigned timestamp key.
        QCOMPARE(model.getFilteredListReference()->at(0).timeStamp().microSeconds(),qint64(0));
        QCOMPARE(model.getFilteredListReference()->at(1).timeStamp().microSeconds(),qint64(1));
        QCOMPARE(model.getFilteredListReference()->at(2).timeStamp().microSeconds(),qint64(-1));
    }
};
QTEST_MAIN(TestCaptureOrder)
#include "tst_captureorder.moc"
