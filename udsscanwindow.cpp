#include "udsscanwindow.h"
#include "ui_udsscanwindow.h"
#include "mainwindow.h"

UDSScanWindow::UDSScanWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UDSScanWindow)
{
    ui->setupUi(this);

    modelFrames = frames;

    waitTimer = new QTimer;
    waitTimer->setInterval(500);

    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    connect(ui->btnScan, &QPushButton::clicked, this, &UDSScanWindow::scanUDS);
    connect(waitTimer, &QTimer::timeout, this, &UDSScanWindow::timeOut);

    ui->cbBuses->addItem("0");
    ui->cbBuses->addItem("1");
    ui->cbBuses->addItem("Both");

}

UDSScanWindow::~UDSScanWindow()
{
    delete ui;
    waitTimer->stop();
    delete waitTimer;
}

void UDSScanWindow::scanUDS()
{
    ui->listResults->clear();
    sendingFrames.clear();

    CANFrame frame;

    int buses = ui->cbBuses->currentIndex();
    buses++;
    if (buses < 1) buses = 1;
    for (int typ = 1; typ < 5; typ++)
    {
        for (int id = 0x7E0; id < 0x7E8; id++)
        {
            frame.ID = id;
            frame.len = 8;
            frame.extended = false;
            frame.data[0] = 2;
            frame.data[1] = 0x10;
            frame.data[2] = typ;
            frame.data[3] = 0;frame.data[4] = 0;frame.data[5] = 0;
            frame.data[6] = 0;frame.data[7] = 0;

            if (buses & 1)
            {
                frame.bus = 0;
                sendingFrames.append(frame);
            }
            if (buses & 2)
            {
                frame.bus = 1;
                sendingFrames.append(frame);
            }
        }
    }

    waitTimer->start();
    currIdx = -1;
    sendNextMsg();
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
        if (numFrames > modelFrames->count()) return;
        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);
            id = thisFrame.ID;
            if (id >= 0x7E8 && id <= 0x7EF)
            {
                id -= 8; //back to original ECU id
                result = "ECU at bus " + QString::number(thisFrame.bus) + " ID: " + QString::number(id, 16) + " responds to mode "
                        + QString::number(sendingFrames[currIdx].data[2]) + " with: " + QString::number(thisFrame.data[0], 16) + " "
                        + QString::number(thisFrame.data[1], 16) + " " + QString::number(thisFrame.data[2], 16)
                        + " " + QString::number(thisFrame.data[3], 16);
                ui->listResults->addItem(result);
                sendNextMsg();
            }
        }
    }
}

void UDSScanWindow::timeOut()
{
    QString result;
    result = "ECU at bus " + QString::number(sendingFrames[currIdx].bus) + " ID: " + QString::number(sendingFrames[currIdx].ID, 16) + " did not respond to mode "
            + QString::number(sendingFrames[currIdx].data[2]);
    ui->listResults->addItem(result);

    sendNextMsg();
}

void UDSScanWindow::sendNextMsg()
{
    currIdx++;
    if (currIdx < sendingFrames.count())
    {
        emit sendCANFrame(&sendingFrames[currIdx], sendingFrames[currIdx].bus);
    }
    else
    {
        waitTimer->stop();
    }
}
