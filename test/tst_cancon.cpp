#include <QtTest>
#include <QVector>

#include "tst_cancon.h"
#include "canconnection.h"
#include "canconfactory.h"


#define QVERIFYB(statement) \
do {\
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__))\
        return false;\
} while (0)

#define QCOMPAREB(actual, expected) \
do {\
    if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))\
        return false;\
} while (0)

Q_DECLARE_METATYPE(QVector<CANFlt>);



TestCanCon::TestCanCon(CANCon::type pType, QString pPortName, int pNbBus):
    mType(pType),
    mPortName(pPortName),
    mNbBus(pNbBus){}

void TestCanCon::create()
{
    CANConnection* conn_p;
    QVERIFY(pCreate(conn_p));
    /* try to get an element from the queue */
    QVERIFY(conn_p->getQueue().get());

    if(conn_p)
        delete conn_p;
}

void TestCanCon::connectToDevice()
{
    CANConnection* conn_p;
    QVERIFY(pCreate(conn_p));

    QSignalSpy spy(conn_p, SIGNAL(status(CANCon::status)));

    /* start connection */
    conn_p->start();

    /* wait for a signal */
    for(int i=0 ; (spy.count() != 1) && (i < 10) ; i++)
        QTest::qWait(500);


    QCOMPARE(spy.count(), 1); // make sure the signal was emitted exactly one time
    QList<QVariant> arguments = spy.takeFirst(); // take the first signal

    QVERIFY(arguments.at(0).toInt() == CANCon::CONNECTED); // verify the first argument

    /* stop connection */
    conn_p->stop();
    delete conn_p;
}

void TestCanCon::recvFrames()
{
    CANConnection* conn_p;
    QVERIFY(pCreate(conn_p));

    /* start connection */
    conn_p->start();

    /* configure */
    QVERIFY(pConfig(conn_p));

    LFQueue<CANFrame>& queue = conn_p->getQueue();

    /* wait for frames to arrive */
    QTest::qWait(1000);

    int i;
    for(i=0 ; queue.peek() && i<1000 ; i++)
    {
        CANFrame* canf_p = queue.peek();
        QVERIFY(pValidateFrame(conn_p, canf_p));

        queue.dequeue();
    }

    QVERIFY(i>0);

    /* stop connection */
    conn_p->stop();
    delete conn_p;
}


void TestCanCon::suspend()
{
    CANConnection* conn_p;
    QVERIFY(pCreate(conn_p));

    /* start connection */
    conn_p->start();

    /* configure */
    QVERIFY(pConfig(conn_p));

    LFQueue<CANFrame>& queue = conn_p->getQueue();

    /* wait for frames to arrive */
    QTest::qWait(1000);

    CANFrame* canf_p = queue.peek();
    QVERIFY(pValidateFrame(conn_p, canf_p));

    conn_p->suspend(true);

    /* the queue should be flushed */
    canf_p = queue.peek();
    QVERIFY(!canf_p);

    /* restart capture */
    conn_p->suspend(false);

    /* wait for frames to arrive */
    QTest::qWait(1000);

    /* get a frame */
    canf_p = queue.peek();
    QVERIFY(pValidateFrame(conn_p, canf_p));

    /* stop connection */
    conn_p->stop();
    delete conn_p;
}


void TestCanCon::filter_data()
{
    CANConnection* conn_p;
    QVERIFY(pCreate(conn_p));

    /* start connection */
    conn_p->start();

    /* configure */
    QVERIFY(pConfig(conn_p));

    LFQueue<CANFrame>& queue = conn_p->getQueue();

    /* wait for frames to arrive */
    QTest::qWait(1000);

    /* find 3 different ids */
    QVector<quint32> ids;

    while( queue.peek() && ids.count()!=3 )
    {
        CANFrame* canf_p = queue.peek();
        QVERIFY(pValidateFrame(conn_p, canf_p));

        if(!ids.contains(canf_p->ID))
            ids.append(canf_p->ID);

        queue.dequeue();
    }

    QCOMPARE(ids.count(), 3);

    /* stop connection */
    conn_p->stop();
    delete conn_p;

    /* prepare test vector */

    QTest::addColumn<QVector<CANFlt>>("filters");
    QTest::addColumn<bool>("filterOut");
    QTest::addColumn<bool>("signalReceived");
    QTest::addColumn<QVector<quint32>>("filtered");

    QVector<CANFlt> filters;
    QVector<quint32> filteredIds;

    /* one filter no signal*/
    filters.clear();
    filteredIds.clear();
    filters.append({ids[0], 0xFFFF, false});
    filteredIds.append(ids[0]);
    QTest::newRow("1filternosignal")        << filters << false << false << filteredIds;

    /* one filter & signal*/
    filters.clear();
    filteredIds.clear();
    filters.append({ids[0], 0xFFFF, true});
    filteredIds.append(ids[0]);
    QTest::newRow("1filtersignal")          << filters << false << true << filteredIds;

    /* 3 filters */
    filters.clear();
    filteredIds.clear();
    foreach(quint32 id, ids) {
        filters.append({id, 0xFFFF, false});
        filteredIds.append(id);
    }
    QTest::newRow("3filters")               << filters << false << false << filteredIds;
}


void TestCanCon::filter()
{
    QFETCH(QVector<CANFlt>, filters);
    QFETCH(bool, filterOut);
    QFETCH(bool, signalReceived);
    QFETCH(QVector<quint32>, filtered);

    CANConnection* conn_p;
    QVERIFY(pCreate(conn_p));

    /* start connection */
    conn_p->start();

    /* set filters */
    for(int i=0 ; i<conn_p->getNumBuses() ; i++)
        QVERIFY(conn_p->setFilters(i, filters, filterOut));

    /* spy signal */
    QSignalSpy spy(conn_p, SIGNAL(notify()));

    /* configure */
    QVERIFY(pConfig(conn_p));

    LFQueue<CANFrame>& queue = conn_p->getQueue();

    /* wait for frames to arrive */
    QTest::qWait(1000);

    if(signalReceived)
        QVERIFY(spy.count()>0);

    int i;
    for(i=0 ; queue.peek() && i<1000 ; i++)
    {
        CANFrame* canf_p = queue.peek();
        QVERIFY(pValidateFrame(conn_p, canf_p));

        if(filterOut)
            QVERIFY(filtered.contains(canf_p->ID));

        queue.dequeue();
    }

    QVERIFY(i>0);

    conn_p->stop();
    delete conn_p;
}


void TestCanCon::write()
{
    CANConnection* conn_p;
    QVERIFY(pCreate(conn_p));

    /* start connection */
    conn_p->start();

    /* configure */
    QVERIFY(pConfig(conn_p));

    QList<CANFrame> frames;
    /* build frames */
    CANFrame frame;
    frame.bus       = 0;
    frame.ID        = 0x1DE;
    frame.data[0]   = 0xDE;
    frame.data[1]   = 0xAD;
    frame.data[2]   = 0xC0;
    frame.data[3]   = 0xDE;
    frame.len       = 4;

    frames.append(frame);

    frame.data[2]   = 0xBE;
    frame.data[3]   = 0xEF;
    frames.append(frame);

    frame.data[2]   = 0xDE;
    frame.data[3]   = 0xAD;


    /* bad frame length */
    unsigned int oldVal = frame.len;
    frame.len = 9;
    QCOMPARE(conn_p->sendFrame(frame), false);
    frame.len       = oldVal;

    /* bad bus id */
    oldVal = frame.bus;
    frame.bus = 48;
    QCOMPARE(conn_p->sendFrame(frame), false);
    frame.bus       = oldVal;

    qDebug() << "Sending DE AD DE AD";
    /* send */
    QVERIFY(conn_p->sendFrame(frame));

    /* leave some time for the frame to be sent */
    QTest::qWait(1000);

    qDebug() << "Sending DE AD C0 DE";
    qDebug() << "Sending DE AD BE EF";
    /* send */
    QVERIFY(conn_p->sendFrames(frames));

    /* leave some time for the frame to be sent */
    QTest::qWait(1000);

    conn_p->stop();
    delete conn_p;
}


/*********************************************************/

bool TestCanCon::pCreate(CANConnection*& pConn_p)
{
    pConn_p = CanConFactory::create(mType, mPortName);
    QVERIFYB(pConn_p);

    QCOMPAREB(pConn_p->getPort(),     mPortName);
    QCOMPAREB(pConn_p->getNumBuses(), mNbBus);
    QCOMPAREB(pConn_p->getType(),     mType);
    QCOMPAREB(pConn_p->getStatus(),   CANCon::NOT_CONNECTED);

    return true;
}

bool TestCanCon::pConfig(CANConnection* pConn_p)
{
    /*configure buses */
    CANBus bus;
    CANBus retBus;
    for(int i=0 ; i<pConn_p->getNumBuses() ; i++)
    {
        /* TODO: fix configuration */
        bus.active = true;
        pConn_p->setBusSettings(i, bus);
        QVERIFYB(pConn_p->getBusSettings(i, retBus));
        QCOMPAREB(bus, retBus);
    }

    return true;
}

bool TestCanCon::pValidateFrame(CANConnection* pConn_p, CANFrame* pCan_p)
{
    QVERIFYB( pCan_p );
    QVERIFYB( (0<=pCan_p->bus) && (pCan_p->bus <= pConn_p->getNumBuses()) );
    QVERIFYB( pCan_p->isReceived);
    QVERIFYB( pCan_p->len<=8 );
    QVERIFYB( (0<=pCan_p->ID) && (pCan_p->ID<2048) );

    return true;
}
