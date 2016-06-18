#include <QtTest/QtTest>
#include <QtConcurrent/qtconcurrentrun.h>

#include "lfqueue.h"

class TestLFQueue: public QObject
{
    Q_OBJECT
private:

private slots:
    void setSize_data();
    void setSize();
    void exchange_data();
    void exchange();
};


void TestLFQueue::setSize_data()
{
    QTest::addColumn<int>("size");
    QTest::addColumn<bool>("result");

    QTest::newRow("-1")     << -1       << false;
    QTest::newRow("0")      <<  0       << true;
    QTest::newRow("10")     << 10       << true;
    QTest::newRow("2000")   << 20000    << true;

}


void TestLFQueue::setSize()
{
    QFETCH(int, size);
    QFETCH(bool, result);

    LFQueue<void*> queue;
    QCOMPARE(queue.setSize(size), result);
}


void readerThread(LFQueue<int>* pQueue_p, int pSize, bool pSleep) {
    int* val_p;

    for(int i=0; i<pSize ; i++) {
        while(! (val_p = pQueue_p->peek()) );
        QVERIFY(val_p);

        QCOMPARE(*val_p, i);
        pQueue_p->dequeue();

        if(pSleep)
            QThread::msleep(1);
    }
}


void TestLFQueue::exchange_data()
{
    QTest::addColumn<int>("size");
    QTest::addColumn<bool>("writerSleep");
    QTest::addColumn<bool>("readerSleep");

    QTest::newRow("nosleep")     << 1000 << false << false;
    QTest::newRow("readersleep") << 1000 << false << true;
    QTest::newRow("writersleep") << 1000 << true  << false;
}


void TestLFQueue::exchange()
{
    LFQueue<int> queue;
    QFETCH(int, size);
    QFETCH(bool, writerSleep);
    QFETCH(bool, readerSleep);

    int* val_p;
    QCOMPARE(queue.setSize(2), true);

    QFuture<void> thread = QtConcurrent::run(readerThread, &queue, size, readerSleep);

    for(int i=0; i<size ; i++) {
        while(! (val_p = queue.get()) );
        QVERIFY(val_p);

        *val_p = i;
        queue.queue();

        if(writerSleep)
            QThread::msleep(1);
    }

    thread.waitForFinished();
}


QTEST_MAIN(TestLFQueue)
#include "tst_lfqueue.moc"
