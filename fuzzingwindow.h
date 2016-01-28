#ifndef FUZZINGWINDOW_H
#define FUZZINGWINDOW_H

#include <QDialog>
#include <QListWidget>
#include <QTimer>
#include "can_structs.h"

namespace Ui {
class FuzzingWindow;
}

class FuzzingWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FuzzingWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~FuzzingWindow();

signals:
    void sendCANFrame(const CANFrame *, int);
    void sendFrameBatch(const QList<CANFrame> *);

private slots:
    void changePlaybackSpeed(int newSpeed);
    void timerTriggered();
    void clearAllFilters();
    void setAllFilters();
    void toggleFuzzing();
    void idListChanged(QListWidgetItem *item);

private:
    Ui::FuzzingWindow *ui;
    const QVector<CANFrame> *modelFrames;
    QTimer *fuzzTimer;
    QList<int> foundIDs;
    QList<int> selectedIDs;
    QList<CANFrame> sendingBuffer;
    int startID, endID, currentID;
    bool seqIDScan, rangeIDSelect, seqBitSelect;
    bool currentlyFuzzing;
    uint8_t currentBytes[8];
    uint8_t bitGrid[64];
    int numSentFrames;

    void refreshIDList();
    void calcNextID();
    void calcNextBitPattern();
};

#endif // FUZZINGWINDOW_H
