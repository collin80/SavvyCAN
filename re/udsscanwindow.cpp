#include "udsscanwindow.h"
#include "ui_udsscanwindow.h"
#include "mainwindow.h"
#include "connections/canconmanager.h"

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
    connect(CANConManager::getInstance(), &CANConManager::framesReceived, this, &UDSScanWindow::rapidFrames);
    connect(ui->btnScan, &QPushButton::clicked, this, &UDSScanWindow::scanUDS);
    connect(waitTimer, &QTimer::timeout, this, &UDSScanWindow::timeOut);

    int numBuses = CANConManager::getInstance()->getNumBuses();
    for (int n = 0; n < numBuses; n++) ui->cbBuses->addItem(QString::number(n));
    ui->cbBuses->addItem(tr("All"));
}

UDSScanWindow::~UDSScanWindow()
{
    delete ui;
    waitTimer->stop();
    delete waitTimer;
}

void UDSScanWindow::sendOnBuses(CANFrame &frame, int buses)
{
    if (buses < ui->cbBuses->count()- 1)
    {
        frame.bus = buses;
        sendingFrames.append(frame);
    }
    else
    {
        for (int c = 0; c < ui->cbBuses->count() - 1; c++)
        {
            frame.bus = c;
            sendingFrames.append(frame);
        }
    }
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

    waitTimer->setInterval(ui->spinDelay->value());

    ui->listResults->clear();
    sendingFrames.clear();

    CANFrame frame;
    int typ, id;
    int startID, endID;
    startID = Utility::ParseStringToNum(ui->txtStartID->text());
    endID = Utility::ParseStringToNum(ui->txtEndID->text());

    int buses = ui->cbBuses->currentIndex();

    for (id = startID; id <= endID; id++)
    {
        frame.ID = id;
        frame.len = 8;
        frame.extended = false;

        if (ui->ckTester->isChecked())
        {
            frame.data[0] = 2;
            frame.data[1] = 0x3E; //tester present
            frame.data[2] = 0;
            frame.data[3] = 0;frame.data[4] = 0;frame.data[5] = 0;
            frame.data[6] = 0;frame.data[7] = 0;
            sendOnBuses(frame, buses);
        }
        if (ui->ckSession->isChecked())
        {
            for (typ = 1; typ < 4; typ++) //try each type of session access
            {
                frame.data[0] = 2;
                frame.data[1] = 0x10;
                frame.data[2] = typ;
                frame.data[3] = 0;frame.data[4] = 0;frame.data[5] = 0;
                frame.data[6] = 0;frame.data[7] = 0;
                sendOnBuses(frame, buses);
            }
        }
        if (ui->ckReset->isChecked()) //try to command a reset of the ECU. You're likely to know if it works. ;)
        {
            frame.data[0] = 2;
            frame.data[1] = 0x11; //Reset
            frame.data[2] = 1; //hard reset. 2 = key off/on 3 = soft reset
            frame.data[3] = 0;frame.data[4] = 0;frame.data[5] = 0;
            frame.data[6] = 0;frame.data[7] = 0;
            sendOnBuses(frame, buses);
        }
        if (ui->ckSecurity->isChecked()) //try to enter security mode - very likely to get a response if an ECU exists.
        {
            frame.data[0] = 2;
            frame.data[1] = 0x27; //request security mode
            frame.data[2] = 1; //request seed from ECU
            frame.data[3] = 0;frame.data[4] = 0;frame.data[5] = 0;
            frame.data[6] = 0;frame.data[7] = 0;
            sendOnBuses(frame, buses);
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
//TODO: I really doubt this works anymore with the new connection system. This breaks the UDS scanner for now! ;(
void UDSScanWindow::rapidFrames(const CANConnection* conn, const QVector<CANFrame>& pFrames)
{
    QString result;
    uint32_t id;
    int offset = ui->spinReplyOffset->value();
    CANFrame sentFrame;
    bool gotReply = false;

    if (pFrames.length() <= 0) return;

    int numSending = sendingFrames.length();
    if (numSending == 0) return;
    if (currIdx >= numSending) return;
    sentFrame = sendingFrames[currIdx];

    foreach(const CANFrame& thisFrame, pFrames)
    {
        if (currIdx >= numSending) return;
        id = thisFrame.ID;

        if ((id == (uint32_t)(sentFrame.ID + offset)) || ui->cbAllowAdaptiveOffset->isChecked())
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
        CANConManager::getInstance()->sendFrame(sendingFrames[currIdx]);
        waitTimer->start();
    }
    else
    {
        waitTimer->stop();
        ui->btnScan->setText("Start Scan");
        currentlyRunning = false;
    }
}
