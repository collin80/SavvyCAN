#include "graphingwindow.h"
#include "ui_graphingwindow.h"

GraphingWindow::GraphingWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GraphingWindow)
{
    ui->setupUi(this);
}

GraphingWindow::~GraphingWindow()
{
    delete ui;
}
