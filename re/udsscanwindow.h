#ifndef UDSSCANWINDOW_H
#define UDSSCANWINDOW_H

#include "can_structs.h"
#include "connections/canconnection.h"
#include "bus_protocols/uds_handler.h"

#include <QDialog>
#include <QFile>
#include <QTreeWidget>


enum SCAN_TYPE
{
    ST_TESTER_PRESENT,
    ST_SESS_CTRL,
    ST_COMM_CTRL,
    ST_ECU_RESET,
    ST_CLEAR_DTC,
    ST_READ_DTC,
    ST_SEC_ACCESS,
    ST_READ_ID,
    ST_READ_ADDR,
    ST_READ_SCALING,
    ST_IO_CTRL,
    ST_ROUTINE_CTRL,
    ST_CUSTOM,
};

//stores the parameters for one scan
class ScanEntry
{
public:
    uint32_t startID, endID;
    int32_t idOffset;
    bool bAdaptiveOffset;
    bool bShowNoReplies;
    uint32_t busToScan;
    uint32_t maxWaitTime; //in milliseconds
    SCAN_TYPE scanType;
    uint32_t sessType;
    uint32_t subfunctLen;
    qint64 subfunctLower;
    qint64 subfunctUpper;
    uint32_t subfunctIncrement;
    uint32_t serviceLower;
    uint32_t serviceUpper;
};

namespace Ui {
class UDSScanWindow;
}

class UDSScanWindow : public QDialog
{
    Q_OBJECT

public:
    explicit UDSScanWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~UDSScanWindow();

private slots:
    void updatedFrames(int numFrames);
    void gotUDSReply(UDS_MESSAGE msg);
    void scanAll();
    void scanSelected();
    void saveResults();
    void timeOut();
    void adaptiveToggled();
    void changedScanType();
    void numBytesChanged();
    void checkIDRange();
    void checkServiceRange();
    void checkSubFuncRange();
    void deleteSelectedScan();
    void addNewScan();
    void loadScans();
    void saveScans();
    void setNoReplyVal();
    void setMaxDelayVal();
    void setIncrementVal();
    void setReplyOffset();
    void setSessType();

private:
    Ui::UDSScanWindow *ui;
    const QVector<CANFrame> *modelFrames;
    UDS_HANDLER *udsHandler;
    QTimer *waitTimer;
    QList<UDS_MESSAGE> sendingFrames;
    QTreeWidgetItem *nodeID;
    QTreeWidgetItem *nodeService;
    QTreeWidgetItem *nodeSubFunc;
    QVector<ScanEntry> scanEntries;
    ScanEntry *currEditEntry;
    int currIdx = 0;
    bool currentlyRunning;
    bool inhibitUpdates;

    void displayScanEntry(int idx);
    QString generateListDesc(int idx);
    void setupScan(int idx);
    void startScan();
    void stopScan();
    void sendNextMsg();
    void sendOnBuses(UDS_MESSAGE frame, int buses);
    void setupNodes(uint32_t replyID);
    void dumpNode(QTreeWidgetItem* item, QFile *file, int indent);
    bool eventFilter(QObject *obj, QEvent *event);
};
#endif // UDSSCANWINDOW_H
