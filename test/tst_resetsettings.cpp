#include <QtTest>
#include <QTableView>
#include <QTemporaryDir>
#include "connections/connectionwindow.h"
#include "connections/canconnectionmodel.h"
#include "connections/canconmanager.h"

class ResetOrigin : public CANConnection
{
public:
    QAtomicInt started;
    CANBus startupSettings;
    explicit ResetOrigin(bool threaded=true) : CANConnection("no-such-test-serial-device", "LAWICEL", CANCon::LAWICEL,
        921600,125000,true,4000000,1,16,threaded), started(0)
    {
        mBusData[0].mBus.setListenOnly(true);
        mBusData[0].mBus.setSingleWire(true);
        mBusData[0].mBus.setActive(true);
    }
    ~ResetOrigin() override { stop(); }
    CANBus expected() const { return mBusData[0].mBus; }
protected:
    void piStarted() override { startupSettings=mBusData[0].mBus; started.storeRelease(1); }
    void piStop() override {}
    void piSetBusSettings(int, CANBus) override {}
    bool piGetBusSettings(int index, CANBus &bus) override { return getBusConfig(index,bus); }
    void piSuspend(bool) override {}
    bool piSendFrame(const CANFrame &) override { return false; }
};

class TestResetSettings : public QObject
{
    Q_OBJECT
    QTemporaryDir settings;
private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SavvyCANTests");
        QCoreApplication::setApplicationName("ResetSettings");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        qRegisterMetaTypeStreamOperators<QVector<int>>();
        qRegisterMetaTypeStreamOperators<QVector<QString>>();
    }
    void resetKeepsParameters_data()
    {
        QTest::addColumn<int>("state");
        QTest::newRow("running-worker") << 1;
        QTest::newRow("manually-stopped-worker") << 2;
        QTest::newRow("unavailable-unconfigured") << 0;
    }
    void resetKeepsParameters()
    {
        QFETCH(int,state);
        ConnectionWindow window;
        auto view=window.findChild<QTableView *>("tableConnections");
        QVERIFY(view);
        auto model=qobject_cast<CANConnectionModel *>(view->model());
        QVERIFY(model);
        QCOMPARE(model->rowCount(),0);
        QSignalBlocker block(view->selectionModel());
        auto original=new ResetOrigin;
        const CANBus expected=original->expected();
        if (state)
        {
            original->start();
            QTRY_COMPARE(original->started.loadAcquire(),1);
            if (state==2) original->stop();
        }
        model->add(original);
        view->selectRow(0);
        QVERIFY(QMetaObject::invokeMethod(&window,"handleResetConn",Qt::DirectConnection));
        auto replacement=model->getAtIdx(0);
        QVERIFY(replacement);
        // Synchronize with startup of the real replacement before stopping it.
        CANBus live;
        replacement->getBusSettings(0,live);
        const int serialSpeed=replacement->getSerialSpeed();
        const QString port=replacement->getPort();
        const QString driver=replacement->getDriver();
        replacement->stop();
        CANBus saved;
        bool got=false;
        const bool invoked=QMetaObject::invokeMethod(replacement,"getStoppedBusSettings",Qt::DirectConnection,
            Q_RETURN_ARG(bool,got),Q_ARG(int,0),Q_ARG(CANBus&,saved));
        model->remove(replacement);
        delete replacement;
        QCOMPARE(model->rowCount(),0);
        QCOMPARE(serialSpeed,921600);
        QCOMPARE(port,QString("no-such-test-serial-device"));
        QCOMPARE(driver,QString("LAWICEL"));
        QVERIFY(invoked);
        QVERIFY(got);
        QVERIFY(saved==expected);
    }
    void snapshotControls()
    {
        ResetOrigin connection;
        CANBus value;
        QVERIFY(connection.getStoppedBusSettings(0,value));
        QVERIFY(value==connection.expected());
        const CANBus unchanged=value;
        QVERIFY(!connection.getStoppedBusSettings(-1,value));
        QVERIFY(!connection.getStoppedBusSettings(1,value));
        QVERIFY(value==unchanged);
        connection.start();
        QTRY_COMPARE(connection.started.loadAcquire(),1);
        QVERIFY(!connection.getStoppedBusSettings(0,value));
        connection.stop();
        QVERIFY(connection.getStoppedBusSettings(0,value));
        QVERIFY(value==unchanged);
    }
    void initialSettingsControls_data()
    {
        QTest::addColumn<bool>("threaded");
        QTest::newRow("worker") << true;
        QTest::newRow("owner-thread") << false;
    }
    void initialSettingsControls()
    {
        QFETCH(bool,threaded);
        ResetOrigin connection(threaded);
        CANBus saved;
        saved.setSpeed(250000);
        saved.setDataRate(8000000);
        saved.setCanFD(false);
        saved.setListenOnly(false);
        saved.setSingleWire(false);
        saved.setActive(false);
        QVERIFY(!connection.setInitialBusSettings(-1,saved));
        QVERIFY(!connection.setInitialBusSettings(1,saved));
        QVERIFY(connection.setInitialBusSettings(0,saved));
        connection.start();
        QTRY_COMPARE(connection.started.loadAcquire(),1);
        QVERIFY(connection.startupSettings==saved);
        QVERIFY(!connection.setInitialBusSettings(0,CANBus()));
        connection.stop();
        QVERIFY(!connection.setInitialBusSettings(0,CANBus()));
        CANBus result;
        QVERIFY(connection.getStoppedBusSettings(0,result));
        QVERIFY(result==saved);
    }
};
QTEST_MAIN(TestResetSettings)
#include "tst_resetsettings.moc"
