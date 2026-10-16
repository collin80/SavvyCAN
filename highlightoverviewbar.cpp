#include "highlightoverviewbar.h"

#include <QPainter>
#include <QTableView>
#include <QScrollBar>
#include <QApplication>

HighlightOverviewBar::HighlightOverviewBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(10);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void HighlightOverviewBar::setup(CommFrameModel *model, QTableView *tableView)
{
    m_model = model;
    m_scrollBar = tableView->verticalScrollBar();

    connect(m_model, &CommFrameModel::highlightChanged, this, QOverload<>::of(&HighlightOverviewBar::update));
    connect(m_model, &CommFrameModel::modelReset,       this, QOverload<>::of(&HighlightOverviewBar::update));
    connect(m_model, &CommFrameModel::rowsInserted,     this, QOverload<>::of(&HighlightOverviewBar::update));
    connect(m_scrollBar, &QScrollBar::valueChanged,     this, QOverload<>::of(&HighlightOverviewBar::update));
}

QSize HighlightOverviewBar::sizeHint() const
{
    return QSize(10, 0);
}

void HighlightOverviewBar::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.fillRect(rect(), QApplication::palette().color(QPalette::Window));

    if (!m_model || !m_scrollBar)
        return;

    const QVector<CommFrame> *frames = m_model->getFilteredListReference();
    if (!frames || frames->isEmpty())
        return;

    const int total = frames->count();
    const int h = height();

    // Match the row background highlight colour exactly.
    const QPalette &pal = QApplication::palette();
    QColor hlColor = pal.color(QPalette::Highlight);
    hlColor.setAlpha(255);

    // Scale line thickness: thicker when fewer frames so highlights stay visible.
    // 1 px at 500+ frames, scaling up to barWidth at 1 frame.
    const int barW = width() - 1;
    const int lineH = qMax(1, qMin(barW, (int)(barW * 500.0 / qMax(total, 1))));

    //scanning every frame is only worth it when something is actually highlighted
    if (!m_model->getHighlightedIds().isEmpty())
    {
        for (int i = 0; i < total; ++i) {
            if (m_model->isHighlighted((int)frames->at(i).frameId())) {
                int y = (int)((qint64)i * h / total);
                p.fillRect(0, y, width(), lineH, hlColor);
            }
        }
    }

    // Draw the viewport indicator rectangle
    int sbMax = m_scrollBar->maximum();
    int sbPage = m_scrollBar->pageStep();
    int sbTotal = sbMax + sbPage;
    if (sbTotal > 0) {
        double topFrac    = (double)m_scrollBar->value() / sbTotal;
        double heightFrac = (double)sbPage / sbTotal;
        int vy = (int)(topFrac * h);
        int vh = qMax(3, (int)(heightFrac * h));

        QColor viewportColor = pal.color(QPalette::WindowText);
        viewportColor.setAlpha(70);
        p.fillRect(0, vy, width(), vh, viewportColor);
        // Border for the viewport box so it's visible even with no highlights
        p.setPen(QPen(viewportColor.lighter(150), 1));
        p.drawRect(0, vy, width() - 1, vh - 1);
    }
}
