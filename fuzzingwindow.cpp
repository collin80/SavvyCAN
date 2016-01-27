#include "fuzzingwindow.h"
#include "ui_fuzzingwindow.h"

FuzzingWindow::FuzzingWindow(const QVector<CANFrame> *frames, SerialWorker *worker, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FuzzingWindow)
{
    ui->setupUi(this);

    modelFrames = frames;
    serialWorker = worker;
}

FuzzingWindow::~FuzzingWindow()
{
    delete ui;
}
