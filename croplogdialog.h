#ifndef CROPLOGDIALOG_H
#define CROPLOGDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPainter>
#include <QMouseEvent>
#include "can_structs.h"

// ---------------------------------------------------------------------------
// RangeSlider – a simple two-handle horizontal range slider
// ---------------------------------------------------------------------------
class RangeSlider : public QWidget
{
    Q_OBJECT

public:
    explicit RangeSlider(QWidget *parent = nullptr);

    void setRange(int min, int max);
    void setLow(int val);
    void setHigh(int val);
    int  low()  const { return m_low;  }
    int  high() const { return m_high; }

    QSize sizeHint() const override { return QSize(300, 30); }
    QSize minimumSizeHint() const override { return QSize(100, 30); }

signals:
    void lowChanged(int val);
    void highChanged(int val);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    int  m_min = 0, m_max = 100;
    int  m_low = 0, m_high = 100;

    enum DragHandle { None, Low, High } m_drag = None;

    static const int kHandle = 12; // handle radius
    static const int kPad    = 14; // horizontal padding for handles

    int    posFromValue(int val) const;
    int    valueFromPos(int pos) const;
    QRect  handleRect(int val) const;
};

// ---------------------------------------------------------------------------
// CropLogDialog
// ---------------------------------------------------------------------------
class CropLogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CropLogDialog(const QVector<CommFrame> *frames, QWidget *parent = nullptr);

    int startIndex() const { return m_startIdx; }
    int endIndex()   const { return m_endIdx;   }

private slots:
    void onSliderLowChanged(int val);
    void onSliderHighChanged(int val);
    void onStartFrameChanged(int val);
    void onEndFrameChanged(int val);
    void onStartTimeChanged(double secs);
    void onEndTimeChanged(double secs);
    void updatePreview();

private:
    void blockAll(bool block);
    int    frameIndexFromTime(double secs) const; // returns 0-based index of nearest frame
    double timeFromFrameIndex(int idx) const;     // seconds from log start

    const QVector<CommFrame> *m_frames;
    int    m_totalFrames  = 0;
    double m_totalSecs    = 0.0;
    double m_startTimeSec = 0.0; // absolute time of first frame

    int    m_startIdx = 0;
    int    m_endIdx   = 0;

    RangeSlider    *m_slider         = nullptr;
    QSpinBox       *m_spinStartFrame = nullptr;
    QSpinBox       *m_spinEndFrame   = nullptr;
    QDoubleSpinBox *m_spinStartTime  = nullptr;
    QDoubleSpinBox *m_spinEndTime    = nullptr;
    QLabel         *m_lblOrigStats   = nullptr;
    QLabel         *m_lblPreviewStats = nullptr;
};

#endif // CROPLOGDIALOG_H
