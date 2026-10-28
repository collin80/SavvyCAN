#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimeZone>
#include <QLabel>
#include <QTableWidget>
#include "lognotesdialog.h"
#include "lognotespanel.h"
#include "markerpresetsdialog.h"

// The log notes dialog (asked when saving a log, and File > Log Notes...) and the main window's Log Notes panel.
class TestLogNotesUi : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testDialogShowsAndReturnsNotes();
    void testDialogTypedFields();
    void testSavingChoices_data();
    void testSavingChoices();
    void testEditingHasNoSkip();
    void testTemplates();
    void testLastNotes();
    void testPanel();
    void testStartingChoices();
    void testFinishingChoices();
    void testPresetsDialog();
    void testPresetsPanel();
    void testTemplatesWithPresets();

private:
    QTemporaryDir settingsDir;
};

static LogNotes sampleNotes()
{
    LogNotes notes;
    notes.title = "E90 HVAC flaps: AC on/off cycle, bench 2";
    notes.author = "tomdebree";
    notes.tags = QStringList{"hvac", "lin"};
    notes.system = "BMW E90 HVAC module\nflap motors 1 to 4";
    notes.setup = "Tapped at the module connector, 120 ohm termination";
    notes.scenario = "1. Ignition on\n2. AC on\n   - wait 5 s\n3. AC off";
    notes.observations = "Motor 3 answers late.";
    notes.markers = {{2000, "AC on"}, {3500, ""}};
    notes.saved = QDateTime(QDate(2026, 10, 25), QTime(14, 3, 11), QTimeZone(3600));
    notes.savvycanVersion = "242";
    notes.logFile = "run1.csv";
    notes.frameCount = 4;
    notes.unfilteredCount = 10;
    notes.hasSpan = true;
    notes.firstUs = 1000;
    notes.lastUs = 4000;
    notes.buses = QStringList{"0: GVRET COM11, 500 kbit/s", "1: peakcan usb0, 500 kbit/s, CAN FD data 2000 kbit/s"};
    notes.dbcFiles = QStringList{"bmw_e90.dbc (bus 0)"};
    notes.otherFields = {qMakePair(QString("bench"), QString("rig 2"))};
    notes.otherSections = {qMakePair(QString("Wiring"), QString("pin 3: CAN-H"))};
    return notes;
}

//with SAVVYCAN_SCREENSHOTS set to a folder, the dialogs are saved there as pictures
static void screenshot(QWidget *widget, const QString &name)
{
    const QString dir = qEnvironmentVariable("SAVVYCAN_SCREENSHOTS");
    if (dir.isEmpty()) return;
    widget->show();
    QCoreApplication::processEvents();
    widget->grab().save(dir + "/" + name + ".png");
}

static QPushButton *buttonWithText(QWidget *parent, const QString &text)
{
    for (QPushButton *button : parent->findChildren<QPushButton *>())
        if (button->text().remove('&') == text) return button;
    return nullptr;
}

void TestLogNotesUi::initTestCase()
{
    //the settings (templates, last notes, ask on save) go to a folder of their own
    QVERIFY(settingsDir.isValid());
    QCoreApplication::setOrganizationName("SavvyCAN-test");
    QCoreApplication::setApplicationName("TestLogNotesUi");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    QSettings().clear();
}

void TestLogNotesUi::testDialogShowsAndReturnsNotes()
{
    LogNotesDialog dialog(LogNotesDialog::Saving);
    dialog.setNotes(sampleNotes());
    dialog.setTarget("run1.csv", true);
    QVERIFY(dialog.notes() == sampleNotes()); //everything back as it was, including what the dialog doesn't show
    QCOMPARE(dialog.findChild<QLineEdit *>("title")->text(), sampleNotes().title);
    QCOMPARE(dialog.findChild<QLineEdit *>("tags")->text(), QString("hvac, lin"));
    QCOMPARE(dialog.findChild<QPlainTextEdit *>("markers")->toPlainText(), QString("0.002000 s: AC on\n0.003500 s:"));
    screenshot(&dialog, "log-notes-save-gvret");

    LogNotesDialog other(LogNotesDialog::Saving);
    other.setNotes(sampleNotes());
    other.setTarget("run1.blf", false);
    screenshot(&other, "log-notes-save-blf");
}

void TestLogNotesUi::testDialogTypedFields()
{
    LogNotesDialog dialog(LogNotesDialog::Editing);
    dialog.setNotes(LogNotes());
    QTest::keyClicks(dialog.findChild<QLineEdit *>("title"), "  Cold start  ");
    QTest::keyClicks(dialog.findChild<QLineEdit *>("tags"), "a, , b ,c");
    dialog.findChild<QPlainTextEdit *>("scenario")->setPlainText("Step 1\n  indented");
    dialog.findChild<QPlainTextEdit *>("markers")->setPlainText("12.4 s: AC on\n\n- 13 s: fan\nnot a marker\n-0.5 s:");

    const LogNotes notes = dialog.notes();
    QCOMPARE(notes.title, QString("Cold start"));
    QCOMPARE(notes.tags, (QStringList{"a", "b", "c"}));
    QCOMPARE(notes.scenario, QString("Step 1\n  indented"));
    QCOMPARE(notes.markers.count(), 3);
    QCOMPARE(notes.markers.at(0).timeUs, qint64(12400000));
    QCOMPARE(notes.markers.at(0).text, QString("AC on"));
    QCOMPARE(notes.markers.at(1).timeUs, qint64(13000000));
    QCOMPARE(notes.markers.at(2).timeUs, qint64(-500000));
    QVERIFY(notes.buses.isEmpty());
}

void TestLogNotesUi::testSavingChoices_data()
{
    QTest::addColumn<QString>("button");
    QTest::addColumn<int>("choice");
    QTest::addColumn<bool>("accepted");

    QTest::newRow("save with notes") << "Save with Notes" << int(LogNotesDialog::WithNotes) << true;
    QTest::newRow("save without notes") << "Save without Notes" << int(LogNotesDialog::WithoutNotes) << true;
    QTest::newRow("cancel") << "Cancel" << int(LogNotesDialog::Cancelled) << false;
}

void TestLogNotesUi::testSavingChoices()
{
    QFETCH(QString, button);
    QFETCH(int, choice);
    QFETCH(bool, accepted);

    LogNotesDialog::setAskOnSave(true);
    LogNotesDialog dialog(LogNotesDialog::Saving);
    dialog.setNotes(sampleNotes());
    QCheckBox *ask = dialog.findChild<QCheckBox *>("ask");
    QVERIFY(ask && ask->isChecked());
    ask->setChecked(false);

    QPushButton *pushButton = buttonWithText(&dialog, button);
    QVERIFY(pushButton);
    pushButton->click();
    QCOMPARE(int(dialog.choice()), choice);
    QCOMPARE(dialog.result() == QDialog::Accepted, accepted);
    //the "ask" box is only taken when the log is saved
    QCOMPARE(LogNotesDialog::askOnSave(), !accepted);
}

void TestLogNotesUi::testEditingHasNoSkip()
{
    LogNotesDialog dialog(LogNotesDialog::Editing);
    dialog.setNotes(sampleNotes());
    dialog.setTarget(QString(), false);
    QVERIFY(!buttonWithText(&dialog, "Save without Notes"));
    QVERIFY(!dialog.findChild<QCheckBox *>("ask")->isVisibleTo(&dialog));
    QPushButton *ok = buttonWithText(&dialog, "OK");
    QVERIFY(ok);
    ok->click();
    QCOMPARE(int(dialog.choice()), int(LogNotesDialog::WithNotes));
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    screenshot(&dialog, "log-notes-edit");
}

void TestLogNotesUi::testTemplates()
{
    QVERIFY(LogNotesDialog::templateNames().isEmpty());
    LogNotesDialog::saveTemplate("Bench 2", sampleNotes());
    LogNotes other;
    other.title = "car";
    LogNotesDialog::saveTemplate("a car", other);
    QCOMPARE(LogNotesDialog::templateNames(), (QStringList{"a car", "Bench 2"}));

    //a template keeps the typed fields only
    LogNotes loaded;
    QVERIFY(LogNotesDialog::loadTemplate("Bench 2", loaded));
    QVERIFY(loaded == sampleNotes().userFields());
    QVERIFY(loaded.markers.isEmpty() && loaded.buses.isEmpty() && loaded.frameCount < 0);
    QVERIFY(!LogNotesDialog::loadTemplate("missing", loaded));

    //using one in the dialog replaces the typed fields and leaves the rest
    LogNotes current;
    current.title = "old title";
    current.buses = QStringList{"0: GVRET COM3, 250 kbit/s"};
    current.markers = {{5, "kept"}};
    LogNotesDialog dialog(LogNotesDialog::Saving);
    dialog.setNotes(current);
    QComboBox *templates = dialog.findChild<QComboBox *>("templates");
    QCOMPARE(templates->count(), 3); //"(pick a template)" and the two
    QVERIFY(!dialog.findChild<QPushButton *>("useTemplate")->isEnabled());
    templates->setCurrentIndex(templates->findText("Bench 2"));
    QVERIFY(dialog.findChild<QPushButton *>("useTemplate")->isEnabled());
    dialog.findChild<QPushButton *>("useTemplate")->click();
    const LogNotes result = dialog.notes();
    QVERIFY(result.userFields() == sampleNotes().userFields());
    QCOMPARE(result.buses, current.buses);
    QCOMPARE(result.markers, current.markers);

    LogNotesDialog::deleteTemplate("Bench 2");
    QCOMPARE(LogNotesDialog::templateNames(), QStringList{"a car"});
    LogNotesDialog::deleteTemplate("a car");
}

void TestLogNotesUi::testLastNotes()
{
    QSettings().remove("LogNotes/Last");
    QVERIFY(LogNotesDialog::lastNotes().isEmpty());
    LogNotesDialog::setLastNotes(sampleNotes());
    QVERIFY(LogNotesDialog::lastNotes() == sampleNotes().userFields());
}

void TestLogNotesUi::testPanel()
{
    LogNotesPanel panel;
    QTextBrowser *browser = panel.findChild<QTextBrowser *>();
    QVERIFY(browser);
    QVERIFY(browser->toPlainText().contains("No notes for this log"));

    LogNotes notes = sampleNotes();
    notes.observations = "Value <0x10 & rising";
    panel.setNotes(notes, "run1.csv");
    const QString text = browser->toPlainText();
    QVERIFY(text.contains(notes.title));
    QVERIFY(text.contains("Value <0x10 & rising")); //escaped in the HTML, shown as typed
    QVERIFY(text.contains("   - wait 5 s"));
    QVERIFY(text.contains("4 (filtered from 10)"));
    QVERIFY(text.contains("0.001000 s to 0.004000 s"));
    QVERIFY(text.contains("1: peakcan usb0, 500 kbit/s, CAN FD data 2000 kbit/s"));
    QVERIFY(!text.contains("0.002000 s: AC on")); //the markers are in their own list
    QVERIFY(text.contains("rig 2"));
    QVERIFY(text.contains("pin 3: CAN-H"));
    QVERIFY(LogNotesPanel::toHtml(notes).contains("&lt;0x10 &amp; rising"));

    QListWidget *markers = panel.findChild<QListWidget *>("markers");
    QVERIFY(markers);
    QCOMPARE(markers->count(), 2);
    QCOMPARE(markers->item(0)->text(), QString("0.002000 s: AC on"));
    QCOMPARE(markers->item(1)->text(), QString("0.003500 s:"));
    QVERIFY(!markers->isHidden());

    qint64 activated = -1;
    connect(&panel, &LogNotesPanel::markerActivated, this, [&activated](qint64 timeUs) { activated = timeUs; });
    emit markers->itemDoubleClicked(markers->item(1));
    QCOMPARE(activated, qint64(3500));

    bool editRequested = false;
    connect(&panel, &LogNotesPanel::editRequested, this, [&editRequested]() { editRequested = true; });
    QPushButton *edit = buttonWithText(&panel, "Edit...");
    QVERIFY(edit);
    edit->click();
    QVERIFY(editRequested);

    bool markerRequested = false;
    connect(&panel, &LogNotesPanel::addMarkerRequested, this, [&markerRequested]() { markerRequested = true; });
    QPushButton *addMarker = buttonWithText(&panel, "Add Marker");
    QVERIFY(addMarker);
    addMarker->click();
    QVERIFY(markerRequested);

    panel.resize(360, 640);
    screenshot(&panel, "log-notes-panel");

    panel.setNotes(LogNotes(), QString());
    QCOMPARE(markers->count(), 0);
    QVERIFY(markers->isHidden());
}

//when capture starts: Keep Notes or Later, and the box is "ask when capture starts"
void TestLogNotesUi::testStartingChoices()
{
    LogNotesDialog::setAskOnCapture(true);
    LogNotesDialog::setAskOnSave(true);
    {
        LogNotesDialog dialog(LogNotesDialog::Starting);
        dialog.setNotes(sampleNotes().userFields());
        dialog.setTarget("continuous.csv", true);
        QVERIFY(!buttonWithText(&dialog, "Cancel"));
        QVERIFY(!buttonWithText(&dialog, "Save with Notes"));
        QCheckBox *ask = dialog.findChild<QCheckBox *>("ask");
        QVERIFY(ask->isVisibleTo(&dialog) && ask->isChecked());
        ask->setChecked(false);
        screenshot(&dialog, "log-notes-capture-start");
        buttonWithText(&dialog, "Keep Notes")->click();
        QCOMPARE(int(dialog.choice()), int(LogNotesDialog::WithNotes));
        QVERIFY(!LogNotesDialog::askOnCapture());
        QVERIFY(LogNotesDialog::askOnSave()); //the other setting isn't touched
    }
    {
        LogNotesDialog dialog(LogNotesDialog::Starting);
        dialog.setNotes(LogNotes());
        dialog.findChild<QCheckBox *>("ask")->setChecked(true);
        buttonWithText(&dialog, "Later")->click();
        QCOMPARE(int(dialog.choice()), int(LogNotesDialog::WithoutNotes));
        QVERIFY(LogNotesDialog::askOnCapture());
    }
}

//when continuous logging stops the log is already written: no Cancel, and closing the dialog means no notes
void TestLogNotesUi::testFinishingChoices()
{
    LogNotesDialog dialog(LogNotesDialog::Finishing);
    dialog.setNotes(sampleNotes());
    dialog.setTarget("continuous.csv", true);
    QVERIFY(buttonWithText(&dialog, "Save with Notes"));
    QVERIFY(buttonWithText(&dialog, "Save without Notes"));
    QVERIFY(!buttonWithText(&dialog, "Cancel"));
    dialog.reject();
    QCOMPARE(int(dialog.choice()), int(LogNotesDialog::WithoutNotes));
}

//setting up marker presets: texts in order, keys refused when taken, warned when common elsewhere, From Scenario
void TestLogNotesUi::testPresetsDialog()
{
    MarkerPresets start;
    start.presets = { { "Ignition on", QKeySequence("Ctrl+1") } };
    MarkerPresetsDialog dialog(start, "1. Ignition on\n   - wait\n2. AC on\n3. AC off");
    QTableWidget *table = dialog.findChild<QTableWidget *>("presets");
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->text(), QString("Ignition on"));
    QVERIFY(dialog.presets() == start);

    //a key SavvyCAN uses is refused and the old one kept
    QVERIFY(!dialog.setKey(0, QKeySequence("Ctrl+O")));
    QVERIFY(dialog.lastKeyMessage().contains("Open Log File"));
    QCOMPARE(dialog.presets().presets[0].key, QKeySequence("Ctrl+1"));

    //a second preset can't share a key; a common one is warned about; a free one is quiet
    buttonWithText(&dialog, "Add")->click();
    QCOMPARE(table->rowCount(), 2);
    table->item(1, 0)->setText("AC on");
    QVERIFY(!dialog.setKey(1, QKeySequence("Ctrl+1")));
    QVERIFY(dialog.lastKeyMessage().contains("Ignition on"));
    QVERIFY(dialog.setKey(1, QKeySequence("Ctrl+S")));
    QVERIFY(dialog.lastKeyMessage().contains("saving"));
    QVERIFY(dialog.setKey(1, QKeySequence("Ctrl+2")));
    QVERIFY(dialog.lastKeyMessage().isEmpty());

    //the Next and Undo keys (-1, -2) are checked against the presets and each other
    QVERIFY(!dialog.setKey(-1, QKeySequence("Ctrl+M")));
    QVERIFY(dialog.setKey(-1, QKeySequence("F9")));
    QVERIFY(!dialog.setKey(-2, QKeySequence("F9")));
    QVERIFY(dialog.lastKeyMessage().contains("Next preset"));
    QVERIFY(!dialog.setKey(0, QKeySequence("Ctrl+Shift+M"))); //the Undo key
    QVERIFY(dialog.setKey(-2, QKeySequence())); //no Undo key
    QVERIFY(dialog.setKey(0, QKeySequence("Ctrl+Shift+M"))); //free now

    //the Scenario's steps, with the Ctrl+digits nothing has
    buttonWithText(&dialog, "From Scenario")->click();
    MarkerPresets result = dialog.presets();
    QCOMPARE(result.presets.count(), 5);
    QCOMPARE(result.presets[2], (MarkerPreset { "Ignition on", QKeySequence("Ctrl+1") }));
    QCOMPARE(result.presets[3], (MarkerPreset { "AC on", QKeySequence("Ctrl+3") }));
    QCOMPARE(result.presets[4], (MarkerPreset { "AC off", QKeySequence("Ctrl+4") }));
    QCOMPARE(result.nextKey, QKeySequence("F9"));
    QVERIFY(result.undoKey.isEmpty());
    dialog.show();
    screenshot(&dialog, "marker-presets");

    //moving and removing; a preset with no text is dropped
    table->selectRow(4);
    buttonWithText(&dialog, "Move Up")->click();
    QCOMPARE(dialog.presets().presets[3].text, QString("AC off"));
    QCOMPARE(dialog.presets().presets[3].key, QKeySequence("Ctrl+4")); //the key moves with it
    table->selectRow(0);
    buttonWithText(&dialog, "Remove")->click();
    QCOMPARE(dialog.presets().presets.count(), 4);
    QCOMPARE(dialog.presets().presets[0].text, QString("AC on"));
    table->item(0, 0)->setText("  ");
    QCOMPARE(dialog.presets().presets.count(), 3);

    //no Scenario: no From Scenario
    MarkerPresetsDialog empty { MarkerPresets(), QString() };
    QVERIFY(!buttonWithText(&empty, "From Scenario")->isEnabled());
}

void TestLogNotesUi::testPresetsPanel()
{
    LogNotesPanel panel;
    QWidget *presetBox = panel.findChild<QWidget *>("presetBox");
    QVERIFY(presetBox && presetBox->isHidden()); //no presets

    MarkerPresets presets;
    presets.presets = { { "Ignition on", QKeySequence("Ctrl+1") }, { "AC on", QKeySequence() }, { "AC off", QKeySequence("F8") } };
    panel.setPresets(presets);
    QVERIFY(!presetBox->isHidden());
    QPushButton *first = panel.findChild<QPushButton *>("preset0");
    QPushButton *second = panel.findChild<QPushButton *>("preset1");
    QVERIFY(first && second && panel.findChild<QPushButton *>("preset2"));
    QVERIFY(first->text().startsWith("Ignition on"));
    QVERIFY(first->text().contains("Ctrl+1"));
    QCOMPARE(second->text(), QString("AC on"));
    QVERIFY(!first->font().bold()); //no Next key: nothing highlighted

    int activated = -1;
    connect(&panel, &LogNotesPanel::presetActivated, this, [&activated](int index) { activated = index; });
    second->click();
    QCOMPARE(activated, 1);

    //with a Next key, the next one is bold and named
    presets.nextKey = QKeySequence("F9");
    presets.advance();
    panel.setPresets(presets);
    QVERIFY(panel.findChild<QPushButton *>("preset1")->font().bold());
    QVERIFY(!panel.findChild<QPushButton *>("preset0")->font().bold());
    QLabel *next = nullptr;
    for (QLabel *label : panel.findChildren<QLabel *>())
        if (label->text().startsWith("Next")) next = label;
    QVERIFY(next);
    QVERIFY(next->text().contains("AC on (2 of 3)"));
    QCOMPARE(panel.findChildren<QPushButton *>(QRegularExpression("^preset\\d$")).count(), 3); //old buttons gone

    bool undo = false, setup = false;
    connect(&panel, &LogNotesPanel::undoMarkerRequested, this, [&undo]() { undo = true; });
    connect(&panel, &LogNotesPanel::presetsRequested, this, [&setup]() { setup = true; });
    buttonWithText(&panel, "Undo Last Marker")->click();
    buttonWithText(&panel, "Presets...")->click();
    QVERIFY(undo && setup);

    panel.resize(360, 700);
    panel.setNotes(sampleNotes(), "run1.csv");
    screenshot(&panel, "log-notes-panel-presets");
}

//a template keeps the marker presets in use; using it brings them back
void TestLogNotesUi::testTemplatesWithPresets()
{
    MarkerPresets presets;
    presets.presets = { { "Ignition on", QKeySequence("Ctrl+1") } };
    presets.nextKey = QKeySequence("F9");
    LogNotesDialog::saveTemplate("with presets", sampleNotes(), presets);
    LogNotesDialog::saveTemplate("without", sampleNotes());

    LogNotes notes;
    MarkerPresets loaded;
    bool hasPresets = false;
    QVERIFY(LogNotesDialog::loadTemplate("with presets", notes, &loaded, &hasPresets));
    QVERIFY(hasPresets);
    QVERIFY(loaded == presets);
    QVERIFY(notes == sampleNotes().userFields()); //the presets aren't one of the notes' sections
    QVERIFY(LogNotesDialog::loadTemplate("without", notes, &loaded, &hasPresets));
    QVERIFY(!hasPresets);

    //Use in the notes dialog: the template's presets become the ones in use; one without leaves them
    MarkerPresets inUse;
    inUse.presets = { { "something else", QKeySequence() } };
    inUse.save();
    LogNotesDialog dialog(LogNotesDialog::Editing);
    dialog.setNotes(LogNotes());
    QComboBox *templates = dialog.findChild<QComboBox *>("templates");
    templates->setCurrentIndex(templates->findText("without"));
    dialog.findChild<QPushButton *>("useTemplate")->click();
    QVERIFY(MarkerPresets::load() == inUse);
    templates->setCurrentIndex(templates->findText("with presets"));
    dialog.findChild<QPushButton *>("useTemplate")->click();
    QVERIFY(MarkerPresets::load() == presets);
    QCOMPARE(dialog.notes().otherSections, sampleNotes().otherSections); //no "Marker presets" among the notes'

    LogNotesDialog::deleteTemplate("with presets");
    LogNotesDialog::deleteTemplate("without");
    QSettings().remove("LogNotes/MarkerPresets");
}

QTEST_MAIN(TestLogNotesUi)
#include "test_log_notes_ui.moc"
