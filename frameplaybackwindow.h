#ifndef FRAMEPLAYBACKWINDOW_H
#define FRAMEPLAYBACKWINDOW_H

#include <QDialog>
#include <QListWidget>
#include "can_structs.h"
#include "framefileio.h"
#include "frameplaybackobject.h"

namespace Ui {
class FramePlaybackWindow;
}

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
    void changeBurstRate(int burst);
    void changeLooping(bool check);
    void changeSendingBus(int newIdx);
    void changeIDFiltering(QListWidgetItem *item);
    void btnSelectAllClick();
    void btnSelectNoneClick();
    void btnLoadFile();
    void btnLoadLive();
    void seqTableCellClicked(int row, int col);
    void seqTableCellChanged(int row, int col);
    void contextMenuFilters(QPoint);
    void saveFilters();
    void loadFilters();
    void useOrigTimingClicked();
    void getStatusUpdate(int frameNum);
    void EndOfFrameCache();
    void updatedFrames(int);

private:
    Ui::FramePlaybackWindow *ui;
    QList<int> foundID;
    QList<CANFrame> frameCache;
    const QVector<CANFrame> *modelFrames;
    QList<SequenceItem> seqItems;
    SequenceItem *currentSeqItem;
    int currentSeqNum;
    FramePlaybackObject playbackObject;
    bool forward;
    bool wantPlaying;
    bool isPlaying;
    int currentPosition;
    bool haveIncomingTraffic = false;

    void refreshIDList();
    void updateFrameLabel();
    void fillIDHash(SequenceItem &item);
    void showEvent(QShowEvent *);
    void closeEvent(QCloseEvent *event);
    void readSettings();
    void writeSettings();
    void calculateWhichBus();
    bool checkNoSeqLoaded();
    bool eventFilter(QObject *obj, QEvent *event);
};

#endif // FRAMEPLAYBACKWINDOW_H
