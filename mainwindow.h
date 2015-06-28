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
    void handleLoadDBC();
    void handleSaveDBC();
    void connButtonPress();
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
    void exitApp();
    void handleSaveDecoded();
    void changeBaudRates();
    void connectionFailed();
    void gotDeviceInfo(int, int);
    void connectionSucceeded(int, int);
    void gridClicked(QModelIndex);
    void interpretToggled(bool);
    void overwriteToggled(bool);
    void showDBCEditor();
    void toggleCapture();
    void normalizeTiming();

public slots:
    void gotFrames(int, int);


signals:
    void sendSerialPort(QSerialPortInfo *port);
    void closeSerialPort();
    void updateBaudRates(int, int);
    void sendCANFrame(const CANFrame *, int);
    void stopFrameCapturing();
    void startFrameCapturing();

    //-1 = frames cleared, -2 = a new file has been loaded (so all frames are different), otherwise # of new frames
    void framesUpdated(int numFrames); //something has updated the frame list

private:
    Ui::MainWindow *ui;
    static MainWindow *selfRef;

    //canbus related data
    CANFrameModel *model;
    DBCHandler *dbcHandler;
    QList<QSerialPortInfo> ports;
    QThread serialWorkerThread;
    SerialWorker *worker;
    QByteArray inputBuffer;
    bool allowCapture;
    bool bDirty; //have frames been added or subtracted since the last save/load?

    //References to other windows we can display
    GraphingWindow *graphingWindow;
    FrameInfoWindow *frameInfoWindow;
    FramePlaybackWindow *playbackWindow;
    FlowViewWindow *flowViewWindow;
    FrameSenderWindow *frameSenderWindow;
    DBCMainEditor *dbcMainEditor;
    FileComparatorWindow *comparatorWindow;
    MainSettingsDialog *settingsDialog;

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
};

#endif // MAINWINDOW_H
