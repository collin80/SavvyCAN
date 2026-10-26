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

    void setListenOnlyCheck(std::function<bool(int bus)> check);
    void setHelpCallback(std::function<void()> callback);

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

protected:
    void closeEvent(QCloseEvent *event) override;
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
    QLabel *lblWarning;
    QLabel *lblStatus;
    QLabel *lblVin;

    QHash<RowKey, int> rows;    //table row of each (ECU, PID)
};

#endif // OBD2WINDOW_H
