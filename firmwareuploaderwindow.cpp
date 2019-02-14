#include "firmwareuploaderwindow.h"
#include "ui_firmwareuploaderwindow.h"
#include "mainwindow.h"

#include <QFile>

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
    qDebug() << "FUW: Got targetted frame with id " << frame.ID;
    if (frame.ID == (uint32_t)(baseAddress + 0x10)) {
        qDebug() << "Start firmware reply";
        if ((frame.data[0] == 0xAD) && (frame.data[1] == 0xDE))
        {
            if ((frame.data[2] == 0xAF) && (frame.data[3] == 0xDE))
            {
                qDebug() << "THere's dead beef here";
                if ((frame.data[4] == (token & 0xFF)) && (frame.data[5] == ((token >> 8) & 0xFF)))
                {
                    if ((frame.data[6] == ((token >> 16) & 0xFF)) && (frame.data[7] == ((token >> 24) & 0xFF)))
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

    if (frame.ID == (uint32_t)(baseAddress + 0x20)) {
        qDebug() << "Firmware reception success reply";
        int seq = frame.data[0] + (256 * frame.data[1]);
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
    output->extended = false;
    output->len = 7;
    output->bus = bus;
    output->ID = baseAddress + 0x16;
    output->data[0] = currentSendingPosition & 0xFF;
    output->data[1] = (currentSendingPosition >> 8) & 0xFF;
    output->data[2] = firmwareData[firmwareLocation++];
    output->data[3] = firmwareData[firmwareLocation++];
    output->data[4] = firmwareData[firmwareLocation++];
    output->data[5] = firmwareData[firmwareLocation++];
    for (int i = 0; i < 6; i++) xorByte = xorByte ^ output->data[i];
    output->data[6] = xorByte;
    sendCANFrame(output);
    timer->start();
}

void FirmwareUploaderWindow::sendFirmwareEnding()
{
    CANFrame *output = new CANFrame;
    output->extended = false;
    output->bus = bus;
    output->len = 4;
    output->ID = baseAddress + 0x30;
    output->data[3] = 0xC0;
    output->data[2] = 0xDE;
    output->data[1] = 0xFA;
    output->data[0] = 0xDE;
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
        output->extended = false;
        output->len = 8;
        output->bus = bus;
        output->ID = baseAddress;

        output->data[0] = 0xEF;
        output->data[1] = 0xBE;
        output->data[2] = 0xAD;
        output->data[3] = 0xDE;
        output->data[4] = token & 0xFF;
        output->data[5] = (token >> 8) & 0xFF;
        output->data[6] = (token >> 16) & 0xFF;
        output->data[7] = (token >> 24) & 0xFF;
        sendCANFrame(output);
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

