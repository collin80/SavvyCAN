#ifndef DELTAWINDOW_H
#define DELTAWINDOW_H

#include <QDialog>

namespace Ui {
class DeltaWindow;
}

class DeltaWindow : public QDialog
{
    Q_OBJECT

public:
    explicit DeltaWindow(QWidget *parent = 0);
    ~DeltaWindow();

private:
    Ui::DeltaWindow *ui;
};

#endif // DELTAWINDOW_H
