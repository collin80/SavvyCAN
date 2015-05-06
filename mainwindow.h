#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include "canframemodel.h"
#include "can_structs.h"
#include "graphingwindow.h"
#include "frameinfowindow.h"
#include "frameplaybackwindow.h"

namespace Ui {
class MainWindow;
}

enum STATE //keep this enum synchronized with the Arduino firmware project
{
    IDLE,
    GET_COMMAND,
    BUILD_CAN_FRAME,
    TIME_SYNC,
    GET_DIG_INPUTS,
    GET_ANALOG_INPUTS,
    SET_DIG_OUTPUTS,
    SETUP_CANBUS
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void handleLoadFile();
    void handleSaveFile();
    void connButtonPress();
    void readSerialData();
    void showGraphingWindow();
    void showFrameDataAnalysis();
    void clearFrames();
    void showPlaybackWindow();

private:
    Ui::MainWindow *ui;
    CANFrameModel *model;
    QSerialPort *port;
    QByteArray inputBuffer;
    STATE rx_state;
    int rx_step;
    CANFrame buildFrame;
    GraphingWindow *graphingWindow;
    FrameInfoWindow *frameInfoWindow;
    FramePlaybackWindow *playbackWindow;
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
    void procRXChar(unsigned char);
};

#endif // MAINWINDOW_H
