#include "piflexconfigdialog.h"
#include "helpwindow.h"

#include <QKeyEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>
#include <QMessageBox>
#include <QSettings>
#include <QStringList>
#include <QRegularExpression>

namespace {

//Rule table columns
enum { COL_R_SLOT = 0, COL_R_CYCLE_BASE, COL_R_CYCLE_MASK, COL_R_WIN_OFF, COL_R_WIN_LEN, COL_R_OVERRIDE, COL_R_COUNT };
//Source table columns
enum { COL_S_SLOT = 0, COL_S_FR1, COL_S_FR2, COL_S_PAYLOAD, COL_S_TRACK, COL_S_COUNT };

//Parse a cell as a number: "0x.." is hex, otherwise decimal. Returns false if it isn't a whole number.
bool parseNum(const QString &text, uint &out)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) { out = 0; return true; } //empty cell = 0
    bool ok = false;
    if (t.startsWith("0x", Qt::CaseInsensitive))
        out = t.mid(2).toUInt(&ok, 16);
    else
        out = t.toUInt(&ok, 10);
    return ok;
}

//Parse a run of hex byte tokens ("11 22 aa" or "11,22,aa") into bytes. Returns false on a bad token / value.
bool parsePayload(const QString &text, QByteArray &out)
{
    out.clear();
    const QStringList tokens = text.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
    for (const QString &tok : tokens) {
        bool ok = false;
        uint v = tok.toUInt(&ok, 16);
        if (!ok || v > 0xFF) return false;
        out.append(static_cast<char>(v));
    }
    return true;
}

QString payloadToHex(const QByteArray &payload)
{
    QStringList parts;
    for (unsigned char b : payload)
        parts << QString("%1").arg(b, 2, 16, QChar('0')).toUpper();
    return parts.join(' ');
}

QTableWidgetItem *checkItem(bool checked)
{
    QTableWidgetItem *item = new QTableWidgetItem();
    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

} // namespace

PiFlexConfigDialog::PiFlexConfigDialog(PicoFlexRay *conn, QWidget *parent) :
    QDialog(parent),
    mConn(conn)
{
    //Show which device this dialog is editing (user label if set, else the port/serial).
    const QString devId = conn ? (conn->getName().isEmpty() ? conn->getPort() : conn->getName()) : QString();
    setWindowTitle(devId.isEmpty() ? QStringLiteral("PiFlex injection + generator config")
                                   : QStringLiteral("PiFlex config — %1").arg(devId));
    setWindowFlags(windowFlags() | Qt::Window);
    buildUi();
    loadFromSettings();
    installEventFilter(this); //F1 context help (dialog is created fresh each open, so no re-install needed)

    if (mConn) {
        //live status updates; auto-disconnected when either object dies (connect with the dialog as context)
        connect(mConn, &PicoFlexRay::injectStatus, this, &PiFlexConfigDialog::onInjectStatus);
        connect(mConn, &PicoFlexRay::deviceMode, this, &PiFlexConfigDialog::onDeviceMode);
        connect(mConn.data(), &QObject::destroyed, this, &QWidget::close);
        mConn->requestInjectStatus(); //ask for an immediate reading
        mConn->requestMode();         //read (never set) the device's current run mode
    }
}

void PiFlexConfigDialog::buildUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);

    // --- Run mode --------------------------------------------------------------------------------------------
    QGroupBox *modeBox = new QGroupBox("Run mode", this);
    QHBoxLayout *modeLayout = new QHBoxLayout(modeBox);
    mModeCombo = new QComboBox(modeBox);
    mModeCombo->addItem("Sniff", static_cast<int>(PicoFlexRay::MODE_SNIFF));
    mModeCombo->addItem("Bridge", static_cast<int>(PicoFlexRay::MODE_BRIDGE));
    mModeCombo->addItem("Inject", static_cast<int>(PicoFlexRay::MODE_INJECT));
    mModeCombo->addItem("Source", static_cast<int>(PicoFlexRay::MODE_SOURCE));
    mModeCombo->setToolTip("Pick a run mode and press Set mode to switch the device. Nothing is sent on connect;\n"
                           "the mode only changes on your explicit Set mode. Injection rules apply in Inject mode,\n"
                           "the generator schedule in Source mode.");
    QPushButton *setModeBtn = new QPushButton("Set mode", modeBox);
    mModeDeviceLabel = new QLabel("Device mode: unknown", modeBox);
    modeLayout->addWidget(new QLabel("Mode:", modeBox));
    modeLayout->addWidget(mModeCombo);
    modeLayout->addWidget(setModeBtn);
    modeLayout->addStretch();
    modeLayout->addWidget(mModeDeviceLabel);
    root->addWidget(modeBox);
    connect(setModeBtn, &QPushButton::clicked, this, &PiFlexConfigDialog::onSetMode);

    // --- Injection rules -------------------------------------------------------------------------------------
    QGroupBox *ruleBox = new QGroupBox("Injection rules", this);
    mRuleBox = ruleBox;
    QVBoxLayout *ruleLayout = new QVBoxLayout(ruleBox);
    mRuleTable = new QTableWidget(0, COL_R_COUNT, ruleBox);
    mRuleTable->setHorizontalHeaderLabels(
        {"Slot ID", "Cycle base", "Cycle mask", "Win offset", "Win length", "Override payload (hex)"});
    mRuleTable->horizontalHeader()->setStretchLastSection(true);
    mRuleTable->setToolTip("A frame matches when its slot ID equals Slot ID and (cycle & mask) == (base & mask).\n"
                           "Cycle mask 0 matches any cycle. Numbers may be decimal or 0x-hex.\n"
                           "Override payload: even-length hex bytes to fire with Send override (uses this row's\n"
                           "slot ID + cycle base). The window offset/length select which of these bytes reach the bus.");
    ruleLayout->addWidget(mRuleTable);
    QHBoxLayout *ruleBtns = new QHBoxLayout();
    QPushButton *addRule = new QPushButton("Add rule", ruleBox);
    QPushButton *delRule = new QPushButton("Delete selected", ruleBox);
    QPushButton *sendOverride = new QPushButton("Send override", ruleBox);
    QPushButton *applyRules = new QPushButton("Apply rules to device", ruleBox);
    QPushButton *clearRules = new QPushButton("Clear device rules", ruleBox);
    sendOverride->setToolTip("Fire a 0x90 override for the selected rule (its slot ID, cycle base and the\n"
                             "Override payload). The override splices only on the cycle equal to Cycle base.\n"
                             "Confirm the splice from the capture - 'Rule matches' counts rule hits, not splices.");
    ruleBtns->addWidget(addRule);
    ruleBtns->addWidget(delRule);
    ruleBtns->addWidget(sendOverride);
    ruleBtns->addStretch();
    ruleBtns->addWidget(applyRules);
    ruleBtns->addWidget(clearRules);
    ruleLayout->addLayout(ruleBtns);
    root->addWidget(ruleBox);

    connect(addRule, &QPushButton::clicked, this, &PiFlexConfigDialog::onAddRule);
    connect(delRule, &QPushButton::clicked, this, &PiFlexConfigDialog::onDeleteRule);
    connect(sendOverride, &QPushButton::clicked, this, &PiFlexConfigDialog::onSendOverride);
    connect(applyRules, &QPushButton::clicked, this, &PiFlexConfigDialog::onApplyRules);
    connect(clearRules, &QPushButton::clicked, this, &PiFlexConfigDialog::onClearRules);

    // --- Generator schedule ----------------------------------------------------------------------------------
    QGroupBox *srcBox = new QGroupBox("Generator schedule", this);
    mSrcBox = srcBox;
    QVBoxLayout *srcLayout = new QVBoxLayout(srcBox);
    mSourceTable = new QTableWidget(0, COL_S_COUNT, srcBox);
    mSourceTable->setHorizontalHeaderLabels(
        {"Slot ID", "FR1", "FR2", "Payload (hex)", "Last byte = cycle"});
    mSourceTable->horizontalHeader()->setStretchLastSection(false);
    mSourceTable->horizontalHeader()->setSectionResizeMode(COL_S_PAYLOAD, QHeaderView::Stretch);
    mSourceTable->setToolTip("Slots the device transmits on its own schedule. Tick FR1/FR2 for the bus(es).\n"
                             "Payload is space-separated hex bytes. 'Last byte = cycle' makes the device write\n"
                             "the current cycle counter into the final payload byte.");
    srcLayout->addWidget(mSourceTable);

    QHBoxLayout *srcBtns = new QHBoxLayout();
    QPushButton *addSrc = new QPushButton("Add slot", srcBox);
    QPushButton *delSrc = new QPushButton("Delete selected", srcBox);
    srcBtns->addWidget(addSrc);
    srcBtns->addWidget(delSrc);
    srcBtns->addStretch();
    srcLayout->addLayout(srcBtns);

    QFormLayout *paramForm = new QFormLayout();
    mPeriodUs = new QSpinBox(srcBox);
    mPeriodUs->setRange(0, 1000000000);
    mPeriodUs->setSuffix(" us");
    mCycleGapUs = new QSpinBox(srcBox);
    mCycleGapUs->setRange(0, 1000000000);
    mCycleGapUs->setSuffix(" us");
    mRun = new QCheckBox("Run generator", srcBox);
    paramForm->addRow("Frame period:", mPeriodUs);
    paramForm->addRow("Inter-cycle gap:", mCycleGapUs);
    paramForm->addRow(QString(), mRun);
    srcLayout->addLayout(paramForm);

    QHBoxLayout *srcApply = new QHBoxLayout();
    srcApply->addStretch();
    QPushButton *applySched = new QPushButton("Apply schedule to device", srcBox);
    QPushButton *clearSched = new QPushButton("Clear device schedule", srcBox);
    srcApply->addWidget(applySched);
    srcApply->addWidget(clearSched);
    srcLayout->addLayout(srcApply);
    root->addWidget(srcBox);

    connect(addSrc, &QPushButton::clicked, this, &PiFlexConfigDialog::onAddSource);
    connect(delSrc, &QPushButton::clicked, this, &PiFlexConfigDialog::onDeleteSource);
    connect(applySched, &QPushButton::clicked, this, &PiFlexConfigDialog::onApplySchedule);
    connect(clearSched, &QPushButton::clicked, this, &PiFlexConfigDialog::onClearSchedule);

    // --- Device status ---------------------------------------------------------------------------------------
    QGroupBox *statusBox = new QGroupBox("Device status (op 0xa3)", this);
    QVBoxLayout *statusLayout = new QVBoxLayout(statusBox);
    mStatusLine = new QLabel("No status yet — connect the device.", statusBox);
    mStatusLine->setTextFormat(Qt::RichText);
    statusLayout->addWidget(mStatusLine);
    root->addWidget(statusBox);

    QHBoxLayout *bottom = new QHBoxLayout();
    bottom->addStretch();
    QPushButton *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    bottom->addWidget(closeBtn);
    root->addLayout(bottom);

    resize(680, 620);
}

void PiFlexConfigDialog::loadFromSettings()
{
    if (!mConn) return;
    QSettings settings;
    const QString base = mConn->settingsGroup();

    const QList<PicoFlexRay::InjectRule> rules =
        PicoFlexRay::deserialiseInjectRules(settings.value(base + "/injectRules").toByteArray());
    for (const PicoFlexRay::InjectRule &r : rules) addRuleRow(r);

    const QList<PicoFlexRay::SourceSlot> slotList =
        PicoFlexRay::deserialiseSourceSlots(settings.value(base + "/sourceSlots").toByteArray());
    for (const PicoFlexRay::SourceSlot &s : slotList) addSourceRow(s);

    mPeriodUs->setValue(settings.value(base + "/srcPeriodUs", 0).toInt());
    mCycleGapUs->setValue(settings.value(base + "/srcCycleGapUs", 0).toInt());
    mRun->setChecked(settings.value(base + "/srcEnable", false).toBool());
}

void PiFlexConfigDialog::addRuleRow(const PicoFlexRay::InjectRule &rule)
{
    const int row = mRuleTable->rowCount();
    mRuleTable->insertRow(row);
    mRuleTable->setItem(row, COL_R_SLOT, new QTableWidgetItem(QString("0x%1").arg(rule.slotId, 0, 16)));
    mRuleTable->setItem(row, COL_R_CYCLE_BASE, new QTableWidgetItem(QString::number(rule.cycleBase)));
    mRuleTable->setItem(row, COL_R_CYCLE_MASK, new QTableWidgetItem(QString("0x%1").arg(rule.cycleMask, 0, 16)));
    mRuleTable->setItem(row, COL_R_WIN_OFF, new QTableWidgetItem(QString::number(rule.winOffset)));
    mRuleTable->setItem(row, COL_R_WIN_LEN, new QTableWidgetItem(QString::number(rule.winLength)));
    mRuleTable->setItem(row, COL_R_OVERRIDE, new QTableWidgetItem(QString())); //transient; filled before Send override
}

void PiFlexConfigDialog::addSourceRow(const PicoFlexRay::SourceSlot &slot)
{
    const int row = mSourceTable->rowCount();
    mSourceTable->insertRow(row);
    mSourceTable->setItem(row, COL_S_SLOT, new QTableWidgetItem(QString("0x%1").arg(slot.slotId, 0, 16)));
    mSourceTable->setItem(row, COL_S_FR1, checkItem(slot.chanMask & 0x01));
    mSourceTable->setItem(row, COL_S_FR2, checkItem(slot.chanMask & 0x02));
    mSourceTable->setItem(row, COL_S_PAYLOAD, new QTableWidgetItem(payloadToHex(slot.payload)));
    mSourceTable->setItem(row, COL_S_TRACK, checkItem(slot.lastByteTracksCycle));
}

bool PiFlexConfigDialog::gatherRules(QList<PicoFlexRay::InjectRule> &out)
{
    out.clear();
    for (int row = 0; row < mRuleTable->rowCount(); row++) {
        auto cell = [&](int col) { QTableWidgetItem *it = mRuleTable->item(row, col); return it ? it->text() : QString(); };
        auto bad = [&](int col, const QString &msg) {
            mRuleTable->setCurrentCell(row, col);
            QMessageBox::warning(this, "Invalid rule", QString("Row %1: %2").arg(row + 1).arg(msg));
            return false;
        };
        uint slot = 0, base = 0, mask = 0, off = 0, len = 0;
        if (!parseNum(cell(COL_R_SLOT), slot)) return bad(COL_R_SLOT, "slot ID isn't a number");
        if (slot > 0x7FF) return bad(COL_R_SLOT, "slot ID is out of range (0-0x7FF)");
        if (!parseNum(cell(COL_R_CYCLE_BASE), base) || base > 0xFF) return bad(COL_R_CYCLE_BASE, "cycle base must be 0-255");
        if (!parseNum(cell(COL_R_CYCLE_MASK), mask) || mask > 0xFF) return bad(COL_R_CYCLE_MASK, "cycle mask must be 0-255");
        if (!parseNum(cell(COL_R_WIN_OFF), off) || off > 0xFF) return bad(COL_R_WIN_OFF, "window offset must be 0-255");
        if (!parseNum(cell(COL_R_WIN_LEN), len) || len > 0xFF) return bad(COL_R_WIN_LEN, "window length must be 0-255");

        PicoFlexRay::InjectRule r;
        r.slotId = static_cast<quint16>(slot);
        r.cycleBase = static_cast<quint8>(base);
        r.cycleMask = static_cast<quint8>(mask);
        r.winOffset = static_cast<quint8>(off);
        r.winLength = static_cast<quint8>(len);
        out.append(r);
    }
    return true;
}

bool PiFlexConfigDialog::gatherSources(QList<PicoFlexRay::SourceSlot> &out)
{
    out.clear();
    for (int row = 0; row < mSourceTable->rowCount(); row++) {
        auto text = [&](int col) { QTableWidgetItem *it = mSourceTable->item(row, col); return it ? it->text() : QString(); };
        auto checked = [&](int col) { QTableWidgetItem *it = mSourceTable->item(row, col); return it && it->checkState() == Qt::Checked; };
        auto bad = [&](int col, const QString &msg) {
            mSourceTable->setCurrentCell(row, col);
            QMessageBox::warning(this, "Invalid generator slot", QString("Row %1: %2").arg(row + 1).arg(msg));
            return false;
        };
        uint slot = 0;
        if (!parseNum(text(COL_S_SLOT), slot)) return bad(COL_S_SLOT, "slot ID isn't a number");
        if (slot > 0x7FF) return bad(COL_S_SLOT, "slot ID is out of range (0-0x7FF)");
        QByteArray payload;
        if (!parsePayload(text(COL_S_PAYLOAD), payload)) return bad(COL_S_PAYLOAD, "payload must be hex bytes (e.g. 11 22 AA)");
        //FlexRay payloads are an even number of bytes (the header length is counted in 2-byte words), so the device
        //rejects an odd-length source slot. Enforce an even length in 2..254 here with a clear, slot-named message.
        if (payload.size() < 2 || payload.size() > CommFrame::FLEXRAY_MAX_PAYLOAD || (payload.size() % 2) != 0)
            return bad(COL_S_PAYLOAD, QString("slot 0x%1: payload must be an even number of bytes (2-254)")
                                          .arg(slot, 0, 16));

        PicoFlexRay::SourceSlot s;
        s.slotId = static_cast<quint16>(slot);
        s.chanMask = static_cast<quint8>((checked(COL_S_FR1) ? 0x01 : 0x00) | (checked(COL_S_FR2) ? 0x02 : 0x00));
        s.payload = payload;
        s.lastByteTracksCycle = checked(COL_S_TRACK);
        out.append(s);
    }
    return true;
}

void PiFlexConfigDialog::onAddRule()
{
    addRuleRow(PicoFlexRay::InjectRule());
}

void PiFlexConfigDialog::onDeleteRule()
{
    const int row = mRuleTable->currentRow();
    if (row >= 0) mRuleTable->removeRow(row);
}

void PiFlexConfigDialog::onApplyRules()
{
    if (!mConn) return;
    QList<PicoFlexRay::InjectRule> rules;
    if (!gatherRules(rules)) return;
    mConn->applyInjectRules(rules);
    mConn->requestInjectStatus();
}

void PiFlexConfigDialog::onClearRules()
{
    if (!mConn) return;
    mRuleTable->setRowCount(0);
    mConn->clearInjectRules();
    mConn->requestInjectStatus();
}

void PiFlexConfigDialog::onSendOverride()
{
    if (!mConn) return;
    const int row = mRuleTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "Send override", "Select an injection rule row first.");
        return;
    }
    auto cell = [&](int col) { QTableWidgetItem *it = mRuleTable->item(row, col); return it ? it->text() : QString(); };

    uint slot = 0, base = 0;
    if (!parseNum(cell(COL_R_SLOT), slot) || slot > 0x7FF) {
        QMessageBox::warning(this, "Send override", "This rule's slot ID is invalid (0-0x7FF).");
        return;
    }
    if (!parseNum(cell(COL_R_CYCLE_BASE), base) || base > 0xFF) {
        QMessageBox::warning(this, "Send override", "This rule's cycle base is invalid (0-255).");
        return;
    }
    QByteArray payload;
    if (!parsePayload(cell(COL_R_OVERRIDE), payload) || payload.isEmpty()) {
        QMessageBox::warning(this, "Send override", "Enter the override payload as hex bytes (e.g. AA BB CC DD).");
        return;
    }
    //FlexRay payloads are an even number of bytes (header length counts 2-byte words), so an odd override can't map
    //onto a slot; the device would reject it.
    if ((payload.size() % 2) != 0) {
        QMessageBox::warning(this, "Send override", "The override payload must be an even number of bytes.");
        return;
    }

    mConn->sendOverride(static_cast<int>(slot), static_cast<int>(base), payload);
    mConn->requestInjectStatus(); //refresh the match/drop counters
}

void PiFlexConfigDialog::onAddSource()
{
    //default a new slot to a clean, valid payload: FlexRay payloads are an even number of bytes (the header length
    //is in 2-byte words), so start with 8 bytes on FR1 rather than an empty/odd default the firmware would reject.
    PicoFlexRay::SourceSlot slot;
    slot.chanMask = 0x01; //FR1
    slot.payload = QByteArray(8, static_cast<char>(0x11));
    addSourceRow(slot);
}

void PiFlexConfigDialog::onDeleteSource()
{
    const int row = mSourceTable->currentRow();
    if (row >= 0) mSourceTable->removeRow(row);
}

void PiFlexConfigDialog::onApplySchedule()
{
    if (!mConn) return;
    QList<PicoFlexRay::SourceSlot> slotList;
    if (!gatherSources(slotList)) return;
    PicoFlexRay::SourceParams params;
    params.periodUs = static_cast<quint32>(mPeriodUs->value());
    params.cycleGapUs = static_cast<quint32>(mCycleGapUs->value());
    params.enable = mRun->isChecked();
    mConn->applySourceSchedule(slotList, params);
    mConn->requestInjectStatus();
}

void PiFlexConfigDialog::onClearSchedule()
{
    if (!mConn) return;
    mSourceTable->setRowCount(0);
    mConn->clearSourceSchedule();
    mConn->requestInjectStatus();
}

void PiFlexConfigDialog::onInjectStatus(QByteArray reply)
{
    PicoFlexRay::InjectStatus st;
    if (!PicoFlexRay::parseInjectStatus(reply, st)) {
        mStatusLine->setText("Device returned an unrecognised status reply.");
        return;
    }
    //"Rule matches" counts forwarded frames that matched an applied rule - NOT overrides applied (a splice
    //increments no counter). Confirm a splice landed from the capture (bytes on the wire), not this number.
    mStatusLine->setText(QString(
        "<b>Injector:</b> %1 &nbsp;&nbsp; <b>Rules on device:</b> %2 &nbsp;&nbsp; "
        "<b>Source slots:</b> %3 &nbsp;&nbsp; <b>Generator:</b> %4<br>"
        "<b>Rule matches:</b> %5 &nbsp;&nbsp; <b>Override drops:</b> %6 &nbsp;&nbsp; "
        "<b>Source drops:</b> %7")
            .arg(st.injectorEnabled ? "enabled" : "disabled")
            .arg(st.ruleCount)
            .arg(st.sourceSlotCount)
            .arg(st.sourceRunning ? "running" : "stopped")
            .arg(st.totalMatches)
            .arg(st.totalDrops)
            .arg(st.sourceSlotDrops));
}

void PiFlexConfigDialog::onSetMode()
{
    if (!mConn) return;
    const int mode = mModeCombo->currentData().toInt();
    mConn->setMode(mode); //explicit user choice: this is the only path that sends 0xa4
    //setMode reads the mode back and emits deviceMode, which refreshes the label + gating.
}

void PiFlexConfigDialog::onDeviceMode(int mode)
{
    if (!PicoFlexRay::isValidMode(mode)) {
        //firmware without mode support: show that, leave the selector disabled, and keep both config boxes usable
        //(the pre-mode inject/source paths still work on that firmware)
        mModeDeviceLabel->setText("Device mode: not supported by this firmware");
        if (mModeCombo) mModeCombo->setEnabled(false);
        if (mRuleBox) mRuleBox->setEnabled(true);
        if (mSrcBox) mSrcBox->setEnabled(true);
        return;
    }
    mModeDeviceLabel->setText(QString("Device mode: <b>%1</b>").arg(PicoFlexRay::modeName(mode)));
    mModeDeviceLabel->setTextFormat(Qt::RichText);
    if (mModeCombo) {
        mModeCombo->setEnabled(true);
        //reflect the device's actual mode (no combo signal is wired to sending, so this won't re-send)
        const int idx = mModeCombo->findData(mode);
        if (idx >= 0) mModeCombo->setCurrentIndex(idx);
    }
    applyModeGating(mode);
}

void PiFlexConfigDialog::applyModeGating(int mode)
{
    //Guide the user: injection rules are only relevant in INJECT mode, the generator schedule only in SOURCE mode.
    if (mRuleBox) mRuleBox->setEnabled(mode == PicoFlexRay::MODE_INJECT);
    if (mSrcBox)  mSrcBox->setEnabled(mode == PicoFlexRay::MODE_SOURCE);
}

bool PiFlexConfigDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_F1) {
            HelpWindow::getRef()->showHelp("piflexconfig.md");
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}
