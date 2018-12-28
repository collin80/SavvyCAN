#include "SnifferDelegate.h"
#include <QPainter>
#include <QDebug>
#include "utility.h"
#include "re/sniffer/snifferitem.h"

SnifferDelegate::SnifferDelegate(QWidget *parent) : QItemDelegate(parent)
{
    blackBrush = QBrush(Qt::black);
    whiteBrush = QBrush(Qt::white);
    //redBrush = QBrush(Qt::red);
    //greenBrush = QBrush(Qt::green);
    //grayBrush = QBrush(QColor(230,230,230));
    mainFont.setPointSize(10);
    mainFontInfo = new QFontInfo(mainFont);
}

void SnifferDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    //qDebug() << "SnifferDelegate Paint Event";

    if (index.column() < 2)
    {
        QItemDelegate::paint(painter, option, index);
        return;
    }

    if (index.column() > 9) return;

    int x, y;
    SnifferItem *item = static_cast<SnifferItem*>(index.internalPointer());
    int val = item->getData(index.column() - 2);
    if (val < 0) return;

    QRect viewport = option.rect;

    int xSpan = viewport.right() - viewport.left();
    int ySpan = viewport.bottom() - viewport.top();

    //qDebug() << "XSpan" << xSpan << " YSpan " << ySpan;

    int xSector = xSpan / 8;

    painter->setPen(QPen(Qt::black));
    painter->setFont(mainFont);

    for (x = 0; x < 8; x++)
    {
        if (val & (1 << x))
        {
            painter->setBrush(blackBrush);
        }
        else
        {
            painter->setBrush(whiteBrush);
        }
        painter->drawRect(viewport.left() + x * xSector, viewport.top(), xSector, xSector);
    }

    painter->drawText(QRect(viewport.left(), viewport.top() + xSector, xSpan, mainFontInfo->pixelSize()), Qt::AlignCenter, Utility::formatNumber(val));
}

QSize SnifferDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize origSize = QItemDelegate::sizeHint(option, index);
    origSize.setHeight(origSize.height() * 2.0);
    return origSize;
}


