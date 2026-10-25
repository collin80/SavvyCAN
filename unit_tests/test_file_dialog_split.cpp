#include <QtTest/QtTest>
#include <QTemporaryDir>
#include "framefileio.h"
#include "dbc/dbchandler.h"

// Loading and saving logs and DBC files without the file dialogs: the file type lists the dialogs use, and the
// functions that load or save by an index into those lists (FrameFileIO::loadFrameFileAs/saveFrameFileAs,
// DBCHandler::loadFileAs/saveDBCFileAs).
class TestFileDialogSplit : public QObject
{
    Q_OBJECT

private slots:
    void testLogFilterLists();
    void testLogSaveAndLoad_data();
    void testLogSaveAndLoad();
    void testLogAutodetect();
    void testLogBadIndex();
    void testDbcFilterLists();
    void testDbcSaveAndLoad();
    void testDbcBadIndex();
};

//8 data bytes each: some formats don't keep shorter frames as they are, which isn't what's tested here
static QVector<CommFrame> sampleFrames()
{
    QVector<CommFrame> frames;
    for (int i = 0; i < 4; i++)
    {
        CommFrame frame;
        frame.setFrameId(0x100 + quint32(i));
        frame.setPayload(QByteArray::fromHex("0102030405060708") + char(i));
        frame.setPayload(frame.payload().right(8));
        frame.setTimeStamp(CommFrame::TimeStamp(0, 1000 * (i + 1)));
        frames.append(frame);
    }
    return frames;
}

void TestFileDialogSplit::testLogFilterLists()
{
    const QStringList load = FrameFileIO::loadFilters();
    const QStringList save = FrameFileIO::saveFilters();
    QCOMPARE(load.count(), 28);
    QCOMPARE(save.count(), 14);
    QVERIFY(load.first().startsWith("Autodetect"));
    QVERIFY(save.first().startsWith("GVRET"));
}

// Each save type, the extension it adds, and the load type that reads it back
void TestFileDialogSplit::testLogSaveAndLoad_data()
{
    QTest::addColumn<int>("saveIndex");
    QTest::addColumn<QString>("extension");
    QTest::addColumn<int>("loadIndex");

    QTest::newRow("GVRET") << 0 << ".csv" << 1;
    QTest::newRow("CRTD") << 1 << ".txt" << 2;
    QTest::newRow("Generic CSV") << 2 << ".csv" << 18;
    QTest::newRow("BusMaster") << 3 << ".log" << 3;
    QTest::newRow("Microchip") << 4 << ".log" << 4;
    QTest::newRow("Vector trace") << 5 << ".trace" << 5;
    QTest::newRow("IXXAT MiniLog") << 6 << ".csv" << 6;
    QTest::newRow("CAN-DO") << 7 << ".can" << 7;
    QTest::newRow("Vehicle Spy") << 8 << ".csv" << 8;
    QTest::newRow("Candump") << 9 << ".log" << 9;
    QTest::newRow("Cabana") << 10 << ".csv" << 19;
    QTest::newRow("CANalyzer ASC") << 11 << ".asc" << 14;
    QTest::newRow("CARBUS") << 12 << ".trc" << 16;
    QTest::newRow("PCAN") << 13 << ".trc" << 11;
}

void TestFileDialogSplit::testLogSaveAndLoad()
{
    QFETCH(int, saveIndex);
    QFETCH(QString, extension);
    QFETCH(int, loadIndex);

    if (saveIndex == 7) QSKIP("saveCANDOFile writes into an empty QByteArray, which Qt 6 doesn't allow: a separate fix");
    if (saveIndex == 8) QSKIP("saveVehicleSpyFile isn't written yet: it returns true without saving anything");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QVector<CommFrame> frames = sampleFrames();

    QString filename = dir.filePath("log");
    QVERIFY(FrameFileIO::saveFrameFileAs(filename, saveIndex, &frames));
    QCOMPARE(filename, dir.filePath("log" + extension));
    QVERIFY(QFile::exists(filename));

    // a name that has an extension keeps it
    QString named = dir.filePath("named.dat");
    QVERIFY(FrameFileIO::saveFrameFileAs(named, saveIndex, &frames));
    QCOMPARE(named, dir.filePath("named.dat"));

    QVector<CommFrame> loaded;
    QVERIFY(FrameFileIO::loadFrameFileAs(filename, loadIndex, &loaded));
    QCOMPARE(loaded.count(), frames.count());
    for (int i = 0; i < frames.count(); i++)
    {
        QCOMPARE(loaded[i].frameId(), frames[i].frameId());
        QCOMPARE(loaded[i].payload(), frames[i].payload());
    }
}

void TestFileDialogSplit::testLogAutodetect()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QVector<CommFrame> frames = sampleFrames();

    QString filename = dir.filePath("log");
    QVERIFY(FrameFileIO::saveFrameFileAs(filename, 0, &frames));

    QVector<CommFrame> loaded;
    QVERIFY(FrameFileIO::loadFrameFileAs(filename, 0, &loaded));
    QCOMPARE(loaded.count(), frames.count());
    QCOMPARE(loaded.last().frameId(), frames.last().frameId());
}

void TestFileDialogSplit::testLogBadIndex()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QVector<CommFrame> frames = sampleFrames();

    // -1 is what QStringList::indexOf gives for a file type that isn't in the list
    QString filename = dir.filePath("log");
    QVERIFY(!FrameFileIO::saveFrameFileAs(filename, -1, &frames));
    QVERIFY(!FrameFileIO::saveFrameFileAs(filename, int(FrameFileIO::saveFilters().count()), &frames));
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files).count(), 0);

    QVERIFY(FrameFileIO::saveFrameFileAs(filename, 0, &frames));
    QVector<CommFrame> loaded;
    QVERIFY(!FrameFileIO::loadFrameFileAs(filename, -1, &loaded));
    QVERIFY(!FrameFileIO::loadFrameFileAs(filename, int(FrameFileIO::loadFilters().count()), &loaded));
    QVERIFY(loaded.isEmpty());
}

void TestFileDialogSplit::testDbcFilterLists()
{
    const QStringList load = DBCHandler::loadFilters();
    QCOMPARE(load.count(), 3);
    QCOMPARE(load.first(), QString("DBC File (*.dbc)"));
    QCOMPARE(DBCHandler::saveFilters(), QStringList{"DBC File (*.dbc)"});
}

void TestFileDialogSplit::testDbcSaveAndLoad()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DBCHandler *handler = DBCHandler::getReference();
    handler->removeAllFiles();

    handler->createBlankFile();
    DBC_MESSAGE *msg = new DBC_MESSAGE; //the file keeps it
    msg->ID = 0x123;
    msg->name = "TestMessage";
    msg->len = 8;
    msg->sender = handler->getFileByIdx(0)->findNodeByName("Vector__XXX");
    QVERIFY(handler->getFileByIdx(0)->messageHandler->addMessage(msg));

    QString filename = dir.filePath("signals");
    QVERIFY(handler->saveDBCFileAs(0, filename));
    QCOMPARE(filename, dir.filePath("signals.dbc"));
    QVERIFY(QFile::exists(filename));

    DBCFile *loaded = handler->loadFileAs(dir.filePath("signals"), 0); // the extension is added here too
    QVERIFY(loaded);
    QCOMPARE(handler->getFileCount(), 2);
    QVERIFY(loaded->messageHandler->findMsgByID(0x123));
    QCOMPARE(loaded->messageHandler->findMsgByID(0x123)->name, QString("TestMessage"));

    handler->removeAllFiles();
}

void TestFileDialogSplit::testDbcBadIndex()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DBCHandler *handler = DBCHandler::getReference();
    handler->removeAllFiles();

    QString filename = dir.filePath("signals");
    QVERIFY(!handler->saveDBCFileAs(0, filename)); // no file loaded
    QVERIFY(!QFile::exists(dir.filePath("signals.dbc")));

    handler->createBlankFile();
    QVERIFY(!handler->saveDBCFileAs(-1, filename));
    QVERIFY(!handler->saveDBCFileAs(1, filename));
    QVERIFY(handler->saveDBCFileAs(0, filename));

    QVERIFY(!handler->loadFileAs(filename, -1));
    QVERIFY(!handler->loadFileAs(filename, 3));
    QCOMPARE(handler->getFileCount(), 1);

    handler->removeAllFiles();
}

QTEST_MAIN(TestFileDialogSplit)
#include "test_file_dialog_split.moc"
