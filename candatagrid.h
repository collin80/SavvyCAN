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
 */

enum GridTextState
{
    NORMAL,
    BOLD_BLUE,
    INVERT
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
    GridTextState geCellTextState(int x, int y);

protected:
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

signals:
    void gridClicked(int x,int y);

private:
    Ui::CANDataGrid *ui;
    unsigned char refData[8];
    unsigned char data[8];
    unsigned char usedData[8];
    GridTextState textStates[8][8];
    QPoint upperLeft, gridSize;
};

#endif // CANDATAGRID_H
