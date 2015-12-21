#ifndef CONNECTIONWINDOW_H
#define CONNECTIONWINDOW_H

#include <QSerialPortInfo>
#include <QDialog>
#include <QDebug>

namespace Ui {
class ConnectionWindow;
}

namespace ConnectionType
{
    enum ConnectionType
    {
        GVRET_SERIAL,
        KVASER,
        SOCKETCAN
    };
}

class ConnectionWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectionWindow(QWidget *parent = 0);
    ~ConnectionWindow();
    void showEvent(QShowEvent *);
    int getSpeed0();
    int getSpeed1();
    QString getPortName(); //name of port to connect to
    ConnectionType::ConnectionType getConnectionType();

signals:
    void updateConnectionSettings(QString connectionType, QString port, int speed0, int speed1);

public slots:
    void setSpeeds(int speed0, int speed1);

private slots:
    void handleOKButton();

private:
    Ui::ConnectionWindow *ui;
    QList<QSerialPortInfo> ports;

    void getSerialPorts();
    void getKvaserPorts();
    void getSocketcanPorts();
};

#endif // CONNECTIONWINDOW_H
