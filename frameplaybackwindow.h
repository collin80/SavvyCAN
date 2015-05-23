#ifndef FRAMEPLAYBACKWINDOW_H
#define FRAMEPLAYBACKWINDOW_H

#include <QDialog>
#include <QListWidget>
#include <QTimer>
#include "can_structs.h"

namespace Ui {
class FramePlaybackWindow;
}

class FramePlaybackWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FramePlaybackWindow(QVector<CANFrame> *frames, QWidget *parent = 0);
    ~FramePlaybackWindow();

private slots:
    void btnBackOneClick();
    void btnPauseClick();
    void btnReverseClick();
    void btnStopClick();
    void btnPlayClick();
    void btnFwdOneClick();
    void changePlaybackSpeed(int newSpeed);
    void changeLooping(bool check);
    void changeSendingBus(int newIdx);
    void changeIDFiltering(QListWidgetItem *item);
    void btnSelectAllClick();
    void btnSelectNoneClick();
    void timerTriggered();

signals:
    void sendCANFrame(const CANFrame *, int);

private:
    Ui::FramePlaybackWindow *ui;
    QList<int> foundID;
    QList<CANFrame> frameCache;
    QVector<CANFrame> *modelFrames;
    int currentPosition;
    QTimer *playbackTimer;
    bool playbackActive;
    bool playbackForward;
    int whichBusSend;

    void refreshIDList();
    void updateFrameLabel();
    void updatePosition(bool forward);
};

#endif // FRAMEPLAYBACKWINDOW_H
