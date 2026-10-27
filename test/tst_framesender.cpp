#include <QtTest>
#include <QTableWidget>
#include <QTemporaryFile>

#include "framesenderwindow.h"
#include "ui_framesenderwindow.h"

class TestFrameSender : public QObject
{
    Q_OBJECT

private:
    const QByteArray payload = QByteArray::fromHex("0322fd13ffffffff");

    void prepare(FrameSenderWindow &window, const QString &trigger)
    {
        window.intervalTimer->stop();
        window.inhibitChanged = true;
        window.ui->tableSender->item(0, SENDTAB_COL_TRIGGER)->setText(trigger);
        window.inhibitChanged = false;
        FrameSendData record;
        record.enabled = false;
        record.count = 0;
        record.bus = 0;
        record.setFrameId(0x18DA7FFA);
        record.setExtendedFrameFormat(true);
        record.setPayload(payload);
        window.sendingData.append(record);
    }

    void setModifiers(FrameSenderWindow &window, const QString &text)
    {
        // Use the real cellChanged connection and onCellChanged handler.
        window.ui->tableSender->item(0, SENDTAB_COL_MODS)->setText(text);
    }

private slots:
    void triggerDoesNotModifyPayload_data()
    {
        QTest::addColumn<QString>("trigger");
        QTest::newRow("count") << "1X";
        QTest::newRow("interval") << "100ms";
        QTest::newRow("received-id") << "ID0x321";
    }

    void triggerDoesNotModifyPayload()
    {
        QFETCH(QString, trigger);
        QVector<CANFrame> frames;
        FrameSenderWindow window(&frames);
        prepare(window, trigger);
        window.processModifierText(0);
        window.doModifiers(0);
        QCOMPARE(window.sendingData[0].payload(), payload);
        QVERIFY(window.sendingData[0].modifiers.isEmpty());
        QCOMPARE(window.ui->tableSender->item(0, SENDTAB_COL_TRIGGER)->text(), trigger);
    }

    void explicitModifierStillWorks()
    {
        QVector<CANFrame> frames;
        FrameSenderWindow window(&frames);
        prepare(window, "1X");
        setModifiers(window, "D0=D0+1");
        window.doModifiers(0);
        QCOMPARE(window.sendingData[0].payload(), QByteArray::fromHex("0422fd13ffffffff"));
        QCOMPARE(window.sendingData[0].frameId(), quint32(0x18DA7FFA));
        QVERIFY(window.sendingData[0].hasExtendedFrameFormat());
        QCOMPARE(window.sendingData[0].bus, 0);
        QCOMPARE(window.ui->tableSender->item(0, SENDTAB_COL_TRIGGER)->text(), QString("1X"));
    }

    void clearingModifiersRemovesProgram_data()
    {
        QTest::addColumn<QString>("blank");
        QTest::newRow("empty") << "";
        QTest::newRow("whitespace") << "   ";
    }

    void clearingModifiersRemovesProgram()
    {
        QFETCH(QString, blank);
        QVector<CANFrame> frames;
        FrameSenderWindow window(&frames);
        prepare(window, "1X");
        setModifiers(window, "D0=D0+1");
        QVERIFY(!window.sendingData[0].modifiers.isEmpty());
        setModifiers(window, blank);
        window.doModifiers(0);
        QCOMPARE(window.sendingData[0].payload(), payload);
        QVERIFY(window.sendingData[0].modifiers.isEmpty());
    }

    void reportedFilePreservesPayload_data()
    {
        QTest::addColumn<QByteArray>("firstByte");
        QTest::newRow("hex") << QByteArray("0x03");
        QTest::newRow("decimal") << QByteArray("3");
    }

    void reportedFilePreservesPayload()
    {
        QFETCH(QByteArray, firstByte);
        QTemporaryFile file;
        QVERIFY(file.open());
        const QByteArray row = "F#0#0x18DA7FFA#msgD_RQ_PF_PP_Phys#8#F#F#" + firstByte
            + " 0x22 0xFD 0x13 0xFF 0xFF 0xFF 0xFF#1X ##\n";
        QCOMPARE(file.write(row), qint64(row.size()));
        QVERIFY(file.flush());
        QVector<CANFrame> frames;
        FrameSenderWindow window(&frames);
        window.intervalTimer->stop();
        window.loadSenderFile(file.fileName());
        QCOMPARE(window.sendingData.size(), 1);
        QCOMPARE(window.sendingData[0].payload(), payload);
        window.doModifiers(0);
        QCOMPARE(window.sendingData[0].payload(), payload);
        QVERIFY(window.sendingData[0].modifiers.isEmpty());
        QCOMPARE(window.sendingData[0].frameId(), quint32(0x18DA7FFA));
        QCOMPARE(window.sendingData[0].triggers.size(), 1);
        QCOMPARE(window.sendingData[0].triggers[0].maxCount, 1);
        QVERIFY(!window.sendingData[0].enabled);
    }
};

QTEST_MAIN(TestFrameSender)
#include "tst_framesender.moc"
