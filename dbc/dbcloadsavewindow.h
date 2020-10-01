#ifndef DBCLOADSAVEWINDOW_H
#define DBCLOADSAVEWINDOW_H

#include <QDialog>
#include <QTableWidget>
#include <QComboBox>
#include "dbchandler.h"
#include "dbcmaineditor.h"

namespace Ui {
class DBCLoadSaveWindow;
}

class DBCLoadSaveWindow : public QDialog
{
    Q_OBJECT

public:
    explicit DBCLoadSaveWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~DBCLoadSaveWindow();

private slots:
    void loadFile();
    void loadJSON();
    void saveFile();
    void removeFile();
    void moveUp();
    void moveDown();
    void editFile();
    void cellChanged(int row, int col);
    void cellDoubleClicked(int row, int col);
    void matchingCriteriaChanged(int index);
    void newFile();

signals:
    void updatedDBCSettings();

private:
    Ui::DBCLoadSaveWindow *ui;
    DBCHandler *dbcHandler;
    DBCFile *currentlyEditingFile;
    const QVector<CANFrame> *referenceFrames;
    DBCMainEditor *editorWindow;
    bool inhibitCellProcessing;

    void swapTableRows(bool up);
    QList<QTableWidgetItem*> takeRow(int row);
    void setRow(int row, const QList<QTableWidgetItem*>& rowItems);
    bool eventFilter(QObject *obj, QEvent *event);
    void updateSettings();
    QComboBox * addMatchingCriteriaCombobox(int row);
};

#endif // DBCLOADSAVEWINDOW_H

