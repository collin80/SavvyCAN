#ifndef MOTORCONTROLLERCONFIGWINDOW_H
#define MOTORCONTROLLERCONFIGWINDOW_H

#include <QDialog>
#include <QTimer>
#include "can_structs.h"

namespace Ui {
class MotorControllerConfigWindow;
}

class MotorControllerConfigWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MotorControllerConfigWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~MotorControllerConfigWindow();

signals:
    void sendCANFrame(const CANFrame *, int);
    void sendFrameBatch(const QList<CANFrame> *);

private slots:
    void updatedFrames(int numFrames);
    void refreshData();
    void saveData();
    void timerTick();

private:
    Ui::MotorControllerConfigWindow *ui;
    const QVector<CANFrame> *modelFrames;
    QTimer timer;
    CANFrame outFrame;
    bool doingRequest;
    int transmitStep;

    void send32BitParam(int param, uint32_t valu);
    void send16BitParam(int param, uint16_t valu);
    void send8BitParam(int param, uint8_t valu);

};

#endif // MOTORCONTROLLERCONFIGWINDOW_H
