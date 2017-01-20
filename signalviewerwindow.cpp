#include "signalviewerwindow.h"
#include "ui_signalviewerwindow.h"

SignalViewerWindow::SignalViewerWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SignalViewerWindow)
{
    ui->setupUi(this);

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

    loadMessages();
}

SignalViewerWindow::~SignalViewerWindow()
{
    delete ui;
}

void SignalViewerWindow::loadMessages()
{
    ui->cbMessages->clear();
    if (dbcHandler == NULL) return;
    if (dbcHandler->getFileCount() == 0) dbcHandler->createBlankFile();
    for (int x = 0; x < dbcHandler->getFileByIdx(0)->messageHandler->getCount(); x++)
    {
        ui->cbMessages->addItem(dbcHandler->getFileByIdx(0)->messageHandler->findMsgByIdx(x)->name);
    }
}

void SignalViewerWindow::loadSignals(int idx)
{
    Q_UNUSED(idx);
    //messages were placed into the list in the same order as they exist
    //in the data structure so it should have been possible to just
    //look it up based on index but by name is probably safer and this operation
    //is not time critical at all.
    DBC_MESSAGE *msg = dbcHandler->getFileByIdx(0)->messageHandler->findMsgByName(ui->cbMessages->currentText());

    if (msg == NULL) return;
    ui->cbSignals->clear();
    for (int x = 0; x < msg->sigHandler->getCount(); x++)
    {
        ui->cbSignals->addItem(msg->sigHandler->findSignalByIdx(x)->name);
    }
}

void SignalViewerWindow::addSignal()
{
    DBC_MESSAGE *msg = dbcHandler->getFileByIdx(0)->messageHandler->findMsgByName(ui->cbMessages->currentText());
    if (!msg) return;
    DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(ui->cbSignals->currentText());
    if (!sig) return;

    signalList.append(sig);

    int rowIdx = ui->tableViewer->rowCount();
    ui->tableViewer->insertRow(rowIdx);
    QTableWidgetItem *item = new QTableWidgetItem(sig->name);
    ui->tableViewer->setItem(rowIdx, 0, item);

}
