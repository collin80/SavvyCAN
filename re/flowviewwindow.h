#ifndef FLOWVIEWWINDOW_H
#define FLOWVIEWWINDOW_H

#include <QDialog>
#include <QLocale>
#include <QSlider>
#include "qcustomplot.h"
#include "can_structs.h"

namespace Ui {
class FlowViewWindow;
}

class FlowViewWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FlowViewWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~FlowViewWindow();
    void showEvent(QShowEvent*);

private slots:
    void btnBackOneClick();
    void btnPauseClick();
    void btnReverseClick();
    void btnStopClick();
    void btnPlayClick();
    void btnFwdOneClick();
    void changePlaybackSpeed(int newSpeed);
    void changeLooping(bool check);
    void timerTriggered();
    void changeID(QString);
    void updatedFrames(int);
    void contextMenuRequestFlow(QPoint pos);
    void contextMenuRequestGraph(QPoint pos);
    void saveFileFlow();
    void saveFileGraph();
    void plottableDoubleClick(QCPAbstractPlottable* plottable, QMouseEvent* event);
    void gotCenterTimeID(int32_t ID, double timestamp);
    void updateTriggerValues();
    void gotCellClick(int x, int y);
    void graphRangeChanged(int range);
    void changeGraphVisibility(int state);

signals:
    void sendCenterTimeID(uint32_t ID, double timestamp);

private:
    Ui::FlowViewWindow *ui;
    QList<quint32> foundID;
    QList<CANFrame> frameCache;
    const QVector<CANFrame> *modelFrames;
    unsigned char refBytes[8];
    unsigned char currBytes[8];
    int triggerValues[8];
    uint64_t triggerBits;
    int currentPosition;
    QTimer *playbackTimer;
    bool playbackActive;
    bool playbackForward;
    static const QColor graphColors[8];
    bool secondsMode;
    bool openGLMode;
    bool useHexTicker;
    QVector<double> x[8], y[8];
    QCPGraph *graphRef[8];

    void refreshIDList();
    void updateFrameLabel();
    void updatePosition(bool forward);
    void gotoFrame(int frame);
    void updateDataView();
    void removeAllGraphs();
    void createGraph(int);
    void updateGraphLocation();
    void closeEvent(QCloseEvent *event);
    void readSettings();
    void writeSettings();
    bool eventFilter(QObject *obj, QEvent *event);
};

#endif // FLOWVIEWWINDOW_H
