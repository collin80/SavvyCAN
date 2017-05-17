#include "udsscanwindow.h"
#include "ui_udsscanwindow.h"
#include "mainwindow.h"
#include "connections/canconmanager.h"
#include "bus_protocols/uds_handler.h"

UDSScanWindow::UDSScanWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UDSScanWindow)
{
    ui->setupUi(this);

    modelFrames = frames;

    currentlyRunning = false;

    waitTimer = new QTimer;
    waitTimer->setInterval(100);

    UDS_HANDLER::getInstance()->setReception(true);

    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    connect(UDS_HANDLER::getInstance(), &UDS_HANDLER::newUDSMessage, this, &UDSScanWindow::gotUDSReply);
    connect(ui->btnScan, &QPushButton::clicked, this, &UDSScanWindow::scanUDS);
    connect(waitTimer, &QTimer::timeout, this, &UDSScanWindow::timeOut);
    connect(ui->btnSaveResults, &QPushButton::clicked, this, &UDSScanWindow::saveResults);

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

void UDSScanWindow::saveResults()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("Text File (*.txt)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        if (!filename.contains('.')) filename += ".txt";
        if (dialog.selectedNameFilter() == filters[0])
        {
            QFile *outFile = new QFile(filename);

            if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
            {
                delete outFile;
                return;
            }

            for (int i = 0; i < ui->listResults->count(); i++)
            {
                outFile->write(ui->listResults->item(i)->text().toUtf8());
                outFile->write("\n");
            }
            outFile->close();
            delete outFile;
        }
    }
}

void UDSScanWindow::sendOnBuses(UDS_MESSAGE test, int buses)
{
    int busList = buses;
    if (busList < ui->cbBuses->count() - 1)
    {
        test.bus = buses;
        sendingFrames.append(test);
    }
    else
    {
        for (int c = 0; c < ui->cbBuses->count() - 1; c++)
        {
            test.bus = c;
            sendingFrames.append(test);
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
        return;
    }

    waitTimer->setInterval(ui->spinDelay->value());

    ui->listResults->clear();
    sendingFrames.clear();

    UDS_MESSAGE test;
    int typ, id;
    int startID, endID;
    startID = Utility::ParseStringToNum(ui->txtStartID->text());
    endID = Utility::ParseStringToNum(ui->txtEndID->text());

    int buses = ui->cbBuses->currentIndex();

    for (id = startID; id <= endID; id++)
    {
        test.ID = id;

        if (ui->ckTester->isChecked())
        {
            test.service = UDS_SERVICES::TESTER_PRESENT;
            test.subFunc = 0;
            sendOnBuses(test, buses);
        }
        if (ui->ckSession->isChecked())
        {
            for (typ = 1; typ < 4; typ++) //try each type of session access
            {
                test.service = UDS_SERVICES::DIAG_CONTROL;
                test.subFunc = typ;
                sendOnBuses(test, buses);
            }
        }
        if (ui->ckReset->isChecked()) //try to command a reset of the ECU. You're likely to know if it works. ;)
        {
            test.service = UDS_SERVICES::ECU_RESET;
            test.subFunc = 1;
            sendOnBuses(test, buses);
        }
        if (ui->ckSecurity->isChecked()) //try to enter security mode - very likely to get a response if an ECU exists.
        {
            test.service = UDS_SERVICES::SECURITY_ACCESS;
            test.subFunc = 1;
            sendOnBuses(test, buses);
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

void UDSScanWindow::gotUDSReply(UDS_MESSAGE &msg)
{
    QString result;
    uint32_t id;
    int offset = ui->spinReplyOffset->value();
    UDS_MESSAGE sentFrame;
    bool gotReply = false;

    int numSending = sendingFrames.length();
    if (numSending == 0) return;
    if (currIdx >= numSending) return;
    sentFrame = sendingFrames[currIdx];

    id = msg.ID;

    if ((id == (uint32_t)(sentFrame.ID + offset)) || ui->cbAllowAdaptiveOffset->isChecked())
    {
        //int temp = thisFrame.data[0] >> 4;
        //if (temp == 0) //single frame reply (maybe)
        //{
            if (msg.service == 0x40 + sendingFrames[currIdx].service)
            {
                result = "Request on bus " + QString::number(sentFrame.bus) + " ID: " + QString::number(sentFrame.ID, 16) + " got response to mode "
                    + QString::number(sentFrame.service, 16)
                    + " " + QString::number(sentFrame.subFunc, 16) + " with affirmation from ID " + QString::number(id, 16)
                    + " on bus " + QString::number(msg.bus) + ".";
                gotReply = true;
            }
            else if ( msg.service == 0x7F)
            {
                result = "Request on bus " + QString::number(sentFrame.bus) + " ID: " + QString::number(sentFrame.ID, 16) + " got response to mode "
                    + QString::number(sentFrame.service, 16)
                    + " " + QString::number(sentFrame.subFunc, 16) + " with an error from ID " + QString::number(id, 16)
                    + " on bus " + QString::number(msg.bus) + ".";
                gotReply = true;
            }
            //}
            /*
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
            } */

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
             + QString::number(sendingFrames[currIdx].service, 16) + " " + QString::number(sendingFrames[currIdx].subFunc, 16);
    ui->listResults->addItem(result);

    sendNextMsg();
}

void UDSScanWindow::sendNextMsg()
{
    QVector<unsigned char> data;

    currIdx++;
    if (currIdx < sendingFrames.count())
    {
        data.clear();
        data.append(sendingFrames[currIdx].subFunc);
        UDS_HANDLER::getInstance()->sendUDSFrame(sendingFrames[currIdx].bus, sendingFrames[currIdx].ID, sendingFrames[currIdx].service, data);
        waitTimer->start();
    }
    else
    {
        waitTimer->stop();
        ui->btnScan->setText("Start Scan");
        currentlyRunning = false;
    }
}
