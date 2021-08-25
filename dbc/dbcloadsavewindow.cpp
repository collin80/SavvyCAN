#include "dbcloadsavewindow.h"
#include "ui_dbcloadsavewindow.h"
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <qevent.h>
#include "helpwindow.h"
#include "connections/canconmanager.h"

DBCLoadSaveWindow::DBCLoadSaveWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCLoadSaveWindow)
{
    setWindowFlags(Qt::Window);

    dbcHandler = DBCHandler::getReference();
    referenceFrames = frames;

    ui->setupUi(this);

    inhibitCellProcessing = true;

    QStringList header;
    header << "Filename" << "Associated Bus" << "Matching criteria" << "Label filters";
    ui->tableFiles->setColumnCount(4);
    ui->tableFiles->setHorizontalHeaderLabels(header);
    ui->tableFiles->setColumnWidth(0, 265);
    ui->tableFiles->setColumnWidth(1, 125);
    ui->tableFiles->setColumnWidth(2, 120);
    ui->tableFiles->setColumnWidth(3, 90);
    ui->tableFiles->horizontalHeader()->setStretchLastSection(true);

    // Populate table
    for (int idx=0; idx<dbcHandler->getFileCount(); idx++)
    {
        DBCFile * file = dbcHandler->getFileByIdx(idx);
        ui->tableFiles->insertRow(ui->tableFiles->rowCount());
        ui->tableFiles->setItem(idx, 0, new QTableWidgetItem(file->getFilename()));
        QString bus = QString::number(file->getAssocBus() );
        ui->tableFiles->setItem(idx, 1, new QTableWidgetItem(bus));

        QComboBox * mc_item = addMatchingCriteriaCombobox(idx);
        int mc = (int)file->messageHandler->getMatchingCriteria();
        mc_item->setCurrentIndex(mc);

        QTableWidgetItem *item = new QTableWidgetItem("");
        ui->tableFiles->setItem(idx, 3, item);
        bool filterLabeling = file->messageHandler->filterLabeling();
        if (filterLabeling)
        {
            item->setCheckState(Qt::Checked);
        }
        else
        {
            item->setCheckState(Qt::Unchecked);
        }

        qDebug() << "Populate DBC table:" << file->getFullFilename() << " (bus:" << bus << " - Matching Criteria:" << mc 
            << "Filter labeling: " << (filterLabeling?"enabled":"disabled") << ")";
    }

    connect(ui->btnEdit, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::editFile);
    connect(ui->btnLoad, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::loadFile);
    connect(ui->btnMoveDown, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::moveDown);
    connect(ui->btnMoveUp, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::moveUp);
    connect(ui->btnRemove, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::removeFile);
    connect(ui->btnSave, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::saveFile);
    connect(ui->btnNewDBC, &QAbstractButton::clicked, this, &DBCLoadSaveWindow::newFile);
    connect(ui->tableFiles, &QTableWidget::cellChanged, this, &DBCLoadSaveWindow::cellChanged);
    connect(ui->tableFiles, &QTableWidget::cellDoubleClicked, this, &DBCLoadSaveWindow::cellDoubleClicked);

    editorWindow = new DBCMainEditor(frames, this);
    currentlyEditingFile = nullptr;

    inhibitCellProcessing = false;

    installEventFilter(this);
}

QComboBox * DBCLoadSaveWindow::addMatchingCriteriaCombobox(int row)
{
    QComboBox *item = new QComboBox();
    item->addItem("Exact");
    item->addItem("J1939");
    item->addItem("GMLAN");
    ui->tableFiles->setCellWidget(row, 2, item);
    connect(item, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        [this](int box_idx) { matchingCriteriaChanged(box_idx); } );
    return item;
}

DBCLoadSaveWindow::~DBCLoadSaveWindow()
{
    removeEventFilter(this);
    delete ui;
}

void DBCLoadSaveWindow::updateSettings()
{
    QSettings settings;
    int filecount = ui->tableFiles->rowCount();
    settings.setValue("DBC/FileCount", filecount);
    for (int i=0; i<filecount; i++)
    {
        DBCFile * file = dbcHandler->getFileByIdx(i);
        if (file)
        {
            qDebug() << "Save DBC settings #" << i << " File: " << file->getFullFilename() 
                << "Bus: " << file->getAssocBus() << "MC: " << file->messageHandler->getMatchingCriteria()
                << "Filter Labeling: " << (file->messageHandler->filterLabeling() ? "enabled" : "disabled");
            settings.setValue("DBC/Filename_" + QString(i), file->getFullFilename());
            settings.setValue("DBC/AssocBus_" + QString(i), file->getAssocBus());
            settings.setValue("DBC/MatchingCriteria_" + QString(i), file->messageHandler->getMatchingCriteria());            
            settings.setValue("DBC/FilterLabeling_" + QString(i), file->messageHandler->filterLabeling());   
        }
    }
    emit updatedDBCSettings();
}

bool DBCLoadSaveWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("dbc_manager.md");
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

    QComboBox * mc_item = addMatchingCriteriaCombobox(idx);
    mc_item->setCurrentIndex(EXACT);

    QTableWidgetItem *item = new QTableWidgetItem("");
    item->setCheckState(Qt::Checked);
    ui->tableFiles->setItem(idx, 3, item);    
}

void DBCLoadSaveWindow::loadFile()
{
    DBCFile *file = nullptr;
    QString filename;
    QFileDialog dialog;
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("DBC File (*.dbc)")));
    filters.append(QString(tr("Tesla JSON File (*.json)")));
    filters.append(QString(tr("Secret CSV Signal Defs (*.csv)")));

    dialog.setDirectory(settings.value("DBC/LoadSaveDirectory", dialog.directory().path()).toString());
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        //right now there is only one file type that can be loaded here so just do it.
        settings.setValue("DBC/LoadSaveDirectory", dialog.directory().path());

        if (dialog.selectedNameFilter() == filters[0])
        {
            if (!filename.contains('.')) filename += ".dbc";
            file = dbcHandler->loadDBCFile(filename);
        }
        if (dialog.selectedNameFilter() == filters[1])
        {
            if (!filename.contains('.')) filename += ".json";
            file = dbcHandler->loadJSONFile(filename);
        }
        if (dialog.selectedNameFilter() == filters[2])
        {
            if (!filename.contains('.')) filename += ".csv";
            file = dbcHandler->loadSecretCSVFile(filename);
        }
    }

    if(file) {
        inhibitCellProcessing=true;
        int idx = ui->tableFiles->rowCount();
        ui->tableFiles->insertRow(ui->tableFiles->rowCount());
        ui->tableFiles->setItem(idx, 0, new QTableWidgetItem(file->getFilename()));
        ui->tableFiles->setItem(idx, 1, new QTableWidgetItem("-1"));

        DBC_ATTRIBUTE *attr = file->findAttributeByName("matchingcriteria");
        QComboBox * mc_item = addMatchingCriteriaCombobox(idx);
        if (attr && attr->defaultValue > 0)
        {
            mc_item->setCurrentIndex(attr->defaultValue.toInt());
        }

        attr = file->findAttributeByName("filterlabeling");
        QTableWidgetItem *item = new QTableWidgetItem("");
        ui->tableFiles->setItem(idx, 3, item);
        if (attr && attr->defaultValue > 0)
        {
            item->setCheckState(Qt::Checked);
        }
        else
        {
            item->setCheckState(Qt::Unchecked);
        }
        inhibitCellProcessing=false;

        updateSettings();
    }
}

void DBCLoadSaveWindow::loadJSON()
{

}

void DBCLoadSaveWindow::saveFile()
{
    int idx = ui->tableFiles->currentRow();
    if (idx < 0) return;
    dbcHandler->saveDBCFile(idx);
    //then update the list to show the new file name (if it changed)
    ui->tableFiles->setItem(idx, 0, new QTableWidgetItem(dbcHandler->getFileByIdx(idx)->getFilename()));
}

void DBCLoadSaveWindow::removeFile()
{
    bool bContinue = true;
    int idx = ui->tableFiles->currentRow();
    if (idx < 0) return;

    if (currentlyEditingFile == dbcHandler->getFileByIdx(idx))
    {
        bContinue = false;
        QMessageBox::StandardButton confirmDialog;
        confirmDialog = QMessageBox::question(this, "Confirm Deletion", "This DBC is currently open for editing.\nMake sure you've saved any changes!\nAre you sure you want to remove this DBC?",
                                          QMessageBox::Yes|QMessageBox::No);
        if (confirmDialog == QMessageBox::Yes) bContinue = true;
    }

    if (bContinue)
    {
        editorWindow->close();
        dbcHandler->removeDBCFile(idx);
        ui->tableFiles->removeRow(idx);
    }
    updateSettings();
}

void DBCLoadSaveWindow::moveUp()
{
    int idx = ui->tableFiles->currentRow();
    if (idx < 1) return;
    dbcHandler->swapFiles(idx - 1, idx);
    swapTableRows(true);
    updateSettings();
}

void DBCLoadSaveWindow::moveDown()
{
    int idx = ui->tableFiles->currentRow();
    if (idx < 0) return;
    if (idx > (dbcHandler->getFileCount() - 2)) return;
    dbcHandler->swapFiles(idx, idx + 1);
    swapTableRows(false);
    updateSettings();
}

void DBCLoadSaveWindow::editFile()
{
    int idx = ui->tableFiles->currentRow();
    if (idx < 0) return;

    editorWindow->setFileIdx(idx);
    editorWindow->show();
}

void DBCLoadSaveWindow::matchingCriteriaChanged(int index)
{
    Q_UNUSED(index)

    if (inhibitCellProcessing) return;
    // We don't know which combobox changed, so we just update all of them
    for (int row=0; row<ui->tableFiles->rowCount(); row++)
    {
        DBCFile *file = dbcHandler->getFileByIdx(row);
        if (file)
        {
            QComboBox *item = (QComboBox*)ui->tableFiles->cellWidget(row, 2);
            MatchingCriteria_t matchingCriteria = (MatchingCriteria_t) item->currentIndex();
            DBC_ATTRIBUTE *attr = file->findAttributeByName("matchingcriteria");
            if (attr)
            {
                attr->defaultValue = matchingCriteria;
                file->messageHandler->setMatchingCriteria(matchingCriteria);
            }
            else
            {
                DBC_ATTRIBUTE attr;

                attr.attrType = ATTR_TYPE_MESSAGE;
                attr.defaultValue = matchingCriteria;
                attr.enumVals.clear();
                attr.lower = 0;
                attr.upper = 0;
                attr.name = "matchingcriteria";
                attr.valType = ATTR_INT;
                file->dbc_attributes.append(attr);
                file->messageHandler->setMatchingCriteria(matchingCriteria);
            }
        }
    }
    updateSettings();
}


void DBCLoadSaveWindow::cellChanged(int row, int col)
{
    if (inhibitCellProcessing) return;
    if (col == 1) //the bus column
    {
        DBCFile *file = dbcHandler->getFileByIdx(row);
        int bus = ui->tableFiles->item(row, col)->text().toInt();
        //int numBuses = CANConManager::getInstance()->getNumBuses();
        if (bus > -2)
        {
            file->setAssocBus(bus);
        }
        updateSettings();
    } 
    else if (col == 3) // labelfilters
    {
        DBCFile *file = dbcHandler->getFileByIdx(row);
        if (file)
        {
            bool labelFilters = ui->tableFiles->item(row, col)->checkState() == Qt::Checked;
            DBC_ATTRIBUTE *attr = file->findAttributeByName("filterlabeling");
            if (attr)
            {
                attr->defaultValue = labelFilters ? 1 : 0;
                file->messageHandler->setFilterLabeling(labelFilters);
            }
            else
            {
                DBC_ATTRIBUTE attr;

                attr.attrType = ATTR_TYPE_MESSAGE;
                attr.defaultValue = labelFilters ? 1 : 0;
                attr.enumVals.clear();
                attr.lower = 0;
                attr.upper = 0;
                attr.name = "labelfilters";
                attr.valType = ATTR_INT;
                file->dbc_attributes.append(attr);
                file->messageHandler->setFilterLabeling(labelFilters);
            }
            updateSettings();
        }        
    }
}

void DBCLoadSaveWindow::cellDoubleClicked(int row, int col)
{
    Q_UNUSED(col)
    currentlyEditingFile = dbcHandler->getFileByIdx(row);
    editorWindow->setFileIdx(row);
    editorWindow->show();
}

void DBCLoadSaveWindow::swapTableRows(bool up)
{
    int idx = ui->tableFiles->currentRow();
    const int destIdx = (up ? idx-1 : idx+1);
    Q_ASSERT(destIdx >= 0 && destIdx < ui->tableFiles->rowCount());

    inhibitCellProcessing = true;
    // take whole rows
    QList<QTableWidgetItem*> sourceItems = takeRow(idx);
    QList<QTableWidgetItem*> destItems = takeRow(destIdx);

    // QCombobox needs separate handling
    int sourceMC = ((QComboBox*)ui->tableFiles->cellWidget(idx,2))->currentIndex();
    int destMC = ((QComboBox*)ui->tableFiles->cellWidget(destIdx,2))->currentIndex();

    // set back in reverse order
    setRow(idx, destItems);
    setRow(destIdx, sourceItems);

    ((QComboBox*)ui->tableFiles->cellWidget(idx,2))->setCurrentIndex(destMC);
    ((QComboBox*)ui->tableFiles->cellWidget(destIdx,2))->setCurrentIndex(sourceMC);

    inhibitCellProcessing = false;
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

