#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "config.h"
#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include "canframemodel.h"
#include "can_structs.h"
#include "framefileio.h"
#include "dbc/dbchandler.h"
#include "bus_protocols/isotp_handler.h"

#include "re/graphingwindow.h"
#include "re/frameinfowindow.h"
#include "frameplaybackwindow.h"
#include "bisectwindow.h"
#include "re/flowviewwindow.h"
#include "framesenderwindow.h"
#include "re/filecomparatorwindow.h"
#include "dbc/dbcmaineditor.h"
#include "mainsettingsdialog.h"
#include "firmwareuploaderwindow.h"
#include "re/discretestatewindow.h"
#include "scriptingwindow.h"
#include "re/rangestatewindow.h"
#include "dbc/dbcloadsavewindow.h"
#include "re/fuzzingwindow.h"
#include "re/udsscanwindow.h"
#include "re/sniffer/snifferwindow.h"
#include "re/isotp_interpreterwindow.h"
#include "motorcontrollerconfigwindow.h"
#include "signalviewerwindow.h"
#include "re/temporalgraphwindow.h"
#include "re/dbccomparatorwindow.h"
#include "canbridgewindow.h"

class CANConnection;
class ConnectionWindow;
class ISOTP_InterpreterWindow;
class ScriptingWindow;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    static QString loadedFileName;
    static MainWindow *getReference();
    CANFrameModel * getCANFrameModel();
    ~MainWindow();

    void handleDroppedFile(const QString &filename);

private slots:
    void handleLoadFile();
    void handleSaveFile();
    void handleSaveFilteredFile();
    void handleSaveFilters();
    void handleLoadFilters();
    void handleContinousLogging();
    void showGraphingWindow();
    void showFrameDataAnalysis();
    void clearFrames();
    void expandAllRows();
    void collapseAllRows();
    void showPlaybackWindow();
    void showFlowViewWindow();
    void showFrameSenderWindow();
    void showSingleMultiWindow();
    void showRangeWindow();
    void showFuzzyScopeWindow();
    void showComparisonWindow();
    void showSettingsDialog();
    void showFirmwareUploaderWindow();
    void showConnectionSettingsWindow();
    void showScriptingWindow();
    void showDBCFileWindow();
    void showFuzzingWindow();
    void showMCConfigWindow();
    void showUDSScanWindow();
    void showISOInterpreterWindow();
    void showSnifferWindow();
    void showBisectWindow();
    void showSignalViewer();
    void showTemporalGraphWindow();
    void showDBCComparisonWindow();
    void showCANBridgeWindow();
    void exitApp();
    void handleSaveDecoded();
    void handleSaveDecodedCsv();
    void connectionStatusUpdated(int conns);
    void gridClicked(QModelIndex);
    void gridDoubleClicked(QModelIndex);
    void gridContextMenuRequest(QPoint pos);
    void setupAddToNewGraph();
    void setupSendToLatestGraphWindow();
    void interpretToggled(bool);
    void overwriteToggled(bool);
    void presistentFiltersToggled(bool state);
    void logReceivedFrame(CANConnection*, QVector<CANFrame>);
    void tickGUIUpdate();
    void toggleCapture();
    void normalizeTiming();
    void updateFilterList();
    void filterListItemChanged(QListWidgetItem *item);
    void busFilterListItemChanged(QListWidgetItem *item);
    void filterSetAll();
    void filterClearAll();
    void headerClicked (int logicalIndex);
    void DBCSettingsUpdated();

public slots:
    void gotFrames(int);
    void updateSettings();
    void readUpdateableSettings();
    void gotCenterTimeID(uint32_t ID, double timestamp);
    void updateConnectionSettings(QString connectionType, QString port, int speed0, int speed1);

signals:
    void sendCANFrame(const CANFrame *, int);
    void suspendCapturing(bool);

    //-1 = frames cleared, -2 = a new file has been loaded (so all frames are different), otherwise # of new frames
    void framesUpdated(int numFrames); //something has updated the frame list (send at gui update frequency)
    void frameUpdateRapid(int numFrames);
    void settingsUpdated();
    void sendCenterTimeID(uint32_t ID, double timestamp);

private:
    Ui::MainWindow *ui;
    static MainWindow *selfRef;

    //canbus related data
    CANFrameModel *model;
    DBCHandler *dbcHandler;
    QByteArray inputBuffer;
    QTimer updateTimer;
    QElapsedTimer *elapsedTime;
    int framesPerSec;
    int rxFrames;
    bool inhibitFilterUpdate;
    bool useHex;
    bool allowCapture;
    bool ignoreDBCColors;
    bool bDirty; //have frames been added or subtracted since the last save/load?
    bool useFiltered; //should sub-windows use the unfiltered or filtered frames list?

    bool continuousLogging;
    int continuousLogFlushCounter;

    //References to other windows we can display

    //Graph window is allowed to instantiate more than once. All the rest are not (yet).
    GraphingWindow *lastGraphingWindow;
    QList<GraphingWindow *> graphWindows;

    FrameInfoWindow *frameInfoWindow;
    FramePlaybackWindow *playbackWindow;
    FlowViewWindow *flowViewWindow;
    FrameSenderWindow *frameSenderWindow;
    DBCMainEditor *dbcMainEditor;
    FileComparatorWindow *comparatorWindow;
    MainSettingsDialog *settingsDialog;
    DiscreteStateWindow *discreteStateWindow;
    FirmwareUploaderWindow *firmwareUploaderWindow;
    ConnectionWindow *connectionWindow;
    ScriptingWindow *scriptingWindow;
    RangeStateWindow *rangeWindow;
    DBCLoadSaveWindow *dbcFileWindow;
    FuzzingWindow *fuzzingWindow;
    UDSScanWindow *udsScanWindow;
    ISOTP_InterpreterWindow *isoWindow;
    SnifferWindow* snifferWindow;
    MotorControllerConfigWindow *motorctrlConfigWindow;
    BisectWindow* bisectWindow;
    SignalViewerWindow *signalViewerWindow;
    TemporalGraphWindow *temporalGraphWindow;
    DBCComparatorWindow *dbcComparatorWindow;
    CANBridgeWindow *canBridgeWindow;

    //various private storage
    QLabel lbStatusConnected;
    QLabel lbStatusFilename;
    QLabel lbStatusDatabase;
    QLabel lbHelp;
    int normalRowHeight;
    bool isConnected;
    QPoint contextMenuPosition;
    bool rowExpansionActive = false;

    //private methods
    QString getSignalNameFromPosition(QPoint pos);
    uint32_t getMessageIDFromPosition(QPoint pos);
    void handleSaveDecodedMethod(bool csv);
    void saveDecodedTextFile(QString);
    void saveDecodedTextFileAsColumns(QString);
    void addFrameToDisplay(CANFrame &, bool);
    void updateFileStatus();
    void closeEvent(QCloseEvent *event);
    void killEmAll();
    void killWindow(QDialog *win);
    void readSettings();
    void writeSettings();
    bool eventFilter(QObject *obj, QEvent *event);
    void manageRowExpansion();
    void disableAutoRowExpansion();
};

#endif // MAINWINDOW_H
