#include <QtTest>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTimer>
#include "dbc/dbchandler.h"

class TestMultiplexRoundTrip : public QObject
{
    Q_OBJECT
    QTemporaryDir temporary;
    QTimer dialogWatcher;
    QStringList warnings;

    QString path(const QString &suffix)
    {
        QString root = QString::fromLocal8Bit(qgetenv("SAVVYCAN_MUX_ARTIFACT_DIR"));
        if (root.isEmpty()) root = temporary.path();
        QDir().mkpath(root);
        return root + "/" + QTest::currentTestFunction() + "-"
            + QString::fromLatin1(QTest::currentDataTag() ? QTest::currentDataTag() : "case") + suffix;
    }
    static QString input(int fallback, const QString &extra, bool multiplexed = true)
    {
        return QString("VERSION \"\"\nNS_ :\nBS_:\nBU_: Vector__XXX\n\n"
            "BO_ 2147485700 MuxMessage: 8 Vector__XXX\n"
            " SG_ cellID M : 0|32@1+ (1,0) [0|4294967295] \"\" Vector__XXX\n"
            " SG_ temperatureSTEF%1 : 32|16@1+ (1,0) [0|65535] \"\" Vector__XXX\n\n%2")
            .arg(multiplexed ? " m" + QString::number(fallback) : QString(), extra);
    }
    static QString extended(const QString &ranges)
    {
        return "SG_MUL_VAL_ 2147485700 temperatureSTEF cellID " + ranges + ";\n";
    }
    static bool write(const QString &filename, const QString &text)
    {
        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly)) return false;
        return file.write(text.toUtf8()) == text.toUtf8().size();
    }
    static void verifySignal(DBCFile &file, const QString &expected,
                             const QList<int> &bounds, bool multiplexed, bool extendedRanges = false)
    {
        auto message = file.messageHandler->findMsgByID(2052);
        QVERIFY(message);
        auto signal = message->sigHandler->findSignalByName("temperatureSTEF");
        auto parent = message->sigHandler->findSignalByName("cellID");
        QVERIFY(signal);
        QVERIFY(parent);
        QCOMPARE(signal->isMultiplexed, multiplexed);
        QCOMPARE(signal->multiplexDbcString(), expected);
        QString uiExpected = expected;
        uiExpected.replace(", ", ";");
        // UI omits the redundant '-N' for singleton ranges.
        for (int i = 0; i < bounds.size(); i += 2)
            if (bounds[i] == bounds[i + 1])
                uiExpected.replace(QString("%1-%1").arg(bounds[i]), QString::number(bounds[i]));
        QCOMPARE(signal->multiplexDbcString(DBC_SIGNAL::MuxStringFormat_UI), uiExpected);
        QCOMPARE(parent->multiplexedChildren.count(signal), multiplexed ? 1 : 0);
        QCOMPARE(signal->multiplexParent, multiplexed ? parent : nullptr);
        QCOMPARE(signal->hasExtendedMultiplexing, extendedRanges);
        const QList<int> values = {-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,
                                  127,128,129,65534,65535,65536,2147483646,2147483647};
        for (int value : values)
        {
            bool accepted = false;
            for (int i = 0; i < bounds.size(); i += 2)
                accepted = accepted || (value >= bounds[i] && value <= bounds[i + 1]);
            QCOMPARE(signal->isValueMatchingMultiplex(value), accepted);
        }
    }
private slots:
    void initTestCase()
    {
        QVERIFY(temporary.isValid());
        QCoreApplication::setOrganizationName("SavvyCANTests");
        QCoreApplication::setApplicationName("MultiplexRoundTrip");
        // Record actual loader warnings and dismiss their modal dialogs.
        // A successful loadFile return alone does not mean a clean parse.
        connect(&dialogWatcher, &QTimer::timeout, this, [this] {
            for (QWidget *widget : QApplication::topLevelWidgets())
                if (auto box = qobject_cast<QMessageBox *>(widget))
                {
                    warnings.append(box->text());
                    box->accept();
                }
        });
        dialogWatcher.start(1);
    }
    void init() { warnings.clear(); }
    void loadRanges_data()
    {
        QTest::addColumn<int>("fallback");
        QTest::addColumn<QString>("extra");
        QTest::addColumn<QString>("expected");
        QTest::addColumn<QList<int>>("bounds");
        QTest::addColumn<bool>("multiplexed");
        QTest::newRow("reported-zero-to128") << 0 << extended("0-128") << QString("0-128") << QList<int>{0,128} << true;
        QTest::newRow("nonzero-excludes-fallback") << 0 << extended("7-12") << QString("7-12") << QList<int>{7,12} << true;
        QTest::newRow("disjoint-ranges") << 0 << extended("1-3, 7-9") << QString("1-3, 7-9") << QList<int>{1,3,7,9} << true;
        QTest::newRow("multiple-singletons") << 3 << extended("3-3, 7-7") << QString("3-3, 7-7") << QList<int>{3,3,7,7} << true;
        QTest::newRow("explicit-singleton") << 0 << extended("5-5") << QString("5-5") << QList<int>{5,5} << true;
        QTest::newRow("explicit-matching-singleton") << 5 << extended("5-5") << QString("5-5") << QList<int>{5,5} << true;
        QTest::newRow("simple-zero") << 0 << QString() << QString("0-0") << QList<int>{0,0} << true;
        QTest::newRow("simple-seven") << 7 << QString() << QString("7-7") << QList<int>{7,7} << true;
        QTest::newRow("duplicate-entry") << 0 << extended("0-128").repeated(2) << QString("0-128") << QList<int>{0,128} << true;
        QTest::newRow("split-entries") << 0 << extended("1-3") + extended("7-9") << QString("1-3, 7-9") << QList<int>{1,3,7,9} << true;
        QTest::newRow("maximum-supported-selector") << 0 << extended("2147483646-2147483647") << QString("2147483646-2147483647") << QList<int>{2147483646,2147483647} << true;
        QTest::newRow("ordinary-signal") << 0 << QString() << QString() << QList<int>{} << false;
    }
    void loadRanges()
    {
        QFETCH(int, fallback); QFETCH(QString, extra); QFETCH(QString, expected);
        QFETCH(QList<int>, bounds); QFETCH(bool, multiplexed);
        const QString filename = path(".dbc");
        QVERIFY(write(filename, input(fallback, extra, multiplexed)));
        DBCFile file;
        QVERIFY(file.loadFile(filename));
        verifySignal(file, expected, bounds, multiplexed, !extra.isEmpty());
        QCOMPARE(warnings.size(), 0);
    }
    void roundTrip_data() { loadRanges_data(); }
    void roundTrip()
    {
        QFETCH(int, fallback); QFETCH(QString, extra); QFETCH(QString, expected);
        QFETCH(QList<int>, bounds); QFETCH(bool, multiplexed);
        QString filename = path("-original.dbc");
        QVERIFY(write(filename, input(fallback, extra, multiplexed)));
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            DBCFile file;
            QVERIFY(file.loadFile(filename));
            const QString saved = path(QString("-saved%1.dbc").arg(cycle));
            QVERIFY(file.saveFile(saved));
            DBCFile reloaded;
            QVERIFY(reloaded.loadFile(saved));
            qInfo() << "cycle" << cycle << "loader warning dialogs" << warnings;
            QCOMPARE(warnings.size(), 0);
            verifySignal(reloaded, expected, bounds, multiplexed, !extra.isEmpty());
            filename = saved;
        }
    }
    void singletonKeepsExplicitParent()
    {
        QString text = input(5, extended("5-5"));
        text.replace("\n\nSG_MUL_VAL_", "\n SG_ otherID M : 56|8@1+ (1,0) [0|255] \"\" Vector__XXX\n\nSG_MUL_VAL_");
        QString filename = path("-original.dbc");
        QVERIFY(write(filename, text));
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            DBCFile file;
            QVERIFY(file.loadFile(filename));
            const QString saved = path(QString("-saved%1.dbc").arg(cycle));
            QVERIFY(file.saveFile(saved));
            DBCFile reloaded;
            QVERIFY(reloaded.loadFile(saved));
            verifySignal(reloaded, "5-5", {5,5}, true, true);
            QCOMPARE(warnings.size(), 0);
            filename = saved;
        }
    }
    void invalidLaterEntry_data()
    {
        QTest::addColumn<bool>("differentParent");
        QTest::newRow("malformed-after-valid") << false;
        QTest::newRow("conflicting-parent") << true;
    }
    void invalidLaterEntry()
    {
        QFETCH(bool, differentParent);
        const QString bad = differentParent
            ? "SG_MUL_VAL_ 2147485700 temperatureSTEF otherID 1-3;\n"
            : extended("1-3, broken");
        QString text = input(0, extended("7-9") + bad);
        text.replace("\n\nSG_MUL_VAL_", "\n SG_ otherID M : 56|8@1+ (1,0) [0|255] \"\" Vector__XXX\n\nSG_MUL_VAL_");
        const QString filename = path(".dbc");
        QVERIFY(write(filename, text));
        DBCFile file;
        QVERIFY(file.loadFile(filename));
        QCOMPARE(warnings.size(), 1);
        verifySignal(file, "7-9", {7,9}, true, true);
        auto message = file.messageHandler->findMsgByID(2052);
        QVERIFY(message);
        auto other = message->sigHandler->findSignalByName("otherID");
        QVERIFY(other);
        QVERIFY(other->multiplexedChildren.isEmpty());
    }
    void addedSingletonsRequireExtendedOutput()
    {
        DBC_SIGNAL signal;
        signal.addMultiplexRange(3,3);
        signal.addMultiplexRange(7,7);
        QVERIFY(signal.hasExtendedMultiplexing);
        QCOMPARE(signal.multiplexDbcString(), QString("3-3, 7-7"));
        QCOMPARE(signal.multiplexDbcString(DBC_SIGNAL::MuxStringFormat_UI), QString("3;7"));
    }
    void malformedRanges_data()
    {
        QTest::addColumn<QString>("ranges");
        QTest::newRow("space-separated") << QString("1-2 5-6");
        QTest::newRow("bad-suffix") << QString("1-2, broken");
        QTest::newRow("reversed") << QString("7-2");
        QTest::newRow("out-of-supported-int-range") << QString("2147483648-2147483649");
    }
    void malformedRanges()
    {
        QFETCH(QString, ranges);
        const QString filename = path(".dbc");
        QVERIFY(write(filename, input(11, extended(ranges))));
        DBCFile file;
        QVERIFY(file.loadFile(filename));
        QCOMPARE(warnings.size(), 1);
        QVERIFY(warnings.first().contains("Number of faulty signal entries: 1"));
        verifySignal(file, "11-11", {11,11}, true);
    }
};
QTEST_MAIN(TestMultiplexRoundTrip)
#include "tst_multiplexroundtrip.moc"
