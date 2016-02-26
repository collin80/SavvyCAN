#ifndef UDSSCANWINDOW_H
#define UDSSCANWINDOW_H

#include "can_structs.h"

#include <QDialog>

namespace Ui {
class UDSScanWindow;
}

class UDSScanWindow : public QDialog
{
    Q_OBJECT

public:
    explicit UDSScanWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~UDSScanWindow();

signals:
    void sendCANFrame(const CANFrame *, int);

private slots:
    void updatedFrames(int numFrames);
    void scanUDS();
    void timeOut();

private:
    Ui::UDSScanWindow *ui;
    const QVector<CANFrame> *modelFrames;
    QTimer *waitTimer;
    QList<CANFrame> sendingFrames;
    int currIdx = 0;
    bool currentlyRunning;

    void sendNextMsg();
};

#endif // UDSSCANWINDOW_H
