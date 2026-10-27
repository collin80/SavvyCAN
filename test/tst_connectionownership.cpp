#include <QtTest>
#include <QPointer>
#include <QTableView>
#include <QTemporaryDir>
#include "connections/canconmanager.h"
#include "connections/canconnectionmodel.h"
#include "connections/connectionwindow.h"

struct Lifetime
{
    int stops = 0;
    int destroyed = 0;
};

class TrackedConnection : public CANConnection
{
    Lifetime &lifetime;
public:
    explicit TrackedConnection(Lifetime &value, CANCon::type type = CANCon::LAWICEL)
        : CANConnection("no-such-ownership-test-device", "LAWICEL", type,
                        115200, 500000, false, 0, 1, 16, false), lifetime(value) {}
    ~TrackedConnection() override { ++lifetime.destroyed; }
protected:
    void piStarted() override {}
    void piStop() override { ++lifetime.stops; }
    void piSetBusSettings(int, CANBus) override {}
    bool piGetBusSettings(int index, CANBus &bus) override { return getBusConfig(index, bus); }
    void piSuspend(bool) override {}
    bool piSendFrame(const CANFrame &) override { return false; }
};

class TestConnectionOwnership : public QObject
{
    Q_OBJECT
    QTemporaryDir settings;

    // Cleanup through guarded pointers also works against the original bug:
    // the failing test never dereferences or deletes an already freed object.
    static void release(QPointer<TrackedConnection> first,
                        QPointer<TrackedConnection> second = {})
    {
        CANConManager::getInstance()->getConnections().clear();
        if (first) { first->stop(); delete first.data(); }
        if (second) { second->stop(); delete second.data(); }
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SavvyCANTests");
        QCoreApplication::setApplicationName("ConnectionOwnership");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        qRegisterMetaTypeStreamOperators<QVector<int>>();
        qRegisterMetaTypeStreamOperators<QVector<QString>>();
    }
    void init()
    {
        QVERIFY(CANConManager::getInstance()->getConnections().isEmpty());
    }
    void cleanup()
    {
        QVERIFY(CANConManager::getInstance()->getConnections().isEmpty());
    }
    void replacementLeavesLifetimeToCaller_data()
    {
        QTest::addColumn<bool>("throughModel");
        QTest::newRow("manager") << false;
        QTest::newRow("model") << true;
    }
    void replacementLeavesLifetimeToCaller()
    {
        QFETCH(bool, throughModel);
        Lifetime oldLife, newLife;
        QPointer<TrackedConnection> old = new TrackedConnection(oldLife);
        QPointer<TrackedConnection> replacement = new TrackedConnection(newLife);
        auto manager = CANConManager::getInstance();
        CANConnectionModel model;
        manager->add(old);
        if (throughModel) model.replace(0, replacement);
        else manager->replace(0, replacement);
        const bool oldAlive = old;
        const bool replacementInstalled = manager->getConnections().at(0) == replacement;
        const int stoppedDuringReplace = oldLife.stops;
        release(old, replacement);
        QVERIFY(oldAlive);
        QVERIFY(replacementInstalled);
        QCOMPARE(stoppedDuringReplace, 0);
        QCOMPARE(oldLife.stops, 1);
        QCOMPARE(oldLife.destroyed, 1);
        QCOMPARE(newLife.destroyed, 1);
    }
    void selfReplacementKeepsObject()
    {
        Lifetime life;
        QPointer<TrackedConnection> connection = new TrackedConnection(life);
        auto manager = CANConManager::getInstance();
        manager->add(connection);
        manager->replace(0, connection);
        const bool alive = connection;
        release(connection);
        QVERIFY(alive);
        QCOMPARE(life.stops, 1);
        QCOMPARE(life.destroyed, 1);
    }
    void reorder_data()
    {
        QTest::addColumn<bool>("up");
        QTest::addColumn<int>("repetitions");
        QTest::addColumn<bool>("modelSignals");
        QTest::newRow("up") << true << 1 << false;
        QTest::newRow("down") << false << 1 << false;
        QTest::newRow("up-down-repeated") << true << 10 << false;
        QTest::newRow("down-up-repeated") << false << 10 << false;
        QTest::newRow("normal-view-up") << true << 1 << true;
        QTest::newRow("normal-view-down") << false << 1 << true;
    }
    void reorder()
    {
        QFETCH(bool, up);
        QFETCH(int, repetitions);
        QFETCH(bool, modelSignals);
        Lifetime firstLife, secondLife;
        ConnectionWindow window;
        auto view = window.findChild<QTableView *>("tableConnections");
        QVERIFY(view);
        auto model = qobject_cast<CANConnectionModel *>(view->model());
        QVERIFY(model);
        QSignalBlocker selectionBlock(view->selectionModel());
        QPointer<TrackedConnection> first = new TrackedConnection(firstLife);
        QPointer<TrackedConnection> second = new TrackedConnection(secondLife);
        model->add(first);
        model->add(second);
        bool ordered = true;
        // Keep the original failing paths from triggering a view repaint of
        // dangling pointers; the real window/model/manager operations still run.
        QSignalBlocker modelBlock(model);
        if (modelSignals)
        {
            modelBlock.unblock();
            selectionBlock.unblock();
        }
        for (int i = 0; i < repetitions && first && second; ++i)
        {
            view->selectRow(up ? 1 : 0);
            ordered = ordered && QMetaObject::invokeMethod(&window,
                up ? "moveConnUp" : "moveConnDown", Qt::DirectConnection);
            ordered = ordered && model->getAtIdx(0) == (i % 2 ? first.data() : second.data());
            ordered = ordered && model->getAtIdx(1) == (i % 2 ? second.data() : first.data());
            up = !up;
        }
        const bool bothAlive = first && second;
        const int stoppedDuringReorder = firstLife.stops + secondLife.stops;
        modelBlock.reblock();
        selectionBlock.reblock();
        release(first, second);
        QVERIFY(bothAlive);
        QVERIFY(ordered);
        QCOMPARE(stoppedDuringReorder, 0);
        QCOMPARE(firstLife.destroyed, 1);
        QCOMPARE(secondLife.destroyed, 1);
    }
    void boundaryMovesLeaveConnectionAlone_data()
    {
        QTest::addColumn<bool>("up");
        QTest::newRow("first-up") << true;
        QTest::newRow("last-down") << false;
    }
    void boundaryMovesLeaveConnectionAlone()
    {
        QFETCH(bool, up);
        Lifetime life;
        ConnectionWindow window;
        auto view = window.findChild<QTableView *>("tableConnections");
        QVERIFY(view);
        auto model = qobject_cast<CANConnectionModel *>(view->model());
        QVERIFY(model);
        QSignalBlocker selectionBlock(view->selectionModel());
        QPointer<TrackedConnection> connection = new TrackedConnection(life);
        model->add(connection);
        view->selectRow(0);
        const bool invoked = QMetaObject::invokeMethod(&window,
            up ? "moveConnUp" : "moveConnDown", Qt::DirectConnection);
        const bool unchanged = connection && model->getAtIdx(0) == connection;
        release(connection);
        QVERIFY(invoked);
        QVERIFY(unchanged);
        QCOMPARE(life.destroyed, 1);
    }
    void reset_data()
    {
        QTest::addColumn<bool>("factorySucceeds");
        QTest::newRow("replacement-created") << true;
        QTest::newRow("factory-rejects-type") << false;
    }
    void reset()
    {
        QFETCH(bool, factorySucceeds);
        Lifetime life;
        ConnectionWindow window;
        auto view = window.findChild<QTableView *>("tableConnections");
        QVERIFY(view);
        auto model = qobject_cast<CANConnectionModel *>(view->model());
        QVERIFY(model);
        QSignalBlocker selectionBlock(view->selectionModel());
        QPointer<TrackedConnection> original = new TrackedConnection(life,
            factorySucceeds ? CANCon::LAWICEL : CANCon::NONE);
        model->add(original);
        view->selectRow(0);
        const bool invoked = QMetaObject::invokeMethod(&window, "handleResetConn", Qt::DirectConnection);
        const bool oldAlive = original;
        const int stops = life.stops;
        const int destroyed = life.destroyed;
        auto remaining = model->getAtIdx(0);
        const bool replaced = remaining != original.data();
        if (factorySucceeds)
        {
            // The actual LAWICEL factory uses a worker. Synchronize startup
            // before stopping it; the serial path is deliberately nonexistent.
            CANBus unused;
            remaining->getBusSettings(0, unused);
        }
        model->remove(remaining);
        remaining->stop();
        delete remaining;
        QVERIFY(invoked);
        QCOMPARE(oldAlive, !factorySucceeds);
        QCOMPARE(stops, 1);
        QCOMPARE(destroyed, factorySucceeds ? 1 : 0);
        QCOMPARE(replaced, factorySucceeds);
        QCOMPARE(life.destroyed, 1);
    }
    void removeStopsAndDeletesOnce()
    {
        Lifetime life;
        {
            ConnectionWindow window;
            auto view = window.findChild<QTableView *>("tableConnections");
            QVERIFY(view);
            auto model = qobject_cast<CANConnectionModel *>(view->model());
            QVERIFY(model);
            QSignalBlocker selectionBlock(view->selectionModel());
            model->add(new TrackedConnection(life));
            view->selectRow(0);
            QVERIFY(QMetaObject::invokeMethod(&window, "handleRemoveConn", Qt::DirectConnection));
            QCOMPARE(model->rowCount(), 0);
        }
        QCOMPARE(life.stops, 1);
        QCOMPARE(life.destroyed, 1);
    }
};
QTEST_MAIN(TestConnectionOwnership)
#include "tst_connectionownership.moc"
