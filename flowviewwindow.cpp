#include "flowviewwindow.h"
#include "ui_flowviewwindow.h"

FlowViewWindow::FlowViewWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FlowViewWindow)
{
    ui->setupUi(this);
}

FlowViewWindow::~FlowViewWindow()
{
    delete ui;
}
