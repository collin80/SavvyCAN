#include "udsscanwindow.h"
#include "ui_udsscanwindow.h"
#include "mainwindow.h"

UDSScanWindow::UDSScanWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UDSScanWindow)
{
    ui->setupUi(this);

    modelFrames = frames;

    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    connect(ui->btnScan, &QPushButton::clicked, this, &UDSScanWindow::scanUDS);

    ui->cbBuses->addItem("0");
    ui->cbBuses->addItem("1");
    ui->cbBuses->addItem("Both");

}

UDSScanWindow::~UDSScanWindow()
{
    delete ui;
}

void UDSScanWindow::scanUDS()
{
    ui->listResults->clear();

    CANFrame *frame;

    int buses = ui->cbBuses->currentIndex();
    buses++;
    if (buses < 1) buses = 1;
    for (int id = 0x7E0; id < 0x7E8; id++)
    {
        if (buses & 1)
        {
            frame = new CANFrame;
            frame->ID = id;
            frame->len = 8;
            frame->extended = false;
            frame->data[0] = 2;
            frame->data[1] = 0x10;
            frame->data[2] = 1;
            frame->data[3] = 0;frame->data[4] = 0;frame->data[5] = 0;
            frame->data[6] = 0;frame->data[7] = 0;
            frame->bus = 0;
            emit sendCANFrame(frame, 0);
        }
        if (buses & 2)
        {
            frame = new CANFrame;
            frame->ID = id;
            frame->len = 8;
            frame->extended = false;
            frame->data[0] = 2;
            frame->data[1] = 0x10;
            frame->data[2] = 1;
            frame->data[3] = 0;frame->data[4] = 0;frame->data[5] = 0;
            frame->data[6] = 0;frame->data[7] = 0;
            frame->bus = 1;
            emit sendCANFrame(frame, 1);
        }
    }
}

void UDSScanWindow::updatedFrames(int numFrames)
{
    CANFrame thisFrame;
    QString result;
    int id;
    if (numFrames == -1) //all frames deleted. We don't care
    {
    }
    else if (numFrames == -2) //all new set of frames. Don't care.
    {
    }
    else //just got some new frames. See if they are relevant.
    {
        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);
            id = thisFrame.ID;
            if (id >= 0x7E8 && id <= 0x7EF)
            {
                if (thisFrame.data[1] == 0x50)
                {
                    result = "ECU at 0x" + QString::number(id, 16) + " bus " + QString::number(thisFrame.bus) + " accepts UDS standard diag";
                    ui->listResults->addItem(result);
                }
                if (thisFrame.data[i] == 0x7f)
                {
                    result = "ECU at 0x" + QString::number(id, 16) + " bus " + QString::number(thisFrame.bus) + " rejects UDS standard diag";
                    ui->listResults->addItem(result);
                }
            }
        }
    }
}
