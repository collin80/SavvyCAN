#include "isotp_interpreterwindow.h"
#include "ui_isotp_interpreterwindow.h"
#include "mainwindow.h"

ISOTP_InterpreterWindow::ISOTP_InterpreterWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ISOTP_InterpreterWindow)
{
    ui->setupUi(this);
    modelFrames = frames;

    decoder = new ISOTP_HANDLER;

    decoder->setReception(true);
    decoder->setProcessAll(true);

    connect(MainWindow::getReference(), &MainWindow::framesUpdated, this, &ISOTP_InterpreterWindow::updatedFrames);
    connect(MainWindow::getReference(), &MainWindow::framesUpdated, decoder, &ISOTP_HANDLER::updatedFrames);
    connect(decoder, &ISOTP_HANDLER::newISOMessage, this, &ISOTP_InterpreterWindow::newISOMessage);

    connect(ui->tableIsoFrames, &QTableWidget::itemSelectionChanged, this, &ISOTP_InterpreterWindow::showDetailView);

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

void ISOTP_InterpreterWindow::updatedFrames(int numFrames)
{
    if (numFrames == -1) //all frames deleted. Kill the display
    {
        messages.clear();
        ui->tableIsoFrames->clear();
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        messages.clear();
        ui->tableIsoFrames->clear();
    }
    else //just got some new frames. See if they are relevant.
    {
    }
}

void ISOTP_InterpreterWindow::showDetailView()
{
    QString buildString;
    ISOTP_MESSAGE *msg;
    int rowNum = ui->tableIsoFrames->currentRow();

    ui->txtFrameDetails->clear();
    if (rowNum == -1) return;

    msg = &messages[rowNum];

    if (msg->len != msg->data.length())
    {
        buildString.append("Message didn't have the correct number of bytes.\rExpected "
                           + QString::number(msg->len) + " got "
                           + QString::number(msg->data.length()) + "\r\r");
    }

    buildString.append(tr("Raw Payload: "));
    for (int i = 0; i < messages[rowNum].data.count(); i++)
    {
        buildString.append(Utility::formatNumber(messages[rowNum].data[i]));
        buildString.append(" ");
    }
    buildString.append("\r\r");

    //if (ui->cb->isChecked())
    //{

    //}

    ui->txtFrameDetails->setText(buildString);

}

void ISOTP_InterpreterWindow::newISOMessage(ISOTP_MESSAGE msg)
{
    int rowNum;
    QString tempString;

    if ((msg.len != msg.data.count()) && !ui->cbShowIncomplete->isChecked()) return;

    messages.append(msg);

    rowNum = ui->tableIsoFrames->rowCount();
    ui->tableIsoFrames->insertRow(rowNum);

    ui->tableIsoFrames->setItem(rowNum, 0, new QTableWidgetItem(QString::number(msg.timestamp)));
    ui->tableIsoFrames->setItem(rowNum, 1, new QTableWidgetItem(QString::number(msg.ID, 16)));
    ui->tableIsoFrames->setItem(rowNum, 2, new QTableWidgetItem(QString::number(msg.bus)));
    if (msg.isReceived) ui->tableIsoFrames->setItem(rowNum, 3, new QTableWidgetItem("Rx"));
    else ui->tableIsoFrames->setItem(rowNum, 3, new QTableWidgetItem("Tx"));
    ui->tableIsoFrames->setItem(rowNum, 4, new QTableWidgetItem(QString::number(msg.len)));

    for (int i = 0; i < msg.data.count(); i++)
    {
        tempString.append(Utility::formatNumber(msg.data[i]));
        tempString.append(" ");
    }
    ui->tableIsoFrames->setItem(rowNum, 5, new QTableWidgetItem(tempString));

}


