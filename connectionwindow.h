#ifndef CONNECTIONWINDOW_H
#define CONNECTIONWINDOW_H

#include "canconnectionmodel.h"

#include <QSerialPortInfo>
#include <QDialog>
#include <QDebug>
#include <QSettings>
#include "canconnection.h"
#include "serialworker.h"

class CANConnectionModel;

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
    explicit ConnectionWindow(CANFrameModel *canModel, QWidget *parent = 0);
    ~ConnectionWindow();
    void showEvent(QShowEvent *);
    int getSpeed();
    QString getPortName(); //name of port to connect to
    ConnectionType::ConnectionType getConnectionType();
    bool getSWMode();

signals:
    void updateBusSettings(CANBus *bus);
    void updatePortName(QString port);

public slots:
    void setSpeed(int speed0);
    void setSWMode(bool mode);
    void sendFrame(const CANFrame *);
    void sendFrameBatch(const QList<CANFrame> *);

private slots:
    void handleOKButton();
    void handleConnTypeChanged();
    void handleConnSelectionChanged();
    void handleRemoveConn();
    void handleEnableAll();
    void handleDisableAll();
    void handleRevert();
    void handleNewConn();
    void receiveBusStatus(int bus, int speed, int status);
    void connectionSuccess(CANConnection *conn);

private:
    Ui::ConnectionWindow *ui;
    QList<QSerialPortInfo> ports;
    QSettings *settings;
    CANConnectionModel *connModel;
    CANFrameModel *canModel;


    void getSerialPorts();
    void getKvaserPorts();
    void getSocketcanPorts();
    bool isSocketCanAvailable();
};

#endif // CONNECTIONWINDOW_H
