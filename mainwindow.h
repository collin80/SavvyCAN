#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include "canframemodel.h"
#include "can_structs.h"
#include "graphingwindow.h"
#include "frameinfowindow.h"
#include "frameplaybackwindow.h"
#include "flowviewwindow.h"
#include "framesenderwindow.h"
#include "dbchandler.h"
#include "dbcmaineditor.h"

#define VERSION 106

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void handleLoadFile();
    void handleSaveFile();
    void handleLoadDBC();
    void handleSaveDBC();
    void showEditSignalsWindow();
    void connButtonPress();
    void showGraphingWindow();
    void showFrameDataAnalysis();
    void clearFrames();
    void showPlaybackWindow();
    void showFlowViewWindow();
    void showFrameSenderWindow();
    void changeBaudRates();
    void connectionFailed();
    void gotDeviceInfo(int, int);
    void connectionSucceeded(int, int);
    void gridClicked(QModelIndex);
    void interpretToggled(bool);
    void overwriteToggled(bool);
    void showDBCEditor();
    void toggleCapture();

public slots:
    void gotFrames(int, int);


signals:
    void sendSerialPort(QSerialPortInfo *port);
    void closeSerialPort();
    void updateBaudRates(int, int);
    void sendCANFrame(const CANFrame *, int);
    void stopFrameCapturing();
    void startFrameCapturing();


private:
    Ui::MainWindow *ui;

    //canbus related data
    CANFrameModel *model;
    DBCHandler *dbcHandler;
    QList<QSerialPortInfo> ports;
    QThread serialWorkerThread;
    QByteArray inputBuffer;
    bool allowCapture;

    //References to other windows we can display
    GraphingWindow *graphingWindow;
    FrameInfoWindow *frameInfoWindow;
    FramePlaybackWindow *playbackWindow;
    FlowViewWindow *flowViewWindow;
    FrameSenderWindow *frameSenderWindow;
    DBCMainEditor *dbcMainEditor;

    //various private storage
    QLabel lbStatusConnected;
    QLabel lbStatusBauds;
    QLabel lbStatusDatabase;
    int normalRowHeight;
    bool isConnected;

    //private methods
    void loadCRTDFile(QString);
    void loadNativeCSVFile(QString);
    void loadGenericCSVFile(QString);
    void loadLogFile(QString);
    void loadMicrochipFile(QString);
    void saveCRTDFile(QString);
    void saveNativeCSVFile(QString);
    void saveGenericCSVFile(QString);
    void saveLogFile(QString);
    void saveMicrochipFile(QString);
    void addFrameToDisplay(CANFrame &, bool);
    void updateBaudLabel(int, int);
};

#endif // MAINWINDOW_H
