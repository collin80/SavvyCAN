#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "can_structs.h"
#include <QDateTime>
#include <QFileDialog>
#include <QtSerialPort/QSerialPortInfo>
#include "canframemodel.h"
#include "utility.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
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
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::addFrameToDisplay(CANFrame &frame)
{
    model->addFrame(frame);
}

void MainWindow::loadCRTDFile(QString filename)
{

}

void MainWindow::loadNativeCSVFile(QString filename)
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
            QList<QByteArray> tokens = line.split(',');
            if (tokens[0].length() > 10)
            {
                long long temp = tokens[0].toInt();
                thisFrame.timestamp = temp;
            }
            else
            {
                QDateTime stamp = QDateTime::currentDateTime();
                thisFrame.timestamp = (long)(((stamp.time().hour() * 3600) + (stamp.time().minute() * 60) + (stamp.time().second()) * 1000) + stamp.time().msec());
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

}

void MainWindow::loadGenericCSVFile(QString filename)
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
            QList<QByteArray> tokens = line.split(',');

            QDateTime stamp = QDateTime::currentDateTime();
            thisFrame.timestamp = (long)(((stamp.time().hour() * 3600) + (stamp.time().minute() * 60) + (stamp.time().second()) * 1000) + stamp.time().msec());
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

}

void MainWindow::loadLogFile(QString filename)
{

}

void MainWindow::loadMicrochipFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    bool inComment = false;

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

                /*
                tokens:
                0 = timestamp
                1 = Transmission direction
                2 = ID
                3 = Data byte length
                4-x = The data bytes
                */
                if (!inComment)
                {
                    QList<QByteArray> tokens = line.split(';');
                    QDateTime stamp = QDateTime::currentDateTime();
                    thisFrame.timestamp = (long)(((stamp.time().hour() * 3600) + (stamp.time().minute() * 60) + (stamp.time().second()) * 1000) + stamp.time().msec());

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

    }

}
