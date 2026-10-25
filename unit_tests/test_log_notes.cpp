#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QTimeZone>
#include "framefileio.h"
#include "lognotes.h"

// Notes saved with a log (LogNotes): the Markdown they're written as, the block at the end of a native CSV log, the
// <log>.notes.md file next to logs of other types, and FrameFileIO::saveLogNotes/loadLogNotes choosing between them.
class TestLogNotes : public QObject
{
    Q_OBJECT

private slots:
    void testMarkdownRoundTrip();
    void testMarkdownLayout();
    void testEmptyNotes();
    void testUnknownFieldsKept();
    void testNotNotes();
    void testSeconds_data();
    void testSeconds();
    void testAddMarker();
    void testSetFrames();
    void testNativeCSVRoundTrip();
    void testNativeCSVReplace();
    void testNativeCSVLongNotes();
    void testNativeCSVNoFrames();
    void testNativeCSVLineEnds();
    void testNativeCSVForeignComments();
    void testSidecar();
    void testBlockBeforeSidecar();
    void testNoNotes();
};

static QVector<CommFrame> sampleFrames()
{
    QVector<CommFrame> frames;
    for (int i = 0; i < 4; i++)
    {
        CommFrame frame;
        frame.setFrameId(0x100 + quint32(i));
        frame.setPayload(QByteArray::fromHex("0102030405060708"));
        frame.setTimeStamp(CommFrame::TimeStamp(0, 1000 * (i + 1)));
        frames.append(frame);
    }
    return frames;
}

//every field filled in, with commas, % signs, indentation and a sub-heading in the text
static LogNotes sampleNotes()
{
    LogNotes notes;
    notes.title = "E90 HVAC flaps: AC on/off cycle, bench 2";
    notes.author = "tomdebree";
    notes.tags = QStringList{"hvac", "lin"};
    notes.system = "BMW E90 HVAC module\nflap motors 1 to 4";
    notes.setup = "Tapped at the module connector, 120 ohm termination\nPower supply at 100% (13.8 V)";
    notes.scenario = "1. Ignition on\n2. AC on\n   - wait 5 s, then check the flaps\n3. AC off";
    notes.observations = "Motor 3 answers late.\n\n### Detail\nAbout 40 ms after the header.";
    notes.markers = {{2000, "AC on"}, {3500, ""}};
    notes.saved = QDateTime(QDate(2026, 10, 25), QTime(14, 3, 11), QTimeZone(3600));
    notes.savvycanVersion = "242";
    notes.logFile = "run1.csv";
    notes.frameCount = 4;
    notes.unfilteredCount = 10;
    notes.hasSpan = true;
    notes.firstUs = 1000;
    notes.lastUs = 4000;
    notes.buses = QStringList{"0: PCAN-USB Pro FD (PCAN_USBBUS1), 500 kbit/s", "1: APGDT001 LIN, 9600 bit/s"};
    notes.dbcFiles = QStringList{"bmw_e90.dbc (bus 0)"};
    notes.otherFields = {qMakePair(QString("bench"), QString("rig 2"))};
    notes.otherSections = {qMakePair(QString("Wiring"), QString("pin 3: CAN-H\npin 4: CAN-L"))};
    return notes;
}

static QByteArray readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QByteArray();
    return file.readAll();
}

static bool writeAll(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return file.write(data) == data.size();
}

static int saveIndexOf(const QString &name)
{
    const QStringList filters = FrameFileIO::saveFilters();
    for (int i = 0; i < filters.count(); i++)
        if (filters.at(i).startsWith(name)) return i;
    return -1;
}

static void compareFrames(const QVector<CommFrame> &loaded, const QVector<CommFrame> &saved)
{
    QCOMPARE(loaded.count(), saved.count());
    for (int i = 0; i < saved.count(); i++)
    {
        QCOMPARE(loaded.at(i).frameId(), saved.at(i).frameId());
        QCOMPARE(loaded.at(i).payload(), saved.at(i).payload());
        QCOMPARE(loaded.at(i).timeStamp().microSeconds(), saved.at(i).timeStamp().microSeconds());
    }
}

void TestLogNotes::testMarkdownRoundTrip()
{
    const LogNotes notes = sampleNotes();
    LogNotes loaded;
    QVERIFY(LogNotes::fromMarkdown(notes.toMarkdown(), loaded));
    QVERIFY(loaded == notes);
    QCOMPARE(loaded.toMarkdown(), notes.toMarkdown());

    //CRLF line ends, as a sidecar file edited on Windows has them
    LogNotes crlf;
    QVERIFY(LogNotes::fromMarkdown(notes.toMarkdown().replace("\n", "\r\n"), crlf));
    QVERIFY(crlf == notes);
}

//the layout described in docs/log-notes-roadmap.md
void TestLogNotes::testMarkdownLayout()
{
    const QStringList lines = sampleNotes().toMarkdown().split('\n');
    QCOMPARE(lines.at(0), QString("# SavvyCAN log notes v1"));
    QCOMPARE(lines.at(1), QString("title: E90 HVAC flaps: AC on/off cycle, bench 2"));
    QVERIFY(lines.contains("tags: hvac, lin"));
    QVERIFY(lines.contains("saved: 2026-10-25T14:03:11+01:00"));
    QVERIFY(lines.contains("frames: 4 (filtered from 10)"));
    QVERIFY(lines.contains("span: 0.001000 s to 0.004000 s"));
    QVERIFY(lines.contains("bench: rig 2"));
    QVERIFY(lines.contains("## Scenario"));
    QVERIFY(lines.contains("   - wait 5 s, then check the flaps"));
    QVERIFY(lines.contains("- 1: APGDT001 LIN, 9600 bit/s"));
    QVERIFY(lines.contains("- 0.002000 s: AC on"));
    QVERIFY(lines.contains("- 0.003500 s:"));
    QVERIFY(lines.indexOf("## Observations") < lines.indexOf("## Wiring"));
    QVERIFY(lines.indexOf("## Wiring") < lines.indexOf("## Buses"));
    QVERIFY(lines.indexOf("## Buses") < lines.indexOf("## Markers"));
    QCOMPARE(lines.last(), QString()); //ends with a line end
}

void TestLogNotes::testEmptyNotes()
{
    LogNotes notes;
    QVERIFY(notes.isEmpty());
    QCOMPARE(notes.toMarkdown(), QString("# SavvyCAN log notes v1\n"));
    LogNotes loaded = sampleNotes();
    QVERIFY(LogNotes::fromMarkdown(notes.toMarkdown(), loaded));
    QVERIFY(loaded.isEmpty());

    notes.frameCount = 0;
    QVERIFY(!notes.isEmpty());
}

//notes written by hand or by a newer SavvyCAN: keys and sections this version doesn't know survive saving again
void TestLogNotes::testUnknownFieldsKept()
{
    const QString markdown = "# SavvyCAN log notes v3\n"
                             "title: hand written\n"
                             "Test bench: rig 7\n"
                             "frames: lots\n"
                             "\n"
                             "## Setup\n"
                             "OBD port\n"
                             "## Calibration\n"
                             "\n"
                             "offset 3\n"
                             "\n"
                             "## Markers\n"
                             "- 1.5 s: start\n"
                             "not a marker\n"
                             "* -0.25 s: before the trigger\n";
    LogNotes notes;
    QVERIFY(LogNotes::fromMarkdown(markdown, notes));
    QCOMPARE(notes.title, QString("hand written"));
    QCOMPARE(notes.setup, QString("OBD port"));
    QCOMPARE(notes.frameCount, qint64(-1));
    QCOMPARE(notes.otherFields.count(), 2);
    QCOMPARE(notes.otherFields.at(0), qMakePair(QString("Test bench"), QString("rig 7")));
    QCOMPARE(notes.otherFields.at(1), qMakePair(QString("frames"), QString("lots"))); //didn't parse, kept as text
    QCOMPARE(notes.otherSections.count(), 1);
    QCOMPARE(notes.otherSections.at(0), qMakePair(QString("Calibration"), QString("offset 3")));
    QCOMPARE(notes.markers.count(), 2);
    QCOMPARE(notes.markers.at(0).timeUs, qint64(1500000));
    QCOMPARE(notes.markers.at(1).timeUs, qint64(-250000));
    QCOMPARE(notes.markers.at(1).text, QString("before the trigger"));

    LogNotes again;
    QVERIFY(LogNotes::fromMarkdown(notes.toMarkdown(), again));
    QVERIFY(again == notes);
    QVERIFY(notes.toMarkdown().contains("Test bench: rig 7\nframes: lots\n"));
}

void TestLogNotes::testNotNotes()
{
    LogNotes notes = sampleNotes();
    QVERIFY(!LogNotes::fromMarkdown("", notes));
    QVERIFY(!LogNotes::fromMarkdown("just some text\nwithout fields\n", notes));
    QVERIFY(notes == sampleNotes()); //left as it was
}

void TestLogNotes::testSeconds_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<qint64>("us");
    QTest::addColumn<QString>("formatted");

    QTest::newRow("zero") << "0.000000" << true << qint64(0) << "0.000000";
    QTest::newRow("whole") << "3" << true << qint64(3000000) << "3.000000";
    QTest::newRow("short fraction") << "12.4" << true << qint64(12400000) << "12.400000";
    QTest::newRow("microsecond") << "0.000001" << true << qint64(1) << "0.000001";
    QTest::newRow("past microseconds") << "1.1234567" << true << qint64(1123456) << "1.123456";
    QTest::newRow("negative") << "-1.5" << true << qint64(-1500000) << "-1.500000";
    QTest::newRow("negative fraction") << "-0.000250" << true << qint64(-250) << "-0.000250";
    QTest::newRow("large") << "86400.000001" << true << qint64(86400000001LL) << "86400.000001";
    QTest::newRow("spaces") << " 2.5 " << true << qint64(2500000) << "2.500000";
    QTest::newRow("empty") << "" << false << qint64(0) << "";
    QTest::newRow("letters") << "1.2e3" << false << qint64(0) << "";
    QTest::newRow("two dots") << "1.2.3" << false << qint64(0) << "";
}

void TestLogNotes::testSeconds()
{
    QFETCH(QString, text);
    QFETCH(bool, valid);
    QFETCH(qint64, us);
    QFETCH(QString, formatted);

    qint64 parsed = 0;
    QCOMPARE(LogNotes::parseSeconds(text, parsed), valid);
    if (!valid) return;
    QCOMPARE(parsed, us);
    QCOMPARE(LogNotes::formatSeconds(us), formatted);
}

//markers stay in time order, a new one going after any others at the same time
void TestLogNotes::testAddMarker()
{
    LogNotes notes;
    notes.addMarker({5000, "c"});
    notes.addMarker({1000, "a"});
    notes.addMarker({5000, "d"});
    notes.addMarker({3000, "b"});
    notes.addMarker({-200, "before"});
    QStringList order;
    for (const LogMarker &marker : notes.markers) order.append(marker.text);
    QCOMPARE(order, (QStringList{"before", "a", "b", "c", "d"}));
}

//frame count and time span from the frames, whatever their order
void TestLogNotes::testSetFrames()
{
    QVector<CommFrame> frames = sampleFrames();
    std::swap(frames[0], frames[3]);
    LogNotes notes;
    notes.setFrames(frames, 10);
    QCOMPARE(notes.frameCount, qint64(4));
    QCOMPARE(notes.unfilteredCount, qint64(10));
    QVERIFY(notes.hasSpan);
    QCOMPARE(notes.firstUs, qint64(1000));
    QCOMPARE(notes.lastUs, qint64(4000));

    notes.setFrames(QVector<CommFrame>());
    QCOMPARE(notes.frameCount, qint64(0));
    QCOMPARE(notes.unfilteredCount, qint64(-1));
    QVERIFY(!notes.hasSpan);
}

void TestLogNotes::testNativeCSVRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("run1.csv");
    const QVector<CommFrame> frames = sampleFrames();
    const LogNotes notes = sampleNotes();

    QVERIFY(FrameFileIO::saveFrameFileAs(path, 0, &frames));
    QVERIFY(FrameFileIO::saveLogNotes(path, 0, notes));
    QVERIFY(!QFile::exists(LogNotes::sidecarPath(path))); //in the log itself

    //the notes come after the frames, and no notes line has a comma for an older loader to split as a frame
    const QList<QByteArray> lines = readAll(path).split('\n');
    QVERIFY(lines.at(4).startsWith("4000,"));
    QVERIFY(lines.at(5).startsWith("# # SavvyCAN log notes v1"));
    for (int i = 5; i < lines.count(); i++)
    {
        if (lines.at(i).isEmpty()) continue;
        QVERIFY2(lines.at(i).startsWith('#'), lines.at(i).constData());
        QVERIFY2(!lines.at(i).contains(','), lines.at(i).constData());
    }
    QVERIFY(readAll(path).contains("# Power supply at 100%25 (13.8 V)"));
    QVERIFY(readAll(path).contains("# title: E90 HVAC flaps: AC on/off cycle%2C bench 2"));

    //the frames load as before, without errors, as native CSV and by autodetect
    QVector<CommFrame> loaded;
    QVERIFY(FrameFileIO::isNativeCSVFile(path));
    QVERIFY(FrameFileIO::loadFrameFileAs(path, 1, &loaded));
    compareFrames(loaded, frames);
    loaded.clear();
    QVERIFY(FrameFileIO::loadFrameFileAs(path, 0, &loaded));
    compareFrames(loaded, frames);

    LogNotes loadedNotes;
    QVERIFY(FrameFileIO::loadLogNotes(path, loadedNotes));
    QVERIFY(loadedNotes == notes);
}

//saving notes again replaces the block; empty notes remove it, leaving the log as it was saved
void TestLogNotes::testNativeCSVReplace()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("run1.csv");
    const QVector<CommFrame> frames = sampleFrames();
    QVERIFY(FrameFileIO::saveFrameFileAs(path, 0, &frames));
    const QByteArray plain = readAll(path);

    QVERIFY(FrameFileIO::saveLogNotes(path, 0, sampleNotes()));
    LogNotes second;
    second.title = "second notes";
    QVERIFY(FrameFileIO::saveLogNotes(path, 0, second));
    QCOMPARE(readAll(path).count("SavvyCAN log notes"), 1);
    QCOMPARE(readAll(path), plain + "# # SavvyCAN log notes v1\n# title: second notes\n");

    LogNotes loaded;
    QVERIFY(FrameFileIO::loadLogNotes(path, loaded));
    QVERIFY(loaded == second);

    QVERIFY(FrameFileIO::saveLogNotes(path, 0, LogNotes()));
    QCOMPARE(readAll(path), plain);
    QVERIFY(!FrameFileIO::loadLogNotes(path, loaded));
}

//notes bigger than the chunks the end of the file is read back in
void TestLogNotes::testNativeCSVLongNotes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("long.csv");
    QVector<CommFrame> frames;
    for (int i = 0; i < 5000; i++)
    {
        CommFrame frame;
        frame.setFrameId(quint32(i & 0x7FF));
        frame.setPayload(QByteArray::fromHex("A1A2A3A4A5A6A7A8"));
        frame.setTimeStamp(CommFrame::TimeStamp(0, 1000 + i));
        frames.append(frame);
    }
    LogNotes notes = sampleNotes();
    QStringList steps;
    for (int i = 0; i < 6000; i++) steps.append(QString("%1. step, with a comma, number %1").arg(i));
    notes.scenario = steps.join('\n'); //about 200 kB
    for (int i = 0; i < 300; i++) notes.markers.append({qint64(i) * 1000, QString("marker %1").arg(i)});

    QVERIFY(FrameFileIO::saveFrameFileAs(path, 0, &frames));
    QVERIFY(FrameFileIO::saveLogNotes(path, 0, notes));

    LogNotes loaded;
    QVERIFY(FrameFileIO::loadLogNotes(path, loaded));
    QVERIFY(loaded == notes);
    QVector<CommFrame> loadedFrames;
    QVERIFY(FrameFileIO::loadNativeCSVFile(path, &loadedFrames));
    QCOMPARE(loadedFrames.count(), frames.count());

    //and replacing them still cuts at the right place
    LogNotes small;
    small.title = "small";
    QVERIFY(FrameFileIO::saveLogNotes(path, 0, small));
    QVERIFY(FrameFileIO::loadLogNotes(path, loaded));
    QVERIFY(loaded == small);
    loadedFrames.clear();
    QVERIFY(FrameFileIO::loadNativeCSVFile(path, &loadedFrames));
    QCOMPARE(loadedFrames.count(), frames.count());
}

//a log with notes and no frames (a capture that got nothing) is still a native CSV log
void TestLogNotes::testNativeCSVNoFrames()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("empty.csv");
    const QVector<CommFrame> frames;
    QVERIFY(FrameFileIO::saveFrameFileAs(path, 0, &frames));
    QVERIFY(FrameFileIO::saveLogNotes(path, 0, sampleNotes()));

    QVERIFY(FrameFileIO::isNativeCSVFile(path));
    QVector<CommFrame> loaded;
    QVERIFY(FrameFileIO::loadNativeCSVFile(path, &loaded));
    QCOMPARE(loaded.count(), 0);
    LogNotes notes;
    QVERIFY(FrameFileIO::loadLogNotes(path, notes));
    QVERIFY(notes == sampleNotes());
}

//logs saved on Windows have CRLF line ends, and the notes block uses them too; a last line without a line end gets one
void TestLogNotes::testNativeCSVLineEnds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QByteArray header = "Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8";
    const QByteArray row1 = "1000,00000100,false,Rx,0,8,01,02,03,04,05,06,07,08,";
    const QByteArray row2 = "2000,00000101,false,Rx,0,8,01,02,03,04,05,06,07,08,";
    LogNotes notes;
    notes.title = "line ends";
    notes.setup = "a, b";

    const QString crlfPath = dir.filePath("crlf.csv");
    QVERIFY(writeAll(crlfPath, header + "\r\n" + row1 + "\r\n" + row2 + "\r\n"));
    QVERIFY(FrameFileIO::saveLogNotes(crlfPath, 0, notes));
    QCOMPARE(readAll(crlfPath), header + "\r\n" + row1 + "\r\n" + row2 + "\r\n"
             + "# # SavvyCAN log notes v1\r\n# title: line ends\r\n#\r\n# ## Setup\r\n# a%2C b\r\n");
    LogNotes loaded;
    QVERIFY(FrameFileIO::loadLogNotes(crlfPath, loaded));
    QVERIFY(loaded == notes);

    const QString openPath = dir.filePath("noend.csv");
    QVERIFY(writeAll(openPath, header + "\n" + row1 + "\n" + row2));
    QVERIFY(FrameFileIO::saveLogNotes(openPath, 0, notes));
    QVERIFY(readAll(openPath).startsWith(header + "\n" + row1 + "\n" + row2 + "\n# # SavvyCAN log notes v1\n"));
    QVector<CommFrame> frames;
    QVERIFY(FrameFileIO::loadNativeCSVFile(openPath, &frames));
    QCOMPARE(frames.count(), 2);
    QVERIFY(FrameFileIO::loadLogNotes(openPath, loaded));
    QVERIFY(loaded == notes);
}

//'#' lines that aren't SavvyCAN's notes aren't read as notes, and saving notes keeps them
void TestLogNotes::testNativeCSVForeignComments()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("comments.csv");
    const QByteArray log = "Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n"
                           "1000,00000100,false,Rx,0,8,01,02,03,04,05,06,07,08,\n"
                           "# exported by some script\n";
    QVERIFY(writeAll(path, log));

    LogNotes notes;
    QVERIFY(!FrameFileIO::loadLogNotes(path, notes));

    LogNotes mine;
    mine.title = "mine";
    QVERIFY(FrameFileIO::saveLogNotes(path, 0, mine));
    QCOMPARE(readAll(path), log + "# # SavvyCAN log notes v1\n# title: mine\n");
    QVERIFY(FrameFileIO::loadLogNotes(path, notes));
    QVERIFY(notes == mine);
}

void TestLogNotes::testSidecar()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const int candump = saveIndexOf("Candump");
    QVERIFY(candump > 0);
    QString path = dir.filePath("run1.log");
    const QVector<CommFrame> frames = sampleFrames();
    QVERIFY(FrameFileIO::saveFrameFileAs(path, candump, &frames));
    const QByteArray plain = readAll(path);

    //no notes, no file
    QVERIFY(FrameFileIO::saveLogNotes(path, candump, LogNotes()));
    QVERIFY(!QFile::exists(LogNotes::sidecarPath(path)));

    const LogNotes notes = sampleNotes();
    QVERIFY(FrameFileIO::saveLogNotes(path, candump, notes));
    QCOMPARE(LogNotes::sidecarPath(path), path + ".notes.md");
    QVERIFY(QFile::exists(path + ".notes.md"));
    QCOMPARE(readAll(path), plain); //the log itself isn't touched
    QVERIFY(readAll(path + ".notes.md").contains("title: E90 HVAC flaps: AC on/off cycle, bench 2")); //plain commas

    LogNotes loaded;
    QVERIFY(FrameFileIO::loadLogNotes(path, loaded));
    QVERIFY(loaded == notes);

    //each log has its own notes file, even with the same base name
    QVERIFY(!FrameFileIO::loadLogNotes(dir.filePath("run1.csv"), loaded));
}

//a native CSV log's own notes win over a notes file next to it; the file is used when the log has none
void TestLogNotes::testBlockBeforeSidecar()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("run1.csv");
    const QVector<CommFrame> frames = sampleFrames();
    QVERIFY(FrameFileIO::saveFrameFileAs(path, 0, &frames));

    LogNotes handWritten;
    handWritten.title = "written by hand";
    QVERIFY(handWritten.writeSidecar(path));
    LogNotes loaded;
    QVERIFY(FrameFileIO::loadLogNotes(path, loaded));
    QVERIFY(loaded == handWritten);

    QVERIFY(FrameFileIO::saveLogNotes(path, 0, sampleNotes()));
    QVERIFY(FrameFileIO::loadLogNotes(path, loaded));
    QVERIFY(loaded == sampleNotes());
}

void TestLogNotes::testNoNotes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    LogNotes notes;
    QVERIFY(!FrameFileIO::loadLogNotes(dir.filePath("missing.csv"), notes));

    QString path = dir.filePath("plain.csv");
    const QVector<CommFrame> frames = sampleFrames();
    QVERIFY(FrameFileIO::saveFrameFileAs(path, 0, &frames));
    QVERIFY(!FrameFileIO::loadLogNotes(path, notes));
    QVERIFY(notes.isEmpty());

    QVERIFY(writeAll(dir.filePath("empty.csv"), QByteArray()));
    QVERIFY(!FrameFileIO::loadLogNotes(dir.filePath("empty.csv"), notes));
    QVERIFY(LogNotes().writeToNativeCSV(dir.filePath("empty.csv")));
}

QTEST_MAIN(TestLogNotes)
#include "test_log_notes.moc"
