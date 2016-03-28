#ifndef FRAMEPLAYBACKWINDOW_H
#define FRAMEPLAYBACKWINDOW_H

#include <QDialog>
#include <QListWidget>
#include <QTimer>
#include "can_structs.h"
#include "serialworker.h"
#include "framefileio.h"

namespace Ui {
class FramePlaybackWindow;
}

//one entry in the sequence of data to use
struct SequenceItem
{
    QString filename;
    QVector<CANFrame> data;
    QHash<int, bool> idFilters;
    int maxLoops;
    int currentLoopCount;
};

class FramePlaybackWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FramePlaybackWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~FramePlaybackWindow();

private slots:
    void btnBackOneClick();
    void btnPauseClick();
    void btnReverseClick();
    void btnStopClick();
    void btnPlayClick();
    void btnFwdOneClick();
    void btnDeleteCurrSeq();
    void changePlaybackSpeed(int newSpeed);
    void changeLooping(bool check);
    void changeSendingBus(int newIdx);
    void changeIDFiltering(QListWidgetItem *item);
    void btnSelectAllClick();
    void btnSelectNoneClick();
    void timerTriggered();
    void btnLoadFile();
    void btnLoadLive();
    void seqTableCellClicked(int row, int col);
    void seqTableCellChanged(int row, int col);
    void contextMenuFilters(QPoint);
    void saveFilters();
    void loadFilters();

signals:
    void sendCANFrame(const CANFrame *);
    void sendFrameBatch(const QList<CANFrame> *);

private:
    Ui::FramePlaybackWindow *ui;
    QList<int> foundID;
    QList<CANFrame> frameCache;
    QList<CANFrame> sendingBuffer;
    const QVector<CANFrame> *modelFrames;
    int currentPosition;
    QTimer *playbackTimer;
    bool playbackActive;
    bool playbackForward;
    int whichBusSend;
    QList<SequenceItem> seqItems;
    SequenceItem *currentSeqItem;
    int currentSeqNum;

    void refreshIDList();
    void updateFrameLabel();
    void updatePosition(bool forward);
    void fillIDHash(SequenceItem &item);    
    void showEvent(QShowEvent *);
    void closeEvent(QCloseEvent *event);
    void readSettings();
    void writeSettings();
};

#endif // FRAMEPLAYBACKWINDOW_H
