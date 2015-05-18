#ifndef FRAMESENDERWINDOW_H
#define FRAMESENDERWINDOW_H

#include <QDialog>
#include <QTimer>
#include "can_structs.h"
#include "can_trigger_structs.h"

namespace Ui {
class FrameSenderWindow;
}

class FrameSenderWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FrameSenderWindow(QList<CANFrame> *frames, QWidget *parent = 0);
    ~FrameSenderWindow();

private slots:
    void onCellChanged(int, int);
    void handleTick();

private:
    Ui::FrameSenderWindow *ui;
    QList<FrameSendData> sendingData;
    QList<CANFrame> frameCache;
    QList<CANFrame> *modelFrames;
    QTimer *intervalTimer;

    void doModifiers(int);
    int fetchOperand(int, ModifierOperand);
    CANFrame* lookupFrame(int, int);
    void processModifierText(int);
    void processTriggerText(int);
    void parseOperandString(QStringList tokens, ModifierOperand&);
    ModifierOperationType parseOperation(QString);
};

#endif // FRAMESENDERWINDOW_H
