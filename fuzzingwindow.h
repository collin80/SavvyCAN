#ifndef FUZZINGWINDOW_H
#define FUZZINGWINDOW_H

#include <QDialog>
#include "can_structs.h"
#include "serialworker.h"

namespace Ui {
class FuzzingWindow;
}

class FuzzingWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FuzzingWindow(const QVector<CANFrame> *frames, SerialWorker *worker, QWidget *parent = 0);
    ~FuzzingWindow();

private:
    Ui::FuzzingWindow *ui;
    const QVector<CANFrame> *modelFrames;
    QTimer *fuzzTimer;
    SerialWorker *serialWorker;
};

#endif // FUZZINGWINDOW_H
