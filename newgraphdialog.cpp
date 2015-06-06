#include "newgraphdialog.h"
#include "ui_newgraphdialog.h"
#include <QColorDialog>
#include "utility.h"

NewGraphDialog::NewGraphDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewGraphDialog)
{
    ui->setupUi(this);

    connect(ui->colorSwatch, SIGNAL(clicked(bool)), this, SLOT(colorSwatchClick()));
    connect(ui->btnAddGraph, SIGNAL(clicked(bool)), this, SLOT(addButtonClicked()));

    // Seed the random generator with current time
    QTime time = QTime::currentTime();
    qsrand((uint)time.msec());

    QPalette p = ui->colorSwatch->palette();
    p.setColor(QPalette::Button, QColor(qrand() % 255,qrand() % 255,qrand() % 255));
    ui->colorSwatch->setPalette(p);

}

NewGraphDialog::~NewGraphDialog()
{
    delete ui;
}

void NewGraphDialog::addButtonClicked()
{
    accept();
}

void NewGraphDialog::colorSwatchClick()
{
    QColor newColor = QColorDialog::getColor(ui->colorSwatch->palette().button().color());

    QPalette p = ui->colorSwatch->palette();
    p.setColor(QPalette::Button, newColor);
    ui->colorSwatch->setPalette(p);

}

void NewGraphDialog::setParams(GraphParams &params)
{
    ui->txtID->setText("0x" + QString::number(params.ID, 16).toUpper());
    ui->txtBias->setText(QString::number(params.bias));
    ui->txtMask->setText("0x" + QString::number(params.mask, 16).toUpper());
    ui->txtScale->setText(QString::number(params.scale));
    ui->txtStride->setText(QString::number(params.stride));
    ui->cbSigned->setChecked(params.isSigned);

    if (params.endByte > -1)
    {
        ui->txtData->setText(QString::number(params.startByte) + "-" + QString::number(params.endByte));
    }
    else
    {
        ui->txtData->setText(QString::number(params.startByte));
    }

    QPalette p = ui->colorSwatch->palette();
    p.setColor(QPalette::Button, params.color);
    ui->colorSwatch->setPalette(p);
}

void NewGraphDialog::getParams(GraphParams &params)
{
    params.ID = Utility::ParseStringToNum(ui->txtID->text());
    params.bias = ui->txtBias->text().toFloat();
    params.color = ui->colorSwatch->palette().button().color();
    params.isSigned = ui->cbSigned->isChecked();
    params.mask = Utility::ParseStringToNum(ui->txtMask->text());
    params.scale = ui->txtScale->text().toFloat();
    params.stride = Utility::ParseStringToNum(ui->txtStride->text());

    QStringList values = ui->txtData->text().split('-');
    params.startByte = -1;
    params.endByte = -1;
    if (values.count() > 0)
    {
        params.startByte = values[0].toInt();
        if (values.count() > 1)
        {
            params.endByte = values[1].toInt();
        }
    }

    //now catch stupidity and bring it to defaults
    if (params.mask == 0) params.mask = 0xFFFFFFFF;
    if (fabs(params.scale) < 0.0001f) params.scale = 1.0f;
    if (params.stride < 1) params.stride = 1;

}
