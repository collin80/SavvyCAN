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

private:
    Ui::CANDataGrid *ui;
    unsigned char refData[8];
    unsigned char data[8];
};

#endif // CANDATAGRID_H
