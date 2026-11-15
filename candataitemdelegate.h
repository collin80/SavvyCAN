#ifndef CANDATAITEMDELEGATE_H
#define CANDATAITEMDELEGATE_H

#include <QStyledItemDelegate>

class CommFrameModel;

/*
 * Lean "changed byte" highlighter for the Frames view Data column.
 *
 * It does NOT re-implement the Data-column rendering (that stays in
 * CommFrameModel::data()). When overwrite mode + hex mode are active and the
 * row reports changed payload bytes, it re-draws the cell as rich text, wrapping
 * only the changed byte pairs in a highlight span and leaving the rest of the
 * model's string (DBC/error text) verbatim. In every other case it falls back to
 * the default delegate, so the normal multi-protocol rendering is untouched.
 */
class CanDataItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit CanDataItemDelegate(CommFrameModel *model, QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    // Builds the highlighted-HTML for this cell, or returns false if no highlighting applies.
    bool buildHtml(const QModelIndex &index, QString &html) const;

    CommFrameModel *m_model;
};

#endif // CANDATAITEMDELEGATE_H
