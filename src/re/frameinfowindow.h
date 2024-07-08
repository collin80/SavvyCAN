#ifndef FRAMEINFOWINDOW_H
#define FRAMEINFOWINDOW_H

#include "bus_protocols/j1939_handler.h"
#include "dbc/dbchandler.h"
#include "main/can_structs.h"
#include "main/candatagrid.h"
#include <QDialog>
#include <QFile>
#include <QListWidget>
#include <QTreeWidget>

#include "main/qcustomplot.h"

namespace Ui
{
class FrameInfoWindow;
}

class FrameInfoWindow : public QDialog
{
  Q_OBJECT

public:
  explicit FrameInfoWindow(const QVector<CANFrame>* frames, QWidget* parent = 0);
  ~FrameInfoWindow();
  void showEvent(QShowEvent*);

private slots:
  void updateDetailsWindow(QString);
  void updatedFrames(int);
  void saveDetails();
  void mousePress();
  void mouseWheel();
  void mouseDoubleClick();

private:
  Ui::FrameInfoWindow* ui;
  QCustomPlot* graphByte[8];
  QCustomPlot* graphHistogram;
  CANDataGrid* heatmap;

  QList<int> foundID;
  QList<CANFrame> frameCache;
  const QVector<CANFrame>* modelFrames;
  bool useOpenGL;
  bool useHexTicker;
  static const QColor byteGraphColors[8];
  static QPen bytePens[8];
  DBCHandler* dbcHandler;

  QCPGraph* graphRef[8];

  void refreshIDList();
  void closeEvent(QCloseEvent* event);
  bool eventFilter(QObject* obj, QEvent* event);
  void setupByteGraph(QCustomPlot* plot, int num);
  void readSettings();
  void writeSettings();
  void dumpNode(QTreeWidgetItem* item, QFile* file, int indent);
};

#endif  // FRAMEINFOWINDOW_H
