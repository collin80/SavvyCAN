#include "udsscanwindow.h"
#include "ui_udsscanwindow.h"

UDSScanWindow::UDSScanWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UDSScanWindow)
{
    ui->setupUi(this);
}

UDSScanWindow::~UDSScanWindow()
{
    delete ui;
}
