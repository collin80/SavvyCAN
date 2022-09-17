#include "signalviewerwindow.h"
#include "ui_signalviewerwindow.h"
#include "helpwindow.h"
#include "mainwindow.h"
#include <QDebug>

SignalViewerWindow::SignalViewerWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SignalViewerWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    modelFrames = frames;

    QStringList headers;
    headers << "Signal" << "Value";
    ui->tableViewer->setHorizontalHeaderLabels(headers);
    ui->tableViewer->setColumnWidth(0, 150);
    ui->tableViewer->setColumnWidth(1, 300);
    QHeaderView *HorzHdr = ui->tableViewer->horizontalHeader();
    HorzHdr->setStretchLastSection(true); //causes the data column to automatically fill the tableview

    dbcHandler = DBCHandler::getReference();

    connect(ui->cbMessages, SIGNAL(currentIndexChanged(int)), this, SLOT(loadSignals(int)));
    connect(ui->btnAdd, SIGNAL(clicked(bool)), this, SLOT(addSignal()));
    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    connect(ui->btnRemove, SIGNAL(clicked(bool)), this, SLOT(removeSelectedSignal()));

    loadMessages();
}

SignalViewerWindow::~SignalViewerWindow()
{
    delete ui;
}

void SignalViewerWindow::updatedFrames(int numFrames)
{
    CANFrame thisFrame;

    if (numFrames == -1) //all frames deleted. Don't care
    {
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        for (int i = 0; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);
            processFrame(thisFrame);
        }
    }
    else //just got some new frames. See if they are relevant.
    {
        if (numFrames > modelFrames->count()) return;

        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);
            processFrame(thisFrame);
        }
    }
}

void SignalViewerWindow::processFrame(CANFrame &frame)
{
    QString sigString;
    DBC_SIGNAL *sig;
    for (int i = 0; i < signalList.count(); i++)
    {
        sig = signalList.at(i);
        if (!sig) return;
        if (sig->parentMessage->ID == frame.frameId())
        {
            if (sig->processAsText(frame, sigString, false)) //if true we could interpret the signal so update it in the list
            {
                QTableWidgetItem *item = ui->tableViewer->item(i, 1);
                if (!item)
                {
                    item = new QTableWidgetItem(sigString);
                    ui->tableViewer->setItem(i, 1, item);
                }
                else item->setText(sigString);
            }
        }
    }
}

void SignalViewerWindow::removeSelectedSignal()
{
    int selRow = ui->tableViewer->currentRow();
    if (selRow < 0) return; //no selected row
    signalList.removeAt(selRow);
    ui->tableViewer->removeRow(selRow);
}

void SignalViewerWindow::loadMessages()
{
    int numFiles;
    ui->cbMessages->clear();
    if (dbcHandler == nullptr) return;
    if ((numFiles = dbcHandler->getFileCount()) == 0) return;
    qDebug() << numFiles;
    for (int f = 0; f < numFiles; f++)
    {
        qDebug() << dbcHandler->getFileByIdx(f)->messageHandler->getCount();

        for (int x = 0; x < dbcHandler->getFileByIdx(f)->messageHandler->getCount(); x++)
        {
            ui->cbMessages->addItem(dbcHandler->getFileByIdx(f)->messageHandler->findMsgByIdx(x)->name);
        }
    }
}

void SignalViewerWindow::loadSignals(int idx)
{
    Q_UNUSED(idx);
    //messages were placed into the list in the same order as they exist
    //in the data structure so it should have been possible to just
    //look it up based on index but by name is probably safer and this operation
    //is not time critical at all.
    DBC_MESSAGE *msg = dbcHandler->findMessage(ui->cbMessages->currentText());

    if (msg == nullptr) return;
    ui->cbSignals->clear();
    for (int x = 0; x < msg->sigHandler->getCount(); x++)
    {
        ui->cbSignals->addItem(msg->sigHandler->findSignalByIdx(x)->name);
    }
}

void SignalViewerWindow::addSignal()
{
    DBC_MESSAGE *msg = dbcHandler->findMessage(ui->cbMessages->currentText());
    if (!msg) return;
    DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(ui->cbSignals->currentText());
    if (!sig) return;

    signalList.append(sig);

    int rowIdx = ui->tableViewer->rowCount();
    ui->tableViewer->insertRow(rowIdx);
    QTableWidgetItem *item = new QTableWidgetItem(msg->sender->name + " - " + sig->name);
    ui->tableViewer->setItem(rowIdx, 0, item);

}
