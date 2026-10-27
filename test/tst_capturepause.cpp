#include <QtTest>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTableView>
#include "connections/connectionwindow.h"
#include "connections/canconmanager.h"
#include "ui_connectionwindow.h"

class StartingConnection : public CANConnection
{
public:
    explicit StartingConnection(bool threaded)
        : CANConnection("", "", CANCon::NONE, 0, 0, false, 0, 1, 16, threaded), started(0) {}
    ~StartingConnection() { stop(); }
    QAtomicInt started;
protected:
    void piStarted() override
    {
        if (!isCapSuspended())
        {
            getQueue().get()->setPayload(QByteArray::fromHex("CAFE"));
            getQueue().queue();
        }
        started.storeRelease(1);
    }
    void piStop() override {}
    void piSetBusSettings(int, CANBus) override {}
    bool piGetBusSettings(int, CANBus &) override { return false; }
    void piSuspend(bool paused) override { setCapSuspended(paused); }
    bool piSendFrame(const CANFrame &) override { return false; }
};

class TestCapturePause : public QObject
{
    Q_OBJECT
    QTemporaryDir settingsDir;

    bool receiveFrame(CANConnection *connection)
    {
        QString remaining;
        return QMetaObject::invokeMethod(connection, "decodeFrames",
            Qt::BlockingQueuedConnection, Q_RETURN_ARG(QString, remaining),
            Q_ARG(QString, QString("< frame 123 1.0 AABB >")), Q_ARG(int, 0));
    }

    CANConnection *addConnection(ConnectionWindow &window, quint16 port)
    {
        auto connection = window.create(CANCon::KAYAK,
            QString("vcan0@Local (can://127.0.0.1:%1)").arg(port), "kayak",
            0, 0, false, 0);
        if (connection) window.connModel->add(connection);
        return connection;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(settingsDir.isValid());
        QCoreApplication::setOrganizationName("SavvyCAN-tests");
        QCoreApplication::setApplicationName("capture-pause");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
        QSettings().clear();
        qRegisterMetaTypeStreamOperators<QVector<int>>();
        qRegisterMetaTypeStreamOperators<QVector<QString>>();
    }

    void startupFrame_data()
    {
        QTest::addColumn<bool>("paused");
        QTest::addColumn<bool>("threaded");
        QTest::newRow("paused-worker") << true << true;
        QTest::newRow("active-worker") << false << true;
        QTest::newRow("paused-inline") << true << false;
        QTest::newRow("active-inline") << false << false;
    }

    void startupFrame()
    {
        QFETCH(bool, paused);
        QFETCH(bool, threaded);
        StartingConnection connection(threaded);
        connection.start(paused);
        QTRY_VERIFY(connection.started.loadAcquire());
        QCOMPARE(connection.getQueue().peek() == nullptr, paused);
    }

    void newConnection_data()
    {
        QTest::addColumn<bool>("paused");
        QTest::addColumn<bool>("resumeFirst");
        QTest::newRow("active") << false << false;
        QTest::newRow("paused") << true << false;
        QTest::newRow("resumed-before-create") << false << true;
    }

    void newConnection()
    {
        QFETCH(bool, paused);
        QFETCH(bool, resumeFirst);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        ConnectionWindow window;
        if (resumeFirst) window.setSuspendAll(true);
        window.setSuspendAll(paused);
        auto connection = addConnection(window, server.serverPort());
        QVERIFY(connection);
        QVERIFY(receiveFrame(connection));
        QCOMPARE(connection->getQueue().peek() == nullptr, paused);
        if (paused)
        {
            window.setSuspendAll(false);
            QVERIFY(receiveFrame(connection));
            QVERIFY(connection->getQueue().peek());
            QCOMPARE(connection->getQueue().peek()->payload(), QByteArray::fromHex("AABB"));
        }
    }

    void resetWhilePaused()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        ConnectionWindow window;
        auto original = addConnection(window, server.serverPort());
        QVERIFY(original);
        window.setSuspendAll(true);
        window.ui->tableConnections->setCurrentIndex(window.connModel->index(0, 0));
        window.handleResetConn();
        auto replacement = CANConManager::getInstance()->getConnections().at(0);
        QVERIFY(receiveFrame(replacement));
        QVERIFY(!replacement->getQueue().peek());
        window.setSuspendAll(false);
        QVERIFY(receiveFrame(replacement));
        QVERIFY(replacement->getQueue().peek());
    }

    void pausingExistingConnectionFlushesFrames()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        ConnectionWindow window;
        auto connection = addConnection(window, server.serverPort());
        QVERIFY(connection);
        QVERIFY(receiveFrame(connection));
        QVERIFY(connection->getQueue().peek());
        window.setSuspendAll(true);
        QVERIFY(!connection->getQueue().peek());
        QVERIFY(receiveFrame(connection));
        QVERIFY(!connection->getQueue().peek());
    }
};

QTEST_MAIN(TestCapturePause)
#include "tst_capturepause.moc"
