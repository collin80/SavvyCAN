#ifndef BISECTWINDOW_H
#define BISECTWINDOW_H

#include <QDialog>
#include "can_structs.h"

namespace Ui {
class BisectWindow;
}

class BisectWindow : public QDialog
{
    Q_OBJECT

public:
    explicit BisectWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~BisectWindow();
    void showEvent(QShowEvent*);

signals:
    void sendCANFrame(const CANFrame *, int);
    void sendFrameBatch(const QList<CANFrame> *);

private slots:
    void updatedFrames(int numFrames);
    void handleSaveButton();
    void handleReplaceButton();
    void handleCalculateButton();
    void updateFrameNumSlider();
    void updatePercentSlider();
    void updateFrameNumText();
    void updatePercentText();

private:
    Ui::BisectWindow *ui;
    const QVector<CANFrame> *modelFrames;
    QVector<CANFrame> splitFrames;
    QList<int> foundID;

    void refreshIDList();
    void refreshFrameNumbers();
    bool eventFilter(QObject *obj, QEvent *event);
};

#endif // BISECTWINDOW_H


