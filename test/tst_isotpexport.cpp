#include <QtTest>
#include <QFileDialog>
#include <QTemporaryDir>
#include <QCheckBox>
#include <QTableWidget>
#include "mainwindow.h"
#include "re/isotp_interpreterwindow.h"

class TestISOTPExport : public QObject
{
    Q_OBJECT
    QTemporaryDir settings;
    MainWindow *mainWindow = nullptr;

    QString save(ISOTP_InterpreterWindow &window, const QString &path)
    {
        bool selected = false;
        QTimer chooser;
        chooser.setInterval(1);
        connect(&chooser, &QTimer::timeout, &window, [&]() {
            auto dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            selected = true;
            chooser.stop();
            dialog->selectFile(path);
            QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
        });
        QTimer watchdog;
        watchdog.setSingleShot(true);
        connect(&watchdog, &QTimer::timeout, &window, [&]() {
            if (auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();
        });
        chooser.start();
        watchdog.start(5000);
        bool invoked = QMetaObject::invokeMethod(&window, "saveList", Qt::DirectConnection);
        if (!invoked || !selected) return QStringLiteral("DIALOG FAILED");
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return QStringLiteral("FILE FAILED");
        return QString::fromUtf8(file.readAll());
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SavvyCANTests");
        QCoreApplication::setApplicationName("ISOTPExport");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        QSettings().setValue("Main/MaximumFrames", 100);
        mainWindow = new MainWindow;
    }
    void cleanupTestCase() { delete mainWindow; }
    void recordsAreWrittenOnce_data()
    {
        QTest::addColumn<int>("count");
        QTest::addColumn<bool>("incomplete");
        QTest::newRow("empty") << 0 << false;
        QTest::newRow("single") << 1 << false;
        QTest::newRow("two") << 2 << false;
        QTest::newRow("three") << 3 << false;
        QTest::newRow("two-with-incomplete") << 2 << true;
        QTest::newRow("incomplete-only") << 0 << true;
    }
    void recordsAreWrittenOnce()
    {
        QFETCH(int, count);
        QFETCH(bool, incomplete);
        ISOTP_InterpreterWindow window(mainWindow->getCANFrameModel()->getListReference());
        if (incomplete)
        {
            auto checkbox = window.findChild<QCheckBox *>("cbShowIncomplete");
            QVERIFY(checkbox);
            checkbox->setChecked(true);
            ISOTP_MESSAGE message;
            message.setTimeStamp(QCanBusFrame::TimeStamp(0, 999));
            message.setFrameId(0x555);
            message.setPayload(QByteArray::fromHex("0100"));
            message.reportedLength = 3;
            message.isMultiframe = false;
            message.lastSequence = 0;
            QVERIFY(QMetaObject::invokeMethod(&window, "newISOMessage", Qt::DirectConnection, Q_ARG(ISOTP_MESSAGE, message)));
        }
        for (int i=0; i<count; ++i)
        {
            ISOTP_MESSAGE message;
            message.setTimeStamp(QCanBusFrame::TimeStamp(0, 550000+i*78000));
            message.setFrameId(0x123+i);
            message.setPayload(QByteArray::fromHex("0100"));
            message.reportedLength = 2;
            message.isMultiframe = false;
            message.lastSequence = 0;
            QVERIFY(QMetaObject::invokeMethod(&window, "newISOMessage", Qt::DirectConnection, Q_ARG(ISOTP_MESSAGE, message)));
        }
        auto table = window.findChild<QTableWidget *>("tableIsoFrames");
        QVERIFY(table);
        QCOMPARE(table->rowCount(), count + (incomplete ? 1 : 0));
        QTemporaryDir output;
        const QString first = save(window, output.filePath("first.txt"));
        QVERIFY(first != "DIALOG FAILED");
        QVERIFY(first != "FILE FAILED");
        QCOMPARE(first.count(QString(57, '*')), count);
        QCOMPARE(first.count("999 555 "), 0);
        for (int i=0; i<count; ++i)
        {
            const QString header = QString::number(550000+i*78000)+" "+QString::number(0x123+i,16)+" ";
            QCOMPARE(first.count(header), 1);
        }
        const QString second = save(window, output.filePath("second.txt"));
        QCOMPARE(second, first);
    }
};
QTEST_MAIN(TestISOTPExport)
#include "tst_isotpexport.moc"
