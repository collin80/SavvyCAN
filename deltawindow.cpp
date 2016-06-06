#include "deltawindow.h"
#include "ui_deltawindow.h"

DeltaWindow::DeltaWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DeltaWindow)
{
    ui->setupUi(this);
}

DeltaWindow::~DeltaWindow()
{
    delete ui;
}
