#ifndef FRAMEPLAYBACKWINDOW_H
#define FRAMEPLAYBACKWINDOW_H

#include <QDialog>
#include "can_structs.h"

namespace Ui {
class FramePlaybackWindow;
}

class FramePlaybackWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FramePlaybackWindow(QList<CANFrame> *frames, QWidget *parent = 0);
    ~FramePlaybackWindow();

private:
    Ui::FramePlaybackWindow *ui;
    QList<int> foundID;
    QList<CANFrame> frameCache;
    QList<CANFrame> *modelFrames;
    int currentPosition;

    void refreshIDList();
    void updateFrameLabel();
};

#endif // FRAMEPLAYBACKWINDOW_H
