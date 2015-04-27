#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "can_structs.h"
#include <QDateTime>
#include <QFileDialog>
#include <QStandardItemModel>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    model = new QStandardItemModel();
    ui->canFramesView->setModel(model);

    QStringList headers;
    headers << "Timestamp" << "ID" << "Ext" << "Bus" << "Len" << "Data";
    model->setHorizontalHeaderLabels(headers);
    ui->canFramesView->setColumnWidth(0, 110);
    ui->canFramesView->setColumnWidth(1, 70);
    ui->canFramesView->setColumnWidth(2, 40);
    ui->canFramesView->setColumnWidth(3, 40);
    ui->canFramesView->setColumnWidth(4, 40);
    ui->canFramesView->setColumnWidth(5, 300);

    /*
    for (int row = 0; row < 4000; row++)
    {
        QStandardItem *item = new QStandardItem(QString("10000ms"));
        model->setItem(row, 0, item);
        item = new QStandardItem(QString("0x107"));
        model->setItem(row, 1, item);
        item = new QStandardItem(QString("F"));
        model->setItem(row, 2, item);
        item = new QStandardItem(QString("1"));
        model->setItem(row, 3, item);
        item = new QStandardItem(QString("8"));
        model->setItem(row, 4, item);
        item = new QStandardItem(QString("0x10 0xE0 0x10 0x30 0x40 0x50 0xFF 0x23"));
        model->setItem(row, 5, item);
    }
    */
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::addFrameToDisplay(CANFrame frame)
{
    int row = model->rowCount();
    QStandardItem *item = new QStandardItem(QString::number(frame.timestamp));
    model->setItem(row, 0, item);
    item = new QStandardItem(QString::number(frame.ID, 16));
    model->setItem(row, 1, item);
    item = new QStandardItem(QString::number(frame.extended));
    model->setItem(row, 2, item);
    item = new QStandardItem(QString::number(frame.bus));
    model->setItem(row, 3, item);
    item = new QStandardItem(QString::number(frame.len));
    model->setItem(row, 4, item);
    QString tempString;
    for (int i = 0; i < frame.len; i++)
    {
        tempString.append(QString::number(frame.data[i], 16));
        tempString.append(" ");
    }
    item = new QStandardItem(tempString);
    model->setItem(row, 5, item);
}

void MainWindow::loadCRTDFile(QString filename)
{

}

void MainWindow::loadNativeCSVFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    while (!inFile->atEnd()) {
        QByteArray line = inFile->readLine();
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
            thisFrame.extended = tokens[2].toInt();
            thisFrame.bus = tokens[3].toInt();
            thisFrame.len = tokens[4].toInt();
            for (int c = 0; c < 8; c++) thisFrame.data[c] = 0;
            for (int d = 0; d < thisFrame.len; d++)
                thisFrame.data[d] = tokens[5 + d].toInt(NULL, 16);
            addFrameToDisplay(thisFrame);
        }
    }
    inFile->close();

}

void MainWindow::loadGenericCSVFile(QString filename)
{

}

void MainWindow::loadLogFile(QString filename)
{

}

void MainWindow::loadMicrochipFile(QString filename)
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
