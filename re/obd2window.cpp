#include "obd2window.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>

namespace
{
    const char *POLL_SET_FORMAT = "SavvyCAN OBD-II poll set";
    const int POLL_SET_VERSION = 1;
    const int ROLE_ECU = Qt::UserRole;
    const int ROLE_PID = Qt::UserRole + 1;

    QString hex(quint32 value, int digits)
    {
        return "0x" + QString::number(value, 16).toUpper().rightJustified(digits, '0');
    }

    QString formatNumber(double value, double scale)
    {
        int decimals = 0;
        if (scale < 1.0) decimals = qMin(4, int(std::ceil(-std::log10(scale))) + 1);
        QString text = QString::number(value, 'f', decimals);
        if (text.contains('.'))
        {
            while (text.endsWith('0')) text.chop(1);
            if (text.endsWith('.')) text.chop(1);
        }
        if (text == "-0") text = "0";
        return text;
    }
}

Obd2Window::Obd2Window(Obd2Poller *poller, int numBuses, QWidget *parent)
    : QDialog(parent), poller(poller)
{
    setWindowFlags(Qt::Window);
    setWindowTitle(tr("OBD-II Poller"));
    poller->setParent(this);

    buildLayout(numBuses);

    connect(poller, &Obd2Poller::ecuFound, this, &Obd2Window::ecuFound);
    connect(poller, &Obd2Poller::discoveryFinished, this, &Obd2Window::discoveryFinished);
    connect(poller, &Obd2Poller::pidUpdated, this, &Obd2Window::pidUpdated);
    connect(poller, &Obd2Poller::pidUnsupported, this, &Obd2Window::pidUnsupported);
    connect(poller, &Obd2Poller::requestTimedOut, this, &Obd2Window::requestTimedOut);
    connect(poller, &Obd2Poller::vehicleInfoRead, this, &Obd2Window::vehicleInfoRead);
    connect(poller, &Obd2Poller::statsChanged, this, &Obd2Window::statsChanged);

    poller->setBus(cbBus->currentIndex() < 0 ? 0 : cbBus->currentIndex());
    poller->setMaxRequestRate(spinRate->value());

    rebuildTable();
    updateControls();
    updateWarning();
    statsChanged();
}

Obd2Window::~Obd2Window()
{
    poller->stop();
}

void Obd2Window::buildLayout(int numBuses)
{
    cbBus = new QComboBox;
    cbBus->setObjectName("busCombo");
    for (int b = 0; b < numBuses; b++) cbBus->addItem(QString::number(b));

    cbAddressing = new QComboBox;
    cbAddressing->addItem(tr("11-bit (0x7DF, 0x7E0-0x7EF)"));
    cbAddressing->addItem(tr("29-bit (0x18DB33F1, 0x18DAxxF1)"));

    spinRate = new QSpinBox;
    spinRate->setRange(1, 100);
    spinRate->setValue(Obd2Poller::DEFAULT_MAX_REQUEST_RATE);
    spinRate->setSuffix(tr(" requests/s"));
    spinRate->setToolTip(tr("The most requests sent a second. Many gateways answer only 10 to 30 a second in all."));

    btnDiscover = new QPushButton(tr("Discover ECUs"));
    btnDiscover->setToolTip(tr("Ask every ECU which PIDs it supports"));
    ckShowAll = new QCheckBox(tr("Show unsupported PIDs"));
    ckShowAll->setObjectName("showAllCheck");
    ckShowAll->setToolTip(tr("After discovery, also list the PIDs the ECUs didn't report as supported"));

    cbVinEcu = new QComboBox;
    btnVin = new QPushButton(tr("Read VIN"));

    QGridLayout *setup = new QGridLayout;
    setup->addWidget(new QLabel(tr("Bus:")), 0, 0);
    setup->addWidget(cbBus, 0, 1);
    setup->addWidget(new QLabel(tr("Addressing:")), 0, 2);
    setup->addWidget(cbAddressing, 0, 3);
    setup->addWidget(new QLabel(tr("Rate limit:")), 0, 4);
    setup->addWidget(spinRate, 0, 5);
    setup->addWidget(btnDiscover, 1, 0, 1, 2);
    setup->addWidget(ckShowAll, 1, 2, 1, 2);
    setup->addWidget(new QLabel(tr("VIN from:")), 1, 4);
    QHBoxLayout *vinRow = new QHBoxLayout;
    vinRow->addWidget(cbVinEcu, 1);
    vinRow->addWidget(btnVin);
    setup->addLayout(vinRow, 1, 5);
    setup->setColumnStretch(3, 1);

    lblWarning = new QLabel;
    lblWarning->setWordWrap(true);
    lblWarning->setStyleSheet("color: #c0392b; font-weight: bold;");
    lblWarning->hide();

    pidTable = new QTableWidget(0, COL_COUNT);
    pidTable->setHorizontalHeaderLabels({tr("Poll"), tr("ECU"), tr("PID"), tr("Name"), tr("Value"),
                                         tr("Interval (ms)"), tr("Answers"), tr("Timeouts")});
    pidTable->verticalHeader()->hide();
    pidTable->verticalHeader()->setDefaultSectionSize(pidTable->fontMetrics().height() + 6);
    pidTable->setWordWrap(false);
    pidTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    pidTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
                              | QAbstractItemView::AnyKeyPressed);
    pidTable->horizontalHeader()->setSectionResizeMode(COL_NAME, QHeaderView::Stretch);
    pidTable->horizontalHeader()->setSectionResizeMode(COL_VALUE, QHeaderView::Stretch);
    pidTable->setColumnWidth(COL_POLL, 40);
    pidTable->setColumnWidth(COL_ECU, 110);
    pidTable->setColumnWidth(COL_PID, 50);
    pidTable->setColumnWidth(COL_INTERVAL, 90);
    pidTable->setColumnWidth(COL_UPDATES, 70);
    pidTable->setColumnWidth(COL_TIMEOUTS, 70);

    btnStart = new QPushButton(tr("Start Polling"));
    btnStop = new QPushButton(tr("Stop"));
    btnSave = new QPushButton(tr("Save Poll Set..."));
    btnLoad = new QPushButton(tr("Load Poll Set..."));
    btnHelp = new QPushButton(tr("Help"));

    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(btnStart);
    buttons->addWidget(btnStop);
    buttons->addStretch(1);
    buttons->addWidget(btnLoad);
    buttons->addWidget(btnSave);
    buttons->addWidget(btnHelp);

    lblStatus = new QLabel;
    lblVin = new QLabel;
    lblVin->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(setup);
    layout->addWidget(lblWarning);
    layout->addWidget(pidTable, 1);
    layout->addWidget(lblVin);
    layout->addWidget(lblStatus);
    layout->addLayout(buttons);

    resize(900, 600);

    connect(cbBus, &QComboBox::currentIndexChanged, this, &Obd2Window::busChanged);
    connect(cbAddressing, &QComboBox::currentIndexChanged, this, &Obd2Window::addressingChanged);
    connect(spinRate, &QSpinBox::valueChanged, this, &Obd2Window::rateChanged);
    connect(btnDiscover, &QPushButton::clicked, this, &Obd2Window::discover);
    connect(ckShowAll, &QCheckBox::toggled, this, &Obd2Window::rebuildTable);
    connect(btnVin, &QPushButton::clicked, this, &Obd2Window::readVin);
    connect(btnStart, &QPushButton::clicked, this, &Obd2Window::startPolling);
    connect(btnStop, &QPushButton::clicked, this, &Obd2Window::stopPolling);
    connect(btnSave, &QPushButton::clicked, this, &Obd2Window::savePollSetDialog);
    connect(btnLoad, &QPushButton::clicked, this, &Obd2Window::loadPollSetDialog);
    connect(btnHelp, &QPushButton::clicked, this, [this]() { if (helpCallback) helpCallback(); });
    connect(pidTable, &QTableWidget::itemChanged, this, &Obd2Window::tableChanged);
}

void Obd2Window::setListenOnlyCheck(std::function<bool(int bus)> check)
{
    listenOnlyCheck = check;
    updateWarning();
}

void Obd2Window::setHelpCallback(std::function<void()> callback)
{
    helpCallback = callback;
}

QString Obd2Window::ecuText(quint32 ecuReplyId) const
{
    if (ecuReplyId == 0) return tr("All (%1)").arg(hex(poller->functionalRequestId(), 3));
    return hex(ecuReplyId, ecuReplyId > 0x7FF ? 8 : 3);
}

QString Obd2Window::formatValues(const QList<OBD2::Value> &values)
{
    QStringList parts;
    for (const OBD2::Value &v : values)
    {
        QString text = v.text.isEmpty() ? formatNumber(v.value, v.field->scale) : v.text;
        if (v.text.isEmpty() && !v.field->unit.isEmpty()) text += " " + v.field->unit;
        if (values.size() > 1) text = v.field->name + " " + text;
        parts.append(text);
    }
    return parts.join(", ");
}

int Obd2Window::rowFor(quint32 ecuReplyId, quint8 pid) const
{
    return rows.value({ecuReplyId, pid}, -1);
}

int Obd2Window::addRow(quint32 ecuReplyId, quint8 pid, bool polled, int intervalMs)
{
    const OBD2::PidInfo *info = OBD2::findMode01Pid(pid);
    int row = pidTable->rowCount();
    pidTable->insertRow(row);

    QTableWidgetItem *poll = new QTableWidgetItem;
    poll->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    poll->setCheckState(polled ? Qt::Checked : Qt::Unchecked);
    poll->setData(ROLE_ECU, ecuReplyId);
    poll->setData(ROLE_PID, pid);
    pidTable->setItem(row, COL_POLL, poll);

    auto readOnly = [](const QString &text) {
        QTableWidgetItem *item = new QTableWidgetItem(text);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        return item;
    };
    pidTable->setItem(row, COL_ECU, readOnly(ecuText(ecuReplyId)));
    pidTable->setItem(row, COL_PID, readOnly(hex(pid, 2)));
    pidTable->setItem(row, COL_NAME, readOnly(info ? info->name : tr("Unknown PID")));
    pidTable->setItem(row, COL_VALUE, readOnly(QString()));
    QTableWidgetItem *interval = new QTableWidgetItem(QString::number(intervalMs));
    interval->setToolTip(tr("Time between requests for this PID; 0 polls it as often as the others allow"));
    pidTable->setItem(row, COL_INTERVAL, interval);
    pidTable->setItem(row, COL_UPDATES, readOnly("0"));
    pidTable->setItem(row, COL_TIMEOUTS, readOnly("0"));

    rows.insert({ecuReplyId, pid}, row);
    return row;
}

void Obd2Window::rebuildTable()
{
    //keep what's ticked, and the intervals, across the rebuild
    QHash<RowKey, int> polled;
    for (auto it = rows.constBegin(); it != rows.constEnd(); ++it)
    {
        if (pidTable->item(it.value(), COL_POLL)->checkState() == Qt::Checked)
            polled.insert(it.key(), pidTable->item(it.value(), COL_INTERVAL)->text().toInt());
    }

    updatingTable = true;
    pidTable->setRowCount(0);
    rows.clear();

    auto addFor = [&](quint32 ecu, quint8 pid) {
        if (rows.contains({ecu, pid})) return;
        addRow(ecu, pid, polled.contains({ecu, pid}), polled.value({ecu, pid}, 0));
    };

    const QList<Obd2Poller::Ecu> &ecus = poller->ecus();
    if (ecus.isEmpty())
    {
        for (const OBD2::PidInfo &info : OBD2::mode01Pids())
        {
            if (!OBD2::isSupportBitmapPid(info.pid)) addFor(0, info.pid);
        }
    }
    else
    {
        for (const Obd2Poller::Ecu &ecu : ecus)
        {
            for (quint8 pid : ecu.supportedPids) addFor(ecu.replyId, pid);
            if (ckShowAll->isChecked())
            {
                for (const OBD2::PidInfo &info : OBD2::mode01Pids())
                {
                    if (!OBD2::isSupportBitmapPid(info.pid)) addFor(ecu.replyId, info.pid);
                }
            }
        }
    }
    //rows ticked before (for all ECUs, or from a poll set) stay
    for (auto it = polled.constBegin(); it != polled.constEnd(); ++it) addFor(it.key().ecu, it.key().pid);

    updatingTable = false;
    applyPollList();
}

void Obd2Window::setPolled(quint32 ecuReplyId, quint8 pid, bool polled, int intervalMs)
{
    int row = rowFor(ecuReplyId, pid);
    updatingTable = true;
    if (row < 0) row = addRow(ecuReplyId, pid, polled, intervalMs);
    else
    {
        pidTable->item(row, COL_POLL)->setCheckState(polled ? Qt::Checked : Qt::Unchecked);
        pidTable->item(row, COL_INTERVAL)->setText(QString::number(intervalMs));
    }
    updatingTable = false;
    applyPollList();
}

void Obd2Window::applyPollList()
{
    QList<Obd2Poller::PollItem> items;
    for (int row = 0; row < pidTable->rowCount(); row++)
    {
        QTableWidgetItem *poll = pidTable->item(row, COL_POLL);
        if (poll->checkState() != Qt::Checked) continue;
        int interval = qMax(0, pidTable->item(row, COL_INTERVAL)->text().toInt());
        items.append({poll->data(ROLE_ECU).toUInt(), static_cast<quint8>(poll->data(ROLE_PID).toUInt()), interval});
    }
    poller->setPollList(items);
}

void Obd2Window::tableChanged()
{
    if (updatingTable) return;
    applyPollList();
}

void Obd2Window::updateControls()
{
    bool running = poller->isRunning();
    bool discovering = poller->isDiscovering();
    cbBus->setEnabled(!running && !discovering);
    cbAddressing->setEnabled(!running && !discovering);
    btnDiscover->setEnabled(!running && !discovering);
    btnStart->setEnabled(!running);
    btnStop->setEnabled(running);
    btnLoad->setEnabled(!running);
}

void Obd2Window::updateWarning()
{
    QString text;
    if (cbBus->count() == 0) text = tr("No bus is open. Open a connection first (Connection > Open Connection Window).");
    else if (listenOnlyCheck && listenOnlyCheck(cbBus->currentIndex()))
        text = tr("Bus %1 is listen-only, so no requests can be sent. Turn listen-only off in the connection "
                  "settings.").arg(cbBus->currentIndex());
    lblWarning->setText(text);
    lblWarning->setVisible(!text.isEmpty());
}

QString Obd2Window::statusText() const
{
    return lblStatus->text();
}

QString Obd2Window::vinText() const
{
    return lblVin->text();
}

QString Obd2Window::warningText() const
{
    return lblWarning->isHidden() ? QString() : lblWarning->text();
}

void Obd2Window::busChanged()
{
    poller->setBus(qMax(0, cbBus->currentIndex()));
    cbVinEcu->clear();
    rebuildTable();
    updateWarning();
}

void Obd2Window::addressingChanged()
{
    poller->setAddressing(cbAddressing->currentIndex() == 1 ? Obd2Poller::Addressing::Extended29Bit
                                                            : Obd2Poller::Addressing::Standard11Bit);
    cbVinEcu->clear();
    rebuildTable();
}

void Obd2Window::rateChanged(int perSecond)
{
    poller->setMaxRequestRate(perSecond);
}

void Obd2Window::discover()
{
    cbVinEcu->clear();
    poller->discover();
    updateControls();
    if (poller->isDiscovering()) lblStatus->setText(tr("Discovering ECUs..."));
}

void Obd2Window::ecuFound(quint32 ecuReplyId)
{
    if (cbVinEcu->findData(ecuReplyId) < 0) cbVinEcu->addItem(ecuText(ecuReplyId), ecuReplyId);
}

void Obd2Window::discoveryFinished()
{
    rebuildTable();
    updateControls();
    int count = poller->ecus().size();
    if (count == 0) lblStatus->setText(tr("No ECU answered. Check the bus, its speed and the addressing."));
    else
    {
        QStringList found;
        for (const Obd2Poller::Ecu &ecu : poller->ecus())
            found.append(tr("%1 (%2 PIDs)").arg(ecuText(ecu.replyId)).arg(ecu.supportedPids.size()));
        lblStatus->setText(tr("Found %n ECU(s): ", "", count) + found.join(", "));
    }
}

void Obd2Window::startPolling()
{
    applyPollList();
    bool any = false;
    for (int row = 0; row < pidTable->rowCount(); row++)
    {
        if (pidTable->item(row, COL_POLL)->checkState() == Qt::Checked) any = true;
    }
    if (!any)
    {
        lblStatus->setText(tr("Tick the PIDs to poll first."));
        return;
    }
    poller->start();
    updateControls();
}

void Obd2Window::stopPolling()
{
    poller->stop();
    updateControls();
}

void Obd2Window::readVin()
{
    quint32 ecu = cbVinEcu->currentData().toUInt();
    if (ecu == 0) //nothing discovered yet: the engine ECU's usual address
        ecu = (poller->addressing() == Obd2Poller::Addressing::Standard11Bit) ? 0x7E8 : 0x18DAF110;
    if (poller->readVin(ecu)) lblVin->setText(tr("Reading the VIN from %1...").arg(ecuText(ecu)));
}

void Obd2Window::pidUpdated(quint32 ecuReplyId, quint8 pid, const QByteArray &data, const QList<OBD2::Value> &values)
{
    if (OBD2::isSupportBitmapPid(pid)) return;
    int row = rowFor(ecuReplyId, pid);
    bool fromAll = false;
    if (row < 0)
    {
        row = rowFor(0, pid); //an answer to a request for all ECUs
        fromAll = true;
    }
    if (row < 0) return;

    QString text = values.isEmpty() ? QString(data.toHex(' ').toUpper()) : formatValues(values);
    if (fromAll) text += QString(" (%1)").arg(ecuText(ecuReplyId));

    updatingTable = true;
    pidTable->item(row, COL_VALUE)->setText(text);
    QTableWidgetItem *updates = pidTable->item(row, COL_UPDATES);
    updates->setText(QString::number(updates->text().toInt() + 1));
    updatingTable = false;
}

void Obd2Window::pidUnsupported(quint32 ecuReplyId, quint8 pid)
{
    int row = rowFor(ecuReplyId, pid);
    if (row < 0) return;
    updatingTable = true;
    pidTable->item(row, COL_VALUE)->setText(tr("Not supported by this ECU"));
    updatingTable = false;
}

void Obd2Window::requestTimedOut(quint32 ecuReplyId, const QByteArray &request)
{
    if (request.size() < 2 || static_cast<quint8>(request[0]) != OBD2::SHOW_CURRENT_DATA) return;
    int row = rowFor(ecuReplyId, static_cast<quint8>(request[1]));
    if (row < 0) return;
    updatingTable = true;
    QTableWidgetItem *timeouts = pidTable->item(row, COL_TIMEOUTS);
    timeouts->setText(QString::number(timeouts->text().toInt() + 1));
    updatingTable = false;
}

void Obd2Window::vehicleInfoRead(quint32 ecuReplyId, quint8 pid, const QString &text)
{
    if (pid == OBD2::VIN_PID) lblVin->setText(tr("VIN (%1): %2").arg(ecuText(ecuReplyId), text));
}

void Obd2Window::statsChanged()
{
    if (poller->isDiscovering()) return;
    const Obd2Poller::Stats &s = poller->stats();
    lblStatus->setText(tr("Requests: %1   Answers: %2   Timeouts: %3   Refused: %4   Answers in the last second: %5")
                           .arg(s.requests).arg(s.replies).arg(s.timeouts).arg(s.negativeReplies)
                           .arg(s.repliesLastSecond));
}

QJsonObject Obd2Window::pollSetToJson() const
{
    QJsonArray items;
    for (int row = 0; row < pidTable->rowCount(); row++)
    {
        QTableWidgetItem *poll = pidTable->item(row, COL_POLL);
        if (poll->checkState() != Qt::Checked) continue;
        quint32 ecu = poll->data(ROLE_ECU).toUInt();
        QJsonObject item;
        item["ecu"] = ecu == 0 ? QString("all") : hex(ecu, ecu > 0x7FF ? 8 : 3);
        item["pid"] = hex(poll->data(ROLE_PID).toUInt(), 2);
        item["intervalMs"] = qMax(0, pidTable->item(row, COL_INTERVAL)->text().toInt());
        items.append(item);
    }

    QJsonObject json;
    json["format"] = POLL_SET_FORMAT;
    json["version"] = POLL_SET_VERSION;
    json["bus"] = qMax(0, cbBus->currentIndex());
    json["addressing"] = cbAddressing->currentIndex() == 1 ? "29-bit" : "11-bit";
    json["maxRequestRate"] = spinRate->value();
    json["items"] = items;
    return json;
}

bool Obd2Window::pollSetFromJson(const QJsonObject &json, QString *error)
{
    auto fail = [error](const QString &message) {
        if (error) *error = message;
        return false;
    };
    if (json["format"].toString() != POLL_SET_FORMAT) return fail(tr("This isn't a SavvyCAN OBD-II poll set."));
    if (json["version"].toInt() > POLL_SET_VERSION)
        return fail(tr("This poll set was saved by a newer SavvyCAN (version %1).").arg(json["version"].toInt()));

    //read every item before changing anything
    struct Item { quint32 ecu; quint8 pid; int interval; };
    QList<Item> items;
    for (const QJsonValue &value : json["items"].toArray())
    {
        QJsonObject o = value.toObject();
        bool okEcu = true, okPid = false;
        QString ecuString = o["ecu"].toString();
        quint32 ecu = (ecuString == "all") ? 0 : ecuString.toUInt(&okEcu, 0);
        uint pid = o["pid"].toString().toUInt(&okPid, 0);
        if (!okEcu || !okPid || pid > 0xFF) return fail(tr("A PID in the poll set can't be read: %1 %2")
                                                             .arg(ecuString, o["pid"].toString()));
        items.append({ecu, static_cast<quint8>(pid), qMax(0, o["intervalMs"].toInt())});
    }

    int bus = json["bus"].toInt();
    if (bus >= 0 && bus < cbBus->count()) cbBus->setCurrentIndex(bus);
    cbAddressing->setCurrentIndex(json["addressing"].toString() == "29-bit" ? 1 : 0);
    if (json.contains("maxRequestRate")) spinRate->setValue(json["maxRequestRate"].toInt());

    updatingTable = true;
    for (int row = 0; row < pidTable->rowCount(); row++) pidTable->item(row, COL_POLL)->setCheckState(Qt::Unchecked);
    updatingTable = false;
    for (const Item &item : items) setPolled(item.ecu, item.pid, true, item.interval);

    if (bus >= cbBus->count() && error) *error = tr("Bus %1 isn't open; using bus %2.").arg(bus).arg(cbBus->currentIndex());
    return true;
}

bool Obd2Window::savePollSet(const QString &filename, QString *error) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(pollSetToJson()).toJson());
    return true;
}

bool Obd2Window::loadPollSet(const QString &filename, QString *error)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!doc.isObject())
    {
        if (error) *error = tr("Not a JSON file: %1").arg(parseError.errorString());
        return false;
    }
    return pollSetFromJson(doc.object(), error);
}

void Obd2Window::savePollSetDialog()
{
    QSettings settings;
    QString filename = QFileDialog::getSaveFileName(this, tr("Save OBD-II Poll Set"),
                                                    settings.value("OBD2/LastPollSetPath").toString(),
                                                    tr("OBD-II poll sets (*.obd)"));
    if (filename.isEmpty()) return;
    if (!filename.endsWith(".obd", Qt::CaseInsensitive)) filename += ".obd";
    QString error;
    if (!savePollSet(filename, &error)) QMessageBox::warning(this, tr("Save Poll Set"), error);
    else settings.setValue("OBD2/LastPollSetPath", filename);
}

void Obd2Window::loadPollSetDialog()
{
    QSettings settings;
    QString filename = QFileDialog::getOpenFileName(this, tr("Load OBD-II Poll Set"),
                                                    settings.value("OBD2/LastPollSetPath").toString(),
                                                    tr("OBD-II poll sets (*.obd);;All files (*)"));
    if (filename.isEmpty()) return;
    QString error;
    bool ok = loadPollSet(filename, &error);
    if (!ok) QMessageBox::warning(this, tr("Load Poll Set"), error);
    else
    {
        settings.setValue("OBD2/LastPollSetPath", filename);
        if (!error.isEmpty()) QMessageBox::information(this, tr("Load Poll Set"), error);
    }
}

void Obd2Window::closeEvent(QCloseEvent *event)
{
    stopPolling(); //nothing keeps asking the car once the window is gone
    event->accept();
}

void Obd2Window::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F1)
    {
        if (helpCallback) helpCallback();
        return;
    }
    QDialog::keyPressEvent(event);
}
