#ifndef CONNECTIONWINDOW_H
#define CONNECTIONWINDOW_H

#include <QSerialPortInfo>
#include <QDialog>
#include <QDebug>
#include <QSettings>

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
    bool getCAN1SWMode();

signals:
    void updateConnectionSettings(QString connectionType, QString port, int speed0, int speed1);

public slots:
    void setSpeeds(int speed0, int speed1);
    void setCAN1SWMode(bool mode);

private slots:
    void handleOKButton();
    void handleConnTypeChanged();

private:
    Ui::ConnectionWindow *ui;
    QList<QSerialPortInfo> ports;
    QSettings *settings;

    ConnectionType::ConnectionType currentConnType;
    QString currentPortName;
    int currentSpeed1, currentSpeed2;

    void getSerialPorts();
    void getKvaserPorts();
    void getSocketcanPorts();
};

#endif // CONNECTIONWINDOW_H
