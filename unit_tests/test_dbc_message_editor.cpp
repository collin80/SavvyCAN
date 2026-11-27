#include <QtTest/QtTest>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFile>
#include <QLineEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "dbc/dbcmessageeditor.h"
#include "dbc/dbchandler.h"
#include "utility.h"

class TestDbcMessageEditor : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void testNewMessageDefaultsToStandardId();
    void testLoadBit31StandardIdAndClearExtendedFlag();
    void testToggleExtendedOnStandardBoundary0x7FF();
    void testFrameIdInteractionAboveAndBelow0x7FF();
    void testFrameIdMasksBit31AndSaveAvoidsOverflow();
    void testLoadMalformedExtendedIdWithoutBit31();

private:
    QString writeTempDbc(const QString &name, const QString &contents);
    static QString readText(const QString &path);

    QTemporaryDir dir;
    QTemporaryDir settingsDir;
};

QString TestDbcMessageEditor::writeTempDbc(const QString &name, const QString &contents)
{
    QString path = dir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return QString();
    file.write(contents.toUtf8());
    file.close();
    return path;
}

QString TestDbcMessageEditor::readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    return QString::fromUtf8(file.readAll());
}

void TestDbcMessageEditor::initTestCase()
{
    QVERIFY(dir.isValid());
    QVERIFY(settingsDir.isValid());
    QCoreApplication::setOrganizationName("SavvyCAN-test");
    QCoreApplication::setApplicationName("TestDbcMessageEditor");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    QSettings().clear();
    Utility::decimalMode = false;
}

void TestDbcMessageEditor::init()
{
    Utility::decimalMode = false;
    DBCHandler::getReference()->removeAllFiles();
}

void TestDbcMessageEditor::cleanup()
{
    DBCHandler::getReference()->removeAllFiles();
    Utility::decimalMode = false;
}

void TestDbcMessageEditor::testNewMessageDefaultsToStandardId()
{
    DBCHandler *handler = DBCHandler::getReference();
    handler->createBlankFile();
    DBCFile *file = handler->getFileByIdx(0);
    QVERIFY(file);

    DBC_MESSAGE *msg = new DBC_MESSAGE();
    QCOMPARE(msg->ID, 0u);
    QVERIFY(!msg->extendedID);

    msg->ID = 0x123;
    msg->name = "NewStdMsg";
    msg->len = 8;
    msg->sender = file->findNodeByName("Vector__XXX");
    QVERIFY(msg->sender);
    QVERIFY(file->messageHandler->addMessage(msg));

    QString outPath = dir.filePath("new_std.dbc");
    QVERIFY(handler->saveDBCFileAs(0, outPath));
    const QString text = readText(outPath);
    QVERIFY(text.contains("BO_ 291 NewStdMsg: 8 Vector__XXX\n"));
    QVERIFY(!text.contains("2147483939"));

    DBCFile reloaded;
    QVERIFY(reloaded.loadFile(outPath));
    DBC_MESSAGE *reloadedMsg = reloaded.messageHandler->findMsgByID(0x123);
    QVERIFY(reloadedMsg);
    QCOMPARE(reloadedMsg->ID, 0x123u);
    QVERIFY(!reloadedMsg->extendedID);
}

void TestDbcMessageEditor::testLoadBit31StandardIdAndClearExtendedFlag()
{
    // 0x80000100 = 2147483904 (ID 0x100 = 256 with bit 31 set)
    const QString dbcText =
        "VERSION \"\"\n"
        "\n"
        "NS_ :\n"
        "\n"
        "BS_:\n"
        "\n"
        "BU_: ECU1\n"
        "\n"
        "BO_ 2147483904 OldExtMsg: 8 ECU1\n"
        " SG_ Mux M : 0|8@1+ (1,0) [0|255] \"\" Vector__XXX\n"
        " SG_ SubMux m1M : 8|8@1+ (1,0) [0|255] \"\" Vector__XXX\n"
        " SG_ ValSig m1 : 16|32@1- (1,0) [0|100] \"\" Vector__XXX\n"
        "\n"
        "CM_ BO_ 2147483904 \"Msg comment\";\n"
        "CM_ SG_ 2147483904 ValSig \"Sig comment\";\n"
        "BA_DEF_ BO_ \"GenMsgCycleTime\" INT 0 10000;\n"
        "BA_DEF_ BO_ \"VFrameFormat\" ENUM \"StandardCAN\",\"ExtendedCAN\",\"reserved\",\"J1939PG\",\"reserved\",\"reserved\",\"reserved\",\"reserved\",\"reserved\",\"reserved\",\"reserved\",\"reserved\",\"reserved\",\"reserved\",\"StandardCAN_FD\",\"ExtendedCAN_FD\";\n"
        "BA_DEF_DEF_ \"GenMsgCycleTime\" 0;\n"
        "BA_DEF_DEF_ \"VFrameFormat\" \"StandardCAN\";\n"
        "BA_ \"GenMsgCycleTime\" BO_ 2147483904 50;\n"
        "BA_ \"VFrameFormat\" BO_ 2147483904 1;\n"
        "VAL_ 2147483904 Mux 0 \"Off\" 1 \"On\";\n"
        "SG_MUL_VAL_ 2147483904 SubMux Mux 1-1;\n"
        "SIG_VALTYPE_ 2147483904 ValSig : 1;\n";

    const QString inPath = writeTempDbc("old_ext_std_id.dbc", dbcText);
    QVERIFY(!inPath.isEmpty());

    DBCHandler *handler = DBCHandler::getReference();
    DBCFile *file = handler->loadFileAs(inPath, 0);
    QVERIFY(file);
    QVERIFY(!file->getDirtyFlag());

    DBC_MESSAGE *msg = file->messageHandler->findMsgByID(0x100);
    QVERIFY(msg);
    QCOMPARE(msg->ID, 0x100u);
    QVERIFY(msg->extendedID);

    DBCMessageEditor editor;
    // Match DBCMainEditor::onTreeDoubleClicked call order: setMessageRef before setFileIdx
    editor.setMessageRef(msg);
    editor.setFileIdx(0);
    editor.refreshView();
    editor.show();
    QCoreApplication::processEvents();

    QVERIFY(!file->getDirtyFlag());

    QCheckBox *checkExtended = editor.findChild<QCheckBox *>("checkExtended");
    QLineEdit *lineFrameID = editor.findChild<QLineEdit *>("lineFrameID");
    QVERIFY(checkExtended);
    QVERIFY(lineFrameID);
    QVERIFY(checkExtended->isChecked());
    QVERIFY(checkExtended->isEnabled());
    QCOMPARE(lineFrameID->text(), QString("0x00000100"));

    QSignalSpy spy(&editor, &DBCMessageEditor::updatedTreeInfo);
    checkExtended->setChecked(false);

    QVERIFY(!msg->extendedID);
    QVERIFY(file->getDirtyFlag());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toUInt(), 0x100u);
    QCOMPARE(lineFrameID->text(), QString("0x100"));

    QString savedPath = dir.filePath("cleared_ext.dbc");
    QVERIFY(handler->saveDBCFileAs(0, savedPath));
    const QString savedText = readText(savedPath);

    QVERIFY(!savedText.contains("2147483904"));
    QVERIFY(savedText.contains("BO_ 256 OldExtMsg: 8 ECU1\n"));
    QVERIFY(savedText.contains("CM_ BO_ 256 \"Msg comment\";\n"));
    QVERIFY(savedText.contains("CM_ SG_ 256 ValSig \"Sig comment\";\n"));
    QVERIFY(savedText.contains("BA_ \"GenMsgCycleTime\" BO_ 256 50;\n"));
    QVERIFY(savedText.contains("BA_ \"VFrameFormat\" BO_ 256 0;\n"));
    QVERIFY(savedText.contains("VAL_ 256 Mux 0 \"Off\" 1 \"On\";\n"));
    QVERIFY(savedText.contains("SG_MUL_VAL_ 256 SubMux Mux 1-1;\n"));
    QVERIFY(savedText.contains("SG_MUL_VAL_ 256 ValSig Mux 1-1;\n"));
    QVERIFY(savedText.contains("SIG_VALTYPE_ 256 ValSig : 1;\n"));

    DBCFile reloaded;
    QVERIFY(reloaded.loadFile(savedPath));
    DBC_MESSAGE *reloadedMsg = reloaded.messageHandler->findMsgByID(0x100);
    QVERIFY(reloadedMsg);
    QCOMPARE(reloadedMsg->ID, 0x100u);
    QVERIFY(!reloadedMsg->extendedID);
    QCOMPARE(reloadedMsg->comment, QString("Msg comment"));
    DBC_ATTRIBUTE_VALUE *vff = reloadedMsg->findAttrValByName("VFrameFormat");
    QVERIFY(vff);
    QCOMPARE(vff->value.toInt(), 0);
}

void TestDbcMessageEditor::testToggleExtendedOnStandardBoundary0x7FF()
{
    DBCHandler *handler = DBCHandler::getReference();
    handler->createBlankFile();
    DBCFile *file = handler->getFileByIdx(0);
    QVERIFY(file);

    DBC_MESSAGE *msg = new DBC_MESSAGE();
    msg->ID = 0x7FF;
    msg->extendedID = false;
    msg->name = "BoundaryStd";
    msg->len = 8;
    msg->sender = file->findNodeByName("Vector__XXX");
    QVERIFY(file->messageHandler->addMessage(msg));
    file->clearDirtyFlag();

    DBCMessageEditor editor;
    editor.setMessageRef(msg);
    editor.setFileIdx(0);
    editor.refreshView();
    editor.show();
    QCoreApplication::processEvents();

    QCheckBox *checkExtended = editor.findChild<QCheckBox *>("checkExtended");
    QLineEdit *lineFrameID = editor.findChild<QLineEdit *>("lineFrameID");
    QVERIFY(checkExtended);
    QVERIFY(lineFrameID);
    QVERIFY(!checkExtended->isChecked());
    QVERIFY(checkExtended->isEnabled());
    QCOMPARE(lineFrameID->text(), QString("0x7FF"));

    checkExtended->setChecked(true);
    QVERIFY(msg->extendedID);
    QVERIFY(file->getDirtyFlag());
    QCOMPARE(lineFrameID->text(), QString("0x000007FF"));

    QString outPath = dir.filePath("boundary_7ff_ext.dbc");
    QVERIFY(handler->saveDBCFileAs(0, outPath));
    const QString savedText = readText(outPath);
    QVERIFY(savedText.contains("BO_ 2147485695 BoundaryStd: 8 Vector__XXX\n"));

    DBCFile reloaded;
    QVERIFY(reloaded.loadFile(outPath));
    DBC_MESSAGE *reloadedMsg = reloaded.messageHandler->findMsgByID(0x7FF);
    QVERIFY(reloadedMsg);
    QCOMPARE(reloadedMsg->ID, 0x7FFu);
    QVERIFY(reloadedMsg->extendedID);
}

void TestDbcMessageEditor::testFrameIdInteractionAboveAndBelow0x7FF()
{
    DBCHandler *handler = DBCHandler::getReference();
    handler->createBlankFile();
    DBCFile *file = handler->getFileByIdx(0);
    QVERIFY(file);

    DBC_MESSAGE *msg = new DBC_MESSAGE();
    msg->ID = 0x100;
    msg->extendedID = false;
    msg->name = "InteractiveMsg";
    msg->len = 8;
    msg->sender = file->findNodeByName("Vector__XXX");
    QVERIFY(file->messageHandler->addMessage(msg));
    file->clearDirtyFlag();

    DBCMessageEditor editor;
    editor.setMessageRef(msg);
    editor.setFileIdx(0);
    editor.refreshView();
    editor.show();
    QCoreApplication::processEvents();

    QCheckBox *checkExtended = editor.findChild<QCheckBox *>("checkExtended");
    QLineEdit *lineFrameID = editor.findChild<QLineEdit *>("lineFrameID");
    QVERIFY(checkExtended);
    QVERIFY(lineFrameID);

    QSignalSpy spy(&editor, &DBCMessageEditor::updatedTreeInfo);

    // 1. Move ID to > 0x7FF (0x800 boundary): forces extendedID and disables checkbox
    lineFrameID->setText("0x800");
    QTest::keyClick(lineFrameID, Qt::Key_Return);
    QCOMPARE(msg->ID, 0x800u);
    QVERIFY(msg->extendedID);
    QVERIFY(file->getDirtyFlag());
    QVERIFY(checkExtended->isChecked());
    QVERIFY(!checkExtended->isEnabled());
    QCOMPARE(lineFrameID->text(), QString("0x00000800"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toUInt(), 0x100u);

    // Programmatic uncheck while ID > 0x7FF must not clear extendedID
    checkExtended->setChecked(false);
    QVERIFY(msg->extendedID);
    QVERIFY(checkExtended->isChecked());
    QVERIFY(!checkExtended->isEnabled());

    // 2. Edit ID again while open (checks origID was updated to 0x800)
    lineFrameID->setText("0x18DAF110");
    QTest::keyClick(lineFrameID, Qt::Key_Return);
    QCOMPARE(msg->ID, 0x18DAF110u);
    QVERIFY(msg->extendedID);
    QVERIFY(checkExtended->isChecked());
    QVERIFY(!checkExtended->isEnabled());
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(1).toUInt(), 0x800u);

    // 3. Lower ID back to <= 0x7FF: preserves extendedID and re-enables checkbox
    lineFrameID->setText("0x123");
    QTest::keyClick(lineFrameID, Qt::Key_Return);
    QCOMPARE(msg->ID, 0x123u);
    QVERIFY(msg->extendedID);
    QVERIFY(checkExtended->isChecked());
    QVERIFY(checkExtended->isEnabled());
    QCOMPARE(lineFrameID->text(), QString("0x00000123"));
    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(2).at(1).toUInt(), 0x18DAF110u);

    // 4. Edit ID once more to verify origID tracked 0x123
    lineFrameID->setText("0x124");
    QTest::keyClick(lineFrameID, Qt::Key_Return);
    QCOMPARE(msg->ID, 0x124u);
    QCOMPARE(spy.count(), 4);
    QCOMPARE(spy.at(3).at(1).toUInt(), 0x123u);

    // 5. User can now clear extended flag on <= 0x7FF
    checkExtended->setChecked(false);
    QVERIFY(!msg->extendedID);
    QCOMPARE(lineFrameID->text(), QString("0x124"));
    QCOMPARE(spy.count(), 5);
    QCOMPARE(spy.at(4).at(1).toUInt(), 0x124u);
}

void TestDbcMessageEditor::testFrameIdMasksBit31AndSaveAvoidsOverflow()
{
    DBCHandler *handler = DBCHandler::getReference();
    handler->createBlankFile();
    DBCFile *file = handler->getFileByIdx(0);
    QVERIFY(file);

    DBC_MESSAGE *msg = new DBC_MESSAGE();
    msg->ID = 0x100;
    msg->extendedID = false;
    msg->name = "MaskedMsg";
    msg->len = 8;
    msg->sender = file->findNodeByName("Vector__XXX");
    QVERIFY(file->messageHandler->addMessage(msg));

    DBCMessageEditor editor;
    editor.setMessageRef(msg);
    editor.setFileIdx(0);
    editor.refreshView();
    editor.show();
    QCoreApplication::processEvents();

    QLineEdit *lineFrameID = editor.findChild<QLineEdit *>("lineFrameID");
    QVERIFY(lineFrameID);
    lineFrameID->setText("0x80000123");
    QTest::keyClick(lineFrameID, Qt::Key_Return);

    QCOMPARE(msg->ID, 0x123u);
    QVERIFY(!msg->extendedID);
    QCOMPARE(lineFrameID->text(), QString("0x123"));

    // Anti-overflow test for saveFile() if an unmasked ID with bit 31 is saved directly
    DBCFile directFile;
    DBC_NODE *node = new DBC_NODE;
    node->name = "Vector__XXX";
    directFile.dbc_nodes.append(node);

    DBC_MESSAGE *unmaskedStd = new DBC_MESSAGE();
    unmaskedStd->ID = 0x80000123u;
    unmaskedStd->extendedID = true;
    unmaskedStd->name = "UnmaskedStd";
    unmaskedStd->len = 8;
    unmaskedStd->sender = node;
    directFile.messageHandler->addMessage(unmaskedStd);

    DBC_MESSAGE *unmaskedExt = new DBC_MESSAGE();
    unmaskedExt->ID = 0x80000800u;
    unmaskedExt->extendedID = true;
    unmaskedExt->name = "UnmaskedExt";
    unmaskedExt->len = 8;
    unmaskedExt->sender = node;
    directFile.messageHandler->addMessage(unmaskedExt);

    const QString outPath = dir.filePath("anti_overflow.dbc");
    QVERIFY(directFile.saveFile(outPath));
    const QString savedText = readText(outPath);
    QVERIFY(savedText.contains("BO_ 2147483939 UnmaskedStd: 8 Vector__XXX\n"));
    QVERIFY(savedText.contains("BO_ 2147485696 UnmaskedExt: 8 Vector__XXX\n"));
}

void TestDbcMessageEditor::testLoadMalformedExtendedIdWithoutBit31()
{
    const QString dbcText =
        "VERSION \"\"\n"
        "\n"
        "NS_ :\n"
        "\n"
        "BS_:\n"
        "\n"
        "BU_: \n"
        "\n"
        "BO_ 2048 NoBit31Msg: 8 Vector__XXX\n";

    const QString inPath = writeTempDbc("no_bit31_gt7ff.dbc", dbcText);
    QVERIFY(!inPath.isEmpty());

    DBCHandler *handler = DBCHandler::getReference();
    DBCFile *file = handler->loadFileAs(inPath, 0);
    QVERIFY(file);
    DBC_MESSAGE *msg = file->messageHandler->findMsgByID(0x800);
    QVERIFY(msg);
    QCOMPARE(msg->ID, 0x800u);
    QVERIFY(msg->extendedID);

    DBCMessageEditor editor;
    editor.setMessageRef(msg);
    editor.setFileIdx(0);
    editor.refreshView();
    editor.show();
    QCoreApplication::processEvents();

    QCheckBox *checkExtended = editor.findChild<QCheckBox *>("checkExtended");
    QVERIFY(checkExtended);
    QVERIFY(checkExtended->isChecked());
    QVERIFY(!checkExtended->isEnabled());
}

QTEST_MAIN(TestDbcMessageEditor)
#include "test_dbc_message_editor.moc"
