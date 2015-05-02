#ifndef FRAMEINFOWINDOW_H
#define FRAMEINFOWINDOW_H

#include <QDialog>
#include "can_structs.h"

namespace Ui {
class FrameInfoWindow;
}

class FrameInfoWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FrameInfoWindow(QList<CANFrame> *frames, QWidget *parent = 0);
    ~FrameInfoWindow();

private slots:
    void updateDetailsWindow(int frameIdx);

private:
    Ui::FrameInfoWindow *ui;

    QList<int> foundID;
    QList<CANFrame> frameCache;
    QList<CANFrame> *modelFrames;

    void refreshIDList();
};

#endif // FRAMEINFOWINDOW_H
