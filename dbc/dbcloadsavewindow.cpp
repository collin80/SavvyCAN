#include "dbcloadsavewindow.h"
#include "ui_dbcloadsavewindow.h"

DBCLoadSaveWindow::DBCLoadSaveWindow(DBCHandler *handler, const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCLoadSaveWindow)
{

    dbcHandler = handler;
    referenceFrames = frames;

    ui->setupUi(this);

    QStringList header;
    header << "Filename" << "Associated Bus";
    ui->tableFiles->setColumnCount(2);
    ui->tableFiles->setHorizontalHeaderLabels(header);
    ui->tableFiles->setColumnWidth(0, 355);
    ui->tableFiles->setColumnWidth(1, 125);

    connect(ui->btnEdit, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::editFile);
    connect(ui->btnLoad, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::loadFile);
    connect(ui->btnMoveDown, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::moveDown);
    connect(ui->btnMoveUp, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::moveUp);
    connect(ui->btnRemove, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::removeFile);
    connect(ui->btnSave, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::saveFile);
    connect(ui->btnNewDBC, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::newFile);
    connect(ui->tableFiles, &QTableWidget::cellChanged, this, &DBCLoadSaveWindow::cellChanged);
    connect(ui->tableFiles, &QTableWidget::cellDoubleClicked, this, &DBCLoadSaveWindow::cellDoubleClicked);

    editorWindow = new DBCMainEditor(handler, frames);
}

DBCLoadSaveWindow::~DBCLoadSaveWindow()
{
    delete ui;
}

void DBCLoadSaveWindow::newFile()
{
    int idx = dbcHandler->createBlankFile();
    idx = ui->tableFiles->rowCount();
    ui->tableFiles->insertRow(ui->tableFiles->rowCount());
    ui->tableFiles->setItem(idx, 0, new QTableWidgetItem("UNNAMEDFILE"));
    ui->tableFiles->setItem(idx, 1, new QTableWidgetItem("-1"));
}

void DBCLoadSaveWindow::loadFile()
{
    DBCFile *file = dbcHandler->loadDBCFile(-1);
    if(file) {
        int idx = ui->tableFiles->rowCount();
        ui->tableFiles->insertRow(ui->tableFiles->rowCount());
        ui->tableFiles->setItem(idx, 0, new QTableWidgetItem(file->getFullFilename()));
        ui->tableFiles->setItem(idx, 1, new QTableWidgetItem("-1"));
    }
}

void DBCLoadSaveWindow::saveFile()
{
    int idx = ui->tableFiles->currentRow();
    if (idx < 0) return;
    dbcHandler->saveDBCFile(idx);
    //then update the list to show the new file name (if it changed)
    ui->tableFiles->setItem(idx, 0, new QTableWidgetItem(dbcHandler->getFileByIdx(idx)->getFullFilename()));
}

void DBCLoadSaveWindow::removeFile()
{
    int idx = ui->tableFiles->currentRow();
    if (idx < 0) return;
    dbcHandler->removeDBCFile(idx);
    ui->tableFiles->removeRow(idx);
}

void DBCLoadSaveWindow::moveUp()
{
    int idx = ui->tableFiles->currentRow();
    if (idx < 1) return;
    dbcHandler->swapFiles(idx - 1, idx);
    swapTableRows(true);
}

void DBCLoadSaveWindow::moveDown()
{
    int idx = ui->tableFiles->currentRow();
    if (idx < 0) return;
    if (idx > (dbcHandler->getFileCount() - 2)) return;
    dbcHandler->swapFiles(idx, idx + 1);
    swapTableRows(false);
}

void DBCLoadSaveWindow::editFile()
{
    int idx = ui->tableFiles->currentRow();
    if (idx < 0) return;

    editorWindow->setFileIdx(idx);
    editorWindow->show();
}

void DBCLoadSaveWindow::cellChanged(int row, int col)
{
    if (col == 1) //the bus column
    {
        DBCFile *file = dbcHandler->getFileByIdx(row);
        int bus = ui->tableFiles->item(row, col)->text().toInt();
        if (bus > -2 && bus < 2)
        {
            file->setAssocBus(bus);
        }
    }
}

void DBCLoadSaveWindow::cellDoubleClicked(int row, int col)
{
    Q_UNUSED(col)
    editorWindow->setFileIdx(row);
    editorWindow->show();
}

void DBCLoadSaveWindow::swapTableRows(bool up)
{
    int idx = ui->tableFiles->currentRow();
    const int destIdx = (up ? idx-1 : idx+1);
    Q_ASSERT(destIdx >= 0 && destIdx < ui->tableFiles->rowCount());

    // take whole rows
    QList<QTableWidgetItem*> sourceItems = takeRow(idx);
    QList<QTableWidgetItem*> destItems = takeRow(destIdx);

    // set back in reverse order
    setRow(idx, destItems);
    setRow(destIdx, sourceItems);
}

QList<QTableWidgetItem*> DBCLoadSaveWindow::takeRow(int row)
{
    QList<QTableWidgetItem*> rowItems;
    for (int col = 0; col < ui->tableFiles->columnCount(); ++col)
    {
        rowItems << ui->tableFiles->takeItem(row, col);
    }
    return rowItems;
}

void DBCLoadSaveWindow::setRow(int row, const QList<QTableWidgetItem*>& rowItems)
{
    for (int col = 0; col < ui->tableFiles->columnCount(); ++col)
    {
        ui->tableFiles->setItem(row, col, rowItems.at(col));
    }
}

