#include "croplogdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPainter>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <algorithm>
#include <cmath>

// =============================================================================
// RangeSlider
// =============================================================================

RangeSlider::RangeSlider(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void RangeSlider::setRange(int min, int max)
{
    m_min = min;
    m_max = (max > min) ? max : min + 1;
    m_low  = std::clamp(m_low,  m_min, m_max);
    m_high = std::clamp(m_high, m_min, m_max);
    update();
}

void RangeSlider::setLow(int val)
{
    val = std::clamp(val, m_min, m_high);
    if (val == m_low) return;
    m_low = val;
    update();
}

void RangeSlider::setHigh(int val)
{
    val = std::clamp(val, m_low, m_max);
    if (val == m_high) return;
    m_high = val;
    update();
}

int RangeSlider::posFromValue(int val) const
{
    if (m_max == m_min) return kPad;
    return kPad + (val - m_min) * (width() - 2 * kPad) / (m_max - m_min);
}

int RangeSlider::valueFromPos(int pos) const
{
    if (m_max == m_min) return m_min;
    int raw = m_min + (pos - kPad) * (m_max - m_min) / (width() - 2 * kPad);
    return std::clamp(raw, m_min, m_max);
}

QRect RangeSlider::handleRect(int val) const
{
    int cx = posFromValue(val);
    int cy = height() / 2;
    return QRect(cx - kHandle, cy - kHandle, kHandle * 2, kHandle * 2);
}

void RangeSlider::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int cy   = height() / 2;
    int xLow = posFromValue(m_low);
    int xHigh= posFromValue(m_high);

    // Track background
    QRect track(kPad, cy - 3, width() - 2 * kPad, 6);
    p.setPen(Qt::NoPen);
    p.setBrush(palette().mid());
    p.drawRoundedRect(track, 3, 3);

    // Active range
    QRect active(xLow, cy - 3, xHigh - xLow, 6);
    p.setBrush(palette().highlight());
    p.drawRoundedRect(active, 3, 3);

    // Low handle
    p.setBrush(palette().button());
    p.setPen(QPen(palette().highlight().color(), 2));
    p.drawEllipse(QPoint(xLow, cy), kHandle - 2, kHandle - 2);

    // High handle
    p.drawEllipse(QPoint(xHigh, cy), kHandle - 2, kHandle - 2);
}

void RangeSlider::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    int pos = e->pos().x();

    // Pick nearest handle
    int dLow  = std::abs(pos - posFromValue(m_low));
    int dHigh = std::abs(pos - posFromValue(m_high));
    m_drag = (dLow <= dHigh) ? Low : High;
}

void RangeSlider::mouseMoveEvent(QMouseEvent *e)
{
    if (m_drag == None) return;
    int val = valueFromPos(e->pos().x());

    if (m_drag == Low)
    {
        val = std::min(val, m_high);
        if (val != m_low)
        {
            m_low = val;
            update();
            emit lowChanged(m_low);
        }
    }
    else
    {
        val = std::max(val, m_low);
        if (val != m_high)
        {
            m_high = val;
            update();
            emit highChanged(m_high);
        }
    }
}

void RangeSlider::mouseReleaseEvent(QMouseEvent *)
{
    m_drag = None;
}

// =============================================================================
// Helpers
// =============================================================================

static QString formatDuration(double secs)
{
    if (secs < 1.0)
        return QString::number(secs * 1000.0, 'f', 1) + " ms";
    if (secs < 60.0)
        return QString::number(secs, 'f', 3) + " s";
    int m = static_cast<int>(secs) / 60;
    double s = secs - m * 60.0;
    return QString("%1 m %2 s").arg(m).arg(s, 0, 'f', 1);
}

// =============================================================================
// CropLogDialog
// =============================================================================

CropLogDialog::CropLogDialog(const QVector<CommFrame> *frames, QWidget *parent)
    : QDialog(parent), m_frames(frames)
{
    setWindowTitle(tr("Crop Log"));
    setMinimumWidth(480);

    m_totalFrames = frames->count();
    m_endIdx      = m_totalFrames - 1;

    // Compute time span (seconds from first frame)
    auto totalMicros = [&]() -> qint64 {
        if (m_totalFrames < 2) return 0;
        const auto &first = frames->first().timeStamp();
        const auto &last  = frames->last().timeStamp();
        qint64 t0 = first.seconds() * 1000000LL + first.microSeconds();
        qint64 t1 = last.seconds()  * 1000000LL + last.microSeconds();
        return t1 - t0;
    };

    qint64 spanUs   = totalMicros();
    m_totalSecs     = spanUs / 1e6;
    m_startTimeSec  = (m_totalFrames > 0)
        ? (frames->first().timeStamp().seconds() + frames->first().timeStamp().microSeconds() / 1e6)
        : 0.0;

    // ---- Build UI -------------------------------------------------------
    auto *vbox = new QVBoxLayout(this);

    // Stats row – original
    m_lblOrigStats = new QLabel(this);
    m_lblOrigStats->setText(
        tr("<b>Original:</b> %1 frames &nbsp;|&nbsp; %2")
            .arg(m_totalFrames)
            .arg(formatDuration(m_totalSecs)));
    vbox->addWidget(m_lblOrigStats);

    // Range slider
    m_slider = new RangeSlider(this);
    m_slider->setRange(0, std::max(0, m_totalFrames - 1));
    m_slider->setLow(0);
    m_slider->setHigh(std::max(0, m_totalFrames - 1));
    vbox->addWidget(m_slider);

    // Grid: columns = Start | End
    //       rows    = Frame | Time
    auto *grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);
    grid->setHorizontalSpacing(16);

    // Header labels
    grid->addWidget(new QLabel(tr("<b>Start</b>"), this), 0, 1, Qt::AlignHCenter);
    grid->addWidget(new QLabel(tr("<b>End</b>"),   this), 0, 3, Qt::AlignHCenter);

    // Frame row
    grid->addWidget(new QLabel(tr("Frame:"), this), 1, 0);
    m_spinStartFrame = new QSpinBox(this);
    m_spinStartFrame->setMinimum(1);
    m_spinStartFrame->setMaximum(m_totalFrames);
    m_spinStartFrame->setValue(1);
    grid->addWidget(m_spinStartFrame, 1, 1);

    grid->addWidget(new QLabel(tr("Frame:"), this), 1, 2);
    m_spinEndFrame = new QSpinBox(this);
    m_spinEndFrame->setMinimum(1);
    m_spinEndFrame->setMaximum(m_totalFrames);
    m_spinEndFrame->setValue(m_totalFrames);
    grid->addWidget(m_spinEndFrame, 1, 3);

    // Time row
    grid->addWidget(new QLabel(tr("Time (s):"), this), 2, 0);
    m_spinStartTime = new QDoubleSpinBox(this);
    m_spinStartTime->setDecimals(4);
    m_spinStartTime->setMinimum(0.0);
    m_spinStartTime->setMaximum(m_totalSecs);
    m_spinStartTime->setSingleStep(0.001);
    m_spinStartTime->setValue(0.0);
    grid->addWidget(m_spinStartTime, 2, 1);

    grid->addWidget(new QLabel(tr("Time (s):"), this), 2, 2);
    m_spinEndTime = new QDoubleSpinBox(this);
    m_spinEndTime->setDecimals(4);
    m_spinEndTime->setMinimum(0.0);
    m_spinEndTime->setMaximum(m_totalSecs);
    m_spinEndTime->setSingleStep(0.001);
    m_spinEndTime->setValue(m_totalSecs);
    grid->addWidget(m_spinEndTime, 2, 3);

    vbox->addLayout(grid);

    // Preview stats
    m_lblPreviewStats = new QLabel(this);
    vbox->addWidget(m_lblPreviewStats);
    updatePreview();

    // Buttons
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Crop"));
    vbox->addWidget(buttons);

    // ---- Connections ----------------------------------------------------
    connect(m_slider,        &RangeSlider::lowChanged,            this, &CropLogDialog::onSliderLowChanged);
    connect(m_slider,        &RangeSlider::highChanged,           this, &CropLogDialog::onSliderHighChanged);
    connect(m_spinStartFrame, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CropLogDialog::onStartFrameChanged);
    connect(m_spinEndFrame,   QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CropLogDialog::onEndFrameChanged);
    connect(m_spinStartTime,  QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CropLogDialog::onStartTimeChanged);
    connect(m_spinEndTime,    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CropLogDialog::onEndTimeChanged);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

double CropLogDialog::timeFromFrameIndex(int idx) const
{
    if (idx < 0 || idx >= m_totalFrames) return 0.0;
    const auto &ts = m_frames->at(idx).timeStamp();
    double abs = ts.seconds() + ts.microSeconds() / 1e6;
    return abs - m_startTimeSec;
}

int CropLogDialog::frameIndexFromTime(double secs) const
{
    // Binary search for the frame whose relative time is closest to secs
    if (m_totalFrames == 0) return 0;
    double target = m_startTimeSec + secs;

    int lo = 0, hi = m_totalFrames - 1;
    while (lo < hi)
    {
        int mid = (lo + hi) / 2;
        const auto &ts = m_frames->at(mid).timeStamp();
        double t = ts.seconds() + ts.microSeconds() / 1e6;
        if (t < target)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

void CropLogDialog::blockAll(bool block)
{
    m_slider->blockSignals(block);
    m_spinStartFrame->blockSignals(block);
    m_spinEndFrame->blockSignals(block);
    m_spinStartTime->blockSignals(block);
    m_spinEndTime->blockSignals(block);
}

void CropLogDialog::updatePreview()
{
    int count    = m_endIdx - m_startIdx + 1;
    double dur   = timeFromFrameIndex(m_endIdx) - timeFromFrameIndex(m_startIdx);

    m_lblPreviewStats->setText(
        tr("<b>Cropped result:</b> %1 frames &nbsp;|&nbsp; %2")
            .arg(count)
            .arg(formatDuration(std::max(0.0, dur))));
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void CropLogDialog::onSliderLowChanged(int val)
{
    m_startIdx = val;
    blockAll(true);
    m_spinStartFrame->setValue(val + 1);
    m_spinStartTime->setValue(timeFromFrameIndex(val));
    blockAll(false);
    updatePreview();
}

void CropLogDialog::onSliderHighChanged(int val)
{
    m_endIdx = val;
    blockAll(true);
    m_spinEndFrame->setValue(val + 1);
    m_spinEndTime->setValue(timeFromFrameIndex(val));
    blockAll(false);
    updatePreview();
}

void CropLogDialog::onStartFrameChanged(int val)
{
    int idx = val - 1; // 0-based
    if (idx > m_endIdx)
    {
        m_spinStartFrame->setValue(m_endIdx + 1);
        return;
    }
    m_startIdx = idx;
    blockAll(true);
    m_slider->setLow(idx);
    m_spinStartTime->setValue(timeFromFrameIndex(idx));
    blockAll(false);
    updatePreview();
}

void CropLogDialog::onEndFrameChanged(int val)
{
    int idx = val - 1;
    if (idx < m_startIdx)
    {
        m_spinEndFrame->setValue(m_startIdx + 1);
        return;
    }
    m_endIdx = idx;
    blockAll(true);
    m_slider->setHigh(idx);
    m_spinEndTime->setValue(timeFromFrameIndex(idx));
    blockAll(false);
    updatePreview();
}

void CropLogDialog::onStartTimeChanged(double secs)
{
    int idx = frameIndexFromTime(secs);
    if (idx > m_endIdx) idx = m_endIdx;
    m_startIdx = idx;
    blockAll(true);
    m_slider->setLow(idx);
    m_spinStartFrame->setValue(idx + 1);
    // Snap displayed time to actual frame time
    m_spinStartTime->setValue(timeFromFrameIndex(idx));
    blockAll(false);
    updatePreview();
}

void CropLogDialog::onEndTimeChanged(double secs)
{
    int idx = frameIndexFromTime(secs);
    if (idx < m_startIdx) idx = m_startIdx;
    m_endIdx = idx;
    blockAll(true);
    m_slider->setHigh(idx);
    m_spinEndFrame->setValue(idx + 1);
    m_spinEndTime->setValue(timeFromFrameIndex(idx));
    blockAll(false);
    updatePreview();
}
