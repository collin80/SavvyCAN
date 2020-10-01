#ifndef FUZZINGWINDOW_H
#define FUZZINGWINDOW_H

#include <QDialog>
#include <QListWidget>
#include <QTimer>
#include "can_structs.h"

namespace Ui {
class FuzzingWindow;
}

namespace BitSequenceType
{
    enum
    {
        Sequential,
        Sweeping,
        Random
    };
}

class FuzzingWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FuzzingWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~FuzzingWindow();

signals:
    void sendCANFrame(const CANFrame *);
    void sendFrameBatch(const QList<CANFrame> *);

private slots:
    void changePlaybackSpeed(int newSpeed);
    void timerTriggered();
    void clearAllFilters();
    void setAllFilters();
    void toggleFuzzing();
    void idListChanged(QListWidgetItem *item);
    void bitfieldClicked(int, int);
    void changedNumDataBytes(int newVal);
    void updatedFrames(int numFrames);

private:
    Ui::FuzzingWindow *ui;
    const QVector<CANFrame> *modelFrames;
    QTimer *fuzzTimer;
    QList<int> foundIDs;
    QList<int> selectedIDs;
    QList<CANFrame> sendingBuffer;
    int startID, endID, currentID, currentIdx;
    bool seqIDScan, rangeIDSelect;
    int bitSequenceType;
    bool currentlyFuzzing;
    uint8_t currentBytes[8];
    uint8_t bitGrid[64];
    uint8_t numBits;
    uint64_t bitAccum;
    int numSentFrames;

    void refreshIDList();
    void calcNextID();
    void calcNextBitPattern();
    void redrawGrid();
    bool eventFilter(QObject *obj, QEvent *event);
    void changedDataByteText(int which, QString valu);
};

#endif // FUZZINGWINDOW_H
