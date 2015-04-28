#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include "canframemodel.h"
#include "can_structs.h"

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
    void connButtonPress();

private:
    Ui::MainWindow *ui;
    CANFrameModel *model;
    QSerialPort *port;
    void loadCRTDFile(QString);
    void loadNativeCSVFile(QString);
    void loadGenericCSVFile(QString);
    void loadLogFile(QString);
    void loadMicrochipFile(QString);
    void addFrameToDisplay(CANFrame &);
};

#endif // MAINWINDOW_H
