#include "candatagrid.h"
#include "ui_candatagrid.h"

#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <QRandomGenerator>

CANDataGrid::CANDataGrid(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CANDataGrid)
{
    ui->setupUi(this);

    gridMode = GridMode::CHANGED_BITS;

    memset(data, 0, 8);
    memset(refData, 0, 8);
    memset(usedData, 0, 8);
    memset(usedSignalNum, -1, 64);
    for (int x = 0; x < 8; x++)
        for (int y = 0; y < 8; y++)
            textStates[x][y] = GridTextState::NORMAL;

    blackBrush = QBrush(Qt::black);
    whiteBrush = QBrush(Qt::white);
    redBrush = QBrush(Qt::red);
    greenBrush = QBrush(Qt::green);
    greenHashBrush = QBrush(QColor(0, 0xB6, 0), Qt::BDiagPattern);
    blackHashBrush = QBrush(QColor(0, 0, 0), Qt::FDiagPattern);
    grayBrush = QBrush(QColor(230,230,230));
}

CANDataGrid::~CANDataGrid()
{
    delete ui;
}

GridMode CANDataGrid::getMode()
{
    return gridMode;
}

void CANDataGrid::setMode(GridMode mode)
{
    gridMode = mode;
}

void CANDataGrid::mousePressEvent(QMouseEvent *event)
{
    QPoint clickedPoint = event->pos();
    if (event->button() == Qt::LeftButton)
    {
        //qDebug() << "Mouse Loc " << clickedPoint;
        clickedPoint -= upperLeft;
        if (clickedPoint.x() < 0 || clickedPoint.y() < 0)
        {
            //qDebug() << "Clicked outside the grid you wanker";
            return;
        }
        int x = clickedPoint.x() / gridSize.x();
        int y = clickedPoint.y() / gridSize.y();
        //qDebug() << "Grid square clicked " << x << " " << y;
        emit gridClicked(x,y);
    }
}

void CANDataGrid::setCellTextState(int x, int y, GridTextState state)
{
    if (x < 0) return;
    if (x > 7) return;
    if (y < 0) return;
    if (y > 7) return;
    textStates[x][y] = state;
    this->update();
}

GridTextState CANDataGrid::getCellTextState(int x, int y)
{
    if (x < 0) return GridTextState::NORMAL;
    if (x > 7) return GridTextState::NORMAL;
    if (y < 0) return GridTextState::NORMAL;
    if (y > 7) return GridTextState::NORMAL;
    return textStates[x][y];
}

void CANDataGrid::setSignalNames(int sigIdx, const QString sigName)
{
    if (sigIdx < 0) return;
    if (sigIdx >= signalNames.size())
    {
        signalNames.resize(sigIdx * 2);
    }
    signalNames[sigIdx] = sigName;
}

void CANDataGrid::clearSignalNames()
{
    signalNames.clear();
    signalNames.resize(40);
    signalColors.clear();
}

void CANDataGrid::setUsedSignalNum(int bit, int signal)
{
    if (bit < 0) return;
    if (bit > 63) return;
    usedSignalNum[bit] = signal;
}

int CANDataGrid::getUsedSignalNum(int bit)
{
    if (bit < 0) return 0;
    if (bit > 63) return 0;
    return usedSignalNum[bit];
}

void CANDataGrid::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    paintCommonBeginning();
    if (gridMode == GridMode::CHANGED_BITS) paintChangedBits();
    if (gridMode == GridMode::SIGNAL_VIEW) paintSignalView();
    paintCommonEnding();
}

void CANDataGrid::paintCommonBeginning()
{
    int x;

    painter = new QPainter(this);
    viewport = painter->viewport();

    xSpan = viewport.right() - viewport.left();
    ySpan = viewport.bottom() - viewport.top();

    qDebug() << "XSpan" << xSpan << " YSpan " << ySpan;

    xSector = xSpan / 9;
    ySector = ySpan / 9;

    //the whole thing is broken up into 81 chunks which are broken up
    //into the 8x8 grid of bits in the bottom right and the other parts
    //taken up by helper text

    if (gridMode == GridMode::CHANGED_BITS)
    {
        bigTextSize = qMin(xSector, ySector) * 0.6;
        smallTextSize = qMin(xSector, ySector) * 0.5;
    }
    if (gridMode == GridMode::SIGNAL_VIEW)
    {
        bigTextSize = qMin(xSector, ySector) * 0.5;
        smallTextSize = qMin(xSector, ySector) * 0.25;
    }
    sigNameTextSize = qMin(xSector, ySector) * 0.19;

    painter->setPen(QPen(QApplication::palette().color(QPalette::Text)));
    mainFont.setPixelSize(bigTextSize);
    painter->setFont(mainFont);
    smallFont.setPixelSize(smallTextSize);
    boldFont.setPixelSize(bigTextSize);
    boldFont.setBold(true);
    sigNameFont.setPixelSize(sigNameTextSize);

    smallMetric = new QFontMetrics(sigNameFont);

    painter->setFont(smallFont);
    painter->drawText(QRect(viewport.left(), viewport.top(), xSector, ySector), Qt::AlignCenter, "BITS ->");
    painter->setFont(boldFont);

    for (x = 0; x < 8; x++)
    {
        painter->drawText(QRect(viewport.left() + (x+1) * xSector, viewport.top(), xSector, ySector), Qt::AlignCenter, QString::number(7-x));
    }
    //for (y = 0; y < 8; y++)
    //{
    //    painter->drawText(QRect(viewport.left() + xSector, viewport.top() + ySector * (y + 2), xSector, ySector), Qt::AlignCenter, QString::number(y));
    //}

    painter->drawText(viewport.left() + 4, viewport.top() + ySector * 3, "B");
    painter->drawText(viewport.left() + 4, viewport.top() + ySector * 4, "Y");
    painter->drawText(viewport.left() + 4, viewport.top() + ySector * 5, "T");
    painter->drawText(viewport.left() + 4, viewport.top() + ySector * 6, "E");
    painter->drawText(viewport.left() + 4, viewport.top() + ySector * 7, "S");


    painter->drawText(QRect(viewport.left() + (xSector / 4.0), viewport.top() + ySector * 1, xSector, ySector), Qt::AlignCenter, "0");
    painter->drawText(QRect(viewport.left() + (xSector / 4.0), viewport.top() + ySector * 2, xSector, ySector), Qt::AlignCenter, "1");
    painter->drawText(QRect(viewport.left() + (xSector / 4.0), viewport.top() + ySector * 3, xSector, ySector), Qt::AlignCenter, "2");
    painter->drawText(QRect(viewport.left() + (xSector / 4.0), viewport.top() + ySector * 4, xSector, ySector), Qt::AlignCenter, "3");
    painter->drawText(QRect(viewport.left() + (xSector / 4.0), viewport.top() + ySector * 5, xSector, ySector), Qt::AlignCenter, "4");
    painter->drawText(QRect(viewport.left() + (xSector / 4.0), viewport.top() + ySector * 6, xSector, ySector), Qt::AlignCenter, "5");
    painter->drawText(QRect(viewport.left() + (xSector / 4.0), viewport.top() + ySector * 7, xSector, ySector), Qt::AlignCenter, "6");
    painter->drawText(QRect(viewport.left() + (xSector / 4.0), viewport.top() + ySector * 8, xSector, ySector), Qt::AlignCenter, "7");

    painter->setPen(QPen(Qt::black));
    painter->setFont(mainFont);
}

void CANDataGrid::paintChangedBits()
{
    int x, y, bit;
    unsigned char prevByte, thisByte;
    bool thisBit, prevBit;
    int usedSigNum;
    QString prevSigName;

    //now, color the bitfield by seeing if a given bit is freshly set/unset in the new data
    //compared to the old. Bits that are not set in either are white, bits set in both are black
    //bits that used to be set but now are unset are red, bits that used to be unset but now are set
    //are green

    for (y = 0; y < 8; y++)
    {
        thisByte = data[y];
        prevByte = refData[y];
        for (x = 0; x < 8; x++)
        {
            bit = (y * 8) + (7 - x);
            thisBit = false;
            prevBit = false;
            if ((thisByte & (1 << (7-x))) == (1 << (7-x))) thisBit = true;
            if ((prevByte & (1 << (7-x))) == (1 << (7-x))) prevBit = true;

            if (thisBit)
            {
                if (prevBit)
                {
                    painter->setBrush(blackBrush);
                }
                else
                {
                    painter->setBrush(greenBrush);
                }
            }
            else
            {
                if (prevBit)
                {
                    painter->setBrush(redBrush);
                }
                else
                {
                    usedSigNum = -1;
                    if ((usedData[y] & (1 << (7-x))) == (1 << (7-x)))
                    {
                        grayBrush = QBrush(QColor(0xB6, 0xB6, 0xB6), Qt::BDiagPattern);
                        painter->setBrush(grayBrush);
                    }
                    else painter->setBrush(whiteBrush);
                }
            }

            //painter->fillRect(viewport.left() + (x+2) * xSector, viewport.top() + (y+2) * ySector, xSector, ySector, redBrush);
            painter->drawRect(viewport.left() + (x+1) * xSector, viewport.top() + (y+1) * ySector, xSector, ySector);
            switch (textStates[x][y])
            {
            case GridTextState::NORMAL:
                if (thisBit && prevBit) painter->setPen(QPen(Qt::gray));
                else painter->setPen(QPen(Qt::black));
                painter->setFont(mainFont);
                break;
            case GridTextState::BOLD_BLUE:
                painter->setPen(QPen(Qt::blue));
                painter->setFont(boldFont);
                break;
            case GridTextState::INVERT:
                painter->setFont(mainFont);
                QColor brushColor = painter->brush().color();
                painter->setPen(QColor(255-brushColor.red(), 255-brushColor.green(), 255-brushColor.blue()));
                break;
            }
            //change style of bit number output for current signal
            //if (thisBit) painter->setFont(boldFont);
           // else painter->setFont(mainFont);
            painter->setFont(smallFont);

            painter->drawText(viewport.left() + (x+1) * xSector + (xSector / 3), viewport.top() + (y + 2) * ySector - (ySector * 0.4), QString::number(bit));

            painter->setFont(mainFont);
            painter->setPen(QPen(Qt::black));
        }
    }
}

void CANDataGrid::paintSignalView()
{
    int x, y, bit;
    unsigned char prevByte, thisByte;
    bool thisBit, prevBit;
    int usedSigNum;
    QString prevSigName;

    //if this is true then generate unique colors for each signal
    if ((signalColors.count() == 0) && (signalNames.count() > 0))
    {
        qDebug() << "Generating colors";
        signalColors.resize(signalNames.count());
        for (int i = 0; i < signalNames.count(); i++)
        {
            QColor newColor;
            while (newColor.saturation() < 40)
                newColor.setRgb(QRandomGenerator::global()->bounded(160) + 60,QRandomGenerator::global()->bounded(160) + 60, QRandomGenerator::global()->bounded(160) + 60);
            qDebug() << newColor;
            signalColors[i] = newColor;
        }
    }

    for (y = 0; y < 8; y++)
    {
        thisByte = data[y];
        prevByte = refData[y];
        for (x = 0; x < 8; x++)
        {
            bit = (y * 8) + (7 - x);
            thisBit = false;
            prevBit = false;
            if ((thisByte & (1 << (7-x))) == (1 << (7-x))) thisBit = true;
            if ((prevByte & (1 << (7-x))) == (1 << (7-x))) prevBit = true;

            if (thisBit)
            {
                if (prevBit)
                {
                    if (signalColors.count() > 0) painter->setBrush(blackHashBrush);
                    else painter->setBrush(blackBrush);
                }
                else
                {
                    if (signalColors.count() > 0) painter->setBrush(greenHashBrush);
                    else painter->setBrush(greenBrush);
                }
            }
            else
            {
                if (prevBit)
                {
                    painter->setBrush(redBrush);
                }
                else
                {
                    usedSigNum = -1;
                    if ((usedData[y] & (1 << (7-x))) == (1 << (7-x)))
                    {
                        usedSigNum = getUsedSignalNum(bit);
                        if (usedSigNum == -1)
                        {
                            grayBrush = QBrush(QColor(0xB6, 0xB6, 0xB6), Qt::BDiagPattern);
                            painter->setBrush(grayBrush);
                        }
                        else painter->setBrush(QBrush(signalColors[usedSigNum]));
                    }
                    else painter->setBrush(whiteBrush);
                }
            }

            //painter->fillRect(viewport.left() + (x+2) * xSector, viewport.top() + (y+2) * ySector, xSector, ySector, redBrush);
            painter->drawRect(viewport.left() + (x+1) * xSector, viewport.top() + (y+1) * ySector, xSector, ySector);
            switch (textStates[x][y])
            {
            case GridTextState::NORMAL:
                //if (thisBit && prevBit) painter->setPen(QPen(Qt::gray));
                /*else*/ painter->setPen(QPen(Qt::black));
                painter->setFont(mainFont);
                break;
            case GridTextState::BOLD_BLUE:
                painter->setPen(QPen(Qt::blue));
                painter->setFont(boldFont);
                break;
            case GridTextState::INVERT:
                painter->setFont(mainFont);
                QColor brushColor = painter->brush().color();
                painter->setPen(QColor(255-brushColor.red(), 255-brushColor.green(), 255-brushColor.blue()));
                break;
            }
            //change style of bit number output for current signal
            //if (thisBit) painter->setFont(boldFont);
           // else painter->setFont(mainFont);
            painter->setFont(smallFont);

            painter->drawText(viewport.left() + (x+1) * xSector + (xSector / 8), viewport.top() + (y + 2) * ySector - (ySector * 0.7), QString::number(bit));

            painter->setFont(mainFont);
            painter->setPen(QPen(Qt::black));
        }
    }

    //now if signal names are loaded we'll go through all the bits again and try to label over top of the grid
    if (signalNames.count() > 0)
    {
        painter->setFont(sigNameFont);
        for (y = 0; y < 8; y++)
        {
            for (x = 0; x < 8; x++)
            {
                bit = (y * 8) + (7 - x);
                usedSigNum = -1;
                if ((usedData[y] & (1 << (7-x))) == (1 << (7-x)))
                {
                    usedSigNum = getUsedSignalNum(bit);
                    if (prevSigName != signalNames[usedSigNum])
                    {
                        prevSigName = signalNames[usedSigNum];

                        int textWidth = smallMetric->horizontalAdvance(prevSigName);

                        if (textWidth > xSector) //signal name is too long for a single cell. Try to wrap it
                        {
                            int numAvgChars = xSector / smallMetric->averageCharWidth();
                            painter->drawText(viewport.left() + (x+1) * xSector, viewport.top() + (y + 2) * ySector - (ySector * 0.4), prevSigName.left(numAvgChars - 1));
                            QString remainder = prevSigName.mid(numAvgChars - 1, -1);
                            textWidth = smallMetric->horizontalAdvance(prevSigName);
                            if (textWidth > xSector)
                            {
                                painter->drawText(viewport.left() + (x+1) * xSector, viewport.top() + (y + 2) * ySector - (ySector * 0.2), remainder.left(numAvgChars - 1));

                            }
                            else painter->drawText(viewport.left() + (x+1) * xSector, viewport.top() + (y + 2) * ySector - (ySector * 0.2), remainder);
                        }
                        else painter->drawText(viewport.left() + (x+1) * xSector, viewport.top() + (y + 2) * ySector - (ySector * 0.4), prevSigName);
                    }
                }
            }
        }
    }
}

void CANDataGrid::paintCommonEnding()
{
    upperLeft.setX(viewport.left() + 1 * xSector);
    upperLeft.setY(viewport.top() + 1 * ySector);
    gridSize.setX(xSector);
    gridSize.setY(ySector);

    delete painter;
    delete smallMetric;
}

void CANDataGrid::saveImage(QString filename, int width, int height)
{
    Q_UNUSED(width); //currently unused but I want to use them in the future
    Q_UNUSED(height);
    //can't quite do the below commented out stuff
    //it works but doesn't scale the image into that pixmap. Need to
    //figure out how to draw the size of the pixmap
    /*
    QSize pSize;

    if (width == 0) pSize.setWidth(this->size().width());
        else pSize.setWidth(width);

    if (height == 0) pSize.setHeight(this->size().height());
        else pSize.setHeight(height);

    QPixmap pixmap(pSize);
    */
    QPixmap pixmap(this->size());

    this->render(&pixmap);
    pixmap.save(filename); //QT will automatically pick the file format given the extension
}

void CANDataGrid::setReference(unsigned char *newRef, bool bUpdate = true)
{
    memcpy(refData, newRef, 8);
    if (bUpdate) this->update();
}

void CANDataGrid::updateData(unsigned char *newData, bool bUpdate = true)
{
    memcpy(data, newData, 8); //on a 64 bit processor this is probably optimized to a single instruction
    if (bUpdate) this->update();
}

void CANDataGrid::setUsed(unsigned char *newData, bool bUpdate = false)
{
    memcpy(usedData, newData, 8);
    if (bUpdate) this->update();
}
