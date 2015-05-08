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
    void showGraphingWindow();
    void showFrameDataAnalysis();
    void clearFrames();
    void showPlaybackWindow();
    void changeBaudRates();

public slots:
    void gotFrame(CANFrame *frame);

signals:
    void sendSerialPort(QString portName);
    void updateBaudRates(int, int);

private:
    Ui::MainWindow *ui;
    CANFrameModel *model;
    QThread serialWorkerThread;
    QByteArray inputBuffer;
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
};

#endif // MAINWINDOW_H
