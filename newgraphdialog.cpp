#include "newgraphdialog.h"
#include "ui_newgraphdialog.h"

NewGraphDialog::NewGraphDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewGraphDialog)
{
    ui->setupUi(this);
}

NewGraphDialog::~NewGraphDialog()
{
    delete ui;
}
