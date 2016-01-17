#ifndef DBCLOADSAVEWINDOW_H
#define DBCLOADSAVEWINDOW_H

#include <QDialog>
#include <QTableWidget>
#include "dbchandler.h"
#include "dbcmaineditor.h"

namespace Ui {
class DBCLoadSaveWindow;
}

class DBCLoadSaveWindow : public QDialog
{
    Q_OBJECT

public:
    explicit DBCLoadSaveWindow(DBCHandler *handler, const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~DBCLoadSaveWindow();

private slots:
    void loadFile();
    void saveFile();
    void removeFile();
    void moveUp();
    void moveDown();
    void editFile();

private:
    Ui::DBCLoadSaveWindow *ui;
    DBCHandler *dbcHandler;
    const QVector<CANFrame> *referenceFrames;
    DBCMainEditor *editorWindow;

    void swapTableRows(bool up);
    QList<QTableWidgetItem*> takeRow(int row);
    void setRow(int row, const QList<QTableWidgetItem*>& rowItems);
};

#endif // DBCLOADSAVEWINDOW_H

