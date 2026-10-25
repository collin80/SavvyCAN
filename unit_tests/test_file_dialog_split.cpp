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
    void testMicrochipBadLengths();
    void testWiresharkBadLengths();
    void testWiresharkLongRecords();
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

// Microchip lines whose byte count doesn't match the bytes given: no more than 8, and no more than the line has
void TestFileDialogSplit::testMicrochipBadLengths()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filename = dir.filePath("bad.log");
    QFile file(filename);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("//---------------------------------\n"
               "Microchip Technology Inc.\n"
               "//---------------------------------\n"
               "1000;RX;0x100;8;0x01;0x02;0x03;\n"     // 8 bytes claimed, 3 given
               "2000;RX;0x101;8;0x04;0x05\n"           // the same without the ; at the end
               "3000;RX;0x102;12;0x01;0x02;0x03;0x04;0x05;0x06;0x07;0x08;0x09;0x0A;0x0B;0x0C;\n" // more than 8
               "4000;RX;0x103;-1;0x01;\n"              // a negative count
               "5000;RX;0x104;5;\n");                  // a count and no bytes
    file.close();

    QVector<CommFrame> loaded;
    QVERIFY(FrameFileIO::loadMicrochipFile(filename, &loaded));
    QCOMPARE(loaded.count(), 5);
    QCOMPARE(loaded[0].payload(), QByteArray::fromHex("010203"));
    QCOMPARE(loaded[1].payload(), QByteArray::fromHex("0405"));
    QCOMPARE(loaded[2].payload(), QByteArray::fromHex("0102030405060708"));
    QCOMPARE(loaded[3].payload(), QByteArray());
    QCOMPARE(loaded[4].payload(), QByteArray());
    for (int i = 0; i < loaded.count(); i++) QCOMPARE(loaded[i].frameId(), 0x100u + quint32(i));
}

// pcapng blocks are padded to 4 bytes
static QByteArray padded4(QByteArray data)
{
    while (data.length() % 4) data += char(0);
    return data;
}

// A pcap (or pcapng) file of Linux cooked capture records: 16 bytes, then the SocketCAN header (ID, length, 3 bytes
// of padding) and the data. Each record is the byte count it claims and the data it has. The record at longRecordAt
// is padded to 300 bytes, more than pcaplite's 256 byte buffer.
static void writeWiresharkFile(const QString &filename, const QList<QPair<int, QByteArray>> &records,
                               int shortRecordAt = -1, bool ng = false, int longRecordAt = -1)
{
    QByteArray out;
    QDataStream stream(&out, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    if (ng)
    {
        // section header block, then an interface description block with microsecond timestamps, a 300 byte
        // comment and the end of the options
        stream << quint32(0x0A0D0D0A) << quint32(28) << quint32(0x1A2B3C4D) << quint16(1) << quint16(0)
               << qint64(-1) << quint32(28);
        QByteArray options;
        QDataStream os(&options, QIODevice::WriteOnly);
        os.setByteOrder(QDataStream::LittleEndian);
        os << quint16(9) << quint16(1) << quint8(6) << quint8(0) << quint16(0);
        os << quint16(1) << quint16(300);
        os.writeRawData(QByteArray(300, 'x').constData(), 300);
        os << quint16(0) << quint16(0);
        stream << quint32(1) << quint32(20 + options.length()) << quint16(113) << quint16(0) << quint32(65535);
        stream.writeRawData(options.constData(), int(options.length()));
        stream << quint32(20 + options.length());
    }
    else
    {
        stream << quint32(0xA1B2C3D4) << quint16(2) << quint16(4) << qint32(0) << quint32(0) << quint32(65535)
               << quint32(113);
    }
    for (int i = 0; i < records.count(); i++)
    {
        QByteArray packet(16, 0);
        QDataStream ps(&packet, QIODevice::WriteOnly | QIODevice::Append);
        ps.setByteOrder(QDataStream::LittleEndian);
        ps << quint32(0x200 + i) << quint8(records[i].first) << quint8(0) << quint8(0) << quint8(0);
        packet += records[i].second;
        if (i == shortRecordAt) packet = packet.left(20); // cut off in the middle of the CAN header
        if (i == longRecordAt) packet = packet.leftJustified(300, char(0));
        if (ng)
        {
            // enhanced packet block
            const QByteArray data = padded4(packet);
            stream << quint32(6) << quint32(32 + data.length()) << quint32(0) << quint32(0) << quint32(1000 * i)
                   << quint32(packet.length()) << quint32(packet.length());
            stream.writeRawData(data.constData(), int(data.length()));
            stream << quint32(32 + data.length());
        }
        else
        {
            stream << quint32(1) << quint32(1000 * i) << quint32(packet.length()) << quint32(packet.length());
            stream.writeRawData(packet.constData(), int(packet.length()));
        }
    }
    QFile file(filename);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(out);
}

// Wireshark records whose byte count doesn't match the data: classic CAN, so no more than 8, and no more than the
// record has
void TestFileDialogSplit::testWiresharkBadLengths()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QByteArray sixtyFour(64, char(0x55));
    const QList<QPair<int, QByteArray>> records = {
        {2, QByteArray::fromHex("AABB")},            // as it should be
        {8, QByteArray::fromHex("010203")},          // 8 bytes claimed, 3 given
        {64, sixtyFour},                             // more than 8
        {0xF0, QByteArray::fromHex("0102030405060708")}, // negative as a signed char
        {5, QByteArray()},                           // a count and no bytes
    };
    const QString filename = dir.filePath("bad.pcap");
    writeWiresharkFile(filename, records);
    QVERIFY(FrameFileIO::isWiresharkFile(filename));

    QVector<CommFrame> loaded;
    QVERIFY(FrameFileIO::loadWiresharkFile(filename, &loaded));
    QCOMPARE(loaded.count(), records.count());
    QCOMPARE(loaded[0].payload(), QByteArray::fromHex("AABB"));
    QCOMPARE(loaded[1].payload(), QByteArray::fromHex("010203"));
    QCOMPARE(loaded[2].payload(), sixtyFour.left(8));
    QCOMPARE(loaded[3].payload(), QByteArray::fromHex("0102030405060708"));
    QCOMPARE(loaded[4].payload(), QByteArray());
    for (int i = 0; i < loaded.count(); i++) QCOMPARE(loaded[i].frameId(), 0x200u + quint32(i));

    // a record too short for the CAN header is skipped, and the file counts as having errors
    const QString shortName = dir.filePath("short.pcap");
    writeWiresharkFile(shortName, records, 1);
    loaded.clear();
    QVERIFY(!FrameFileIO::loadWiresharkFile(shortName, &loaded));
    QCOMPARE(loaded.count(), records.count() - 1);
    QCOMPARE(loaded[1].frameId(), 0x202u);
    QCOMPARE(loaded[1].payload(), sixtyFour.left(8));
}

// Wireshark records longer than pcaplite's 256 byte buffer are skipped, in pcap and pcapng files; a record or block
// with an impossible length ends the file
void TestFileDialogSplit::testWiresharkLongRecords()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QList<QPair<int, QByteArray>> records = {
        {2, QByteArray::fromHex("AABB")},
        {8, QByteArray::fromHex("0102030405060708")},
        {3, QByteArray::fromHex("CCDDEE")},
    };
    for (const bool ng : {false, true})
    {
        const QString filename = dir.filePath(ng ? "long.pcapng" : "long.pcap");
        writeWiresharkFile(filename, records, -1, ng, 1);
        QVERIFY(FrameFileIO::isWiresharkFile(filename));

        QVector<CommFrame> loaded;
        QVERIFY(FrameFileIO::loadWiresharkFile(filename, &loaded));
        QCOMPARE(loaded.count(), 2);
        QCOMPARE(loaded[0].frameId(), 0x200u);
        QCOMPARE(loaded[0].payload(), QByteArray::fromHex("AABB"));
        QCOMPARE(loaded[1].frameId(), 0x202u);
        QCOMPARE(loaded[1].payload(), QByteArray::fromHex("CCDDEE"));

        // then a record (pcap) or block (pcapng) with an impossible length: the good records after it aren't loaded
        QFile file(filename);
        QVERIFY(file.open(QIODevice::Append));
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        if (ng) stream << quint32(3) << quint32(0) << quint32(0);       // block of 0 bytes
        else stream << quint32(1) << quint32(0) << quint32(0xFFFFFFF0) << quint32(0xFFFFFFF0); // 4 GB record
        file.close();
        QFile header(dir.filePath("header"));
        writeWiresharkFile(header.fileName(), {}, -1, ng);
        QFile good(dir.filePath("good"));
        writeWiresharkFile(good.fileName(), records, -1, ng);
        QVERIFY(good.open(QIODevice::ReadOnly));
        QVERIFY(file.open(QIODevice::Append));
        file.write(good.readAll().mid(header.size()));
        file.close();

        loaded.clear();
        QVERIFY(FrameFileIO::loadWiresharkFile(filename, &loaded));
        QCOMPARE(loaded.count(), 2);
        QCOMPARE(loaded[1].frameId(), 0x202u);
    }
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
