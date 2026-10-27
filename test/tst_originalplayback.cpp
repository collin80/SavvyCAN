#include <QtTest>
#include "frameplaybackobject.h"

class TestOriginalPlayback : public QObject
{
    Q_OBJECT
    void setup(FramePlaybackObject &player, SequenceItem &sequence, const QList<qint64> &times,
               bool forward = true, int loops = 1)
    {
        sequence.maxLoops = loops;
        sequence.currentLoopCount = 0;
        for (int i = 0; i < times.count(); ++i)
        {
            CANFrame frame;
            frame.setFrameId(0x100 + i);
            frame.setTimeStamp(QCanBusFrame::TimeStamp(0, times[i]));
            frame.setPayload(QByteArray(1, char(i)));
            frame.bus = 2;
            sequence.data.append(frame);
            sequence.idFilters.insert(frame.frameId(), true);
        }
        player.playbackTimer = new QTimer(&player);
        player.currentSeqItem = &sequence;
        player.currentPosition = forward ? 0 : times.count() - 1;
        player.playbackForward = forward;
        player.playbackActive = true;
        player.whichBusSend = -2;
        player.numBuses = 3;
    }
    QList<int> sent(const FramePlaybackObject &player)
    {
        QList<int> result;
        for (const auto &frame : player.sendingBuffer) result.append(uchar(frame.payload()[0]));
        return result;
    }
private slots:
    void firstFrameDue()
    {
        SequenceItem sequence; FramePlaybackObject player;
        setup(player, sequence, {0, 1000000, 2000000});
        player.playbackLastTimeStamp = 0;
        player.processOriginalTiming(0);
        QCOMPARE(sent(player), QList<int>({0}));
        QCOMPARE(player.currentPosition, 1);
    }
    void lastFrameNotEarly()
    {
        SequenceItem sequence; FramePlaybackObject player;
        setup(player, sequence, {0, 1000000, 2000000});
        player.currentPosition = 2;
        player.playbackLastTimeStamp = 1000000;
        player.processOriginalTiming(1);
        QVERIFY(player.sendingBuffer.isEmpty());
        QCOMPARE(sequence.currentLoopCount, 0);
    }
    void reverseFirstDue()
    {
        SequenceItem sequence; FramePlaybackObject player;
        setup(player, sequence, {0, 1000000, 2000000}, false);
        player.playbackLastTimeStamp = 2000000;
        player.processOriginalTiming(1);
        QCOMPARE(sent(player), QList<int>({2}));
        QCOMPARE(player.currentPosition, 1);
    }
    void forwardFutureControl()
    {
        SequenceItem sequence; FramePlaybackObject player;
        setup(player, sequence, {0, 1000000, 2000000});
        player.currentPosition = 1;
        player.playbackLastTimeStamp = 0;
        player.processOriginalTiming(1);
        QVERIFY(player.sendingBuffer.isEmpty());
    }
    void reverseFutureControl()
    {
        SequenceItem sequence; FramePlaybackObject player;
        setup(player, sequence, {0, 1000000, 2000000}, false);
        player.currentPosition = 1;
        player.playbackLastTimeStamp = 2000000;
        player.processOriginalTiming(1);
        QVERIFY(player.sendingBuffer.isEmpty());
    }
    void catchUp_data()
    {
        QTest::addColumn<bool>("forward");
        QTest::newRow("forward") << true;
        QTest::newRow("reverse") << false;
    }
    void catchUp()
    {
        QFETCH(bool, forward);
        SequenceItem sequence; FramePlaybackObject player;
        setup(player, sequence, {0, 10, 20, 2000}, forward);
        QSignalSpy end(&player, &FramePlaybackObject::EndOfFrameCache);
        player.playbackLastTimeStamp = forward ? 0 : 2000;
        player.processOriginalTiming(forward ? 20 : 1980);
        QCOMPARE(sent(player), forward ? QList<int>({0,1,2}) : QList<int>({3,2}));
        QCOMPARE(end.count(), 0);
        player.sendingBuffer.clear();
        player.processOriginalTiming(forward ? 1980 : 20);
        QCOMPARE(sent(player), forward ? QList<int>({3}) : QList<int>({1,0}));
        QCOMPARE(end.count(), 1);
        QVERIFY(!player.playbackActive);
        player.sendingBuffer.clear();
        player.processOriginalTiming(1000000);
        QVERIFY(player.sendingBuffer.isEmpty());
        QCOMPARE(end.count(), 1);
    }
    void equalTimestamps_data()
    {
        QTest::addColumn<bool>("forward");
        QTest::addColumn<int>("loops");
        QTest::newRow("forward-once") << true << 1;
        QTest::newRow("reverse-once") << false << 1;
        QTest::newRow("forward-twice") << true << 2;
        QTest::newRow("reverse-twice") << false << 2;
    }
    void equalTimestamps()
    {
        QFETCH(bool, forward); QFETCH(int, loops);
        SequenceItem sequence; FramePlaybackObject player;
        setup(player, sequence, {0,0,0}, forward, loops);
        QSignalSpy end(&player, &FramePlaybackObject::EndOfFrameCache);
        player.playbackLastTimeStamp = 0;
        for (int pass = 1; pass <= loops; ++pass)
        {
            player.sendingBuffer.clear();
            player.processOriginalTiming(0);
            QCOMPARE(sent(player), forward ? QList<int>({0,1,2}) : QList<int>({2,1,0}));
            QCOMPARE(sequence.currentLoopCount, pass);
            QCOMPARE(player.playbackActive, pass < loops);
            QCOMPARE(end.count(), pass == loops ? 1 : 0);
        }
        player.sendingBuffer.clear();
        player.processOriginalTiming(0);
        QVERIFY(player.sendingBuffer.isEmpty());
        QCOMPARE(end.count(), 1);
    }
    void oneFrame_data()
    {
        QTest::addColumn<bool>("forward");
        QTest::newRow("forward") << true;
        QTest::newRow("reverse") << false;
    }
    void oneFrame()
    {
        QFETCH(bool, forward);
        SequenceItem sequence; FramePlaybackObject player;
        setup(player, sequence, {5000}, forward);
        QSignalSpy end(&player, &FramePlaybackObject::EndOfFrameCache);
        player.playbackLastTimeStamp = forward ? 4000 : 6000;
        player.processOriginalTiming(999);
        QVERIFY(player.sendingBuffer.isEmpty());
        player.processOriginalTiming(1);
        QCOMPARE(sent(player), QList<int>({0}));
        QCOMPARE(end.count(), 1);
        QVERIFY(!player.playbackActive);
    }
    void repeatClock_data() { catchUp_data(); }
    void repeatClock()
    {
        QFETCH(bool, forward);
        SequenceItem sequence; FramePlaybackObject player;
        setup(player, sequence, {10000, 20000}, forward, 2);
        player.playbackLastTimeStamp = forward ? 10000 : 20000;
        player.processOriginalTiming(1000000);
        QCOMPARE(sent(player), forward ? QList<int>({0,1}) : QList<int>({1,0}));
        QCOMPARE(sequence.currentLoopCount, 1);
        QCOMPARE(player.playbackLastTimeStamp, quint64(forward ? 10000 : 20000));
        player.sendingBuffer.clear();
        player.processOriginalTiming(0);
        QCOMPARE(sent(player), forward ? QList<int>({0}) : QList<int>({1}));
        player.sendingBuffer.clear();
        player.processOriginalTiming(10000);
        QCOMPARE(sent(player), forward ? QList<int>({1}) : QList<int>({0}));
        QVERIFY(!player.playbackActive);
        QCOMPARE(sequence.currentLoopCount, 2);
    }
    void filtersAndBuses_data()
    {
        QTest::addColumn<int>("bus");
        QTest::newRow("file") << -2;
        QTest::newRow("all") << -1;
        QTest::newRow("override") << 1;
    }
    void filtersAndBuses()
    {
        QFETCH(int, bus);
        SequenceItem sequence; FramePlaybackObject player;
        setup(player, sequence, {0,0,0});
        sequence.idFilters[0x101] = false;
        player.whichBusSend = bus;
        player.playbackLastTimeStamp = 0;
        player.processOriginalTiming(0);
        QCOMPARE(sent(player), bus == -1 ? QList<int>({0,0,0,2,2,2}) : QList<int>({0,2}));
        for (int i = 0; i < player.sendingBuffer.size(); ++i)
            QCOMPARE(player.sendingBuffer[i].bus, uint32_t(bus == -1 ? i % 3 : bus == -2 ? 2 : bus));
        for (const auto &frame : sequence.data) QCOMPARE(frame.bus, uint32_t(2));
        QVERIFY(!player.playbackActive);
    }
    void endCallbackReplacesCache()
    {
        SequenceItem sequence; SequenceItem replacement;
        FramePlaybackObject player;
        setup(player, sequence, {1234});
        replacement.maxLoops = 1;
        replacement.currentLoopCount = 0;
        player.playbackLastTimeStamp = 1234;
        int endCount = 0;
        connect(&player, &FramePlaybackObject::EndOfFrameCache, &player, [&]() {
            ++endCount;
            QVector<CANFrame>().swap(sequence.data);
            player.setSequenceObject(&replacement);
        }, Qt::DirectConnection);
        player.processOriginalTiming(0);
        QCOMPARE(sent(player), QList<int>({0}));
        QCOMPARE(player.sendingBuffer.first().timeStamp().microSeconds(), qint64(1234));
        QCOMPARE(endCount, 1);
        QVERIFY(!player.playbackActive);
        QCOMPARE(player.currentSeqItem, &replacement);
        player.sendingBuffer.clear();
        player.processOriginalTiming(10000);
        QVERIFY(player.sendingBuffer.isEmpty());
        QCOMPARE(endCount, 1);
    }
    void pausedAndEmpty()
    {
        SequenceItem sequence; FramePlaybackObject player;
        setup(player, sequence, {0});
        player.playbackLastTimeStamp = 0;
        player.playbackActive = false;
        player.processOriginalTiming(10000);
        QVERIFY(player.sendingBuffer.isEmpty());
        QCOMPARE(sequence.currentLoopCount, 0);
        player.playbackActive = true;
        sequence.data.clear();
        player.processOriginalTiming(10000);
        QVERIFY(player.sendingBuffer.isEmpty());
        QVERIFY(!player.playbackActive);
        player.currentSeqItem = nullptr;
        player.playbackActive = true;
        player.processOriginalTiming(10000);
        QVERIFY(!player.playbackActive);
    }

};
QTEST_MAIN(TestOriginalPlayback)
#include "tst_originalplayback.moc"
