#ifndef HIGHLIGHTITEMDELEGATE_H
#define HIGHLIGHTITEMDELEGATE_H

#include <QStyledItemDelegate>
#include "canframemodel.h"

class HighlightItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit HighlightItemDelegate(CommFrameModel *model, QObject *parent = nullptr);

    void setEnabled(bool enabled);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    bool editorEvent(QEvent *event, QAbstractItemModel *abstractModel,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;

private:
    static constexpr int INDICATOR_WIDTH = 52; // count text (≤4 digits) + circle

    CommFrameModel *m_model;
    bool m_enabled;
};

#endif // HIGHLIGHTITEMDELEGATE_H
