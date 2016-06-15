#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "config.h"
#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include "canframemodel.h"
#include "can_structs.h"
#include "framefileio.h"
#include "graphingwindow.h"
#include "frameinfowindow.h"
#include "frameplaybackwindow.h"
#include "flowviewwindow.h"
#include "framesenderwindow.h"
#include "filecomparatorwindow.h"
#include "dbchandler.h"
#include "dbcmaineditor.h"
#include "mainsettingsdialog.h"
#include "firmwareuploaderwindow.h"
#include "discretestatewindow.h"
#include "connectionwindow.h"
#include "scriptingwindow.h"
#include "rangestatewindow.h"
#include "dbcloadsavewindow.h"
#include "fuzzingwindow.h"
#include "udsscanwindow.h"
#include "isotp_interpreterwindow.h"

class ConnectionWindow;

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
    ~MainWindow();

private slots:
    void handleLoadFile();
    void handleSaveFile();
    void handleSaveFilteredFile();
    void handleSaveFilters();
    void handleLoadFilters();
    void showGraphingWindow();
    void showFrameDataAnalysis();
    void clearFrames();
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
    void showUDSScanWindow();
    void showISOInterpreterWindow();
    void exitApp();
    void handleSaveDecoded();
    void connectionFailed();
    void gotDeviceInfo(int, int);
    void connectionSucceeded(int, int);
    void gridClicked(QModelIndex);
    void gridDoubleClicked(QModelIndex);
    void interpretToggled(bool);
    void overwriteToggled(bool);
    void tickGUIUpdate();
    void toggleCapture();
    void normalizeTiming();
    void updateFilterList();
    void filterListItemChanged(QListWidgetItem *item);
    void filterSetAll();
    void filterClearAll();

public slots:
    void gotFrames(int);
    void updateSettings();
    void gotCenterTimeID(int32_t ID, double timestamp);
    void updateConnectionSettings(QString connectionType, QString port, int speed0, int speed1);

signals:
    void sendCANFrame(const CANFrame *, int);
    void suspendCapturing(bool);

    //-1 = frames cleared, -2 = a new file has been loaded (so all frames are different), otherwise # of new frames
    void framesUpdated(int numFrames); //something has updated the frame list
    void frameUpdateRapid(int numFrames);
    void settingsUpdated();
    void sendCenterTimeID(int32_t ID, double timestamp);

private:
    Ui::MainWindow *ui;
    static MainWindow *selfRef;

    //canbus related data
    CANFrameModel *model;
    DBCHandler *dbcHandler;    
    QByteArray inputBuffer;
    QTimer updateTimer;
    QTime *elapsedTime;
    int framesPerSec;
    int rxFrames;
    bool inhibitFilterUpdate;
    bool useHex;
    bool allowCapture;
    bool secondsMode;
    bool bDirty; //have frames been added or subtracted since the last save/load?
    bool useFiltered; //should sub-windows use the unfiltered or filtered frames list?

    //References to other windows we can display
    GraphingWindow *graphingWindow;
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

    //various private storage
    QLabel lbStatusConnected;
    QLabel lbStatusFilename;
    QLabel lbStatusDatabase;
    int normalRowHeight;
    bool isConnected;

    //private methods
    void saveDecodedTextFile(QString);
    void addFrameToDisplay(CANFrame &, bool);
    void updateFileStatus();
    void closeEvent(QCloseEvent *event);
    void readSettings();
    void readUpdateableSettings();
    void writeSettings();
};

#endif // MAINWINDOW_H
