#include "firmwareuploaderwindow.h"
#include "ui_firmwareuploaderwindow.h"
#include "mainwindow.h"

#include <QFile>

//You might wonder: Collin, what in the hell is this for? Firmware uploader? For what? I'm interested! Well, it's a custom
//firmware uploader for a motor controller I built. Why would that be in this project. Cuz. It's not really relevant
//to anyone else but might serve as a decent reference for a few things: How to make an uploader interface that runs over CAN,
//how to lay out a screen like this, how to make a comm protocol for firmware updating over CAN. But, most things use UDS
//for firmware updates and wouldn't need this specific code. But, it might be able to be turned into a UDS firmware uploader or downloader.
//Note that this screen is specifically hidden by default because of it's oddball status. You have to re-enable it in mainwindow.cpp to see it.

FirmwareUploaderWindow::FirmwareUploaderWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FirmwareUploaderWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    transferInProgress = false;
    startedProcess = false;
    firmwareSize = 0;
    currentSendingPosition = 0;
    baseAddress = 0;
    bus = 0;
    modelFrames = frames;

    ui->lblFilename->setText("");
    ui->spinBus->setValue(0);
    ui->spinBus->setMaximum(1);
    ui->txtBaseAddr->setText("0x100");
    updateProgress();

    timer = new QTimer();
    timer->setInterval(100); //100ms without a reply will cause us to attempt a resend

    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    connect(ui->btnLoadFile, SIGNAL(clicked(bool)), this, SLOT(handleLoadFile()));
    connect(ui->btnStartStop, SIGNAL(clicked(bool)), this, SLOT(handleStartStopTransfer()));
    connect(timer, SIGNAL(timeout()), this, SLOT(timerElapsed()));
}

FirmwareUploaderWindow::~FirmwareUploaderWindow()
{
    timer->stop();
    CANConManager::getInstance()->removeAllTargettedFrames(this);
    delete timer;
    delete ui;
}

void FirmwareUploaderWindow::updateProgress()
{
    ui->lblProgress->setText(QString::number(currentSendingPosition * 4) + " of " + QString::number(firmwareSize) + " transferred");
}

void FirmwareUploaderWindow::updatedFrames(int numFrames)
{
    //CANFrame thisFrame;
    if (numFrames == -1) //all frames deleted.
    {
    }
    else if (numFrames == -2) //all new set of frames.
    {
    }
    else //just got some new frames. See if they are relevant.
    {
        /*
        //run through the supposedly new frames in order
        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);
        }
        */
    }
}

void FirmwareUploaderWindow::gotTargettedFrame(CANFrame frame)
{
    const unsigned char *data = reinterpret_cast<const unsigned char *>(frame.payload().constData());
    int dataLen = frame.payload().count();

    qDebug() << "FUW: Got targetted frame with id " << frame.frameId();
    if (frame.frameId() == (uint32_t)(baseAddress + 0x10) && (dataLen == 8) ) {
        qDebug() << "Start firmware reply";
        if ((data[0] == 0xAD) && (data[1] == 0xDE))
        {
            if ((data[2] == 0xAF) && (data[3] == 0xDE))
            {
                qDebug() << "There's dead beef here";
                if ( (data[4] == (token & 0xFF)) && (data[5] == ((token >> 8) & 0xFF) ) )
                {
                    if ((data[6] == ((token >> 16) & 0xFF)) && (data[7] == ((token >> 24) & 0xFF)))
                    {
                        qDebug() << "starting firmware process";
                        //MainWindow::getReference()->setTargettedID(baseAddress + 0x20);
                        transferInProgress = true;
                        sendFirmwareChunk();
                    }
                }
            }
        }
    }

    if (frame.frameId() == (uint32_t)(baseAddress + 0x20)) {
        qDebug() << "Firmware reception success reply";
        int seq = data[0] + (256 * data[1]);
        if (seq == currentSendingPosition)
        {
            currentSendingPosition++;
            if (currentSendingPosition * 4 > firmwareSize || currentSendingPosition > 65535)
            {
                transferInProgress = false;
                timer->stop();
                handleStartStopTransfer();
                ui->progressBar->setValue(100);
                sendFirmwareEnding();
            }
            else
            {
                ui->progressBar->setValue((400 * currentSendingPosition) / firmwareSize);
                updateProgress();
                sendFirmwareChunk();
            }
        }
    }
}

void FirmwareUploaderWindow::timerElapsed()
{
    sendFirmwareChunk(); //resend
}

void FirmwareUploaderWindow::sendFirmwareChunk()
{
    CANFrame *output = new CANFrame;
    int firmwareLocation = currentSendingPosition * 4;
    int xorByte = 0;
    output->setExtendedFrameFormat(false);
    QByteArray bytes(7,0);
    output->bus = bus;
    output->setFrameId(baseAddress + 0x16);
    output->payload()[0] = currentSendingPosition & 0xFF;
    output->payload()[1] = (currentSendingPosition >> 8) & 0xFF;
    output->payload()[2] = firmwareData[firmwareLocation++];
    output->payload()[3] = firmwareData[firmwareLocation++];
    output->payload()[4] = firmwareData[firmwareLocation++];
    output->payload()[5] = firmwareData[firmwareLocation++];
    for (int i = 0; i < 6; i++) xorByte = xorByte ^ static_cast<unsigned char>(output->payload()[i]);
    output->payload()[6] = xorByte;
    output->setPayload(bytes);
    CANConManager::getInstance()->sendFrame(*output);
    timer->start();
}

void FirmwareUploaderWindow::sendFirmwareEnding()
{
    CANFrame *output = new CANFrame;
    output->setExtendedFrameFormat(false);
    output->bus = bus;
    QByteArray bytes(4,0);
    output->setFrameId(baseAddress + 0x30);
    output->payload()[3] = 0xC0;
    output->payload()[2] = 0xDE;
    output->payload()[1] = 0xFA;
    output->payload()[0] = 0xDE;
    output->setPayload(bytes);
    //sendCANFrame(output, bus);
}

void FirmwareUploaderWindow::handleStartStopTransfer()
{
    startedProcess = !startedProcess;

    if (startedProcess) //start the process
    {
        ui->progressBar->setValue(0);
        ui->btnStartStop->setText("Stop Upload");
        token = Utility::ParseStringToNum(ui->txtToken->text());
        bus = ui->spinBus->value();
        baseAddress = Utility::ParseStringToNum(ui->txtBaseAddr->text());
        qDebug() << "Base address: " + QString::number(baseAddress);
        CANConManager::getInstance()->addTargettedFrame(bus, baseAddress + 0x10, 0x7FF, this);
        CANConManager::getInstance()->addTargettedFrame(bus, baseAddress + 0x20, 0x7FF, this);
        CANFrame *output = new CANFrame;
        output->setExtendedFrameFormat(false);
        QByteArray bytes(8,0);
        output->bus = bus;
        output->setFrameId(baseAddress);
        output->setFrameType(QCanBusFrame::DataFrame);

        bytes[0] = 0xEF;
        bytes[1] = 0xBE;
        bytes[2] = 0xAD;
        bytes[3] = 0xDE;
        bytes[4] = token & 0xFF;
        bytes[5] = (token >> 8) & 0xFF;
        bytes[6] = (token >> 16) & 0xFF;
        bytes[7] = (token >> 24) & 0xFF;
        output->setPayload(bytes);
        CANConManager::getInstance()->sendFrame(*output);
    }
    else //stop anything in process
    {
        ui->btnStartStop->setText("Start Upload");
        CANConManager::getInstance()->removeAllTargettedFrames(this);
    }
}

void FirmwareUploaderWindow::handleLoadFile()
{
    QString filename;
    QFileDialog dialog;

    QStringList filters;
    filters.append(QString(tr("Raw firmware binary (*.bin)")));

    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];

        loadBinaryFile(filename);
    }
}

void FirmwareUploaderWindow::loadBinaryFile(QString filename)
{

    if (transferInProgress) handleStartStopTransfer();

    QFile *inFile = new QFile(filename);

    if (!inFile->open(QIODevice::ReadOnly))
    {
        delete inFile;
        return;
    }

    firmwareData = inFile->readAll();

    currentSendingPosition = 0;
    firmwareSize = firmwareData.length();

    updateProgress();

    inFile->close();
    delete inFile;
}

