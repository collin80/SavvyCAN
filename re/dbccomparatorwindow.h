#ifndef DBCCOMPARATORWINDOW_H
#define DBCCOMPARATORWINDOW_H

#include <QDialog>
#include <QDebug>
#include <QTreeWidget>
#include "framefileio.h"
#include "dbc/dbc_classes.h"
#include "dbc/dbchandler.h"
#include "utility.h"

namespace Ui {
class DBCComparatorWindow;
}

class DBCComparatorWindow : public QDialog
{
    Q_OBJECT

public:
    explicit DBCComparatorWindow(QWidget *parent = 0);
    ~DBCComparatorWindow();

private slots:
    void loadFirstFile();
    void loadSecondFile();
    void saveDetails();

private:
    Ui::DBCComparatorWindow *ui;
    DBCFile *firstDBC;
    DBCFile *secondDBC;
    QString firstDBCFilename;
    QString secondDBCFilename;

    void calculateDetails();
    void showEvent(QShowEvent *);
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    QString loadDBC(DBCFile **file);
    void readSettings();
    void writeSettings();
};

#endif //DBCCOMPARATORWINDOW_H
