#ifndef CANDATAGRID_H
#define CANDATAGRID_H

#include <QWidget>

namespace Ui {
class CANDataGrid;
}

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

protected:
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

signals:
    void gridClicked(int x,int y);

private:
    Ui::CANDataGrid *ui;
    unsigned char refData[8];
    unsigned char data[8];
    unsigned char usedData[8];
    QPoint upperLeft, gridSize;
};

#endif // CANDATAGRID_H
