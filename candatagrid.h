#ifndef CANDATAGRID_H
#define CANDATAGRID_H

#include <QWidget>

namespace Ui {
class CANDataGrid;
}

/*
 * CANDataGrid is a custom QT widget that proves that it's possible to shoehorn 12x too much crap into one widget. The purported
 * goal of the widget was to show an 8x8 grid of bits that show whether each bit in a given message is set or not. It allows one to quickly
 * visualize the bits within a CAN frame's data bytes. From there it evolved to also show whether a given bit was freshly set recently, freshly
 * unset recently, or has stayed set or unset for a while. It can also gray out bytes that don't exist in the current CAN frame.
 * This is handled by the calling code.
 *
 * After that functionality was added so it could also emit a signal when it is clicked. The signal will tell you which "bit" cell was clicked.
 *
 * Then, support was added for modifying the text color of each cell. This can be used for whatever the calling code wants. An example is
 * FlowView that uses all of the above functionality plus this functionality in order to show which bits are set as triggers for stopping
 * the flowview playback.
 *
 * Now the control also tracks which signal is using which bit. The graphical representation does exist now but some tweaking is probably
 * still needed. Also, it seems like it is necessary to allow for a variety of modes.
 */

enum GridTextState
{
    NORMAL,
    BOLD_BLUE,
    INVERT
};

enum GridMode
{
    CHANGED_BITS,
    SIGNAL_VIEW
};

class CANDataGrid : public QWidget
{
    Q_OBJECT

public:
    explicit CANDataGrid(QWidget *parent = 0);
    ~CANDataGrid();
    void paintEvent(QPaintEvent *event);
    void setReference(unsigned char *, bool);
    void updateData(unsigned char *, bool);
    void setUsed(unsigned char *, bool);
    void saveImage(QString filename, int width, int height);
    void setCellTextState(int x, int y, GridTextState state);
    GridTextState getCellTextState(int x, int y);
    void setUsedSignalNum(int bit, int signal);
    void setSignalNames(int sigIdx, const QString sigName);
    void clearSignalNames();
    int getUsedSignalNum(int bit);
    GridMode getMode();
    void setMode(GridMode mode);

protected:
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

signals:
    void gridClicked(int x,int y);

private:
    Ui::CANDataGrid *ui;
    unsigned char refData[8];
    unsigned char data[8];
    unsigned char usedData[8];
    int usedSignalNum[64]; //so we can specify which signal claims this bit
    QVector<QString> signalNames;
    QVector<QColor> signalColors;
    GridTextState textStates[8][8];
    QPoint upperLeft, gridSize;
    GridMode gridMode;
    QBrush blackBrush, whiteBrush, redBrush, greenBrush, grayBrush;
    QBrush greenHashBrush, blackHashBrush;
    QPainter *painter;
    QRect viewport;
    int xSpan;
    int ySpan;
    int xSector;
    int ySector;
    double bigTextSize, smallTextSize, sigNameTextSize;
    QFont mainFont;
    QFont smallFont;
    QFont boldFont;
    QFont sigNameFont;
    QFontMetrics *smallMetric;

    void paintChangedBits();
    void paintSignalView();
    void paintCommonBeginning();
    void paintCommonEnding();
};

#endif // CANDATAGRID_H
