#ifndef CONNECTIONWINDOW_H
#define CONNECTIONWINDOW_H

#include "connections/canconnection.h"
#include "connections/canconnectionmodel.h"

#include <QCanBusDeviceInfo>
#include <QDebug>
#include <QDialog>
#include <QItemSelection>
#include <QSerialPortInfo>
#include <QSettings>
#include <QTimer>
#include <QUdpSocket>

class CANConnectionModel;

namespace Ui
{
class ConnectionWindow;
}

class ConnectionWindow : public QDialog
{
  Q_OBJECT

public:
  explicit ConnectionWindow(QWidget* parent = 0);
  ~ConnectionWindow();

signals:
  void updateBusSettings(CANBus* bus);
  void updatePortName(QString port);
  void sendDebugData(QByteArray bytes);

public slots:
  void getDebugText(QString debugText);
  void setSuspendAll(bool pSuspend);

private slots:
  void currentRowChanged(const QModelIndex& current, const QModelIndex& previous);
  void currentTabChanged(int newIdx);
  void consoleEnableChanged(bool checked);
  void handleRemoveConn();
  void handleNewConn();
  void handleResetConn();
  void handleClearDebugText();
  void handleSendHex();
  void handleSendText();
  void saveBusSettings();
  void moveConnUp();
  void moveConnDown();
  void connectionStatus(CANConStatus);
  void readPendingDatagrams();

private:
  Ui::ConnectionWindow* ui;
  QSettings* settings;
  CANConnectionModel* connModel;
  QUdpSocket* rxBroadcastGVRET;
  QUdpSocket* rxBroadcastKayak;
  QVector<QString> remoteDeviceIPGVRET;
  QVector<QString> remoteDeviceKayak;

  CANConnection* create(CANCon::type pTye, QString pPortName, QString pDriver, int pSerialSpeed, int pBusSpeed,
                        bool pCanFd, int pDataRate);
  void populateBusDetails(int offset);
  void loadConnections();
  void saveConnections();
  void showEvent(QShowEvent*);
  void closeEvent(QCloseEvent* event);
  bool eventFilter(QObject* obj, QEvent* event);
  void readSettings();
  void writeSettings();
};

#endif  // CONNECTIONWINDOW_H
