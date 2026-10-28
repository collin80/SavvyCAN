#ifndef OBD2WINDOW_H
#define OBD2WINDOW_H

#include <QDialog>
#include <QHash>
#include <QJsonObject>
#include <functional>

#include "bus_protocols/obd2_poller.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

/*
 * Send Frames > OBD-II Poller: discovers the ECUs on a bus, polls the PIDs ticked in the table and shows their
 * values. The requests and replies go into the capture like any other traffic, so the log holds them next to the
 * other buses.
 *
 * The window drives an Obd2Poller it's given (and takes ownership of). What it needs from the rest of SavvyCAN
 * comes in through setters, so the unit tests can build it without a MainWindow.
 */
class Obd2Window : public QDialog
{
    Q_OBJECT

public:
    enum Column
    {
        COL_POLL,
        COL_ECU,
        COL_PID,
        COL_NAME,
        COL_VALUE,
        COL_INTERVAL,
        COL_UPDATES,
        COL_TIMEOUTS,
        COL_COUNT
    };

    Obd2Window(Obd2Poller *poller, int numBuses, QWidget *parent = nullptr);
    ~Obd2Window();

    //(ECU reply ID, DBC signal name) of one value to graph
    using GraphSignal = QPair<quint32, QString>;
    //Loads the DBC file just written and attaches it to the bus; false (with a reason) if it can't
    using DbcLoader = std::function<bool(const QString &path, int bus, const QList<quint32> &ecus, QString *error)>;
    //Opens a graph window with these signals of the OBD DBC on that bus; false (with a reason) if it can't
    using GraphOpener = std::function<bool(int bus, const QList<GraphSignal> &sigs, QString *error)>;

    void setListenOnlyCheck(std::function<bool(int bus)> check);
    void setHelpCallback(std::function<void()> callback);
    void setDbcLoader(DbcLoader loader);
    void setGraphOpener(GraphOpener opener);
    //True when the buses come from adapters with clocks of their own (more than one, and not the system clock)
    void setClockCheck(std::function<bool()> check);

    //The ECUs the OBD DBC gets a message for: those discovered, or the usual reply IDs before discovery
    QList<quint32> dbcEcus() const;
    //Writes the OBD DBC to path and hands it to the DBC loader
    bool makeDbc(const QString &path, QString *error = nullptr);
    //The signals for the ticked PIDs (a PID for all ECUs gives one per ECU in dbcEcus())
    QList<GraphSignal> polledSignals() const;

    //Poll sets (.obd files): bus, addressing, rate and the ticked PIDs with their intervals
    QJsonObject pollSetToJson() const;
    bool pollSetFromJson(const QJsonObject &json, QString *error = nullptr);
    bool savePollSet(const QString &filename, QString *error = nullptr) const;
    bool loadPollSet(const QString &filename, QString *error = nullptr);

    //One line of values for display, e.g. "1726 rpm" or "0.900 V, 0 %"
    static QString formatValues(const QList<OBD2::Value> &values);

    //For the tests
    QTableWidget *table() const { return pidTable; }
    Obd2Poller *obd2Poller() const { return poller; }
    int rowFor(quint32 ecuReplyId, quint8 pid) const;
    QString statusText() const;
    QString vinText() const;
    QString warningText() const;
    void setPolled(quint32 ecuReplyId, quint8 pid, bool polled, int intervalMs = 0);

public slots:
    void discover();
    void startPolling();
    void stopPolling();
    void readVin();
    void graphPolledPids();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void busChanged();
    void addressingChanged();
    void rateChanged(int perSecond);
    void ecuFound(quint32 ecuReplyId);
    void discoveryFinished();
    void pidUpdated(quint32 ecuReplyId, quint8 pid, const QByteArray &data, const QList<OBD2::Value> &values);
    void pidUnsupported(quint32 ecuReplyId, quint8 pid);
    void requestTimedOut(quint32 ecuReplyId, const QByteArray &request);
    void vehicleInfoRead(quint32 ecuReplyId, quint8 pid, const QString &text);
    void statsChanged();
    void tableChanged();
    void savePollSetDialog();
    void loadPollSetDialog();
    void makeDbcDialog();

private:
    struct RowKey
    {
        quint32 ecu;
        quint8 pid;
        bool operator==(const RowKey &o) const { return ecu == o.ecu && pid == o.pid; }
    };
    friend size_t qHash(const RowKey &key, size_t seed) { return qHash((quint64(key.ecu) << 8) | key.pid, seed); }

    void buildLayout(int numBuses);
    void rebuildTable();
    int addRow(quint32 ecuReplyId, quint8 pid, bool polled, int intervalMs);
    void applyPollList();
    void updateControls();
    void updateWarning();
    QString ecuText(quint32 ecuReplyId) const;

    Obd2Poller *poller;
    std::function<bool(int)> listenOnlyCheck;
    std::function<void()> helpCallback;
    DbcLoader dbcLoader;
    GraphOpener graphOpener;
    std::function<bool()> clockCheck;
    bool updatingTable = false;

    QComboBox *cbBus;
    QComboBox *cbAddressing;
    QSpinBox *spinRate;
    QPushButton *btnDiscover;
    QPushButton *btnVin;
    QComboBox *cbVinEcu;
    QCheckBox *ckShowAll;
    QTableWidget *pidTable;
    QPushButton *btnStart;
    QPushButton *btnStop;
    QPushButton *btnSave;
    QPushButton *btnLoad;
    QPushButton *btnHelp;
    QPushButton *btnMakeDbc;
    QPushButton *btnGraph;
    QLabel *lblWarning;
    QLabel *lblStatus;
    QLabel *lblVin;

    QHash<RowKey, int> rows;    //table row of each (ECU, PID)
};

#endif // OBD2WINDOW_H
