#ifndef FRAMEINFOWINDOW_H
#define FRAMEINFOWINDOW_H

#include <QDialog>
#include <QListWidget>
#include "can_structs.h"

namespace Ui {
class FrameInfoWindow;
}

class FrameInfoWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FrameInfoWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~FrameInfoWindow();
    void showEvent(QShowEvent*);

private slots:
    void updateDetailsWindow(QString);
    void updatedFrames(int);

private:
    Ui::FrameInfoWindow *ui;

    QList<int> foundID;
    QList<CANFrame> frameCache;
    const QVector<CANFrame> *modelFrames;

    void refreshIDList();
};

#endif // FRAMEINFOWINDOW_H
