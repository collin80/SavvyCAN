#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "can_structs.h"
#include <QDateTime>
#include <QFileDialog>
#include <QtSerialPort/QSerialPortInfo>
#include "canframemodel.h"
#include "utility.h"
#include "serialworker.h"

/*
This first order of business is to attempt to gain feature parity (roughly) with GVRET-PC so  that this
can replace it as a cross platform solution.

Here are things yet to do
- Ability to use programmatic frame sending interface (only I use it and it's complicated but helpful)

Things that were planned for the GVRET-PC project but never completed.
Single / Multi state - The goal is to find bits that change based on toggles or discrete state items (shifters, etc)
Range state - Find things that range like accelerator pedal inputs, road speed, tach, etc
fuzzy scope - Try to find potential places where a given value might be stored - offer guesses and the program tries to find candidates for you


Things currently broken or in need of attention:
1. Currently no screen supports dynamic updates when new frames come in.
*/

QString MainWindow::loadedFileName = "";
MainWindow *MainWindow::selfRef = NULL;

MainWindow *MainWindow::getReference()
{
    return selfRef;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    selfRef = this;

    this->setWindowTitle("Savvy CAN V" + QString::number(VERSION));

    model = new CANFrameModel();
    ui->canFramesView->setModel(model);

    QStringList headers;
    headers << "Timestamp" << "ID" << "Ext" << "Bus" << "Len" << "Data";
    //model->setHorizontalHeaderLabels(headers);
    ui->canFramesView->setColumnWidth(0, 110);
    ui->canFramesView->setColumnWidth(1, 70);
    ui->canFramesView->setColumnWidth(2, 40);
    ui->canFramesView->setColumnWidth(3, 40);
    ui->canFramesView->setColumnWidth(4, 40);
    ui->canFramesView->setColumnWidth(5, 275);
    QHeaderView *HorzHdr = ui->canFramesView->horizontalHeader();
    HorzHdr->setStretchLastSection(true); //causes the data column to automatically fill the tableview
    //enabling the below line kills performance in every way imaginable. Left here as a warning. Do not do this.
    //ui->canFramesView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    ports = QSerialPortInfo::availablePorts();

    for (int i = 0; i < ports.count(); i++)
    {
        ui->cbSerialPorts->addItem(ports[i].portName());
    }

    ui->cbSpeed1->addItem(tr("Disabled"));
    ui->cbSpeed1->addItem(tr("125000"));
    ui->cbSpeed1->addItem(tr("250000"));
    ui->cbSpeed1->addItem(tr("500000"));
    ui->cbSpeed1->addItem(tr("1000000"));
    ui->cbSpeed1->addItem(tr("33333"));
    ui->cbSpeed2->addItem(tr("Disabled"));
    ui->cbSpeed2->addItem(tr("125000"));
    ui->cbSpeed2->addItem(tr("250000"));
    ui->cbSpeed2->addItem(tr("500000"));
    ui->cbSpeed2->addItem(tr("1000000"));
    ui->cbSpeed2->addItem(tr("33333"));

    SerialWorker *worker = new SerialWorker(model);
    worker->moveToThread(&serialWorkerThread);
    connect(&serialWorkerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &MainWindow::sendSerialPort, worker, &SerialWorker::setSerialPort, Qt::QueuedConnection);
    connect(worker, &SerialWorker::frameUpdateTick, this, &MainWindow::gotFrames, Qt::QueuedConnection);
    connect(this, &MainWindow::updateBaudRates, worker, &SerialWorker::updateBaudRates, Qt::QueuedConnection);
    connect(this, &MainWindow::sendCANFrame, worker, &SerialWorker::sendFrame, Qt::QueuedConnection);
    connect(worker, &SerialWorker::connectionSuccess, this, &MainWindow::connectionSucceeded, Qt::QueuedConnection);
    connect(worker, &SerialWorker::connectionFailure, this, &MainWindow::connectionFailed, Qt::QueuedConnection);
    connect(worker, &SerialWorker::deviceInfo, this, &MainWindow::gotDeviceInfo, Qt::QueuedConnection);
    connect(this, &MainWindow::closeSerialPort, worker, &SerialWorker::closeSerialPort, Qt::QueuedConnection);
    connect(this, &MainWindow::startFrameCapturing, worker, &SerialWorker::startFrameCapture);
    connect(this, &MainWindow::stopFrameCapturing, worker, &SerialWorker::stopFrameCapture);
    serialWorkerThread.start();

    graphingWindow = NULL;
    frameInfoWindow = NULL;
    playbackWindow = NULL;
    flowViewWindow = NULL;
    frameSenderWindow = NULL;
    dbcMainEditor = NULL;
    dbcHandler = new DBCHandler;
    bDirty = false;

    model->setDBCHandler(dbcHandler);

    connect(ui->btnConnect, SIGNAL(clicked(bool)), this, SLOT(connButtonPress()));
    connect(ui->actionOpen_Log_File, SIGNAL(triggered(bool)), this, SLOT(handleLoadFile()));
    connect(ui->actionGraph_Dta, SIGNAL(triggered(bool)), this, SLOT(showGraphingWindow()));
    connect(ui->actionFrame_Data_Analysis, SIGNAL(triggered(bool)), this, SLOT(showFrameDataAnalysis()));
    connect(ui->btnClearFrames, SIGNAL(clicked(bool)), this, SLOT(clearFrames()));
    connect(ui->actionSave_Log_File, SIGNAL(triggered(bool)), this, SLOT(handleSaveFile()));
    connect(ui->action_Playback, SIGNAL(triggered(bool)), this, SLOT(showPlaybackWindow()));
    connect(ui->btnBaudSet, SIGNAL(clicked(bool)), this, SLOT(changeBaudRates()));
    connect(ui->actionFlow_View, SIGNAL(triggered(bool)), this, SLOT(showFlowViewWindow()));
    connect(ui->action_Custom, SIGNAL(triggered(bool)), this, SLOT(showFrameSenderWindow()));
    connect(ui->actionLoad_DBC_File, SIGNAL(triggered(bool)), this, SLOT(handleLoadDBC()));
    connect(ui->actionEdit_Messages_Signals, SIGNAL(triggered(bool)), this, SLOT(showEditSignalsWindow()));
    connect(ui->actionSave_DBC_File, SIGNAL(triggered(bool)), this, SLOT(handleSaveDBC()));
    connect(ui->canFramesView, SIGNAL(clicked(QModelIndex)), this, SLOT(gridClicked(QModelIndex)));
    connect(ui->cbInterpret, SIGNAL(toggled(bool)), this, SLOT(interpretToggled(bool)));
    connect(ui->cbOverwrite, SIGNAL(toggled(bool)), this, SLOT(overwriteToggled(bool)));
    connect(ui->actionEdit_Messages_Signals, SIGNAL(triggered(bool)), this, SLOT(showDBCEditor()));
    connect(ui->btnCaptureToggle, SIGNAL(clicked(bool)), this, SLOT(toggleCapture()));

    lbStatusConnected.setText(tr("Not connected"));
    updateFileStatus();
    lbStatusDatabase.setText(tr("No DBC database loaded"));
    ui->statusBar->addWidget(&lbStatusConnected);
    ui->statusBar->addWidget(&lbStatusFilename);
    ui->statusBar->addWidget(&lbStatusDatabase);

    ui->lbFPS->setText("0");
    ui->lbNumFrames->setText("0");

    isConnected = false;
    allowCapture = true;

    //create a temporary frame to be able to capture the correct
    //default height of an item in the table. Need to do this in case
    //of scaling or font differences between different computers.
    CANFrame temp;
    model->addFrame(temp, true);
    normalRowHeight = ui->canFramesView->rowHeight(0);
    qDebug() << "normal row height = " << normalRowHeight;
    model->clearFrames();
}

MainWindow::~MainWindow()
{
    serialWorkerThread.quit();
    serialWorkerThread.wait();

    if (graphingWindow) delete graphingWindow;
    if (frameInfoWindow) delete frameInfoWindow;
    if (playbackWindow) delete playbackWindow;
    if (flowViewWindow) delete flowViewWindow;
    if (frameSenderWindow) delete frameSenderWindow;
    if (dbcMainEditor) delete dbcMainEditor;

    delete ui;
    delete dbcHandler;
    model->clearFrames();
    delete model;
}

void MainWindow::gridClicked(QModelIndex idx)
{
    if (ui->canFramesView->rowHeight(idx.row()) > normalRowHeight)
    {
        ui->canFramesView->setRowHeight(idx.row(), normalRowHeight);
    }
    else {
        ui->canFramesView->resizeRowToContents(idx.row());
    }
}

void MainWindow::interpretToggled(bool state)
{
    model->setInterpetMode(state);
}

void MainWindow::overwriteToggled(bool state)
{
    model->setOverwriteMode(state);
}

//most of the work is handled elsewhere. Need only to update the # of frames
//and maybe auto scroll
void MainWindow::gotFrames(int FPS, int framesSinceLastUpdate)
{
    ui->lbNumFrames->setText(QString::number(model->rowCount()));
    if (ui->cbAutoScroll->isChecked()) ui->canFramesView->scrollToBottom();
    ui->lbFPS->setText(QString::number(FPS));
    if (framesSinceLastUpdate > 0)
    {
        bDirty = true;
        emit framesUpdated(framesSinceLastUpdate); //anyone care that frames were updated?
    }
}

void MainWindow::addFrameToDisplay(CANFrame &frame, bool autoRefresh = false)
{
    model->addFrame(frame, autoRefresh);
    if (ui->cbAutoScroll->isChecked()) ui->canFramesView->scrollToBottom();
    if (autoRefresh)
    {
        ui->lbNumFrames->setText(QString::number(model->rowCount()));
    }
}

void MainWindow::clearFrames()
{
    ui->canFramesView->scrollToTop();
    model->clearFrames();
    ui->lbNumFrames->setText(QString::number(model->rowCount()));
    bDirty = false;
    loadedFileName = "";
    updateFileStatus();
    emit framesUpdated(-1);
}

void MainWindow::changeBaudRates()
{
    int Speed1 = 0, Speed2 = 0;

    Speed1 = ui->cbSpeed1->currentText().toInt();
    Speed2 = ui->cbSpeed2->currentText().toInt();

    emit updateBaudRates(Speed1, Speed2);
}

//CRTD format from Mark Webb-Johnson / OVMS project
/*
Sample data in CRTD format
1320745424.000 CXX OVMS Tesla Roadster cando2crtd converted log
1320745424.000 CXX OVMS Tesla roadster log: charge.20111108.csv
1320745424.002 R11 402 FA 01 C3 A0 96 00 07 01
1320745424.015 R11 400 02 9E 01 80 AB 80 55 00
1320745424.066 R11 400 01 01 00 00 00 00 4C 1D
1320745424.105 R11 100 A4 53 46 5A 52 45 38 42
1320745424.106 R11 100 A5 31 35 42 33 30 30 30
1320745424.106 R11 100 A6 35 36 39
1320745424.106 CEV Open charge port door
1320745424.106 R11 100 9B 97 A6 31 03 15 05 06
1320745424.107 R11 100 07 64

tokens:
0 = timestamp
1 = line type
2 = ID
3-x = The data bytes
*/

void MainWindow::loadCRTDFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QStringList fileList = filename.split('/');
    loadedFileName = fileList[fileList.length() - 1];

    line = inFile->readLine(); //read out the header first and discard it.

    while (!inFile->atEnd()) {
        line = inFile->readLine().simplified();
        if (line.length() > 2)
        {
            QList<QByteArray> tokens = line.split(' ');
            int multiplier;
            int idxOfDecimal = tokens[0].indexOf('.');
            if (idxOfDecimal > -1) {
                int decimalPlaces = tokens[0].length() - tokens[0].indexOf('.') - 1;
                //the result of the above is the # of digits after the decimal.
                //This program deals in microsecond so turn the value into microseconds
                multiplier = 1000000; //turn the decimal into full microseconds
            }
            else
            {
                multiplier = 1; //special case. Assume no decimal means microseconds
            }
            //qDebug() << "decimal places " << decimalPlaces;
            thisFrame.timestamp = (int64_t)(tokens[0].toDouble() * multiplier);
            if (tokens[1] == "R11" || tokens[1] == "R29")
            {
                thisFrame.ID = tokens[2].toInt(NULL, 16);
                if (tokens[1] == "R29") thisFrame.extended = true;
                else thisFrame.extended = false;
                thisFrame.bus = 0;
                thisFrame.len = tokens.length() - 3;
                for (int d = 0; d < thisFrame.len; d++)
                {
                    if (tokens[d + 3] != "")
                    {
                        thisFrame.data[d] = tokens[d + 3].toInt(NULL, 16);
                    }
                    else thisFrame.data[d] = 0;
                }
                addFrameToDisplay(thisFrame);
            }
        }
    }
    inFile->close();
    model->sendRefresh();
    ui->lbNumFrames->setText(QString::number(model->rowCount()));
    if (ui->cbAutoScroll->isChecked()) ui->canFramesView->scrollToBottom();

    updateFileStatus();
    emit framesUpdated(-2);
}

void MainWindow::saveCRTDFile(QString filename)
{
    QFile *outFile = new QFile(filename);
    QVector<CANFrame> *frames = model->getListReference();

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QStringList fileList = filename.split('/');
    loadedFileName = fileList[fileList.length() - 1];

    //write in float format with 6 digits after the decimal point
    outFile->write(QString::number(frames->at(0).timestamp / 1000000.0, 'f', 6).toUtf8() + tr(" CXX GVRET-PC Reverse Engineering Tool Output V").toUtf8() + QString::number(VERSION).toUtf8());
    outFile->write("\n");

    for (int c = 0; c < frames->count(); c++)
    {
        outFile->write(QString::number(frames->at(c).timestamp / 1000000.0, 'f', 6).toUtf8());
        outFile->putChar(' ');
        if (frames->at(c).extended)
        {
            outFile->write("R29 ");
        }
        else outFile->write("R11 ");
        outFile->write(QString::number(frames->at(c).ID, 16).toUpper().rightJustified(8, '0').toUtf8());
        outFile->putChar(' ');

        for (int temp = 0; temp < frames->at(c).len; temp++)
        {
            outFile->write(QString::number(frames->at(c).data[temp], 16).toUpper().rightJustified(2, '0').toUtf8());
            outFile->putChar(' ');
        }

        outFile->write("\n");
    }
    outFile->close();

    updateFileStatus();
}

//The "native" file format for this program
void MainWindow::loadNativeCSVFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    long long timeStamp = Utility::GetTimeMS();

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QStringList fileList = filename.split('/');
    loadedFileName = fileList[fileList.length() - 1];

    line = inFile->readLine(); //read out the header first and discard it.

    while (!inFile->atEnd()) {
        line = inFile->readLine();
        if (line.length() > 2)
        {
            QList<QByteArray> tokens = line.split(',');
            if (tokens[0].length() > 3)
            {
                long long temp = tokens[0].right(10).toLongLong();
                thisFrame.timestamp = temp;
            }
            else
            {
                timeStamp += 5;
                thisFrame.timestamp = timeStamp;
            }

            thisFrame.ID = tokens[1].toInt(NULL, 16);
            if (tokens[2].contains("True")) thisFrame.extended = 1;
            else thisFrame.extended = 0;
            thisFrame.bus = tokens[3].toInt();
            thisFrame.len = tokens[4].toInt();
            for (int c = 0; c < 8; c++) thisFrame.data[c] = 0;
            for (int d = 0; d < thisFrame.len; d++)
                thisFrame.data[d] = tokens[5 + d].toInt(NULL, 16);
            addFrameToDisplay(thisFrame);
        }
    }
    inFile->close();
    model->sendRefresh();
    ui->lbNumFrames->setText(QString::number(model->rowCount()));
    if (ui->cbAutoScroll->isChecked()) ui->canFramesView->scrollToBottom();

    updateFileStatus();
    emit framesUpdated(-2);
}

void MainWindow::saveNativeCSVFile(QString filename)
{
    QFile *outFile = new QFile(filename);
    QVector<CANFrame> *frames = model->getListReference();

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QStringList fileList = filename.split('/');
    loadedFileName = fileList[fileList.length() - 1];

    outFile->write("Time Stamp,ID,Extended,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8");
    outFile->write("\n");

    for (int c = 0; c < frames->count(); c++)
    {
        outFile->write(QString::number(frames->at(c).timestamp).toUtf8());
        outFile->putChar(44);

        outFile->write(QString::number(frames->at(c).ID, 16).toUpper().rightJustified(8, '0').toUtf8());
        outFile->putChar(44);

        if (frames->at(c).extended) outFile->write("true,");
        else outFile->write("false,");

        outFile->write(QString::number(frames->at(c).bus).toUtf8());
        outFile->putChar(44);

        outFile->write(QString::number(frames->at(c).len).toUtf8());
        outFile->putChar(44);


        for (int temp = 0; temp < 8; temp++)
        {
            outFile->write(QString::number(frames->at(c).data[temp], 16).toUpper().rightJustified(2, '0').toUtf8());
            outFile->putChar(44);
        }

        outFile->write("\n");


    }
    outFile->close();

    updateFileStatus();
}

void MainWindow::loadGenericCSVFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    long long timeStamp = Utility::GetTimeMS();

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QStringList fileList = filename.split('/');
    loadedFileName = fileList[fileList.length() - 1];

    line = inFile->readLine(); //read out the header first and discard it.

    while (!inFile->atEnd()) {
        line = inFile->readLine();
        if (line.length() > 2)
        {
            QList<QByteArray> tokens = line.split(',');

            timeStamp += 5;
            thisFrame.timestamp = timeStamp;
            thisFrame.ID = tokens[0].toInt(NULL, 16);
            if (thisFrame.ID > 0x7FF) thisFrame.extended = true;
            else thisFrame.extended  = false;
            thisFrame.bus = 0;
            QList<QByteArray> dataTok = tokens[1].split(' ');
            thisFrame.len = dataTok.length();
            if (thisFrame.len > 8) thisFrame.len = 8;
            for (int d = 0; d < thisFrame.len; d++) thisFrame.data[d] = dataTok[d].toInt(NULL, 16);

            addFrameToDisplay(thisFrame);
        }
    }
    inFile->close();
    model->sendRefresh();
    ui->lbNumFrames->setText(QString::number(model->rowCount()));
    if (ui->cbAutoScroll->isChecked()) ui->canFramesView->scrollToBottom();

    updateFileStatus();
    emit framesUpdated(-2);
}

void MainWindow::saveGenericCSVFile(QString filename)
{

}

//busmaster log file
/*
tokens:
0 = timestamp
1 = Transmission direction
2 = Channel
3 = ID
4 = Type (s = standard, I believe x = extended)
5 = Data byte length
6-x = The data bytes

Sample chunk of a busmaster log:
***BUSMASTER Ver 2.4.0***
***PROTOCOL CAN***
***NOTE: PLEASE DO NOT EDIT THIS DOCUMENT***
***[START LOGGING SESSION]***
***START DATE AND TIME 8:8:2014 11:49:7:965***
***HEX***
***SYSTEM MODE***
***START CHANNEL BAUD RATE***
***CHANNEL 1 - Kvaser - Kvaser Leaf Light HS #0 (Channel 0), Serial Number- 0, Firmware- 0x00000037 0x00020000 - 500000 bps***
***END CHANNEL BAUD RATE***
***START DATABASE FILES (DBF/DBC)***
***END OF DATABASE FILES (DBF/DBC)***
***<Time><Tx/Rx><Channel><CAN ID><Type><DLC><DataBytes>***
11:49:12:9420 Rx 1 0x023 s 1 40
11:49:12:9440 Rx 1 0x460 s 8 03 E0 00 00 C0 00 00 00
11:49:12:9530 Rx 1 0x023 s 1 40
11:49:12:9680 Rx 1 0x408 s 8 0F 02 00 30 00 00 7F 00
11:49:12:9680 Rx 1 0x40B s 8 00 00 00 00 00 10 60 00
11:49:12:9690 Rx 1 0x045 s 8 40 00 00 00 00 00 00 00
*/
void MainWindow::loadLogFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    uint64_t timeStamp = Utility::GetTimeMS();

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QStringList fileList = filename.split('/');
    loadedFileName = fileList[fileList.length() - 1];

    line = inFile->readLine(); //read out the header first and discard it.

    while (!inFile->atEnd()) {
        line = inFile->readLine();
        if (line.startsWith("***")) continue;
        if (line.length() > 1)
        {
            QList<QByteArray> tokens = line.split(' ');
            QList<QByteArray> timeToks = tokens[0].split(':');
            timeStamp = (timeToks[0].toInt() * (1000ul * 1000ul * 60ul * 60ul)) + (timeToks[1].toInt() * (1000ul * 1000ul * 60ul))
                      + (timeToks[2].toInt() * (1000ul * 1000ul)) + (timeToks[3].toInt() * 100ul);
            thisFrame.timestamp = timeStamp;
            thisFrame.ID = tokens[3].right(tokens[3].length() - 2).toInt(NULL, 16);
            if (tokens[4] == "s") thisFrame.extended = false;
            else thisFrame.extended = true;
            thisFrame.bus = tokens[2].toInt() - 1;
            thisFrame.len = tokens[5].toInt();
            for (int d = 0; d < thisFrame.len; d++) thisFrame.data[d] = tokens[d + 6].toInt(NULL, 16);
        }
        addFrameToDisplay(thisFrame);
    }
    inFile->close();
    model->sendRefresh();
    ui->lbNumFrames->setText(QString::number(model->rowCount()));
    if (ui->cbAutoScroll->isChecked()) ui->canFramesView->scrollToBottom();

    updateFileStatus();
    emit framesUpdated(-2);
}

void MainWindow::saveLogFile(QString filename)
{

}

//log file from microchip tool
/*
tokens:
0 = timestamp
1 = Transmission direction
2 = ID
3 = Data byte length
4-x = The data bytes
*/
void MainWindow::loadMicrochipFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    bool inComment = false;
    long long timeStamp = Utility::GetTimeMS();

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QStringList fileList = filename.split('/');
    loadedFileName = fileList[fileList.length() - 1];

    line = inFile->readLine(); //read out the header first and discard it.

    while (!inFile->atEnd()) {
        line = inFile->readLine();
        if (line.length() > 2)
        {
            if (line.startsWith("//"))
            {
                inComment = !inComment;
            }
            else
            {
                if (!inComment)
                {
                    QList<QByteArray> tokens = line.split(';');
                    timeStamp += 5;
                    thisFrame.timestamp = timeStamp;

                    //thisFrame.ID = Utility::ParseStringToNum(tokens[2]);
                    if (thisFrame.ID <= 0x7FF) thisFrame.extended = false;
                    else thisFrame.extended = true;
                    thisFrame.bus = 0;
                    thisFrame.len = tokens[3].toInt();
                    //for (int d = 0; d < thisFrame.len; d++) thisFrame.data[d] = (unsigned char)Utility::ParseStringToNum(tokens[4 + d]);
                    addFrameToDisplay(thisFrame);
                }
            }
        }
    }
    inFile->close();
    model->sendRefresh();
    ui->lbNumFrames->setText(QString::number(model->rowCount()));
    if (ui->cbAutoScroll->isChecked()) ui->canFramesView->scrollToBottom();

    updateFileStatus();
    emit framesUpdated(-2);
}

void MainWindow::saveMicrochipFile(QString filename)
{

}

void MainWindow::handleLoadFile()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("CRTD Logs (*.txt)")));
    filters.append(QString(tr("GVRET Logs (*.csv)")));
    filters.append(QString(tr("Generic ID/Data CSV (*.csv)")));
    filters.append(QString(tr("BusMaster Log (*.log)")));
    filters.append(QString(tr("Microchip Log (*.log)")));

    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec())
    {
        filename = dialog.selectedFiles()[0];
    }

    ui->canFramesView->scrollToTop();
    model->clearFrames();

    if (dialog.selectedNameFilter() == filters[0]) loadCRTDFile(filename);
    if (dialog.selectedNameFilter() == filters[1]) loadNativeCSVFile(filename);
    if (dialog.selectedNameFilter() == filters[2]) loadGenericCSVFile(filename);
    if (dialog.selectedNameFilter() == filters[3]) loadLogFile(filename);
    if (dialog.selectedNameFilter() == filters[4]) loadMicrochipFile(filename);
}

void MainWindow::handleSaveFile()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("CRTD Logs (*.txt)")));
    filters.append(QString(tr("GVRET Logs (*.csv)")));
    filters.append(QString(tr("Generic ID/Data CSV (*.csv)")));
    filters.append(QString(tr("BusMaster Log (*.log)")));
    filters.append(QString(tr("Microchip Log (*.log)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec())
    {
        filename = dialog.selectedFiles()[0];
    }

    if (dialog.selectedNameFilter() == filters[0]) saveCRTDFile(filename);
    if (dialog.selectedNameFilter() == filters[1]) saveNativeCSVFile(filename);
    if (dialog.selectedNameFilter() == filters[2]) saveGenericCSVFile(filename);
    if (dialog.selectedNameFilter() == filters[3]) saveLogFile(filename);
    if (dialog.selectedNameFilter() == filters[4]) saveMicrochipFile(filename);
}

void MainWindow::handleLoadDBC()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("DBC File (*.dbc)")));

    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec())
    {
        filename = dialog.selectedFiles()[0];
    }

    //right now there is only one file type that can be loaded here so just do it.
    dbcHandler->loadDBCFile(filename);
    //dbcHandler->listDebugging();
    QStringList fileList = filename.split('/');
    lbStatusDatabase.setText(fileList[fileList.length() - 1] + tr(" loaded."));
}

void MainWindow::handleSaveDBC()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("DBC File (*.dbc)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec())
    {
        filename = dialog.selectedFiles()[0];
    }

}

void MainWindow::showEditSignalsWindow()
{

}

void MainWindow::connButtonPress()
{
    if (!isConnected)
    {
        for (int x = 0; x < ports.count(); x++)
        {
            if (ports.at(x).portName() == ui->cbSerialPorts->currentText())
            {
                emit sendSerialPort(&ports[x]);
                lbStatusConnected.setText(tr("Attempting to connect to port ") + ports[x].portName());
                return;
            }
        }
    }
    else
    {
        emit closeSerialPort();
        isConnected = false;
        lbStatusConnected.setText(tr("Not Connected"));
        ui->btnConnect->setText(tr("Connect to GVRET"));
    }
}

void MainWindow::toggleCapture()
{
    allowCapture = !allowCapture;
    if (allowCapture)
    {
        ui->btnCaptureToggle->setText("Suspend Capturing");
        emit startFrameCapturing();
    }
    else
    {
        ui->btnCaptureToggle->setText("Restart Capturing");
        emit stopFrameCapturing();
    }
}

void MainWindow::connectionSucceeded(int baud0, int baud1)
{
    lbStatusConnected.setText(tr("Connected to GVRET"));
    //finally, find the baud rate in the list of rates or
    //add it to the bottom if needed (that'll be weird though...

    int idx = ui->cbSpeed1->findText(QString::number(baud0));
    if (idx > -1) ui->cbSpeed1->setCurrentIndex(idx);
    else
    {
        ui->cbSpeed1->addItem(QString::number(baud0));
        ui->cbSpeed1->setCurrentIndex(ui->cbSpeed1->count() - 1);
    }

    idx = ui->cbSpeed2->findText(QString::number(baud1));
    if (idx > -1) ui->cbSpeed2->setCurrentIndex(idx);
    else
    {
        ui->cbSpeed2->addItem(QString::number(baud1));
        ui->cbSpeed2->setCurrentIndex(ui->cbSpeed2->count() - 1);
    }
    //ui->btnConnect->setEnabled(false);
    ui->btnConnect->setText(tr("Disconnect from GVRET"));
    isConnected = true;
}

void MainWindow::connectionFailed()
{
    lbStatusConnected.setText(tr("Failed to connect!"));
}

void MainWindow::updateFileStatus()
{
    QString output;
    if (model->rowCount() == 0)
    {
        output = tr("No file loaded");
    }
    else
    {
        if (loadedFileName.length() > 2)
        {
            output = loadedFileName + " loaded";
        }
        else
        {
            output = tr("No file loaded");
        }

        if (bDirty)
        {
            output += " (X)";
        }
    }
    lbStatusFilename.setText(output);
}

void MainWindow::gotDeviceInfo(int build, int swCAN)
{
    QString str = tr("Connected to GVRET ") + QString::number(build);
    if (swCAN == 1) str += "(SW)";
    lbStatusConnected.setText(str);
}

void MainWindow::showGraphingWindow()
{
    if (!graphingWindow) graphingWindow = new GraphingWindow(model->getListReference());
    graphingWindow->show();
}

void MainWindow::showFrameDataAnalysis()
{
    //only create an instance of the object if we dont have one. Otherwise just display the existing one.
    if (!frameInfoWindow)
    {
        frameInfoWindow = new FrameInfoWindow(model->getListReference());
    }
    frameInfoWindow->show();
}

void MainWindow::showFrameSenderWindow()
{
    if (!frameSenderWindow) frameSenderWindow = new FrameSenderWindow(model->getListReference());
    frameSenderWindow->show();
}

void MainWindow::showPlaybackWindow()
{
    if (!playbackWindow)
    {
        playbackWindow = new FramePlaybackWindow(model->getListReference());
        //connect signal to signal to pass the signal through to whoever is handling the slot
        connect(playbackWindow, SIGNAL(sendCANFrame(const CANFrame*,int)), this, SIGNAL(sendCANFrame(const CANFrame*,int)));
    }
    playbackWindow->show();
}

void MainWindow::showFlowViewWindow()
{
    if (!flowViewWindow)
    {
        flowViewWindow = new FlowViewWindow(model->getListReference());
    }
    flowViewWindow->show();
}

void MainWindow::showDBCEditor()
{
    if (!dbcMainEditor)
    {
        dbcMainEditor = new DBCMainEditor(dbcHandler);
    }
    dbcMainEditor->show();
}
