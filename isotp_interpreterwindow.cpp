#include "isotp_interpreterwindow.h"
#include "ui_isotp_interpreterwindow.h"
#include "mainwindow.h"

ISOTP_InterpreterWindow::ISOTP_InterpreterWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ISOTP_InterpreterWindow)
{
    ui->setupUi(this);
    modelFrames = frames;

    decoder = new ISOTP_DECODER(modelFrames);

    connect(MainWindow::getReference(), &MainWindow::framesUpdated, decoder, &ISOTP_DECODER::updatedFrames);
    connect(decoder, &ISOTP_DECODER::newISOMessage, this, &ISOTP_InterpreterWindow::newISOMessage);

    QStringList headers;
    headers << "Timestamp" << "ID" << "Bus" << "Dir" << "Length" << "Data";
    ui->tableIsoFrames->setColumnCount(6);
    ui->tableIsoFrames->setColumnWidth(0, 100);
    ui->tableIsoFrames->setColumnWidth(1, 50);
    ui->tableIsoFrames->setColumnWidth(2, 50);
    ui->tableIsoFrames->setColumnWidth(3, 50);
    ui->tableIsoFrames->setColumnWidth(4, 75);
    ui->tableIsoFrames->setColumnWidth(5, 200);
    ui->tableIsoFrames->setHorizontalHeaderLabels(headers);
    QHeaderView *HorzHdr = ui->tableIsoFrames->horizontalHeader();
    HorzHdr->setStretchLastSection(true);
}

ISOTP_InterpreterWindow::~ISOTP_InterpreterWindow()
{
    delete decoder;
    delete ui;
}

void ISOTP_InterpreterWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    readSettings();
    decoder->updatedFrames(-2);
}

void ISOTP_InterpreterWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
}

void ISOTP_InterpreterWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("ISODecodeWindow/WindowSize", this->size()).toSize());
        move(settings.value("ISODecodeWindow/WindowPos", QPoint(50, 50)).toPoint());
    }
}

void ISOTP_InterpreterWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("ISODecodeWindow/WindowSize", size());
        settings.setValue("ISODecodeWindow/WindowPos", pos());
    }
}

void ISOTP_InterpreterWindow::newISOMessage(ISOTP_MESSAGE &msg)
{
    int rowNum;
    QString tempString;

    messages.append(msg);

    rowNum = ui->tableIsoFrames->rowCount();
    ui->tableIsoFrames->insertRow(rowNum);

    ui->tableIsoFrames->setItem(rowNum, 0, new QTableWidgetItem(QString::number(msg.timestamp)));
    ui->tableIsoFrames->setItem(rowNum, 1, new QTableWidgetItem(QString::number(msg.ID, 16)));
    ui->tableIsoFrames->setItem(rowNum, 2, new QTableWidgetItem(QString::number(msg.bus)));
    if (msg.isReceived) ui->tableIsoFrames->setItem(rowNum, 3, new QTableWidgetItem("Rx"));
    else ui->tableIsoFrames->setItem(rowNum, 3, new QTableWidgetItem("Tx"));
    ui->tableIsoFrames->setItem(rowNum, 4, new QTableWidgetItem(QString::number(msg.len)));

    for (int i = 0; i < msg.data.length(); i++)
    {
        tempString.append(Utility::formatNumber(msg.data[i]));
        tempString.append(" ");
    }
    ui->tableIsoFrames->setItem(rowNum, 5, new QTableWidgetItem(tempString));

}


