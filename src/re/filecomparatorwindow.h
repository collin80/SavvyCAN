#ifndef FILECOMPARATORWINDOW_H
#define FILECOMPARATORWINDOW_H

#include "dbc/dbchandler.h"
#include "main/can_structs.h"
#include "main/framefileio.h"
#include "main/utility.h"
#include <QDebug>
#include <QDialog>
#include <QTreeWidget>

namespace Ui
{
class FileComparatorWindow;
}

struct FrameData
{
  uint32_t ID;
  int dataLen;
  uint64_t bitmap;
  int values[8][256];  // first index is the data byte, second is # of times we saw that value
  QHash<QString, QList<QString>> signalInstances;
};

class FileComparatorWindow : public QDialog
{
  Q_OBJECT

public:
  explicit FileComparatorWindow(QWidget* parent = 0);
  ~FileComparatorWindow();

private slots:
  void loadInterestedFile();
  void loadReferenceFile();
  void clearReference();
  void saveDetails();

private:
  Ui::FileComparatorWindow* ui;
  QVector<CANFrame> interestedFrames;
  QVector<CANFrame> referenceFrames;
  QString interestedFilename;
  DBCHandler* dbcHandler;

  void calculateDetails();
  void showEvent(QShowEvent*);
  void closeEvent(QCloseEvent* event);
  bool eventFilter(QObject* obj, QEvent* event);
  void readSettings();
  void writeSettings();
};

#endif  // FILECOMPARATORWINDOW_H
