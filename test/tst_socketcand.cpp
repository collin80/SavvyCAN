#include <QtTest>
#include <QTcpServer>
#include <QPointer>
#include "connections/socketcand.h"

class PendingSocket : public QTcpSocket
{
public:
    PendingSocket() { setSocketState(QAbstractSocket::ConnectingState); }
};

class InspectableSocketCANd : public SocketCANd
{
public:
    explicit InspectableSocketCANd(const QString &port) : SocketCANd(port) {}
    QTcpSocket *socket(int index) { return tcpClient.at(index); }
    int socketCount() const { return tcpClient.size(); }
    void connectNow() { QMetaObject::invokeMethod(this, "connectDevice", Qt::DirectConnection); }
    void checkNow() { QMetaObject::invokeMethod(this, "checkConnection", Qt::DirectConnection); }
    void seedSocket(QTcpSocket *socket) { tcpClient.append(socket); }
};

class TestSocketCANd : public QObject
{
    Q_OBJECT

    QString portName(const QTcpServer &server, bool multiple = false)
    {
        return QString("%1@Local (can://127.0.0.1:%2)")
            .arg(multiple ? "vcan0,vcan1" : "vcan0").arg(server.serverPort());
    }

private slots:
    void quietConnectedSocketIsKept()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        InspectableSocketCANd connection(portName(server));
        connection.connectNow();
        QVERIFY(connection.socket(0)->waitForConnected(1000));
        QPointer<QTcpSocket> original = connection.socket(0);
        QCOMPARE(original->readBufferSize(), qint64(0));
        connection.checkNow();
        connection.checkNow();
        QVERIFY(original);
        QCOMPARE(connection.socket(0), original.data());
        QCOMPARE(original->state(), QAbstractSocket::ConnectedState);
    }

    void closedSocketReconnectsAndReportsStatus()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        InspectableSocketCANd connection(portName(server));
        connection.connectNow();
        QVERIFY(connection.socket(0)->waitForConnected(1000));
        QPointer<QTcpSocket> original = connection.socket(0);
        original->abort();
        QSignalSpy statusSpy(&connection, &CANConnection::status);
        connection.checkNow();
        QVERIFY(!original);
        QCOMPARE(connection.socketCount(), 1);
        QVERIFY(connection.socket(0)->waitForConnected(1000));
        QCOMPARE(statusSpy.count(), 1);
        QPointer<QTcpSocket> recovered = connection.socket(0);
        connection.checkNow();
        QVERIFY(recovered);
    }

    void allQuietBusesAreKept()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        InspectableSocketCANd connection(portName(server, true));
        connection.connectNow();
        QCOMPARE(connection.socketCount(), 2);
        QVERIFY(connection.socket(0)->waitForConnected(1000));
        QVERIFY(connection.socket(1)->waitForConnected(1000));
        QPointer<QTcpSocket> first = connection.socket(0);
        QPointer<QTcpSocket> second = connection.socket(1);
        connection.checkNow();
        QVERIFY(first);
        QVERIFY(second);
    }

    void oneClosedBusTriggersRecovery()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        InspectableSocketCANd connection(portName(server, true));
        connection.connectNow();
        QVERIFY(connection.socket(0)->waitForConnected(1000));
        QVERIFY(connection.socket(1)->waitForConnected(1000));
        QPointer<QTcpSocket> closed = connection.socket(1);
        closed->abort();
        connection.checkNow();
        QVERIFY(!closed);
        QCOMPARE(connection.socketCount(), 2);
        QVERIFY(connection.socket(0)->waitForConnected(1000));
        QVERIFY(connection.socket(1)->waitForConnected(1000));
    }

    void connectionInProgressIsKept()
    {
        InspectableSocketCANd connection("vcan0@Local (can://127.0.0.1:1)");
        QPointer<QTcpSocket> pending = new PendingSocket;
        connection.seedSocket(pending);
        QCOMPARE(pending->state(), QAbstractSocket::ConnectingState);
        connection.checkNow();
        QVERIFY(pending);
    }
};

QTEST_MAIN(TestSocketCANd)
#include "tst_socketcand.moc"
