#ifndef DBCSIGNALEDITOR_H
#define DBCSIGNALEDITOR_H

#include "dbc/dbchandler.h"
#include "main/utility.h"
#include <QDialog>

namespace Ui
{
class DBCSignalEditor;
}

class DBCSignalEditor : public QDialog
{
  Q_OBJECT

public:
  explicit DBCSignalEditor(QWidget* parent = 0);
  void setMessageRef(DBC_MESSAGE* msg);
  void setFileIdx(int idx);
  void setSignalRef(DBC_SIGNAL* sig);
  void showEvent(QShowEvent*);
  ~DBCSignalEditor();
  void refreshView();

signals:
  void updatedTreeInfo(DBC_SIGNAL* sig);

private slots:
  void bitfieldLeftClicked(int bit);
  void bitfieldRightClicked(int bit);
  void onValuesCellChanged(int row, int col);
  void onCustomMenuValues(QPoint);
  void deleteCurrentValue();

private:
  Ui::DBCSignalEditor* ui;
  DBCHandler* dbcHandler;
  DBC_MESSAGE* dbcMessage;
  DBC_SIGNAL* currentSignal;
  QList<DBC_SIGNAL> undoBuffer;
  DBCFile* dbcFile;
  bool inhibitCellChanged;
  bool inhibitMsgProc;

  void fillSignalForm(DBC_SIGNAL* sig);
  void fillValueTable(DBC_SIGNAL* sig);
  void generateUsedBits();
  void refreshBitGrid();

  void closeEvent(QCloseEvent* event);
  bool eventFilter(QObject* obj, QEvent* event);
  void readSettings();
  void writeSettings();
  void pushToUndoBuffer();
  void popFromUndoBuffer();
};

#endif  // DBCSIGNALEDITOR_H
