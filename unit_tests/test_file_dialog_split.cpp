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
    void testCANDOFile();
    void testDbcFilterLists();
    void testDbcSaveAndLoad();
    void testDbcBadIndex();
};

//8 data bytes, then shorter frames down to the shortest given (none by default)
static QVector<CommFrame> sampleFrames(int shortest = 0)
{
    QVector<CommFrame> frames;
    const int lengths[] = {8, 8, 8, 8, 7, 3, 1, 0};
    for (int i = 0; i < int(std::size(lengths)); i++)
    {
        if (lengths[i] < shortest) continue;
        CommFrame frame;
        frame.setFrameId(0x100 + quint32(i));
        frame.setPayload((QByteArray::fromHex("0102030405060708") + char(i)).right(8).left(lengths[i]));
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
    QCOMPARE(save.count(), 13);
    QVERIFY(load.first().startsWith("Autodetect"));
    QVERIFY(save.first().startsWith("GVRET"));
    // Vehicle Spy files can be loaded but not saved
    QVERIFY(load.filter("Vehicle Spy").count() == 1);
    QVERIFY(save.filter("Vehicle Spy").isEmpty());
}

// Each save type, the extension it adds, the load type that reads it back, and the shortest frame that comes back
// as it was. Below 1 or 8 is a known issue in that format (see FORK-CHANGES.md):
// - Vector trace: loading a frame with no data crashes;
// - IXXAT MiniLog: a frame with no data loads back with one byte;
// - Cabana: shorter frames load back padded to 8 bytes;
// - PCAN: a frame with no data isn't loaded.
void TestFileDialogSplit::testLogSaveAndLoad_data()
{
    QTest::addColumn<int>("saveIndex");
    QTest::addColumn<QString>("extension");
    QTest::addColumn<int>("loadIndex");
    QTest::addColumn<int>("shortest");

    QTest::newRow("GVRET") << 0 << ".csv" << 1 << 0;
    QTest::newRow("CRTD") << 1 << ".txt" << 2 << 0;
    QTest::newRow("Generic CSV") << 2 << ".csv" << 18 << 0;
    QTest::newRow("BusMaster") << 3 << ".log" << 3 << 0;
    QTest::newRow("Microchip") << 4 << ".log" << 4 << 0;
    QTest::newRow("Vector trace") << 5 << ".trace" << 5 << 1;
    QTest::newRow("IXXAT MiniLog") << 6 << ".csv" << 6 << 1;
    QTest::newRow("CAN-DO") << 7 << ".can" << 7 << 0;
    QTest::newRow("Candump") << 8 << ".log" << 9 << 0;
    QTest::newRow("Cabana") << 9 << ".csv" << 19 << 8;
    QTest::newRow("CANalyzer ASC") << 10 << ".asc" << 14 << 0;
    QTest::newRow("CARBUS") << 11 << ".trc" << 16 << 0;
    QTest::newRow("PCAN") << 12 << ".trc" << 11 << 1;
}

void TestFileDialogSplit::testLogSaveAndLoad()
{
    QFETCH(int, saveIndex);
    QFETCH(QString, extension);
    QFETCH(int, loadIndex);
    QFETCH(int, shortest);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QVector<CommFrame> frames = sampleFrames(shortest);

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

// What the round trip above doesn't cover for CAN-DO: the record it starts with, frames it can't hold, and its time
// stamp, which counts up to a minute then starts again
void TestFileDialogSplit::testCANDOFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVector<CommFrame> frames;
    const qint64 times[] = {59500000, 59999000, 60250000, 61000000}; // microseconds
    for (int i = 0; i < 4; i++)
    {
        CommFrame frame;
        frame.setFrameId(0x7F0 + quint32(i));
        frame.setPayload(QByteArray(i + 1, char(0x80 + i))); // bytes and lengths that are negative as a signed char
        frame.setTimeStamp(CommFrame::TimeStamp(0, times[i]));
        frames.append(frame);
    }
    CommFrame extended = frames[1];
    extended.setExtendedFrameFormat(true);
    extended.setFrameId(0x12345678);
    frames.insert(2, extended);

    const QString filename = dir.filePath("log.can");
    QVERIFY(FrameFileIO::saveCANDOFile(filename, &frames));
    QCOMPARE(QFileInfo(filename).size(), qint64(12 * 5)); // the initial record, then each 11 bit frame
    QVERIFY(FrameFileIO::isCANDOFile(filename));

    QVector<CommFrame> loaded;
    QVERIFY(FrameFileIO::loadCANDOFile(filename, &loaded));
    frames.remove(2);
    QCOMPARE(loaded.count(), frames.count());
    for (int i = 0; i < frames.count(); i++)
    {
        QCOMPARE(loaded[i].frameId(), frames[i].frameId());
        QCOMPARE(loaded[i].payload(), frames[i].payload());
        // to the millisecond, and counted on past the minute
        QCOMPARE(loaded[i].timeStamp().microSeconds(), times[i]);
    }

    // a file whose length isn't a whole number of records
    QFile file(filename);
    QVERIFY(file.open(QIODevice::Append));
    file.write("abc");
    file.close();
    QVERIFY(!FrameFileIO::isCANDOFile(filename));
    loaded.clear();
    QVERIFY(!FrameFileIO::loadCANDOFile(filename, &loaded));
    QCOMPARE(loaded.count(), frames.count());

    // Vehicle Spy files can't be saved
    QVERIFY(!FrameFileIO::saveVehicleSpyFile(dir.filePath("spy.csv"), &frames));
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
