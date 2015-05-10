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

private:
    Ui::CANDataGrid *ui;
};

#endif // CANDATAGRID_H
