#include "SnifferDelegate.h"
#include <QPainter>
#include <QApplication>
#include <QDebug>
#include "utility.h"
#include "re/sniffer/snifferitem.h"

SnifferDelegate::SnifferDelegate(QWidget *parent) : QItemDelegate(parent)
{
    QColor TextColor = QApplication::palette().color(QPalette::Text);
    if (TextColor.red() + TextColor.green() + TextColor.blue() < 200)
    {
        mDarkMode = false;
        redBrush = QBrush(Qt::red);
        greenBrush = QBrush(Qt::green);
    }
    else
    {
        mDarkMode = true;
        redBrush = QBrush(QColor(128,0,0));
        greenBrush = QBrush(QColor(0,128,0));
    }
    blackBrush = QBrush(Qt::black);
    whiteBrush = QBrush(Qt::white);
    grayBrush = QBrush(QColor(230,230,230));
    mainFont.setPointSize(10);
    mainFontInfo = new QFontInfo(mainFont);
    mFadeInactive = false;

}

bool SnifferDelegate::getFadeInactive()
{
    return mFadeInactive;
}

void SnifferDelegate::setFadeInactive(bool val)
{
    mFadeInactive = val;
}

void SnifferDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    //qDebug() << "SnifferDelegate Paint Event";

    if (index.column() < 3) //allow default handling of the first two columns
    {
        QItemDelegate::paint(painter, option, index);
        return;
    }

    if (index.column() > 10) return;

    int x;
    SnifferItem *item = static_cast<SnifferItem*>(index.internalPointer());
    int idx = index.column() - 3;
    int val = item->getData(idx);
    int prevVal = item->getLastData(idx);
    int notchPattern = item->getNotchPattern(idx);
    int maskPattern;
    if (val < 0) return;

    QRect viewport = option.rect;

    int xSpan = viewport.width();
    int ySpan = viewport.height();
    int textHeight = qMax(mainFontInfo->pixelSize() + 2, 12);
    int graphHeight = ySpan - textHeight;
    if (graphHeight < 8) graphHeight = ySpan;

    int xSector = xSpan / 8;
    if (xSector < 1) xSector = 1;
    if (xSector > graphHeight) xSector = graphHeight;

    int graphWidth = xSector * 8;
    int xOffset = (xSpan - graphWidth) / 2;
    if (xOffset < 0) xOffset = 0;

    int yOffset = (graphHeight - xSector) / 2;
    if (yOffset < 0) yOffset = 0;

    int v = item->getSeqInterval(index.column() - 3) * 10;
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
        if (mFadeInactive) painter->setOpacity((255 - v) / 255.0);
        else painter->setOpacity(1.0);
        painter->drawRect(viewport.left() + xOffset + x * xSector, viewport.top() + yOffset, xSector, xSector);
    }

    //painter->setPen(QPen(QColor(v,v,v,255)));
    painter->setOpacity(1.0);
    painter->setPen(QApplication::palette().color(QPalette::Text));
    painter->setFont(mainFont);
    QRect textRect(viewport.left(), viewport.top() + graphHeight, xSpan, ySpan - graphHeight);
    painter->drawText(textRect, Qt::AlignCenter, Utility::formatNumber((unsigned char)val));
}

QSize SnifferDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize origSize = QItemDelegate::sizeHint(option, index);
    origSize.setHeight(origSize.height() * 2.0);
    return origSize;
}


