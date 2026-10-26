#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QPushButton>
#include <QJsonDocument>
#include <QTableWidget>
#include <QTemporaryDir>
#include "isotp_handler.h"
#include "obd2_poller.h"
#include "re/obd2window.h"

class TestObd2Window : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testFormatValues();
    void testTableBeforeDiscovery();
    void testPollTickedPids();
    void testValuesInTable();
    void testDiscoveryRebuildsTable();
    void testTicksKeptAcrossDiscovery();
    void testTimeoutsAndRefusals();
    void testNothingTicked();
    void testReadVin();
    void testWarnings();
    void testPollSetRoundTrip();
    void testPollSetFile();
    void testBadPollSets();
    void testCloseStopsPolling();
    void testHelp();
    void testScreenshot();

signals:
    void framesReceived(void*, const QVector<CommFrame>& frames);

private:
    void receive(quint32 id, const char *hex);
    void advance(qint64 ms) { now += ms; window->obd2Poller()->service(); }
    QString cell(quint32 ecu, quint8 pid, int column) const;
    std::unique_ptr<Obd2Window> makeWindow(std::unique_ptr<ISOTP_HANDLER> &handler, int numBuses = 2);

    qint64 now = 0;
    std::vector<CommFrame> sentFrames;
    QVector<CommFrame> dummyFrames;
    std::unique_ptr<ISOTP_HANDLER> isotp;
    std::unique_ptr<Obd2Window> window;
};

std::unique_ptr<Obd2Window> TestObd2Window::makeWindow(std::unique_ptr<ISOTP_HANDLER> &handler, int numBuses)
{
    auto pendingConn = [this](ISOTP_HANDLER* h) -> QMetaObject::Connection {
        return QObject::connect(this, &TestObd2Window::framesReceived, h,
            [h](void*, const QVector<CommFrame>& frames) { h->rapidFrames(frames); });
    };
    handler = std::make_unique<ISOTP_HANDLER>(
        dummyFrames,
        [this](const CommFrame& frame) { sentFrames.push_back(frame); },
        [numBuses]() -> int { return numBuses; },
        pendingConn);
    auto w = std::make_unique<Obd2Window>(new Obd2Poller(handler.get(), [this]() { return now; }), numBuses);
    return w;
}

void TestObd2Window::init()
{
    now = 1000;
    sentFrames.clear();
    window = makeWindow(isotp);
}

void TestObd2Window::cleanup()
{
    window.reset();
    isotp.reset();
}

void TestObd2Window::receive(quint32 id, const char *hex)
{
    CommFrame frame;
    frame.setBus(0);
    frame.setFrameId(id);
    QByteArray data = QByteArray::fromHex(hex);
    while (data.size() < 8) data.append(char(0x55));
    frame.setPayload(data);
    emit framesReceived(nullptr, {frame});
}

QString TestObd2Window::cell(quint32 ecu, quint8 pid, int column) const
{
    int row = window->rowFor(ecu, pid);
    if (row < 0) return "<no row>";
    return window->table()->item(row, column)->text();
}

void TestObd2Window::testFormatValues()
{
    QList<OBD2::Value> values;
    QVERIFY(OBD2::decode(*OBD2::findMode01Pid(0x0C), QByteArray::fromHex("1AF8"), values));
    QCOMPARE(Obd2Window::formatValues(values), QString("1726 rpm"));
    QVERIFY(OBD2::decode(*OBD2::findMode01Pid(0x0C), QByteArray::fromHex("1AF9"), values));
    QCOMPARE(Obd2Window::formatValues(values), QString("1726.25 rpm"));
    QVERIFY(OBD2::decode(*OBD2::findMode01Pid(0x05), QByteArray::fromHex("00"), values));
    QCOMPARE(Obd2Window::formatValues(values), QString("-40 degC"));
    QVERIFY(OBD2::decode(*OBD2::findMode01Pid(0x42), QByteArray::fromHex("3A99"), values));
    QCOMPARE(Obd2Window::formatValues(values), QString("15.001 V"));
    QVERIFY(OBD2::decode(*OBD2::findMode01Pid(0x14), QByteArray::fromHex("B480"), values));
    QCOMPARE(Obd2Window::formatValues(values), QString("O2S1_Voltage 0.9 V, O2S1_ShortTermFuelTrim 0 %"));
    QVERIFY(OBD2::decode(*OBD2::findMode01Pid(0x51), QByteArray::fromHex("08"), values));
    QCOMPARE(Obd2Window::formatValues(values), QString("Electric"));
    QVERIFY(OBD2::decode(*OBD2::findMode01Pid(0x06), QByteArray::fromHex("7F"), values));
    QCOMPARE(Obd2Window::formatValues(values), QString("-0.78 %"));
}

void TestObd2Window::testTableBeforeDiscovery()
{
    int expected = 0;
    for (const OBD2::PidInfo &info : OBD2::mode01Pids())
        if (!OBD2::isSupportBitmapPid(info.pid)) expected++;
    QCOMPARE(window->table()->rowCount(), expected);
    QCOMPARE(cell(0, 0x0C, Obd2Window::COL_ECU), QString("All (0x7DF)"));
    QCOMPARE(cell(0, 0x0C, Obd2Window::COL_NAME), QString("Engine speed"));
    QCOMPARE(cell(0, 0x0C, Obd2Window::COL_PID), QString("0x0C"));
    QCOMPARE(window->rowFor(0, 0x00), -1); //no bitmap rows
}

void TestObd2Window::testPollTickedPids()
{
    QTableWidgetItem *poll = window->table()->item(window->rowFor(0, 0x0D), Obd2Window::COL_POLL);
    poll->setCheckState(Qt::Checked); //as a click does
    window->startPolling();
    QVERIFY(window->obd2Poller()->isRunning());
    QCOMPARE(sentFrames.size(), 1u);
    QCOMPARE(sentFrames[0].frameId(), 0x7DFu);
    QCOMPARE(sentFrames[0].payload(), QByteArray::fromHex("02010DAAAAAAAAAA"));

    window->stopPolling();
    QVERIFY(!window->obd2Poller()->isRunning());
}

void TestObd2Window::testValuesInTable()
{
    window->setPolled(0, 0x0C, true);
    window->startPolling();
    receive(0x7E8, "04410C1AF8");
    QCOMPARE(cell(0, 0x0C, Obd2Window::COL_VALUE), QString("1726 rpm (0x7E8)"));
    QCOMPARE(cell(0, 0x0C, Obd2Window::COL_UPDATES), QString("1"));
    receive(0x7E9, "04410C1AF9");
    QCOMPARE(cell(0, 0x0C, Obd2Window::COL_VALUE), QString("1726.25 rpm (0x7E9)"));
    QCOMPARE(cell(0, 0x0C, Obd2Window::COL_UPDATES), QString("2"));
    QVERIFY(window->statusText().contains("Answers: 2"));
}

void TestObd2Window::testDiscoveryRebuildsTable()
{
    window->discover();
    QVERIFY(window->statusText().contains("Discovering"));
    receive(0x7E8, "06410098180001"); //0x01, 0x04, 0x05, 0x0C, 0x0D, 0x20
    advance(50);
    receive(0x7E8, "06412000000001"); //0x40
    advance(50); //the window's rate limit: 20 requests a second
    receive(0x7E8, "06414040000000"); //0x42

    QVERIFY(!window->obd2Poller()->isDiscovering());
    QCOMPARE(window->table()->rowCount(), 6);
    QCOMPARE(cell(0x7E8, 0x0C, Obd2Window::COL_ECU), QString("0x7E8"));
    QVERIFY(window->rowFor(0x7E8, 0x42) >= 0);
    QCOMPARE(window->rowFor(0, 0x0C), -1);
    QVERIFY(window->statusText().contains("0x7E8 (6 PIDs)"));

    QCheckBox *showAll = window->findChild<QCheckBox *>("showAllCheck");
    QVERIFY(showAll);
    showAll->setChecked(true);
    QVERIFY(window->table()->rowCount() > 80);
    showAll->setChecked(false);
    QCOMPARE(window->table()->rowCount(), 6);

    //polling a discovered PID goes to that ECU alone
    window->setPolled(0x7E8, 0x42, true);
    window->startPolling();
    advance(50);
    QCOMPARE(sentFrames.back().frameId(), 0x7E0u);
    QCOMPARE(sentFrames.back().payload(), QByteArray::fromHex("020142AAAAAAAAAA"));
}

void TestObd2Window::testTicksKeptAcrossDiscovery()
{
    window->setPolled(0, 0x0D, true, 500);
    window->discover();
    receive(0x7E8, "06410080000000"); //0x01 only
    advance(50);
    QCOMPARE(window->table()->rowCount(), 2); //0x01 for the ECU, and the ticked row for all ECUs
    QCOMPARE(window->table()->item(window->rowFor(0, 0x0D), Obd2Window::COL_POLL)->checkState(), Qt::Checked);
    QCOMPARE(cell(0, 0x0D, Obd2Window::COL_INTERVAL), QString("500"));
}

void TestObd2Window::testTimeoutsAndRefusals()
{
    window->setPolled(0x7E8, 0x0C, true);
    window->setPolled(0x7E8, 0x5C, true);
    window->startPolling();
    advance(50); //0x0C times out
    QCOMPARE(cell(0x7E8, 0x0C, Obd2Window::COL_TIMEOUTS), QString("1"));
    receive(0x7E8, "037F0112"); //0x5C refused
    QCOMPARE(cell(0x7E8, 0x5C, Obd2Window::COL_VALUE), QString("Not supported by this ECU"));
    QVERIFY(window->statusText().contains("Refused: 1"));
}

void TestObd2Window::testNothingTicked()
{
    window->startPolling();
    QVERIFY(!window->obd2Poller()->isRunning());
    QVERIFY(sentFrames.empty());
    QVERIFY(window->statusText().contains("Tick"));
}

void TestObd2Window::testReadVin()
{
    window->readVin(); //nothing discovered: the engine's usual address
    QCOMPARE(sentFrames.size(), 1u);
    QCOMPARE(sentFrames[0].frameId(), 0x7E0u);
    QCOMPARE(sentFrames[0].payload(), QByteArray::fromHex("020902AAAAAAAAAA"));
    receive(0x7E8, "1014490201314731");
    receive(0x7E8, "214A433534343452");
    receive(0x7E8, "2237323532333637");
    QCOMPARE(window->vinText(), QString("VIN (0x7E8): 1G1JC5444R7252367"));
}

void TestObd2Window::testWarnings()
{
    QVERIFY(window->warningText().isEmpty());
    window->setListenOnlyCheck([](int bus) { return bus == 1; });
    QVERIFY(window->warningText().isEmpty());
    QComboBox *bus = window->findChild<QComboBox *>("busCombo");
    bus->setCurrentIndex(1);
    QVERIFY(window->warningText().contains("listen-only"));
    QCOMPARE(window->obd2Poller()->bus(), 1);
    bus->setCurrentIndex(0);
    QVERIFY(window->warningText().isEmpty());

    std::unique_ptr<ISOTP_HANDLER> otherHandler;
    auto noBus = makeWindow(otherHandler, 0);
    QVERIFY(noBus->warningText().contains("No bus"));
    noBus.reset();
}

void TestObd2Window::testPollSetRoundTrip()
{
    window->setPolled(0, 0x0D, true, 0);
    window->setPolled(0x7E8, 0x05, true, 1000);
    window->setPolled(0, 0x0C, true);
    window->setPolled(0, 0x0C, false); //ticked and unticked again: not saved
    QJsonObject json = window->pollSetToJson();

    QCOMPARE(json["format"].toString(), QString("SavvyCAN OBD-II poll set"));
    QCOMPARE(json["version"].toInt(), 1);
    QCOMPARE(json["addressing"].toString(), QString("11-bit"));
    QCOMPARE(json["maxRequestRate"].toInt(), 20);
    QJsonArray items = json["items"].toArray();
    QCOMPARE(items.size(), 2);

    std::unique_ptr<ISOTP_HANDLER> otherHandler;
    auto other = makeWindow(otherHandler);
    QString error;
    QVERIFY2(other->pollSetFromJson(json, &error), qPrintable(error));
    QCOMPARE(other->table()->item(other->rowFor(0, 0x0D), Obd2Window::COL_POLL)->checkState(), Qt::Checked);
    int row = other->rowFor(0x7E8, 0x05); //an ECU that wasn't discovered there gets its row
    QVERIFY(row >= 0);
    QCOMPARE(other->table()->item(row, Obd2Window::COL_INTERVAL)->text(), QString("1000"));
    QCOMPARE(other->table()->item(other->rowFor(0, 0x0C), Obd2Window::COL_POLL)->checkState(), Qt::Unchecked);
    QCOMPARE(other->pollSetToJson(), json);

    //29-bit and another rate come back too
    json["addressing"] = "29-bit";
    json["maxRequestRate"] = 5;
    json["bus"] = 1;
    QVERIFY(other->pollSetFromJson(json, &error));
    QCOMPARE(other->obd2Poller()->addressing(), Obd2Poller::Addressing::Extended29Bit);
    QCOMPARE(other->obd2Poller()->bus(), 1);
    QCOMPARE(other->pollSetToJson()["maxRequestRate"].toInt(), 5);
    other.reset();
}

void TestObd2Window::testPollSetFile()
{
    QTemporaryDir dir;
    QString path = dir.filePath("engine.obd");
    window->setPolled(0, 0x0C, true, 100);
    QString error;
    QVERIFY(window->savePollSet(path, &error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(QJsonDocument::fromJson(file.readAll()).isObject());
    file.close();

    window->setPolled(0, 0x0C, false);
    QVERIFY(window->loadPollSet(path, &error));
    QCOMPARE(window->table()->item(window->rowFor(0, 0x0C), Obd2Window::COL_POLL)->checkState(), Qt::Checked);
    QCOMPARE(cell(0, 0x0C, Obd2Window::COL_INTERVAL), QString("100"));

    QVERIFY(!window->loadPollSet(dir.filePath("missing.obd"), &error));
    QVERIFY(!error.isEmpty());
}

void TestObd2Window::testBadPollSets()
{
    QString error;
    QVERIFY(!window->pollSetFromJson(QJsonObject{{"format", "something else"}}, &error));
    QVERIFY(error.contains("isn't a SavvyCAN"));

    QJsonObject newer = window->pollSetToJson();
    newer["version"] = 99;
    QVERIFY(!window->pollSetFromJson(newer, &error));
    QVERIFY(error.contains("newer"));

    QJsonObject bad = window->pollSetToJson();
    bad["items"] = QJsonArray{QJsonObject{{"ecu", "all"}, {"pid", "0x1FF"}, {"intervalMs", 0}}};
    window->setPolled(0, 0x0D, true);
    QVERIFY(!window->pollSetFromJson(bad, &error));
    QVERIFY(error.contains("can't be read"));
    //nothing changed by a set that couldn't be read
    QCOMPARE(window->table()->item(window->rowFor(0, 0x0D), Obd2Window::COL_POLL)->checkState(), Qt::Checked);

    QTemporaryDir dir;
    QFile file(dir.filePath("broken.obd"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{ not json");
    file.close();
    QVERIFY(!window->loadPollSet(file.fileName(), &error));
    QVERIFY(error.contains("Not a JSON"));
}

void TestObd2Window::testCloseStopsPolling()
{
    window->setPolled(0, 0x0C, true);
    window->show();
    window->startPolling();
    QVERIFY(window->obd2Poller()->isRunning());
    window->close();
    QVERIFY(!window->obd2Poller()->isRunning());
}

void TestObd2Window::testHelp()
{
    int shown = 0;
    window->setHelpCallback([&]() { shown++; });
    QTest::keyClick(window.get(), Qt::Key_F1);
    QCOMPARE(shown, 1);
    for (QPushButton *button : window->findChildren<QPushButton *>())
    {
        if (button->text() == "Help") button->click();
    }
    QCOMPARE(shown, 2);
}

//with SAVVYCAN_SCREENSHOTS set to a folder, the window is saved there as a picture
void TestObd2Window::testScreenshot()
{
    window->discover();
    receive(0x7E8, "064100BE3FA813");
    advance(50);
    receive(0x7E8, "06412080000001");
    advance(50);
    receive(0x7E8, "06414040000000");
    QVERIFY(!window->obd2Poller()->isDiscovering());
    window->setPolled(0x7E8, 0x0C, true);
    window->setPolled(0x7E8, 0x0D, true);
    window->setPolled(0x7E8, 0x05, true, 1000);
    window->setPolled(0x7E8, 0x42, true, 500);
    advance(50);
    window->startPolling();
    QHash<char, const char *> replies = {{0x0C, "04410C1AF8"}, {0x0D, "03410D32"}, {0x05, "0341057B"},
                                         {0x42, "0441423A98"}};
    for (int i = 0; i < 8; i++)
    {
        receive(0x7E8, replies.value(sentFrames.back().payload()[2]));
        advance(50);
    }
    window->readVin(); //goes out once the poll request outstanding is answered
    receive(0x7E8, replies.value(sentFrames.back().payload()[2]));
    advance(50);
    QCOMPARE(sentFrames.back().payload().left(3), QByteArray::fromHex("020902"));
    receive(0x7E8, "1014490201314731");
    receive(0x7E8, "214A433534343452");
    receive(0x7E8, "2237323532333637");
    QCOMPARE(cell(0x7E8, 0x0D, Obd2Window::COL_VALUE), QString("50 km/h"));

    const QString dir = qEnvironmentVariable("SAVVYCAN_SCREENSHOTS");
    if (!dir.isEmpty())
    {
        window->show();
        window->grab().save(dir + "/obd2-window.png");
    }
}

QTEST_MAIN(TestObd2Window)
#include "test_obd2_window.moc"
