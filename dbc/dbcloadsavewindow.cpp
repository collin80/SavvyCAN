#include "dbcloadsavewindow.h"
#include "ui_dbcloadsavewindow.h"
#include <QCheckBox>
#include "helpwindow.h"

DBCLoadSaveWindow::DBCLoadSaveWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCLoadSaveWindow)
{
    setWindowFlags(Qt::Window);

    dbcHandler = DBCHandler::getReference();
    referenceFrames = frames;

    ui->setupUi(this);

    QStringList header;
    header << "Filename" << "Associated Bus" << "J1939";
    ui->tableFiles->setColumnCount(3);
    ui->tableFiles->setHorizontalHeaderLabels(header);
    ui->tableFiles->setColumnWidth(0, 265);
    ui->tableFiles->setColumnWidth(1, 125);
    ui->tableFiles->setColumnWidth(2, 80);
    ui->tableFiles->horizontalHeader()->setStretchLastSection(true);

    connect(ui->btnEdit, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::editFile);
    connect(ui->btnLoad, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::loadFile);
    connect(ui->btnMoveDown, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::moveDown);
    connect(ui->btnMoveUp, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::moveUp);
    connect(ui->btnRemove, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::removeFile);
    connect(ui->btnSave, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::saveFile);
    connect(ui->btnNewDBC, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::newFile);
    connect(ui->tableFiles, &QTableWidget::cellChanged, this, &DBCLoadSaveWindow::cellChanged);
    connect(ui->tableFiles, &QTableWidget::cellDoubleClicked, this, &DBCLoadSaveWindow::cellDoubleClicked);

    editorWindow = new DBCMainEditor(frames);

    installEventFilter(this);
}

DBCLoadSaveWindow::~DBCLoadSaveWindow()
{
    removeEventFilter(this);
    delete ui;
}

bool DBCLoadSaveWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("dbc_manager.html");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void DBCLoadSaveWindow::newFile()
{
    int idx = dbcHandler->createBlankFile();
    idx = ui->tableFiles->rowCount();
    ui->tableFiles->insertRow(ui->tableFiles->rowCount());
    ui->tableFiles->setItem(idx, 0, new QTableWidgetItem("UNNAMEDFILE"));
    ui->tableFiles->setItem(idx, 1, new QTableWidgetItem("-1"));

    QTableWidgetItem *item = new QTableWidgetItem("");
    item->setCheckState(Qt::Unchecked);
    ui->tableFiles->setItem(idx, 2, item);
}

void DBCLoadSaveWindow::loadFile()
{
    DBCFile *file = dbcHandler->loadDBCFile(-1);
    if(file) {
        int idx = ui->tableFiles->rowCount();
        ui->tableFiles->insertRow(ui->tableFiles->rowCount());
        ui->tableFiles->setItem(idx, 0, new QTableWidgetItem(file->getFullFilename()));
        ui->tableFiles->setItem(idx, 1, new QTableWidgetItem("-1"));
        DBC_ATTRIBUTE *attr = file->findAttributeByName("isj1939dbc");
        QTableWidgetItem *item = new QTableWidgetItem("");
        ui->tableFiles->setItem(idx, 2, item);
        if (attr && attr->defaultValue > 0)
        {
            item->setCheckState(Qt::Checked);
        }
        else
        {
            item->setCheckState(Qt::Unchecked);
        }
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
    else if (col == 2)
    {
        DBCFile *file = dbcHandler->getFileByIdx(row);
        if (file)
        {
            //int isj1939dbc = ui->tableFiles->item(row, col)->text().toInt();
            bool isj1939dbc = ui->tableFiles->item(row, col)->checkState() == Qt::Checked;
            DBC_ATTRIBUTE *attr = file->findAttributeByName("isj1939dbc");
            if (attr)
            {
                attr->defaultValue = isj1939dbc ? 1 : 0;
                file->messageHandler->setJ1939(isj1939dbc);
            }
            else
            {
                DBC_ATTRIBUTE attr;

                attr.attrType = MESSAGE;
                attr.defaultValue = isj1939dbc ? 1 : 0;
                attr.enumVals.clear();
                attr.lower = 0;
                attr.upper = 0;
                attr.name = "isj1939dbc";
                attr.valType = QINT;
                file->dbc_attributes.append(attr);
                file->messageHandler->setJ1939(isj1939dbc);
            }
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

