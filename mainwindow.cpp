#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "can_structs.h"
#include <QDateTime>
#include <QFileDialog>
#include <QtSerialPort/QSerialPortInfo>
#include "canframemodel.h"
#include "utility.h"
#include "serialworker.h"

/*
This first order of business is to attempt to gain feature parity (roughly) with GVRET-PC so  that this
can replace it as a cross platform solution.

Here are things yet to do
- Ability to use programmatic frame sending interface (only I use it and it's complicated but helpful)

Things that were planned for the GVRET-PC project but never completed.
Single / Multi state - The goal is to find bits that change based on toggles or discrete state items (shifters, etc)
Range state - Find things that range like accelerator pedal inputs, road speed, tach, etc
fuzzy scope - Try to find potential places where a given value might be stored - offer guesses and the program tries to find candidates for you


Things currently broken or in need of attention: (push these over to github issues as soon as you can)
*/

QString MainWindow::loadedFileName = "";
MainWindow *MainWindow::selfRef = NULL;

MainWindow *MainWindow::getReference()
{
    return selfRef;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    useHex = true;

    //These things are used by QSettings to set up setting storage
    QCoreApplication::setOrganizationName("EVTV");
    QCoreApplication::setOrganizationDomain("evtv.me");
    QCoreApplication::setApplicationName("SavvyCAN");

    selfRef = this;

    this->setWindowTitle("Savvy CAN V" + QString::number(VERSION));

    model = new CANFrameModel();
    ui->canFramesView->setModel(model);

    readSettings();

    QStringList headers;
    headers << "Timestamp" << "ID" << "Ext" << "Bus" << "Len" << "Data";
    //model->setHorizontalHeaderLabels(headers);
    ui->canFramesView->setColumnWidth(0, 110);
    ui->canFramesView->setColumnWidth(1, 70);
    ui->canFramesView->setColumnWidth(2, 40);
    ui->canFramesView->setColumnWidth(3, 40);
    ui->canFramesView->setColumnWidth(4, 40);
    ui->canFramesView->setColumnWidth(5, 275);
    QHeaderView *HorzHdr = ui->canFramesView->horizontalHeader();
    HorzHdr->setStretchLastSection(true); //causes the data column to automatically fill the tableview
    //enabling the below line kills performance in every way imaginable. Left here as a warning. Do not do this.
    //ui->canFramesView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    ports = QSerialPortInfo::availablePorts();

    for (int i = 0; i < ports.count(); i++)
    {
        ui->cbSerialPorts->addItem(ports[i].portName());
    }

    ui->cbSpeed1->addItem(tr("Disabled"));
    ui->cbSpeed1->addItem(tr("125000"));
    ui->cbSpeed1->addItem(tr("250000"));
    ui->cbSpeed1->addItem(tr("500000"));
    ui->cbSpeed1->addItem(tr("1000000"));
    ui->cbSpeed1->addItem(tr("33333"));
    ui->cbSpeed2->addItem(tr("Disabled"));
    ui->cbSpeed2->addItem(tr("125000"));
    ui->cbSpeed2->addItem(tr("250000"));
    ui->cbSpeed2->addItem(tr("500000"));
    ui->cbSpeed2->addItem(tr("1000000"));
    ui->cbSpeed2->addItem(tr("33333"));

    worker = new SerialWorker(model);
    worker->moveToThread(&serialWorkerThread);
    connect(&serialWorkerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &MainWindow::sendSerialPort, worker, &SerialWorker::setSerialPort, Qt::QueuedConnection);
    connect(worker, &SerialWorker::frameUpdateTick, this, &MainWindow::gotFrames, Qt::QueuedConnection);
    connect(this, &MainWindow::updateBaudRates, worker, &SerialWorker::updateBaudRates, Qt::QueuedConnection);
    connect(this, &MainWindow::sendCANFrame, worker, &SerialWorker::sendFrame, Qt::QueuedConnection);
    connect(worker, &SerialWorker::connectionSuccess, this, &MainWindow::connectionSucceeded, Qt::QueuedConnection);
    connect(worker, &SerialWorker::connectionFailure, this, &MainWindow::connectionFailed, Qt::QueuedConnection);
    connect(worker, &SerialWorker::deviceInfo, this, &MainWindow::gotDeviceInfo, Qt::QueuedConnection);
    connect(this, &MainWindow::closeSerialPort, worker, &SerialWorker::closeSerialPort, Qt::QueuedConnection);
    connect(this, &MainWindow::startFrameCapturing, worker, &SerialWorker::startFrameCapture);
    connect(this, &MainWindow::stopFrameCapturing, worker, &SerialWorker::stopFrameCapture);
    connect(this, &MainWindow::settingsUpdated, worker, &SerialWorker::readSettings);
    serialWorkerThread.start();
    serialWorkerThread.setPriority(QThread::HighPriority);

    graphingWindow = NULL;
    frameInfoWindow = NULL;
    playbackWindow = NULL;
    flowViewWindow = NULL;
    frameSenderWindow = NULL;
    dbcMainEditor = NULL;
    comparatorWindow = NULL;
    settingsDialog = NULL;
    dbcHandler = new DBCHandler;
    bDirty = false;
    inhibitFilterUpdate = false;

    model->setDBCHandler(dbcHandler);

    connect(ui->btnConnect, SIGNAL(clicked(bool)), this, SLOT(connButtonPress()));
    connect(ui->actionOpen_Log_File, SIGNAL(triggered(bool)), this, SLOT(handleLoadFile()));
    connect(ui->actionGraph_Dta, SIGNAL(triggered(bool)), this, SLOT(showGraphingWindow()));
    connect(ui->actionFrame_Data_Analysis, SIGNAL(triggered(bool)), this, SLOT(showFrameDataAnalysis()));
    connect(ui->btnClearFrames, SIGNAL(clicked(bool)), this, SLOT(clearFrames()));
    connect(ui->actionSave_Log_File, SIGNAL(triggered(bool)), this, SLOT(handleSaveFile()));
    connect(ui->actionSave_Filtered_Log_File, SIGNAL(triggered(bool)), this, SLOT(handleSaveFilteredFile()));
    connect(ui->actionLoad_Filter_Definition, SIGNAL(triggered(bool)), this, SLOT(handleLoadFilters()));
    connect(ui->actionSave_Filter_Definition, SIGNAL(triggered(bool)), this, SLOT(handleSaveFilters()));
    connect(ui->action_Playback, SIGNAL(triggered(bool)), this, SLOT(showPlaybackWindow()));
    connect(ui->btnBaudSet, SIGNAL(clicked(bool)), this, SLOT(changeBaudRates()));
    connect(ui->actionFlow_View, SIGNAL(triggered(bool)), this, SLOT(showFlowViewWindow()));
    connect(ui->action_Custom, SIGNAL(triggered(bool)), this, SLOT(showFrameSenderWindow()));
    connect(ui->actionLoad_DBC_File, SIGNAL(triggered(bool)), this, SLOT(handleLoadDBC()));
    connect(ui->actionSave_DBC_File, SIGNAL(triggered(bool)), this, SLOT(handleSaveDBC()));
    connect(ui->canFramesView, SIGNAL(clicked(QModelIndex)), this, SLOT(gridClicked(QModelIndex)));
    connect(ui->cbInterpret, SIGNAL(toggled(bool)), this, SLOT(interpretToggled(bool)));
    connect(ui->cbOverwrite, SIGNAL(toggled(bool)), this, SLOT(overwriteToggled(bool)));
    connect(ui->actionEdit_Messages_Signals, SIGNAL(triggered(bool)), this, SLOT(showDBCEditor()));
    connect(ui->btnCaptureToggle, SIGNAL(clicked(bool)), this, SLOT(toggleCapture()));
    connect(ui->actionExit_Application, SIGNAL(triggered(bool)), this, SLOT(exitApp()));
    connect(ui->actionFuzzy_Scope, SIGNAL(triggered(bool)), this, SLOT(showFuzzyScopeWindow()));
    connect(ui->actionRange_State, SIGNAL(triggered(bool)), this, SLOT(showRangeWindow()));
    connect(ui->actionSave_Decoded_Frames, SIGNAL(triggered(bool)), this, SLOT(handleSaveDecoded()));
    connect(ui->actionSingle_Multi_State, SIGNAL(triggered(bool)), this, SLOT(showSingleMultiWindow()));
    connect(ui->actionFile_Comparison, SIGNAL(triggered(bool)), this, SLOT(showComparisonWindow()));
    connect(ui->btnNormalize, SIGNAL(clicked(bool)), this, SLOT(normalizeTiming()));
    connect(ui->actionPreferences, SIGNAL(triggered(bool)), this, SLOT(showSettingsDialog()));
    connect(model, SIGNAL(updatedFiltersList()), this, SLOT(updateFilterList()));
    connect(ui->listFilters, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(filterListItemChanged(QListWidgetItem*)));
    connect(ui->btnFilterAll, SIGNAL(clicked(bool)), this, SLOT(filterSetAll()));
    connect(ui->btnFilterNone, SIGNAL(clicked(bool)), this, SLOT(filterClearAll()));

    lbStatusConnected.setText(tr("Not connected"));
    updateFileStatus();
    lbStatusDatabase.setText(tr("No DBC database loaded"));
    ui->statusBar->addWidget(&lbStatusConnected);
    ui->statusBar->addWidget(&lbStatusFilename);
    ui->statusBar->addWidget(&lbStatusDatabase);

    ui->lbFPS->setText("0");
    ui->lbNumFrames->setText("0");

    isConnected = false;
    allowCapture = true;

    //create a temporary frame to be able to capture the correct
    //default height of an item in the table. Need to do this in case
    //of scaling or font differences between different computers.
    CANFrame temp;
    temp.bus = 0;
    temp.ID = 0x100;
    temp.len = 0;
    model->addFrame(temp, true);
    normalRowHeight = ui->canFramesView->rowHeight(0);
    qDebug() << "normal row height = " << normalRowHeight;
    model->clearFrames();
}

MainWindow::~MainWindow()
{
    serialWorkerThread.quit();
    serialWorkerThread.wait();
    //delete worker;

    if (graphingWindow)
    {
        graphingWindow->close();
        delete graphingWindow;
    }

    if (frameInfoWindow)
    {
        frameInfoWindow->close();
        delete frameInfoWindow;
    }

    if (playbackWindow)
    {
        playbackWindow->close();
        delete playbackWindow;
    }

    if (flowViewWindow)
    {
        flowViewWindow->close();
        delete flowViewWindow;
    }

    if (frameSenderWindow)
    {
        frameSenderWindow->close();
        delete frameSenderWindow;
    }

    if (comparatorWindow)
    {
        comparatorWindow->close();
        delete comparatorWindow;
    }

    if (dbcMainEditor)
    {
        dbcMainEditor->close();
        delete dbcMainEditor;
    }

    if (settingsDialog)
    {
        settingsDialog->close();
        delete settingsDialog;
    }

    delete ui;
    delete dbcHandler;
    model->clearFrames();
    delete model;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();
    exitApp();
}

void MainWindow::updateSettings()
{
    readUpdateableSettings();
    emit settingsUpdated();
}

void MainWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("Main/WindowSize", QSize(800, 750)).toSize());
        move(settings.value("Main/WindowPos", QPoint(100, 100)).toPoint());
    }
    if (settings.value("Main/AutoScroll", false).toBool())
    {
        ui->cbAutoScroll->setChecked(true);
    }
    readUpdateableSettings();
}

void MainWindow::readUpdateableSettings()
{
    QSettings settings;
    useHex = settings.value("Main/UseHex", true).toBool();
    model->setHexMode(useHex);
    model->setSecondsMode(settings.value("Main/TimeSeconds", false).toBool());
    useFiltered = settings.value("Main/UseFiltered", false).toBool();
}

void MainWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("Main/WindowSize", size());
        settings.setValue("Main/WindowPos", pos());
    }
}

void MainWindow::gridClicked(QModelIndex idx)
{
    if (ui->canFramesView->rowHeight(idx.row()) > normalRowHeight)
    {
        ui->canFramesView->setRowHeight(idx.row(), normalRowHeight);
    }
    else {
        ui->canFramesView->resizeRowToContents(idx.row());
    }
}

void MainWindow::interpretToggled(bool state)
{
    model->setInterpetMode(state);
}

void MainWindow::overwriteToggled(bool state)
{
    model->setOverwriteMode(state);
}

void MainWindow::updateFilterList()
{
    if (model == NULL) return;
    const QMap<int, bool> *filters = model->getFiltersReference();
    if (filters == NULL) return;

    qDebug() << "updateFilterList called on MainWindow";

    ui->listFilters->clear();

    if (filters->isEmpty()) return;

    QMap<int, bool>::const_iterator filterIter;
    for (filterIter = filters->begin(); filterIter != filters->end(); ++filterIter)
    {
        QListWidgetItem *thisItem = new QListWidgetItem();
        if (useHex) thisItem->setText(QString::number(filterIter.key(), 16).toUpper().rightJustified(4,'0'));
            else thisItem->setText(QString::number(filterIter.key()));
        thisItem->setFlags(thisItem->flags() | Qt::ItemIsUserCheckable);
        if (filterIter.value()) thisItem->setCheckState(Qt::Checked);
            else thisItem->setCheckState(Qt::Unchecked);
        ui->listFilters->addItem(thisItem);
    }
}

void MainWindow::filterListItemChanged(QListWidgetItem *item)
{
    if (inhibitFilterUpdate) return;
    //qDebug() << item->text();
    int ID;
    bool isSet = false;
    if (useHex) ID = item->text().toInt(NULL, 16);
        else ID = item->text().toInt();
    if (item->checkState() == Qt::Checked) isSet = true;

    model->setFilterState(ID, isSet);
}

void MainWindow::filterSetAll()
{
    inhibitFilterUpdate = true;
    for (int i = 0; i < ui->listFilters->count(); i++)
    {
        ui->listFilters->item(i)->setCheckState(Qt::Checked);
    }
    inhibitFilterUpdate = false;
    model->setAllFilters(true);
}

void MainWindow::filterClearAll()
{
    inhibitFilterUpdate = true;
    for (int i = 0; i < ui->listFilters->count(); i++)
    {
        ui->listFilters->item(i)->setCheckState(Qt::Unchecked);
    }
    inhibitFilterUpdate = false;
    model->setAllFilters(false);
}

//most of the work is handled elsewhere. Need only to update the # of frames
//and maybe auto scroll
void MainWindow::gotFrames(int FPS, int framesSinceLastUpdate)
{
    ui->lbNumFrames->setText(QString::number(model->rowCount()));
    if (ui->cbAutoScroll->isChecked()) ui->canFramesView->scrollToBottom();
    ui->lbFPS->setText(QString::number(FPS));
    if (framesSinceLastUpdate > 0)
    {
        bDirty = true;
        emit framesUpdated(framesSinceLastUpdate); //anyone care that frames were updated?
    }

    if (model->needsFilterRefresh()) updateFilterList();
}

void MainWindow::addFrameToDisplay(CANFrame &frame, bool autoRefresh = false)
{
    model->addFrame(frame, autoRefresh);    
    if (autoRefresh)
    {
        if (ui->cbAutoScroll->isChecked()) ui->canFramesView->scrollToBottom();
        ui->lbNumFrames->setText(QString::number(model->rowCount()));
    }
}

void MainWindow::clearFrames()
{
    ui->canFramesView->scrollToTop();
    model->clearFrames();
    ui->lbNumFrames->setText(QString::number(model->rowCount()));
    bDirty = false;
    loadedFileName = "";
    updateFileStatus();
    emit framesUpdated(-1);
}

void MainWindow::normalizeTiming()
{
    model->normalizeTiming();
}

void MainWindow::changeBaudRates()
{
    int Speed1 = 0, Speed2 = 0;

    Speed1 = ui->cbSpeed1->currentText().toInt();
    Speed2 = ui->cbSpeed2->currentText().toInt();

    emit updateBaudRates(Speed1, Speed2);
}

void MainWindow::handleLoadFile()
{
    QString filename;
    QFileDialog dialog(this);
    bool result = false;

    QStringList filters;
    filters.append(QString(tr("CRTD Logs (*.txt)")));
    filters.append(QString(tr("GVRET Logs (*.csv)")));
    filters.append(QString(tr("Generic ID/Data CSV (*.csv)")));
    filters.append(QString(tr("BusMaster Log (*.log)")));
    filters.append(QString(tr("Microchip Log (*.can)")));

    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        ui->canFramesView->scrollToTop();
        model->clearFrames();

        QVector<CANFrame> tempFrames;

        QProgressDialog progress(this);
        progress.setWindowModality(Qt::WindowModal);
        progress.setLabelText("Loading file...");
        progress.setCancelButton(0);
        progress.setRange(0,0);
        progress.setMinimumDuration(0);
        progress.show();

        qApp->processEvents();

        if (dialog.selectedNameFilter() == filters[0]) result = FrameFileIO::loadCRTDFile(filename, &tempFrames);
        if (dialog.selectedNameFilter() == filters[1]) result = FrameFileIO::loadNativeCSVFile(filename, &tempFrames);
        if (dialog.selectedNameFilter() == filters[2]) result = FrameFileIO::loadGenericCSVFile(filename, &tempFrames);
        if (dialog.selectedNameFilter() == filters[3]) result = FrameFileIO::loadLogFile(filename, &tempFrames);
        if (dialog.selectedNameFilter() == filters[4]) result = FrameFileIO::loadMicrochipFile(filename, &tempFrames);

        progress.cancel();

        if (result)
        {
            model->insertFrames(tempFrames);

            QStringList fileList = filename.split('/');
            loadedFileName = fileList[fileList.length() - 1];

            model->recalcOverwrite();
            ui->lbNumFrames->setText(QString::number(model->rowCount()));
            if (ui->cbAutoScroll->isChecked()) ui->canFramesView->scrollToBottom();

            updateFileStatus();
            emit framesUpdated(-2);
        }
    }
}

void MainWindow::handleSaveFile()
{
    QString filename;
    QFileDialog dialog(this);
    bool result = false;

    QStringList filters;
    filters.append(QString(tr("CRTD Logs (*.txt)")));
    filters.append(QString(tr("GVRET Logs (*.csv)")));
    filters.append(QString(tr("Generic ID/Data CSV (*.csv)")));
    filters.append(QString(tr("BusMaster Log (*.log)")));
    filters.append(QString(tr("Microchip Log (*.can)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        const QVector<CANFrame> *frames = model->getListReference();        
        filename = dialog.selectedFiles()[0];

        QProgressDialog progress(this);
        progress.setWindowModality(Qt::WindowModal);
        progress.setLabelText("Saving file...");
        progress.setCancelButton(0);
        progress.setRange(0,0);
        progress.setMinimumDuration(0);
        progress.show();

        qApp->processEvents();

        if (dialog.selectedNameFilter() == filters[0])
        {
            result = FrameFileIO::saveCRTDFile(filename, frames);
        }
        if (dialog.selectedNameFilter() == filters[1])
        {
            result = FrameFileIO::saveNativeCSVFile(filename, frames);
        }
        if (dialog.selectedNameFilter() == filters[2])
        {
            result = FrameFileIO::saveGenericCSVFile(filename, frames);
        }
        if (dialog.selectedNameFilter() == filters[3])
        {
            result = FrameFileIO::saveLogFile(filename, frames);
        }
        if (dialog.selectedNameFilter() == filters[4])
        {
            result = FrameFileIO::saveMicrochipFile(filename, frames);
        }

        progress.cancel();

        if (result)
        {
            QStringList fileList = filename.split('/');
            loadedFileName = fileList[fileList.length() - 1];
            updateFileStatus();
        }
    }
}

void MainWindow::handleSaveFilteredFile()
{
    QString filename;
    QFileDialog dialog(this);
    bool result = false;

    QStringList filters;
    filters.append(QString(tr("CRTD Logs (*.txt)")));
    filters.append(QString(tr("GVRET Logs (*.csv)")));
    filters.append(QString(tr("Generic ID/Data CSV (*.csv)")));
    filters.append(QString(tr("BusMaster Log (*.log)")));
    filters.append(QString(tr("Microchip Log (*.log)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        const QVector<CANFrame> *frames = model->getFilteredListReference();
        filename = dialog.selectedFiles()[0];

        QProgressDialog progress(this);
        progress.setWindowModality(Qt::WindowModal);
        progress.setLabelText("Saving filtered file...");
        progress.setCancelButton(0);
        progress.setRange(0,0);
        progress.setMinimumDuration(0);
        progress.show();

        qApp->processEvents();

        if (dialog.selectedNameFilter() == filters[0]) result = FrameFileIO::saveCRTDFile(filename, frames);
        if (dialog.selectedNameFilter() == filters[1]) result = FrameFileIO::saveNativeCSVFile(filename, frames);
        if (dialog.selectedNameFilter() == filters[2]) result = FrameFileIO::saveGenericCSVFile(filename, frames);
        if (dialog.selectedNameFilter() == filters[3]) result = FrameFileIO::saveLogFile(filename, frames);
        if (dialog.selectedNameFilter() == filters[4]) result = FrameFileIO::saveMicrochipFile(filename, frames);

        progress.cancel();

        if (result)
        {
            QStringList fileList = filename.split('/');
            loadedFileName = fileList[fileList.length() - 1];
            updateFileStatus();
        }
    }
}

void MainWindow::handleSaveFilters()
{
    QString filename;
    QFileDialog dialog(this);
    bool result = false;

    QStringList filters;
    filters.append(QString(tr("Filter list (*.ftl)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        if (dialog.selectedNameFilter() == filters[0]) model->saveFilterFile(filename);
    }
}

void MainWindow::handleLoadFilters()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("Filter List (*.ftl)")));

    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        //right now there is only one file type that can be loaded here so just do it.
        model->loadFilterFile(filename);
    }
}

void MainWindow::handleLoadDBC()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("DBC File (*.dbc)")));

    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        //right now there is only one file type that can be loaded here so just do it.
        dbcHandler->loadDBCFile(filename);
        //dbcHandler->listDebugging();
        QStringList fileList = filename.split('/');
        lbStatusDatabase.setText(fileList[fileList.length() - 1] + tr(" loaded."));
    }
}

void MainWindow::handleSaveDBC()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("DBC File (*.dbc)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        dbcHandler->saveDBCFile(filename);
        QStringList fileList = filename.split('/');
        lbStatusDatabase.setText(fileList[fileList.length() - 1] + tr(" loaded."));
    }

}

void MainWindow::handleSaveDecoded()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("Text File (*.txt)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        saveDecodedTextFile(filename);
    }
}

void MainWindow::saveDecodedTextFile(QString filename)
{
    QFile *outFile = new QFile(filename);
    const QVector<CANFrame> *frames = model->getListReference();

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
        return;
/*
Time: 205.173000   ID: 0x20E Std Bus: 0 Len: 8
Data Bytes: 88 10 00 13 BB 00 06 00
    SignalName	Value
*/
    for (int c = 0; c < frames->count(); c++)
    {
        CANFrame thisFrame = frames->at(c);
        QString builderString;
        builderString += tr("Time: ") + QString::number((thisFrame.timestamp / 1000000.0), 'f', 6);
        builderString += tr("    ID: 0x") + QString::number(thisFrame.ID, 16).toUpper().rightJustified(8, '0');
        if (thisFrame.extended) builderString += tr(" Ext ");
        else builderString += tr(" Std ");
        builderString += tr("Bus: ") + QString::number(thisFrame.bus);
        builderString += " Len: " + QString::number(thisFrame.len) + "\n";
        outFile->write(builderString.toUtf8());

        builderString = tr("Data Bytes: ");
        for (int temp = 0; temp < thisFrame.len; temp++)
        {
            builderString += QString::number(thisFrame.data[temp], 16).toUpper().rightJustified(2, '0') + " ";
        }
        builderString += "\n";
        outFile->write(builderString.toUtf8());

        builderString = "";
        if (dbcHandler != NULL)
        {
            DBC_MESSAGE *msg = dbcHandler->findMsgByID(thisFrame.ID);
            if (msg != NULL)
            {
                for (int j = 0; j < msg->msgSignals.length(); j++)
                {

                    builderString.append("\t" + dbcHandler->processSignal(thisFrame, msg->msgSignals.at(j)));
                    builderString.append("\n");
                }
            }
            builderString.append("\n");
            outFile->write(builderString.toUtf8());
        }
    }
    outFile->close();
}

void MainWindow::connButtonPress()
{
    if (!isConnected)
    {
        for (int x = 0; x < ports.count(); x++)
        {
            if (ports.at(x).portName() == ui->cbSerialPorts->currentText())
            {
                emit sendSerialPort(&ports[x]);
                lbStatusConnected.setText(tr("Attempting to connect to port ") + ports[x].portName());
                return;
            }
        }
    }
    else
    {
        emit closeSerialPort();
        isConnected = false;
        lbStatusConnected.setText(tr("Not Connected"));
        ui->btnConnect->setText(tr("Connect to GVRET"));
    }
}

void MainWindow::toggleCapture()
{
    allowCapture = !allowCapture;
    if (allowCapture)
    {
        ui->btnCaptureToggle->setText("Suspend Capturing");
        emit startFrameCapturing();
    }
    else
    {
        ui->btnCaptureToggle->setText("Restart Capturing");
        emit stopFrameCapturing();
    }
}

void MainWindow::connectionSucceeded(int baud0, int baud1)
{
    lbStatusConnected.setText(tr("Connected to GVRET"));
    //finally, find the baud rate in the list of rates or
    //add it to the bottom if needed (that'll be weird though...

    int idx = ui->cbSpeed1->findText(QString::number(baud0));
    if (idx > -1) ui->cbSpeed1->setCurrentIndex(idx);
    else
    {
        ui->cbSpeed1->addItem(QString::number(baud0));
        ui->cbSpeed1->setCurrentIndex(ui->cbSpeed1->count() - 1);
    }

    idx = ui->cbSpeed2->findText(QString::number(baud1));
    if (idx > -1) ui->cbSpeed2->setCurrentIndex(idx);
    else
    {
        ui->cbSpeed2->addItem(QString::number(baud1));
        ui->cbSpeed2->setCurrentIndex(ui->cbSpeed2->count() - 1);
    }
    //ui->btnConnect->setEnabled(false);
    ui->btnConnect->setText(tr("Disconnect from GVRET"));
    isConnected = true;
}

void MainWindow::connectionFailed()
{
    lbStatusConnected.setText(tr("Failed to connect!"));

    QMessageBox msgBox;
    msgBox.setText("Connection to the GVRET firmware failed.\nYou might have an old version of GVRET\nor may have chosen the wrong serial port.");
    msgBox.exec();
}

void MainWindow::updateFileStatus()
{
    QString output;
    if (model->rowCount() == 0)
    {
        output = tr("No file loaded");
    }
    else
    {
        if (loadedFileName.length() > 2)
        {
            output = loadedFileName + " loaded";
        }
        else
        {
            output = tr("No file loaded");
        }

        if (bDirty)
        {
            output += " (X)";
        }
    }
    lbStatusFilename.setText(output);
}

void MainWindow::gotDeviceInfo(int build, int swCAN)
{
    QString str = tr("Connected to GVRET ") + QString::number(build);
    if (swCAN == 1) str += "(SW)";
    lbStatusConnected.setText(str);

    if (build < CURRENT_GVRET_VER)
    {
        QMessageBox msgBox;
        msgBox.setText("It appears that you are not running\nan up-to-date version of GVRET\n\nYour version: "
                       + QString::number(build) +"\nCurrent Version: " + QString::number(CURRENT_GVRET_VER)
                       + "\n\nPlease upgrade your firmware version.\nOtherwise, you may experience difficulties.");
        msgBox.exec();
    }
}

void MainWindow::showSettingsDialog()
{
    if (!settingsDialog)
    {
        settingsDialog = new MainSettingsDialog();
        connect(settingsDialog, SIGNAL(finished(int)), this, SLOT(updateSettings()));
    }
    settingsDialog->show();
}

//always gets unfiltered list. You ask for the graphs so there is no need to send filtered frames
void MainWindow::showGraphingWindow()
{
    if (!graphingWindow) {
        graphingWindow = new GraphingWindow(dbcHandler, model->getListReference());
    }
    graphingWindow->show();
}

void MainWindow::showFrameDataAnalysis()
{
    //only create an instance of the object if we dont have one. Otherwise just display the existing one.
    if (!frameInfoWindow)
    {
        if (!useFiltered)
            frameInfoWindow = new FrameInfoWindow(model->getListReference());
        else
            frameInfoWindow = new FrameInfoWindow(model->getFilteredListReference());
    }
    frameInfoWindow->show();
}

void MainWindow::showFrameSenderWindow()
{
    if (!frameSenderWindow)
    {
        if (!useFiltered)
            frameSenderWindow = new FrameSenderWindow(model->getListReference());
        else
            frameSenderWindow = new FrameSenderWindow(model->getFilteredListReference());
    }
    frameSenderWindow->show();
}

void MainWindow::showPlaybackWindow()
{
    if (!playbackWindow)
    {
        if (!useFiltered)
            playbackWindow = new FramePlaybackWindow(model->getListReference(), worker);
        else
            playbackWindow = new FramePlaybackWindow(model->getFilteredListReference(), worker);
    }
    playbackWindow->show();
}

void MainWindow::showComparisonWindow()
{
    if (!comparatorWindow)
    {
        comparatorWindow = new FileComparatorWindow();
    }
    comparatorWindow->show();
}

void MainWindow::showSingleMultiWindow()
{
    //not done yet
}

void MainWindow::showRangeWindow()
{
    //not done yet
}

void MainWindow::showFuzzyScopeWindow()
{
    //not done yet
}

void MainWindow::exitApp()
{
    if (graphingWindow) graphingWindow->close();
    if (frameInfoWindow) frameInfoWindow->close();
    if (playbackWindow) playbackWindow->close();
    if (flowViewWindow) flowViewWindow->close();
    if (frameSenderWindow) frameSenderWindow->close();
    if (dbcMainEditor) dbcMainEditor->close();
    this->close();
}

void MainWindow::showFlowViewWindow()
{
    if (!flowViewWindow)
    {
        if (!useFiltered)
            flowViewWindow = new FlowViewWindow(model->getListReference());
        else
            flowViewWindow = new FlowViewWindow(model->getFilteredListReference());
    }
    flowViewWindow->show();
}


//this one always gets the unfiltered list intentionally.
void MainWindow::showDBCEditor()
{
    if (!dbcMainEditor)
    {
        dbcMainEditor = new DBCMainEditor(dbcHandler, model->getListReference());
    }
    dbcMainEditor->show();
}
