#ifndef UDSSCANWINDOW_H
#define UDSSCANWINDOW_H

#include <QDialog>

namespace Ui {
class UDSScanWindow;
}

class UDSScanWindow : public QDialog
{
    Q_OBJECT

public:
    explicit UDSScanWindow(QWidget *parent = 0);
    ~UDSScanWindow();

private:
    Ui::UDSScanWindow *ui;
};

#endif // UDSSCANWINDOW_H
