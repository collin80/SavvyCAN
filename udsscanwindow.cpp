#include "udsscanwindow.h"
#include "ui_udsscanwindow.h"
#include "mainwindow.h"

UDSScanWindow::UDSScanWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UDSScanWindow)
{
    ui->setupUi(this);

    modelFrames = frames;

    currentlyRunning = false;

    waitTimer = new QTimer;
    waitTimer->setInterval(100);

    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    connect(MainWindow::getReference(), SIGNAL(frameUpdateRapid(int)), this, SLOT(rapidFrames(int)));
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
    if (currentlyRunning)
    {
        waitTimer->stop();
        sendingFrames.clear();
        currentlyRunning = false;
        ui->btnScan->setText("Start Scan");
    }

    ui->listResults->clear();
    sendingFrames.clear();

    CANFrame frame;
    int typ, id;
    int startID, endID;
    startID = Utility::ParseStringToNum(ui->txtStartID->text());
    endID = Utility::ParseStringToNum(ui->txtEndID->text());

    int buses = ui->cbBuses->currentIndex();
    buses++;
    if (buses < 1) buses = 1;

    //start out by sending tester present to every address to see if anyone replies
    for (id = startID; id <= endID; id++)
    {
        frame.ID = id;
        frame.len = 8;
        frame.extended = false;
        frame.data[0] = 2;
        frame.data[1] = 0x3E; //tester present
        frame.data[2] = 0;
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

    //then try asking for the various diagnostic session types
    for (typ = 1; typ < 5; typ++)
    {
        for (id = startID; id <= endID; id++)
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
    currentlyRunning = true;
    ui->btnScan->setText("Abort Scan");
    sendNextMsg();
}

//Updates here are sent about every 1/4 second. That's fine for most windows but not this one.
void UDSScanWindow::updatedFrames(int numFrames)
{
    if (numFrames == -1) //all frames deleted. We don't care
    {
    }
    else if (numFrames == -2) //all new set of frames. Don't care.
    {
    }
    else //just got some new frames. See if they are relevant.
    {
    }
}

//Updates here are nearly once per millisecond if there is heavy traffic. That's more like it!
void UDSScanWindow::rapidFrames(int numFrames)
{
    CANFrame thisFrame;
    QString result;
    int id;
    int offset = ui->spinReplyOffset->value();
    CANFrame sentFrame;
    bool gotReply = false;

    if (numFrames > modelFrames->count()) return;

    int numSending = sendingFrames.length();
    if (numSending == 0) return;
    if (currIdx >= numSending) return;
    sentFrame = sendingFrames[currIdx];

    for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
    {
        if (currIdx >= numSending) return;
        thisFrame = modelFrames->at(i);
        id = thisFrame.ID;

        if ((id == (sentFrame.ID + offset)) || ui->cbAllowAdaptiveOffset->isChecked())
        {
            int temp = thisFrame.data[0] >> 4;
            if (temp == 0) //single frame reply (maybe)
            {
                if (thisFrame.data[1] == 0x40 + sendingFrames[currIdx].data[1])
                {
                    result = "Request on bus " + QString::number(sentFrame.bus) + " ID: " + QString::number(sentFrame.ID, 16) + " got response to mode "
                        + QString::number(sentFrame.data[1], 16)
                        + " " + QString::number(sentFrame.data[2], 16) + " with affirmation from ID " + QString::number(id, 16)
                        + " on bus " + QString::number(thisFrame.bus) + ".";
                    gotReply = true;
                }
                else if ( thisFrame.data[1] == 0x7F)
                {
                    result = "Request on bus " + QString::number(sentFrame.bus) + " ID: " + QString::number(sentFrame.ID, 16) + " got response to mode "
                        + QString::number(sentFrame.data[1], 16)
                        + " " + QString::number(sentFrame.data[2], 16) + " with an error from ID " + QString::number(id, 16)
                        + " on bus " + QString::number(thisFrame.bus) + ".";
                    gotReply = true;
                }
            }

            if (temp == 1) //start of a multiframe reply
            {
                if (thisFrame.data[2] == 0x40 + sendingFrames[currIdx].data[1])
                {
                    result = "Request on bus " + QString::number(sentFrame.bus) + " ID: " + QString::number(sentFrame.ID, 16) + " got response to mode "
                        + QString::number(sentFrame.data[1], 16)
                        + " " + QString::number(sentFrame.data[2], 16) + " with affirmation from ID " + QString::number(id, 16)
                        + " on bus " + QString::number(thisFrame.bus) + ".";
                    gotReply = true;
                }
                //error replies are never multiframe so the check doesn't have to be done here.
            }
        }
    }
    if (gotReply)
    {
        ui->listResults->addItem(result);
        sendNextMsg();
    }
}

void UDSScanWindow::timeOut()
{
    QString result;
    result = "Request on bus " + QString::number(sendingFrames[currIdx].bus) + " ID: " + QString::number(sendingFrames[currIdx].ID, 16) + " got no response to mode "
             + QString::number(sendingFrames[currIdx].data[1], 16) + " " + QString::number(sendingFrames[currIdx].data[2], 16);
    ui->listResults->addItem(result);

    sendNextMsg();
}

void UDSScanWindow::sendNextMsg()
{
    currIdx++;
    if (currIdx < sendingFrames.count())
    {
        emit sendCANFrame(&sendingFrames[currIdx], sendingFrames[currIdx].bus);
        waitTimer->start();
    }
    else
    {
        waitTimer->stop();
        ui->btnScan->setText("Start Scan");
        currentlyRunning = false;
    }
}
