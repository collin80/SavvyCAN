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
}

SignalViewerWindow::~SignalViewerWindow()
{
    delete ui;
}
