#ifndef FLOWVIEWWINDOW_H
#define FLOWVIEWWINDOW_H

#include <QDialog>
#include "can_structs.h"

namespace Ui {
class FlowViewWindow;
}

class FlowViewWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FlowViewWindow(QList<CANFrame> *frames, QWidget *parent = 0);
    ~FlowViewWindow();

private slots:
    void btnBackOneClick();
    void btnPauseClick();
    void btnReverseClick();
    void btnStopClick();
    void btnPlayClick();
    void btnFwdOneClick();
    void changePlaybackSpeed(int newSpeed);
    void changeLooping(bool check);
    void timerTriggered();
    void changeID(QString);

private:
    Ui::FlowViewWindow *ui;
    QList<int> foundID;
    QList<CANFrame> frameCache;
    QList<CANFrame> *modelFrames;
    unsigned char refBytes[8];
    unsigned char currBytes[8];
    int currentPosition;
    QTimer *playbackTimer;
    bool playbackActive;
    bool playbackForward;

    void refreshIDList();
    void updateFrameLabel();
    void updatePosition(bool forward);
    void updateDataView();
};

#endif // FLOWVIEWWINDOW_H
