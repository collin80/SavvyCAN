#ifndef FIRMWAREUPLOADERWINDOW_H
#define FIRMWAREUPLOADERWINDOW_H

#include <QDialog>
#include <QTimer>
#include "can_structs.h"
#include "connections/canconmanager.h"
#include "utility.h"

namespace Ui {
class FirmwareUploaderWindow;
}

class FirmwareUploaderWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FirmwareUploaderWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~FirmwareUploaderWindow();

public slots:
    void gotTargettedFrame(CANFrame frame);

private slots:
    void handleLoadFile();
    void handleStartStopTransfer();
    void updatedFrames(int);
    void timerElapsed();

private:
    void updateProgress();
    void loadBinaryFile(QString);
    void sendFirmwareChunk();
    void sendFirmwareEnding();

    Ui::FirmwareUploaderWindow *ui;
    bool transferInProgress;
    bool startedProcess;
    int firmwareSize;
    int currentSendingPosition;
    int baseAddress;
    int bus;
    uint32_t token;
    QByteArray firmwareData;
    const QVector<CANFrame> *modelFrames;
    QTimer *timer;
};

#endif // FIRMWAREUPLOADERWINDOW_H
