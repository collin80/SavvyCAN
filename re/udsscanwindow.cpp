#include "udsscanwindow.h"
#include "ui_udsscanwindow.h"
#include "mainwindow.h"
#include "connections/canconmanager.h"
#include "bus_protocols/uds_handler.h"
#include "utility.h"
#include "helpwindow.h"


static QVector<QString> SCANTYPE_NAMES = {
    QString("Tester Present"),
    QString("Session Control"),
    QString("Communication Control"),
    QString("ECU Reset"),
    QString("Clear DTCs"),
    QString("Read DTCs"),
    QString("Security Access"),
    QString("Read By ID"),
    QString("Read By Address"),
    QString("Read Scaling Data By ID"),
    QString("IO Control"),
    QString("Routine Control"),
    QString("Custom UDS"),
};

UDSScanWindow::UDSScanWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UDSScanWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    modelFrames = frames;

    currentlyRunning = false;

    waitTimer = new QTimer;
    waitTimer->setInterval(100);

    udsHandler = new UDS_HANDLER;
    inhibitUpdates = false;

    for (int i = 0; i < 13; i++) ui->cbScanType->addItem(SCANTYPE_NAMES[i]);

    ui->cbSessType->addItem("No Change");
    ui->cbSessType->addItem("Default");
    ui->cbSessType->addItem("Programming");
    ui->cbSessType->addItem("Extended Diag");
    ui->cbSessType->addItem("Safety Sys Diag");

    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    connect(udsHandler, &UDS_HANDLER::newUDSMessage, this, &UDSScanWindow::gotUDSReply);
    connect(ui->btnScanAll, &QPushButton::clicked, this, &UDSScanWindow::scanAll);
    connect(ui->btnScanSelected, &QPushButton::clicked, this, &UDSScanWindow::scanSelected);
    connect(waitTimer, &QTimer::timeout, this, &UDSScanWindow::timeOut);
    connect(ui->btnSaveResults, &QPushButton::clicked, this, &UDSScanWindow::saveResults);
    connect(ui->cbScanType, &QComboBox::currentTextChanged, this, &UDSScanWindow::changedScanType);
    connect(ui->cbAllowAdaptiveOffset, &QCheckBox::toggled, this, &UDSScanWindow::adaptiveToggled);
    connect(ui->spinNumBytes, SIGNAL(valueChanged(int)), this, SLOT(numBytesChanged()));
    connect(ui->spinLowerService, SIGNAL(valueChanged(int)), this, SLOT(checkServiceRange()));
    connect(ui->spinUpperService, SIGNAL(valueChanged(int)), this, SLOT(checkServiceRange()));
    connect(ui->spinLowerSubfunc, SIGNAL(valueChanged(int)), this, SLOT(checkSubFuncRange()));
    connect(ui->spinUpperSubfunc, SIGNAL(valueChanged(int)), this, SLOT(checkSubFuncRange()));
    connect(ui->spinStartID, SIGNAL(valueChanged(int)), this, SLOT(checkIDRange()));
    connect(ui->spinEndID, SIGNAL(valueChanged(int)), this, SLOT(checkIDRange()));
    connect(ui->btnAdd, &QPushButton::clicked, this, &UDSScanWindow::addNewScan);
    connect(ui->btnDelete, &QPushButton::clicked, this, &UDSScanWindow::deleteSelectedScan);
    connect(ui->btnSave, &QPushButton::clicked, this, &UDSScanWindow::saveScans);
    connect(ui->btnLoad, &QPushButton::clicked, this, &UDSScanWindow::loadScans);
    connect(ui->listScansToRun, &QListWidget::currentRowChanged, this, &UDSScanWindow::displayScanEntry);
    connect(ui->ckShowNoReply, &QCheckBox::toggled, this, &UDSScanWindow::setNoReplyVal);
    connect(ui->spinDelay, SIGNAL(valueChanged(int)), this, SLOT(setMaxDelayVal()));
    connect(ui->spinIncrement, SIGNAL(valueChanged(int)), this, SLOT(setIncrementVal()));
    connect(ui->spinReplyOffset, SIGNAL(valueChanged(int)), this, SLOT(setReplyOffset()));
    connect(ui->cbSessType, &QComboBox::currentTextChanged, this, &UDSScanWindow::setSessType);

//not handling show no reply, max reply delay, reply offset, increment
    int numBuses = CANConManager::getInstance()->getNumBuses();
    for (int n = 0; n < numBuses; n++) ui->cbBuses->addItem(QString::number(n));

    connect(ui->cbBuses, &QComboBox::currentTextChanged, this, &UDSScanWindow::setBusToScan);

    installEventFilter(this);

    addNewScan();

    checkIDRange();
    checkServiceRange();
    checkSubFuncRange();
}

UDSScanWindow::~UDSScanWindow()
{
    removeEventFilter(this);
    delete ui;
    waitTimer->stop();
    delete waitTimer;
    delete udsHandler;
}

bool UDSScanWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("uds_scanner.md");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void UDSScanWindow::setControlState(QWidget & widget, bool valid)
{
    QPalette pal = widget.palette();
    pal.setColor(QPalette::ColorRole::Base , valid ? Qt::white : Qt::red);
    widget.setPalette(pal);
}

void UDSScanWindow::displayScanEntry(int idx)
{
    if (inhibitUpdates) return;
    if (idx == -1) return;
    inhibitUpdates = true; //need to do this so it doesn't mess things up as we fill in the fields
    currEditEntry = &scanEntries.data()[idx];
    ui->spinStartID->setValue(currEditEntry->startID);
    ui->spinEndID->setValue(currEditEntry->endID);
    ui->spinReplyOffset->setValue(currEditEntry->idOffset);
    ui->cbAllowAdaptiveOffset->setChecked(currEditEntry->bAdaptiveOffset);
    ui->ckShowNoReply->setChecked(currEditEntry->bShowNoReplies);
    ui->cbBuses->setCurrentIndex(currEditEntry->busToScan);
    ui->spinDelay->setValue(currEditEntry->maxWaitTime);
    ui->cbScanType->setCurrentIndex((int)currEditEntry->scanType);
    ui->cbSessType->setCurrentIndex(currEditEntry->sessType);
    ui->spinNumBytes->setValue(currEditEntry->subfunctLen);
    ui->spinLowerService->setValue(currEditEntry->serviceLower);
    ui->spinUpperService->setValue(currEditEntry->serviceUpper);
    ui->spinLowerSubfunc->setValue(currEditEntry->subfunctLower);
    ui->spinUpperSubfunc->setValue(currEditEntry->subfunctUpper);
    ui->spinIncrement->setValue(currEditEntry->subfunctIncrement);
    inhibitUpdates = false;
}

void UDSScanWindow::deleteSelectedScan()
{
    int idx = ui->listScansToRun->currentRow();
    if (idx == -1) return;
    scanEntries.removeAt(idx);

    QListWidgetItem *item = ui->listScansToRun->takeItem(idx);
    delete item;
    int rows = ui->listScansToRun->count();
    ui->listScansToRun->setCurrentRow(rows - 1);

    for (int i = 0; i < rows; i++)
    {
        QListWidgetItem *item = ui->listScansToRun->item(i);
        item->setText(generateListDesc(i));
    }
}

void UDSScanWindow::addNewScan()
{
    ScanEntry newEntry;
    newEntry.busToScan = 0;
    newEntry.scanType = SCAN_TYPE::ST_TESTER_PRESENT;
    newEntry.sessType = 0;
    newEntry.bAdaptiveOffset = false;
    newEntry.bShowNoReplies = false;
    newEntry.idOffset = 8;
    newEntry.startID = 0x7E0;
    newEntry.endID = 0x7E7;
    newEntry.maxWaitTime = 100;
    newEntry.subfunctLen = 1;
    newEntry.subfunctIncrement = 1;
    newEntry.subfunctLower = 0;
    newEntry.subfunctUpper = 0;
    newEntry.serviceLower = 1;
    newEntry.serviceUpper = 1;
    scanEntries.append(newEntry);
    displayScanEntry(scanEntries.length() - 1);
    ui->listScansToRun->addItem(generateListDesc(scanEntries.length() - 1));
    ui->listScansToRun->setCurrentRow(ui->listScansToRun->count() - 1);
}

void UDSScanWindow::loadScans()
{
    QString filename;
    QFileDialog dialog(qApp->activeWindow());
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("UDS Test Specification (*.uds *.UDS)")));

    dialog.setDirectory(settings.value("FileIO/LoadSaveDirectory", dialog.directory().path()).toString());
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);    

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        QFile inFile(filename);
        inFile.open(QIODevice::ReadOnly);
        QDataStream load(&inFile);
        int fileVersion;
        load >> fileVersion;
        if (fileVersion != 1)
        {
            QMessageBox::warning(this, "Cannot Load File", "File is not a supported version.\nCannot load it!");
            return;
        }
        inhibitUpdates = true;
        scanEntries.clear();
        ui->listScansToRun->clear();
        int numEntries;
        load >> numEntries;
        for (int i = 0; i < numEntries; i++)
        {
            ScanEntry entry;
            load >> entry.startID;
            load >> entry.endID;
            load >> entry.idOffset;
            load >> entry.bAdaptiveOffset;
            load >> entry.bShowNoReplies;
            load >> entry.busToScan;
            load >> entry.maxWaitTime;
            load >> entry.scanType;
            load >> entry.sessType;
            load >> entry.subfunctLen;
            load >> entry.subfunctLower;
            load >> entry.subfunctUpper;
            load >> entry.subfunctIncrement;
            load >> entry.serviceLower;
            load >> entry.serviceUpper;

            scanEntries.append(entry);
            ui->listScansToRun->addItem(generateListDesc(scanEntries.length() - 1));
        }
        inFile.close();
        inhibitUpdates = false;
        ui->listScansToRun->setCurrentRow(ui->listScansToRun->count() - 1);
        displayScanEntry(scanEntries.length() - 1);
    }
}

void UDSScanWindow::saveScans()
{
    QString filename;
    QFileDialog dialog(qApp->activeWindow());
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("UDS Test Specification (*.uds *.UDS)")));

    dialog.setDirectory(settings.value("FileIO/LoadSaveDirectory", dialog.directory().path()).toString());
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix(".uds");

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        QFile outFile(filename);
        outFile.open(QIODevice::WriteOnly);
        QDataStream save(&outFile);
        save << 1; //file version
        save << scanEntries.count();
        for (int i = 0; i < scanEntries.count(); i++)
        {
            save << scanEntries[i].startID;
            save << scanEntries[i].endID;
            save << scanEntries[i].idOffset;
            save << scanEntries[i].bAdaptiveOffset;
            save << scanEntries[i].bShowNoReplies;
            save << scanEntries[i].busToScan;
            save << scanEntries[i].maxWaitTime;
            save << scanEntries[i].scanType;
            save << scanEntries[i].sessType;
            save << scanEntries[i].subfunctLen;
            save << scanEntries[i].subfunctLower;
            save << scanEntries[i].subfunctUpper;
            save << scanEntries[i].subfunctIncrement;
            save << scanEntries[i].serviceLower;
            save << scanEntries[i].serviceUpper;
        }
        outFile.close();
    }
}

QString UDSScanWindow::generateListDesc(int idx)
{
    QString builder;
    ScanEntry entry = scanEntries[idx];
    builder = "(0x" + QString::number(entry.startID, 16) + " - 0x" + QString::number(entry.endID, 16) + ") ";
    switch (entry.scanType)
    {
    case ST_TESTER_PRESENT:
        builder += "TP";
        break;
    case ST_SESS_CTRL:
        builder += "SESS";
        break;
    case ST_COMM_CTRL:
        builder += "COMM";
        break;
    case ST_ECU_RESET:
        builder += "ECU";
        break;
    case ST_CLEAR_DTC:
        builder += "CDTC";
        break;
    case ST_READ_DTC:
        builder += "RDTC";
        break;
    case ST_SEC_ACCESS:
        builder += "SECU";
        break;
    case ST_READ_ID:
        builder += "RID";
        break;
    case ST_READ_ADDR:
        builder += "RADD";
        break;
    case ST_READ_SCALING:
        builder += "RSCAL";
        break;
    case ST_IO_CTRL:
        builder += "IO";
        break;
    case ST_ROUTINE_CTRL:
        builder += "ROUT";
        break;
    case ST_CUSTOM:
        builder += "CUST";
        break;
    }
    return builder;
}

void UDSScanWindow::adaptiveToggled()
{
    if (inhibitUpdates) return;
    if (ui->cbAllowAdaptiveOffset->isChecked()) ui->spinReplyOffset->setEnabled(false);
    else ui->spinReplyOffset->setEnabled(true);
    if (currEditEntry) currEditEntry->bAdaptiveOffset = ui->cbAllowAdaptiveOffset->isChecked();
}

void UDSScanWindow::setSessType()
{
    if (inhibitUpdates) return;
    if (currEditEntry) currEditEntry->sessType = ui->cbSessType->currentIndex();
}

void UDSScanWindow::setBusToScan()
{
    if (inhibitUpdates) return;
    if (currEditEntry) currEditEntry->busToScan = ui->cbBuses->currentIndex();
}

void UDSScanWindow::setNoReplyVal()
{
    if (inhibitUpdates) return;
    if (currEditEntry) currEditEntry->bShowNoReplies = ui->ckShowNoReply->isChecked();
}

void UDSScanWindow::setMaxDelayVal()
{
    if (inhibitUpdates) return;
    if (currEditEntry) currEditEntry->maxWaitTime = ui->spinDelay->value();
}

void UDSScanWindow::setIncrementVal()
{
    if (inhibitUpdates) return;
    if (currEditEntry) currEditEntry->subfunctIncrement = ui->spinIncrement->value();
}

void UDSScanWindow::setReplyOffset()
{
    if (inhibitUpdates) return;
    if (currEditEntry) currEditEntry->idOffset = ui->spinReplyOffset->value();
}

void UDSScanWindow::changedScanType()
{
    if (inhibitUpdates) return;
    int idx = ui->cbScanType->currentIndex();
    if (idx == -1)
    {
        ui->cbScanType->setCurrentIndex(0);
        idx = 0;
    }

    if (currEditEntry) currEditEntry->scanType = (SCAN_TYPE)idx;

    QListWidgetItem* item = ui->listScansToRun->currentItem();
    item->setText(generateListDesc(ui->listScansToRun->currentRow()));

    switch (idx)
    {
    case ST_TESTER_PRESENT:
    case ST_SESS_CTRL:
        ui->cbSessType->setEnabled(false);
        ui->spinLowerService->setEnabled(false);
        ui->spinLowerSubfunc->setEnabled(false);
        ui->spinUpperService->setEnabled(false);
        ui->spinUpperSubfunc->setEnabled(false);
        ui->spinNumBytes->setEnabled(false);
        ui->spinIncrement->setEnabled(false);
        ui->cbSessType->setCurrentIndex(0);
        break;
    case ST_COMM_CTRL:
    case ST_ECU_RESET:
    case ST_CLEAR_DTC:
    case ST_SEC_ACCESS:
    case ST_READ_DTC:
        ui->cbSessType->setEnabled(true);
        ui->spinLowerService->setEnabled(false);
        ui->spinUpperService->setEnabled(false);
        ui->spinLowerSubfunc->setEnabled(false);
        ui->spinUpperSubfunc->setEnabled(false);
        ui->spinNumBytes->setEnabled(false);
        ui->spinIncrement->setEnabled(false);
        break;
    case ST_IO_CTRL:
    case ST_ROUTINE_CTRL:
    case ST_READ_SCALING:
        ui->cbSessType->setEnabled(true);
        ui->spinLowerService->setEnabled(false);
        ui->spinUpperService->setEnabled(false);
        ui->spinLowerSubfunc->setEnabled(true);
        ui->spinUpperSubfunc->setEnabled(true);
        ui->spinNumBytes->setEnabled(false);
        ui->spinNumBytes->setValue(2);
        ui->spinIncrement->setEnabled(true);
        break;
    case ST_READ_ID:
    case ST_READ_ADDR:
        ui->cbSessType->setEnabled(true);
        ui->spinLowerService->setEnabled(false);
        ui->spinUpperService->setEnabled(false);
        ui->spinLowerSubfunc->setEnabled(true);
        ui->spinUpperSubfunc->setEnabled(true);
        ui->spinNumBytes->setEnabled(true);
        ui->spinIncrement->setEnabled(true);
        break;
    case ST_CUSTOM:
        ui->cbSessType->setEnabled(true);
        ui->spinLowerService->setEnabled(true);
        ui->spinUpperService->setEnabled(true);
        ui->spinLowerSubfunc->setEnabled(true);
        ui->spinUpperSubfunc->setEnabled(true);
        ui->spinNumBytes->setEnabled(true);
        ui->spinIncrement->setEnabled(true);
        break;
    }
}

void UDSScanWindow::numBytesChanged()
{
    if (inhibitUpdates) return;
    uint64_t upperBound =  (1ull << (8ull * ui->spinNumBytes->value())) - 1;
    if (upperBound > 0x7FFFFFFF) upperBound = 0x7FFFFFFF;
    ui->spinUpperSubfunc->setMaximum(upperBound);
    ui->spinLowerSubfunc->setMaximum(upperBound);
    if (currEditEntry) currEditEntry->subfunctLen = ui->spinNumBytes->value();
}

void UDSScanWindow::checkIDRange()
{
    if (inhibitUpdates) return;

    const bool isValid = ui->spinStartID->value() <= ui->spinEndID->value();
    setControlState(*ui->spinStartID, isValid);
    setControlState(*ui->spinEndID, isValid);
    
    if (currEditEntry) currEditEntry->startID = ui->spinStartID->value();
    if (currEditEntry) currEditEntry->endID = ui->spinEndID->value();
    if (QListWidgetItem* item = ui->listScansToRun->currentItem())
    {
        item->setText(generateListDesc(ui->listScansToRun->currentRow()));
    }
}

void UDSScanWindow::checkServiceRange()
{
    if (inhibitUpdates) return;

    const bool isValid = ui->spinLowerService->value() <= ui->spinUpperService->value();
    setControlState(*ui->spinLowerService, isValid);
    setControlState(*ui->spinUpperService, isValid);    
    if (currEditEntry) currEditEntry->serviceLower = ui->spinLowerService->value();
    if (currEditEntry) currEditEntry->serviceUpper = ui->spinUpperService->value();
}

void UDSScanWindow::checkSubFuncRange()
{
    if (inhibitUpdates) return;
    const bool isValid = ui->spinLowerSubfunc->value() <= ui->spinUpperSubfunc->value();
    setControlState(*ui->spinLowerSubfunc, isValid);
    setControlState(*ui->spinUpperSubfunc, isValid);
    if (currEditEntry) currEditEntry->subfunctLower = ui->spinLowerSubfunc->value();
    if (currEditEntry) currEditEntry->subfunctUpper = ui->spinUpperSubfunc->value();
}

void UDSScanWindow::saveResults()
{
    QString filename;
    QFileDialog dialog(this);
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("Text File (*.txt)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDirectory(settings.value("UDSScan/LoadSaveDirectory", dialog.directory().path()).toString());

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        settings.setValue("UDSScan/LoadSaveDirectory", dialog.directory().path());

        if (!filename.contains('.')) filename += ".txt";
        if (dialog.selectedNameFilter() == filters[0])
        {
            QFile *outFile = new QFile(filename);

            if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
            {
                delete outFile;
                return;
            }
            outFile->write("UDS Scan Log:\n\n");
            dumpNode(ui->treeResults->invisibleRootItem(), outFile, 0);

            outFile->close();
            delete outFile;
        }
    }
}

void UDSScanWindow::dumpNode(QTreeWidgetItem* item, QFile *file, int indent)
{
    if (indent > 0) for (int i = 0; i < (indent - 1); i++) file->write("\t");
    file->write(item->text(0).toUtf8());
    file->write("\n");
    for( int i = 0; i < item->childCount(); ++i )
        dumpNode( item->child(i), file, indent + 1 );
    if (indent == 1) file->write("\n");
}

void UDSScanWindow::sendOnBuses(UDS_MESSAGE test, int buses)
{
    test.bus = buses;
    sendingFrames.append(test);
}

void UDSScanWindow::scanAll()
{
    sendingFrames.clear();
    for (int i = 0; i < scanEntries.count(); i++)
    {
        setupScan(i);
    }
    startScan();
}

void UDSScanWindow::scanSelected()
{
    sendingFrames.clear();
    int idx = ui->listScansToRun->currentRow();
    if (idx < 0) return;
    setupScan(idx);
    startScan();
}

void UDSScanWindow::startScan()
{
    if (sendingFrames.isEmpty()) return;

    udsHandler->setReception(true);
    udsHandler->setProcessAllIDs(true);
    udsHandler->setFlowCtrl(true);

    waitTimer->setInterval(ui->spinDelay->value());

    ui->treeResults->clear();
    nodeService = nullptr;
    nodeID = nullptr;
    nodeSubFunc = nullptr;

    waitTimer->start();
    currIdx = -1;
    currentlyRunning = true;
    //ui->btnScan->setText("Abort Scan");
    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(sendingFrames.length());
    qDebug() << "Number of operations: " << sendingFrames.length();
    sendNextMsg();
}

void UDSScanWindow::stopScan()
{
    waitTimer->stop();
    sendingFrames.clear();
    udsHandler->setReception(false);
    udsHandler->setProcessAllIDs(false);
    udsHandler->setFlowCtrl(false);
    currentlyRunning = false;
    //ui->btnScan->setText("Start Scan");
}

void UDSScanWindow::setupScan(int idx)
{
    UDS_MESSAGE test;

    qDebug() << "Generating scan id: " << idx;

    for (uint32_t id = scanEntries[idx].startID; id <= scanEntries[idx].endID; id++)
    {
        test.setFrameId( id );

        if (scanEntries[idx].sessType > 0)
        {
            test.payload().clear();
            test.service = UDS_SERVICES::DIAG_CONTROL;
            test.subFuncLen = 1;
            test.subFunc = scanEntries[ idx].sessType;
            sendOnBuses(test, scanEntries[idx].busToScan);
        }

        test.payload().clear();

        qDebug() << "Generating scan on ID " << QString::number(id, 16) << " of type " << scanEntries[idx].scanType;

        switch (scanEntries[idx].scanType)
        {
        case ST_TESTER_PRESENT:
            test.service = UDS_SERVICES::TESTER_PRESENT;
            test.subFuncLen = 1;
            test.subFunc = 0;
            sendOnBuses(test, scanEntries[idx].busToScan);
            break;
        case ST_SESS_CTRL:
            for (int typ = 1; typ < 4; typ++) //try each type of session access
            {
                test.service = UDS_SERVICES::DIAG_CONTROL;
                test.subFuncLen = 1;
                test.subFunc = typ;
                sendOnBuses(test, scanEntries[idx].busToScan);
            }
            break;
        case ST_COMM_CTRL:
            test.service = UDS_SERVICES::COMM_CTRL;
            test.subFuncLen = 2; //need two bytes for this one
            test.subFunc = 0x100; //00 01 on the bus = enable Rx/Tx
            sendOnBuses(test, scanEntries[idx].busToScan);
            break;
        case ST_ECU_RESET:
            for (int typ = 1; typ < 4; typ++) //try each type of session access
            {
                test.service = UDS_SERVICES::ECU_RESET;
                test.subFuncLen = 1;
                test.subFunc = typ;
                sendOnBuses(test, scanEntries[idx].busToScan);
            }
            break;
        case ST_CLEAR_DTC:
            test.service = UDS_SERVICES::CLEAR_DIAG;
            test.subFuncLen = 3; //DTC groups are sent as 3 bytes
            test.subFunc = 0xFFFFFF; //clear everything!
            sendOnBuses(test, scanEntries[idx].busToScan);
            break;
        case ST_READ_DTC:
            test.service = UDS_SERVICES::READ_DTC;
            test.subFuncLen = 2;
            test.subFunc = 0x02FF; //get DTCs by mask (FF is the mask)
            sendOnBuses(test, scanEntries[idx].busToScan);
            break;
        case ST_SEC_ACCESS:
            for (int typ = 1; typ < 0x42; typ = typ + 2) //try each type of session access. In practice only the first 1-3 are likely to work
            {
                test.service = UDS_SERVICES::SECURITY_ACCESS;
                test.subFuncLen = 1;
                test.subFunc = typ;
                sendOnBuses(test, scanEntries[idx].busToScan);
            }
            break;
        case ST_READ_ID:
            test.subFuncLen = scanEntries[idx].subfunctLen;
            test.service = UDS_SERVICES::READ_BY_ID;
            for (int subf = scanEntries[idx].subfunctLower; subf <= scanEntries[idx].subfunctUpper; subf += scanEntries[idx].subfunctIncrement)
            {
                test.subFunc = subf;
                sendOnBuses(test, scanEntries[idx].busToScan);
            }
            break;
        case ST_READ_ADDR:
            test.subFuncLen = scanEntries[idx].subfunctLen;
            test.service = UDS_SERVICES::READ_BY_ADDR;
            for (int subf = scanEntries[idx].subfunctLower; subf <= scanEntries[idx].subfunctUpper; subf += scanEntries[idx].subfunctIncrement)
            {
                test.subFunc = subf;
                sendOnBuses(test, scanEntries[idx].busToScan);
            }
            break;
        case ST_READ_SCALING:
            //sub function should always be a length of 2 though. Should conform to this in the GUI
            test.subFuncLen = 2;
            test.service = UDS_SERVICES::READ_SCALING_ID;
            for (int subf = scanEntries[idx].subfunctLower; subf <= scanEntries[idx].subfunctUpper; subf += scanEntries[idx].subfunctIncrement)
            {
                test.subFunc = subf;
                sendOnBuses(test, scanEntries[idx].busToScan);
            }
            break;
        case ST_IO_CTRL:
            test.subFuncLen = 3;
            test.service = UDS_SERVICES::IO_CTRL;
            for (int subf = scanEntries[idx].subfunctLower; subf <= scanEntries[idx].subfunctUpper; subf += scanEntries[idx].subfunctIncrement)
            {
                test.subFunc = subf; //the upper byte will be 0 which is what we want. 0 = Return control to ECU
                sendOnBuses(test, scanEntries[idx].busToScan);
            }
            break;
        case ST_ROUTINE_CTRL:
            test.subFuncLen = 3;
            test.service = UDS_SERVICES::ROUTINE_CTRL;
            for (int subf = scanEntries[idx].subfunctLower; subf <= scanEntries[idx].subfunctUpper; subf += scanEntries[idx].subfunctIncrement)
            {
                //request results of routine for all the addresses we're testing. This is the safest thing to do.
                //starting or stopping arbitrary routines is super dangerous. Don't do that unless you really know
                //what the hell you're doing or something really crazy might happen.
                test.subFunc = (subf << 8) + 3;
                sendOnBuses(test, scanEntries[idx].busToScan);
            }
            break;
        case ST_CUSTOM:
            test.subFuncLen = scanEntries[idx].subfunctLen;

            for (uint32_t typ = scanEntries[idx].serviceLower; typ <= scanEntries[idx].serviceUpper; typ++)
            {
                test.service = typ;
                for (int subTyp = scanEntries[idx].subfunctLower; subTyp <= scanEntries[idx].subfunctUpper; subTyp += scanEntries[idx].subfunctIncrement)
                {
                    test.subFunc = subTyp;
                    sendOnBuses(test, scanEntries[idx].busToScan);
                }
            }
            break;
        }
    }
}

//Updates here are sent about every 1/4 second. That's fine for most windows but not this one.
void UDSScanWindow::updatedFrames(int numFrames)
{
    if (numFrames == -1) //all frames deleted. We don't care
    {
    }
    else if (numFrames == -2) //all new set of frames. Don't care.
    {
    }
    else //just got some new frames. See if they are relevant.
    {
    }
}

void UDSScanWindow::gotUDSReply(UDS_MESSAGE msg)
{
    QString result;
    QString serviceShortName;
    uint32_t id;
    int offset = ui->spinReplyOffset->value();
    UDS_MESSAGE sentFrame;
    bool gotReply = false;
    const unsigned char *data = reinterpret_cast<const unsigned char *>(msg.payload().constData());
    int dataLen = msg.payload().length();

    int numSending = sendingFrames.length();
    if (numSending == 0) return;
    if (currIdx >= numSending) return;
    sentFrame = sendingFrames[currIdx];

    id = msg.frameId();

    qDebug() << "UDS message ID " << QString::number(msg.frameId(),16) << "  service: " << QString::number(msg.service, 16) << " subfunc: " << QString::number(msg.subFunc, 16);

    if ((id == (uint32_t)(sentFrame.frameId() + offset)) || ui->cbAllowAdaptiveOffset->isChecked())
    {
        serviceShortName = udsHandler->getServiceShortDesc(sentFrame.service);
        if (serviceShortName.length() < 3) serviceShortName = QString::number(sentFrame.service, 16);
        if (msg.service == (0x40 + sendingFrames[currIdx].service) )
        {
            setupNodes(id);

            QTreeWidgetItem *nodePositive = new QTreeWidgetItem();
            QString reply = "POSITIVE ";
            for (int i = 0; i < dataLen; i++)
            {
                reply.append(" ");
                reply.append(Utility::formatHexNum(data[i]));
            }
            nodePositive->setText(0, reply);
            nodePositive->setForeground(0, QBrush(Qt::darkGreen));
            nodeSubFunc->addChild(nodePositive);
            nodeSubFunc->setForeground(0, QBrush(Qt::darkGreen));
            gotReply = true;
        }
        else if ( msg.isErrorReply && (msg.service == sendingFrames[currIdx].service) )
        {
            if (dataLen)
            {
                setupNodes(id);
                QTreeWidgetItem *nodeNegative = new QTreeWidgetItem();
                qDebug() << ui->spinNumBytes->value();
                nodeNegative->setText(0, "NEGATIVE - " + udsHandler->getNegativeResponseShort(data[0]));
                nodeNegative->setForeground(0, QBrush(Qt::darkRed));
                nodeSubFunc->addChild(nodeNegative);
                nodeSubFunc->setForeground(0, QBrush(Qt::darkRed));
                gotReply = true;
            }
        }
    }
    if (gotReply)
    {
        //ui->listResults->addItem(result);
        sendNextMsg();
    }
}

void UDSScanWindow::setupNodes(uint32_t replyID)
{
    QString serviceShortName = udsHandler->getServiceShortDesc(sendingFrames[currIdx].service);
    if (serviceShortName.length() < 3) serviceShortName = QString::number(sendingFrames[currIdx].service, 16);
    QTreeWidgetItem *replyNode = nullptr;

    if (!nodeID || nodeID->text(0) != Utility::formatHexNum(sendingFrames[currIdx].frameId()))
    {
        nodeID = new QTreeWidgetItem();
        nodeID->setText(0, Utility::formatHexNum(sendingFrames[currIdx].frameId()));
        ui->treeResults->addTopLevelItem(nodeID);
        nodeService = nullptr;
    }

    bool foundReplyMatch = false;
    for (int i = 0; i < nodeID->childCount(); i++)
    {
        if (nodeID->child(i)->text(0) == Utility::formatHexNum(replyID) || ((replyID == 0xDEAD5EA1) && (nodeID->child(i)->text(0) == "NO REPLY")))
        {
            foundReplyMatch = true;
            replyNode = nodeID->child(i);
            break;
        }
    }
    if (!foundReplyMatch)
    {
        QTreeWidgetItem *replyItem = new QTreeWidgetItem();
        if (replyID != 0xDEAD5EA1) replyItem->setText(0, Utility::formatHexNum(replyID));
        else replyItem->setText(0, "NO REPLY");
        nodeID->addChild(replyItem);
        replyNode = replyItem;
        nodeService = nullptr;
    }

    bool foundServiceMatch = false;

    for (int i = 0; i < replyNode->childCount(); i++)
    {
        if ( replyNode->child(i)->text(0) == serviceShortName)
        {
            foundServiceMatch = true;
            nodeService = replyNode->child(i);
            break;
        }
    }
    if (!foundServiceMatch)
    {
        nodeService = new QTreeWidgetItem();
        nodeService->setText(0, serviceShortName);
        replyNode->addChild(nodeService);
    }

    nodeSubFunc = new QTreeWidgetItem();
    nodeSubFunc->setText(0, Utility::formatHexNum(sendingFrames[currIdx].subFunc));
    nodeService->addChild(nodeSubFunc);
}

void UDSScanWindow::timeOut()
{
    if (ui->ckShowNoReply->isChecked())
    {
        setupNodes(0xDEAD5EA1);
        nodeSubFunc->setForeground(0, QBrush(Qt::gray));
    }

    sendNextMsg();
}

void UDSScanWindow::sendNextMsg()
{
    currIdx++;
    if (currIdx < sendingFrames.count())
    {
        udsHandler->sendUDSFrame(sendingFrames[currIdx]);
        waitTimer->start();
    }
    else
    {
        waitTimer->stop();
        udsHandler->setReception(false);
        udsHandler->setProcessAllIDs(false);
        udsHandler->setFlowCtrl(false);
        //ui->btnScan->setText("Start Scan");
        currentlyRunning = false;
    }
    ui->progressBar->setValue(currIdx);
}
