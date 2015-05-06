#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "can_structs.h"
#include <QDateTime>
#include <QFileDialog>
#include <QtSerialPort/QSerialPortInfo>
#include "canframemodel.h"
#include "utility.h"

/*
This first order of business is to attempt to gain feature parity (roughly) with GVRET-PC so  that this
can replace it as a cross platform solution.

Here are things yet to do
- Add flow view - ability to see bits change over time
- Ability to send frames via playback mechanism (people use this so get it working!)
- Ability to use programmatic frame sending interface (only I use it and it's complicated but helpful)
*/


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->btnConnect, SIGNAL(clicked(bool)), this, SLOT(connButtonPress()));
    connect(ui->actionOpen_Log_File, SIGNAL(triggered(bool)), this, SLOT(handleLoadFile()));
    connect(ui->actionGraph_Dta, SIGNAL(triggered(bool)), this, SLOT(showGraphingWindow()));
    connect(ui->actionFrame_Data_Analysis, SIGNAL(triggered(bool)), this, SLOT(showFrameDataAnalysis()));
    connect(ui->btnClearFrames, SIGNAL(clicked(bool)), this, SLOT(clearFrames()));
    connect(ui->actionSave_Log_File, SIGNAL(triggered(bool)), this, SLOT(handleSaveFile()));
    connect(ui->action_Playback, SIGNAL(triggered(bool)), this, SLOT(showPlaybackWindow()));

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
    ui->canFramesView->setColumnWidth(5, 300);

    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

    for (int i = 0; i < ports.count(); i++)
    {
        ui->cbSerialPorts->addItem(ports[i].portName());
    }

    port = new QSerialPort(this);

    rx_state = IDLE;
    rx_step = 0;

    graphingWindow = NULL;
    frameInfoWindow = NULL;
    playbackWindow = NULL;
}

MainWindow::~MainWindow()
{
    delete ui;
    if (graphingWindow) delete graphingWindow;
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

    line = inFile->readLine(); //read out the header first and discard it.

    while (!inFile->atEnd()) {
        line = inFile->readLine();
        if (line.length() > 2)
        {
            QList<QByteArray> tokens = line.split(' ');
            thisFrame.timestamp = (int64_t)(tokens[0].toDouble() * 1000);
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
}

void MainWindow::saveCRTDFile(QString filename)
{
    QFile *outFile = new QFile(filename);
    QList<CANFrame> *frames = model->getListReference();

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    outFile->write(QString::number(frames->at(0).timestamp / 1000.0).toUtf8() + tr(" CXX GVRET-PC Reverse Engineering Tool Output").toUtf8());
    outFile->write("\n");

    for (int c = 0; c < frames->count(); c++)
    {
        outFile->write(QString::number(frames->at(c).timestamp).toUtf8());
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

    line = inFile->readLine(); //read out the header first and discard it.

    while (!inFile->atEnd()) {
        line = inFile->readLine();
        if (line.length() > 2)
        {
            QList<QByteArray> tokens = line.split(',');
            if (tokens[0].length() > 10)
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
}

void MainWindow::saveNativeCSVFile(QString filename)
{
    QFile *outFile = new QFile(filename);
    QList<CANFrame> *frames = model->getListReference();

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
        return;

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
}

void MainWindow::loadGenericCSVFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    long long timeStamp = Utility::GetTimeMS();

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
        return;

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
    long long timeStamp = Utility::GetTimeMS();

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    line = inFile->readLine(); //read out the header first and discard it.

    while (!inFile->atEnd()) {
        line = inFile->readLine();
        if (line.startsWith("***")) continue;
        if (line.length() > 1)
        {
            QList<QByteArray> tokens = line.split(' ');
            QList<QByteArray> timeToks = tokens[0].split(':');
            timeStamp = (timeToks[0].toInt() * (1000 * 60 * 60)) + (timeToks[1].toInt() * (1000 * 60))
                      + (timeToks[2].toInt() * (1000)) + (timeToks[3].toInt() / 10);
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

void MainWindow::connButtonPress()
{
    if (port->isOpen())
    {
        port->close();
    }
    else
    {
        port->setPortName(ui->cbSerialPorts->currentText());
        port->open(QIODevice::ReadWrite);
        QByteArray output;
        output.append(0xE7);
        output.append(0xE7);
        port->write(output);
        ///isConnected = true;
        connect(port, SIGNAL(readyRead()), this, SLOT(readSerialData()));

    }
}

void MainWindow::showGraphingWindow()
{
    if (!graphingWindow) graphingWindow = new GraphingWindow(model->getListReference());
    graphingWindow->show();
}

void MainWindow::showFrameDataAnalysis()
{
    //only create an instance of the object if we dont have one. Otherwise just display the existing one.
    if (!frameInfoWindow) frameInfoWindow = new FrameInfoWindow(model->getListReference());
    frameInfoWindow->show();
}

void MainWindow::showPlaybackWindow()
{
    if (!playbackWindow) playbackWindow = new FramePlaybackWindow(model->getListReference());
    playbackWindow->show();
}

void MainWindow::readSerialData()
{
    QByteArray data = port->readAll();
    unsigned char c;
    qDebug() << (tr("Got data from serial. Len = %0").arg(data.length()));
    for (int i = 0; i < data.length(); i++)
    {
        c = data.at(i);
        procRXChar(c);
    }
}

void MainWindow::procRXChar(unsigned char c)
{
    switch (rx_state)
    {
    case IDLE:
        if (c == 0xF1) rx_state = GET_COMMAND;
        break;
    case GET_COMMAND:
        switch (c)
        {
        case 0: //receiving a can frame
            rx_state = BUILD_CAN_FRAME;
            rx_step = 0;
            break;
        case 1: //we don't accept time sync commands from the firmware
            rx_state = IDLE;
            break;
        case 2: //process a return reply for digital input states.
            rx_state = GET_DIG_INPUTS;
            rx_step = 0;
            break;
        case 3: //process a return reply for analog inputs
            rx_state = GET_ANALOG_INPUTS;
            break;
        case 4: //we set digital outputs we don't accept replies so nothing here.
            rx_state = IDLE;
            break;
        case 5: //we set canbus specs we don't accept replies.
            rx_state = IDLE;
            break;
        }
        break;
    case BUILD_CAN_FRAME:
        switch (rx_step)
        {
        case 0:
            buildFrame.timestamp = c;
            break;
        case 1:
            buildFrame.timestamp |= (uint)(c << 8);
            break;
        case 2:
            buildFrame.timestamp |= (uint)c << 16;
            break;
        case 3:
            buildFrame.timestamp |= (uint)c << 24;
            break;
        case 4:
            buildFrame.ID = c;
            break;
        case 5:
            buildFrame.ID |= c << 8;
            break;
        case 6:
            buildFrame.ID |= c << 16;
            break;
        case 7:
            buildFrame.ID |= c << 24;
            if ((buildFrame.ID & 1 << 31) == 1 << 31)
            {
                buildFrame.ID &= 0x7FFFFFFF;
                buildFrame.extended = true;
            }
            else buildFrame.extended = false;
            break;
        case 8:
            buildFrame.len = c & 0xF;
            if (buildFrame.len > 8) buildFrame.len = 8;
            buildFrame.bus = (c & 0xF0) >> 4;
            break;
        default:
            if (rx_step < buildFrame.len + 9)
            {
                buildFrame.data[rx_step - 9] = c;
            }
            else
            {
                rx_state = IDLE;
                rx_step = 0;
                addFrameToDisplay(buildFrame, true);
                ui->lbNumFrames->setText(QString::number(model->rowCount()));
            }
            break;
        }
        rx_step++;
        break;
    case GET_ANALOG_INPUTS: //get 9 bytes - 2 per analog input plus checksum
        switch (rx_step)
        {
        case 0:
            break;
        }
        rx_step++;
        break;
    case GET_DIG_INPUTS: //get two bytes. One for digital in status and one for checksum.
        switch (rx_step)
        {
        case 0:
            break;
        case 1:
            rx_state = IDLE;
            break;
        }
        rx_step++;
        break;
    }
}
