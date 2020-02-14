#include "candatagrid.h"
#include "ui_candatagrid.h"

#include <QPainter>
#include <QDebug>
#include <QMouseEvent>

CANDataGrid::CANDataGrid(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CANDataGrid)
{
    ui->setupUi(this);

    memset(data, 0, 8);
    memset(refData, 0, 8);
    memset(usedData, 0, 8);
    for (int x = 0; x < 8; x++)
        for (int y = 0; y < 8; y++)
            textStates[x][y] = GridTextState::NORMAL;
}

CANDataGrid::~CANDataGrid()
{
    delete ui;
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

GridTextState CANDataGrid::geCellTextState(int x, int y)
{
    if (x < 0) return GridTextState::NORMAL;
    if (x > 7) return GridTextState::NORMAL;
    if (y < 0) return GridTextState::NORMAL;
    if (y > 7) return GridTextState::NORMAL;
    return textStates[x][y];
}

void CANDataGrid::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    qDebug() << "CANDataGrid Paint Event";
    int x, y;
    unsigned char prevByte, thisByte;
    bool thisBit, prevBit;
    QBrush blackBrush, whiteBrush, redBrush, greenBrush, grayBrush;
    QPainter painter(this);


    QRect viewport = painter.viewport();

    int xSpan = viewport.right() - viewport.left();
    int ySpan = viewport.bottom() - viewport.top();

    qDebug() << "XSpan" << xSpan << " YSpan " << ySpan;

    int xSector = xSpan / 10;
    int ySector = ySpan / 10;

    blackBrush = QBrush(Qt::black);
    whiteBrush = QBrush(Qt::white);
    redBrush = QBrush(Qt::red);
    greenBrush = QBrush(Qt::green);
    grayBrush = QBrush(QColor(230,230,230));

    //the whole thing is broken up into 100 chunks which are allocated as such:
    //The entirety of the upper row is taken up by "BITS"
    //The next row down skips two columns and then has 7 6 5 4 3 2 1 0
    //The bottom right 64 entries are the grid
    //The left most column has "BYTES" written vertically down it
    //the next left most column has "0 1 2 3 4 5 6 7" written down along the grid

    painter.setPen(QPen(QApplication::palette().color(QPalette::Text)));
    QFont mainFont;
    mainFont.setPixelSize(qMin(xSector, ySector) - 10);
    painter.setFont(mainFont);
    QFont smallFont;
    smallFont.setPixelSize(qMin(xSector, ySector) - 15);
    QFont boldFont;
    boldFont.setPixelSize(qMin(xSector, ySector) - 10);
    boldFont.setBold(true);

    painter.drawText(QRect(viewport.left(), viewport.top(), xSpan, ySector), Qt::AlignCenter, tr("BITS"));

    for (x = 0; x < 8; x++)
    {
        painter.drawText(QRect(viewport.left() + (x+2) * xSector, viewport.top() + ySector, xSector, ySector), Qt::AlignCenter, QString::number(7-x));
    }
    for (y = 0; y < 8; y++)
    {
        painter.drawText(QRect(viewport.left() + xSector, viewport.top() + ySector * (y + 2), xSector, ySector), Qt::AlignCenter, QString::number(y));
    }

    painter.drawText(viewport.left() + 10, viewport.top() + ySector * 4, "B");
    painter.drawText(viewport.left() + 10, viewport.top() + ySector * 5, "Y");
    painter.drawText(viewport.left() + 10, viewport.top() + ySector * 6, "T");
    painter.drawText(viewport.left() + 10, viewport.top() + ySector * 7, "E");
    painter.drawText(viewport.left() + 10, viewport.top() + ySector * 8, "S");

    //now, color the bitfield by seeing if a given bit is freshly set/unset in the new data
    //compared to the old. Bits that are not set in either are white, bits set in both are black
    //bits that used to be set but now are unset are red, bits that used to be unset but now are set
    //are green

    painter.setPen(QPen(Qt::gray));
    //painter.setFont(smallFont);

    for (y = 0; y < 8; y++)
    {
        thisByte = data[y];
        prevByte = refData[y];
        for (x = 0; x < 8; x++)
        {
            thisBit = false;
            prevBit = false;
            if ((thisByte & (1 << (7-x))) == (1 << (7-x))) thisBit = true;
            if ((prevByte & (1 << (7-x))) == (1 << (7-x))) prevBit = true;            

            if (thisBit)
            {
                if (prevBit)
                {
                    painter.setBrush(blackBrush);
                }
                else
                {
                    painter.setBrush(greenBrush);
                }
            }
            else
            {
                if (prevBit)
                {
                    painter.setBrush(redBrush);
                }
                else
                {
                    if ((usedData[y] & (1 << (7-x))) == (1 << (7-x))) painter.setBrush(grayBrush);
                    else painter.setBrush(whiteBrush);
                }
            }

            //painter.fillRect(viewport.left() + (x+2) * xSector, viewport.top() + (y+2) * ySector, xSector, ySector, redBrush);
            painter.drawRect(viewport.left() + (x+2) * xSector, viewport.top() + (y+2) * ySector, xSector, ySector);
            switch (textStates[x][y])
            {
            case GridTextState::NORMAL:
                painter.setPen(QPen(Qt::gray));
                painter.setFont(mainFont);
                break;
            case GridTextState::BOLD_BLUE:
                painter.setPen(QPen(Qt::blue));
                painter.setFont(boldFont);
                break;
            case GridTextState::INVERT:
                painter.setFont(mainFont);
                QColor brushColor = painter.brush().color();
                painter.setPen(QColor(255-brushColor.red(), 255-brushColor.green(), 255-brushColor.blue()));
                break;
            }

            painter.drawText(viewport.left() + (x+2) * xSector + (xSector / 8), viewport.top() + (y + 3) * ySector - (ySector / 3), QString::number(y * 8 + (7-x)));
            painter.setPen(QPen(Qt::gray));
        }
    }
    upperLeft.setX(viewport.left() + 2 * xSector);
    upperLeft.setY(viewport.top() + 2 * ySector);
    gridSize.setX(xSector);
    gridSize.setY(ySector);
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
