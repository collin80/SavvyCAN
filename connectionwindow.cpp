#include "connectionwindow.h"
#include "ui_connectionwindow.h"

ConnectionWindow::ConnectionWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectionWindow)
{
    ui->setupUi(this);

    getSerialPorts();

    ui->cbSpeed0->addItem(tr("<Default>"));
    ui->cbSpeed0->addItem(tr("Disabled"));
    ui->cbSpeed0->addItem(tr("125000"));
    ui->cbSpeed0->addItem(tr("250000"));
    ui->cbSpeed0->addItem(tr("500000"));
    ui->cbSpeed0->addItem(tr("1000000"));
    ui->cbSpeed0->addItem(tr("33333"));

    ui->cbSpeed1->addItem(tr("<Default>"));
    ui->cbSpeed1->addItem(tr("Disabled"));
    ui->cbSpeed1->addItem(tr("125000"));
    ui->cbSpeed1->addItem(tr("250000"));
    ui->cbSpeed1->addItem(tr("500000"));
    ui->cbSpeed1->addItem(tr("1000000"));
    ui->cbSpeed1->addItem(tr("33333"));
}

ConnectionWindow::~ConnectionWindow()
{
    delete ui;
}

void ConnectionWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    qDebug() << "Show connectionwindow";
    if (ui->rbGVRET->isChecked()) getSerialPorts();
    if (ui->rbKvaser->isChecked()) getKvaserPorts();
    if (ui->rbSocketCAN->isChecked()) getSocketcanPorts();
}

void ConnectionWindow::getSerialPorts()
{
    ui->cbPort->clear();
    ports = QSerialPortInfo::availablePorts();

    for (int i = 0; i < ports.count(); i++)
    {
        ui->cbPort->addItem(ports[i].portName());
    }
}

void ConnectionWindow::getKvaserPorts()
{

}

void ConnectionWindow::getSocketcanPorts()
{

}

void ConnectionWindow::setSpeeds(int speed0, int speed1)
{
    bool found = false;

    for (int i = 0; i < ui->cbSpeed0->count(); i++)
    {
        if (ui->cbSpeed0->itemText(i).toInt() == speed0)
        {
            ui->cbSpeed0->setCurrentIndex(i);
            found = true;
        }
    }
    if (!found)
    {
        ui->cbSpeed0->addItem(QString::number(speed0));
        ui->cbSpeed0->setCurrentIndex(ui->cbSpeed0->count() - 1);
    }

    found = false;
    for (int i = 0; i < ui->cbSpeed1->count(); i++)
    {
        if (ui->cbSpeed1->itemText(i).toInt() == speed1)
        {
            ui->cbSpeed1->setCurrentIndex(i);
            found = true;
        }
    }
    if (!found)
    {
        ui->cbSpeed1->addItem(QString::number(speed1));
        ui->cbSpeed1->setCurrentIndex(ui->cbSpeed1->count() - 1);
    }
}


//-1 means leave it at whatever it booted up to. 0 means disable. Otherwise the actual rate we want.
int ConnectionWindow::getSpeed0()
{
    switch (ui->cbSpeed0->currentIndex())
    {
    case -1:
        return -1;
        break;
    case 0:
        return -1;
        break;
    case 1:
        return 0;
        break;
    default:
        return (ui->cbSpeed0->currentText().toInt());
        break;
    }
}

int ConnectionWindow::getSpeed1()
{
    switch (ui->cbSpeed1->currentIndex())
    {
    case -1:
        return -1;
        break;
    case 0:
        return -1;
        break;
    case 1:
        return 0;
        break;
    default:
        return (ui->cbSpeed1->currentText().toInt());
        break;
    }
}

QString ConnectionWindow::getPortName()
{
    return (ui->cbPort->currentText());
}

ConnectionType::ConnectionType ConnectionWindow::getConnectionType()
{
    if (ui->rbGVRET->isChecked()) return ConnectionType::GVRET_SERIAL;
    if (ui->rbKvaser->isChecked()) return ConnectionType::KVASER;
    if (ui->rbSocketCAN->isChecked()) return ConnectionType::SOCKETCAN;
}
