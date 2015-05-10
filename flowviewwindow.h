#ifndef FLOWVIEWWINDOW_H
#define FLOWVIEWWINDOW_H

#include <QDialog>

namespace Ui {
class FlowViewWindow;
}

class FlowViewWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FlowViewWindow(QWidget *parent = 0);
    ~FlowViewWindow();

private:
    Ui::FlowViewWindow *ui;
};

#endif // FLOWVIEWWINDOW_H
