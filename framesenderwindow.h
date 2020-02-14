#ifndef FRAMESENDERWINDOW_H
#define FRAMESENDERWINDOW_H

#include <QDialog>
#include <QTimer>
#include <QElapsedTimer>
#include <QTime>
#include "can_structs.h"
#include "can_trigger_structs.h"

namespace Ui {
class FrameSenderWindow;
}

class FrameSenderWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FrameSenderWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~FrameSenderWindow();

private slots:
    void onCellChanged(int, int);
    void handleTick();
    void enableAll();
    void disableAll();
    void clearGrid();
    void saveGrid();
    void loadGrid();
    void updatedFrames(int);

private:
    Ui::FrameSenderWindow *ui;
    QList<FrameSendData> sendingData;
    QHash<int, CANFrame> frameCache; //hash with frame ID as the key and the most recent frame as the value
    const QVector<CANFrame> *modelFrames;
    QTimer *intervalTimer;
    QElapsedTimer elapsedTimer;
    bool inhibitChanged = false;

    void createBlankRow();
    void doModifiers(int);
    int fetchOperand(int, ModifierOperand);
    CANFrame* lookupFrame(int, int);
    void processModifierText(int);
    void processTriggerText(int);
    void parseOperandString(QStringList tokens, ModifierOperand&);
    ModifierOperationType parseOperation(QString);
    void saveSenderFile(QString filename);
    void loadSenderFile(QString filename);
    void updateGridRow(int idx);
    void processCellChange(int line, int col);
    void buildFrameCache();
    void processIncomingFrame(CANFrame *frame);
    bool eventFilter(QObject *obj, QEvent *event);
    void setupGrid();
};

#endif // FRAMESENDERWINDOW_H
