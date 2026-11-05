#include "highlightitemdelegate.h"
#include "filterutility.h"
#include "utility.h"

#include <QPainter>
#include <QApplication>
#include <QMouseEvent>
#include <QListWidget>

HighlightItemDelegate::HighlightItemDelegate(CommFrameModel *model, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_model(model)
    , m_enabled(true)
{
}

void HighlightItemDelegate::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

// Helper: get frame ID from index display text, avoiding the viewport/QListWidget cast issue.
static int idFromIndex(const QModelIndex &index)
{
    QString text = index.data(Qt::DisplayRole).toString();
    QString idStr = FilterUtility::getId(text);       // strips any DBC label suffix
    return (int)Utility::ParseStringToNum(idStr);
}

void HighlightItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    // Shrink the option rect to leave room for the indicator on the right
    QStyleOptionViewItem opt = option;
    if (m_enabled)
        opt.rect.adjust(0, 0, -INDICATOR_WIDTH, 0);

    QStyledItemDelegate::paint(painter, opt, index);

    if (!m_enabled)
        return;

    int id = idFromIndex(index);
    bool highlighted = m_model->isHighlighted(id);

    // Draw indicator strip on the right: [count text | circle]
    QRect indicatorRect(option.rect.right() - INDICATOR_WIDTH + 1, option.rect.top(),
                        INDICATOR_WIDTH, option.rect.height());

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const int CIRCLE_AREA = 20; // rightmost pixels reserved for the circle
    const int COUNT_AREA  = INDICATOR_WIDTH - CIRCLE_AREA;

    // Draw count text in the left part of the strip
    int cnt = m_model->getFrameCountForId(id);
    if (cnt > 0) {
        QRect countRect(indicatorRect.left(), indicatorRect.top(),
                        COUNT_AREA, indicatorRect.height());
        QFont smallFont = painter->font();
        smallFont.setPointSizeF(qMax(6.0, smallFont.pointSizeF() - 2));
        painter->setFont(smallFont);
        painter->setPen(QApplication::palette().color(QPalette::WindowText));
        painter->drawText(countRect, Qt::AlignRight | Qt::AlignVCenter,
                          cnt >= 10000 ? QStringLiteral("9999+") : QString::number(cnt));
    }

    // Draw circle in the right part of the strip
    QRect circleRect(indicatorRect.right() - CIRCLE_AREA + 1, indicatorRect.top(),
                     CIRCLE_AREA, indicatorRect.height());
    int cx = circleRect.left() + circleRect.width() / 2;
    int cy = circleRect.top() + circleRect.height() / 2;
    int r  = qMin(7, circleRect.height() / 2 - 2);

    // Match the exact same color that CommFrameModel uses for highlighted rows
    const QPalette &pal = QApplication::palette();
    QColor hlColor = pal.color(QPalette::Highlight);
    bool isDark = pal.color(QPalette::Window).lightness() < 128;
    if (isDark && hlColor.lightness() < 120)
        hlColor = hlColor.lighter(210);
    QColor fgColor = pal.color(QPalette::WindowText);

    if (highlighted) {
        painter->setBrush(hlColor);
        painter->setPen(QPen(hlColor.darker(140), 1));
    } else {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(fgColor, 1.5));
    }
    painter->drawEllipse(QPoint(cx, cy), r, r);

    painter->restore();
}

QSize HighlightItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const
{
    QSize base = QStyledItemDelegate::sizeHint(option, index);
    if (m_enabled)
        base.rwidth() += INDICATOR_WIDTH;
    return base;
}

bool HighlightItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *abstractModel,
                                         const QStyleOptionViewItem &option,
                                         const QModelIndex &index)
{
    // Check if the click is inside the indicator strip before consuming it
    if (m_enabled && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            int clickX = me->position().toPoint().x();
            if (clickX >= option.rect.right() - INDICATOR_WIDTH) {
                // Click is in the indicator area — toggle highlight
                m_model->toggleHighlight(idFromIndex(index));
                return true; // consumed; do NOT forward to parent (would also toggle checkbox)
            }
        }
    }

    // For all other clicks (including the checkbox area), let the base class handle it.
    // This is what actually toggles Qt::ItemIsUserCheckable checkboxes.
    return QStyledItemDelegate::editorEvent(event, abstractModel, option, index);
}
