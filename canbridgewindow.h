#ifndef CANBRIDGEWINDOW_H
#define CANBRIDGEWINDOW_H

#include <QDialog>
#include "connections/canconmanager.h"

namespace Ui {
class CANBridgeWindow;
}

class CANBridgeWindow : public QDialog
{
    Q_OBJECT

public:
    explicit CANBridgeWindow(const QVector<CommFrame> *frames, QWidget *parent = nullptr);
    ~CANBridgeWindow();
    void showEvent(QShowEvent*);


private slots:
    void updatedFrames(int);
    void recalcSides();

private:
    Ui::CANBridgeWindow *ui;
    const QVector<CommFrame> *modelFrames;
    QMap<int, bool> foundIDSide1;
    QMap<int, bool> foundIDSide2;
    int side1BusNum;
    int side2BusNum;

    void processIncomingFrame(CommFrame *frame);
    bool eventFilter(QObject *obj, QEvent *event);

};

#endif // CANBRIDGEWINDOW_H
