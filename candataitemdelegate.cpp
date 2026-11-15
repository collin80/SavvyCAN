#include "candataitemdelegate.h"
#include "canframemodel.h"

#include <QApplication>
#include <QPainter>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>

CanDataItemDelegate::CanDataItemDelegate(CommFrameModel *model, QObject *parent)
    : QStyledItemDelegate(parent), m_model(model)
{
}

bool CanDataItemDelegate::buildHtml(const QModelIndex &index, QString &html) const
{
    // Only highlight in overwrite + hex mode (the only mode where "changed since last frame" is meaningful,
    // and where each byte is a fixed 2-hex-char field so the prefix math below holds).
    if (!m_model || !m_model->getOverwriteMode() || !m_model->getHexMode())
        return false;

    const quint64 changed = index.data(CommFrameModel::ChangedBytesRole).toULongLong();
    if (changed == 0)
        return false;

    const QByteArray payload = index.data(CommFrameModel::PayloadRole).toByteArray();
    const int dataLen = static_cast<int>(payload.size());
    if (dataLen <= 0)
        return false;

    int bpl = m_model->getBytesPerLine();
    if (bpl < 1) bpl = 8;

    // The model prints each byte as 2 upper-hex chars followed by one separator
    // (a space, or a newline at each bytesPerLine group). So the byte region is
    // exactly dataLen*3 chars long; rebuild it with highlight spans and keep the
    // trailing model text (DBC signals / error text) verbatim.
    QString out;
    for (int i = 0; i < dataLen; i++)
    {
        const QString hex = QString("%1").arg(static_cast<quint8>(payload.at(i)), 2, 16, QChar('0')).toUpper();
        if ((changed >> i) & 1ull)
            out += "<span style=\"background-color:#FFD54F; color:#000000;\">" + hex + "</span>";
        else
            out += hex;

        const bool wrap = (((i + 1) % bpl) == 0) && (i != dataLen - 1);
        out += wrap ? QStringLiteral("<br>") : QStringLiteral("&nbsp;");
    }

    const QString full = index.data(Qt::DisplayRole).toString();
    const int prefixLen = dataLen * 3; // 2 hex chars + 1 separator per byte
    if (full.length() > prefixLen)
    {
        QString rest = full.mid(prefixLen).toHtmlEscaped();
        rest.replace('\n', QStringLiteral("<br>"));
        out += rest;
    }

    html = out;
    return true;
}

void CanDataItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QString html;
    if (!buildHtml(index, html))
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.text.clear(); // we draw the text ourselves

    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    // Draw the item background / selection highlight with the text blanked out.
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    QTextDocument doc;
    doc.setDefaultFont(opt.font);
    doc.setHtml(html);
    doc.setDocumentMargin(0);

    const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

    painter->save();
    painter->translate(textRect.topLeft());

    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.clip = QRect(0, 0, textRect.width(), textRect.height());
    // Keep text readable when the row is selected.
    if (opt.state & QStyle::State_Selected)
        ctx.palette.setColor(QPalette::Text, opt.palette.color(QPalette::Active, QPalette::HighlightedText));
    else
        ctx.palette.setColor(QPalette::Text, opt.palette.color(QPalette::Active, QPalette::Text));

    doc.documentLayout()->draw(painter, ctx);
    painter->restore();
}

QSize CanDataItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QString html;
    if (!buildHtml(index, html))
        return QStyledItemDelegate::sizeHint(option, index);

    QTextDocument doc;
    doc.setDefaultFont(option.font);
    doc.setHtml(html);
    doc.setDocumentMargin(0);
    return QSize(static_cast<int>(doc.idealWidth()), static_cast<int>(doc.size().height()));
}
