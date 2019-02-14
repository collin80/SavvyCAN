#include "SnifferDelegate.h"
#include <QPainter>
#include <QDebug>
#include "utility.h"
#include "re/sniffer/snifferitem.h"

SnifferDelegate::SnifferDelegate(QWidget *parent) : QItemDelegate(parent)
{
    blackBrush = QBrush(Qt::black);
    whiteBrush = QBrush(Qt::white);
    redBrush = QBrush(Qt::red);
    greenBrush = QBrush(Qt::green);
    grayBrush = QBrush(QColor(230,230,230));
    mainFont.setPointSize(10);
    mainFontInfo = new QFontInfo(mainFont);
}

void SnifferDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    //qDebug() << "SnifferDelegate Paint Event";

    if (index.column() < 2) //allow default handling of the first two columns
    {
        QItemDelegate::paint(painter, option, index);
        return;
    }

    if (index.column() > 9) return;

    int x, y;
    SnifferItem *item = static_cast<SnifferItem*>(index.internalPointer());
    int idx = index.column() - 2;
    int val = item->getData(idx);
    int prevVal = item->getLastData(idx);
    int notchPattern = item->getNotchPattern(idx);
    int maskPattern;
    if (val < 0) return;

    QRect viewport = option.rect;

    int xSpan = viewport.right() - viewport.left();
    int ySpan = viewport.bottom() - viewport.top();

    //qDebug() << "XSpan" << xSpan << " YSpan " << ySpan;

    int xSector = xSpan / 8;
    int v = item->getSeqInterval(index.column() - 2) * 10;
    if (v > 225) v = 225;
    if (v < 0) v = 0;

    for (x = 0; x < 8; x++)
    {
        maskPattern = 1 << (7-x);
        //We look to see if each bit has changed since the last update or not. If not we
        //straight draw it black if set or white if unset. If it changed then draw it green if it is newly set
        //and red if newly unset
        //But also, if a bit is notched we just plain draw it gray no matter what it's doing
        if (notchPattern & maskPattern)
        {
            painter->setBrush(grayBrush);
        }
        else if ( (val & maskPattern) == (prevVal & maskPattern) ) //wasn't notched so has it changed? No? Then...
        {
            if (val & maskPattern)
            {
                painter->setBrush(blackBrush);
            }
            else
            {
                painter->setBrush(whiteBrush);
            }
        }
        else //wasn't notched and did change since last time.
        {
            if (val & maskPattern)
            {
                painter->setBrush(greenBrush);
            }
            else
            {
                painter->setBrush(redBrush);
            }
        }
        painter->setOpacity((255 - v) / 255.0);
        painter->drawRect(viewport.left() + x * xSector, viewport.top(), xSector, xSector);
    }

    //painter->setPen(QPen(QColor(v,v,v,255)));
    painter->setPen(Qt::black);
    painter->setFont(mainFont);
    painter->drawText(QRect(viewport.left(), viewport.top() + xSector, xSpan, mainFontInfo->pixelSize()), Qt::AlignCenter, Utility::formatNumber(val));
}

QSize SnifferDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize origSize = QItemDelegate::sizeHint(option, index);
    origSize.setHeight(origSize.height() * 2.0);
    return origSize;
}


