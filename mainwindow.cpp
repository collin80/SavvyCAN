#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "can_structs.h"
#include <QDateTime>
#include <QFileDialog>
#include <QtSerialPort/QSerialPortInfo>
#include "connections/canconmanager.h"
#include "connections/connectionwindow.h"
#include "utility.h"

/*
Some notes on things I'd like to put into the program but haven't put on github (yet)

Allow scripts to read/write signals from DBC files
allow scripts to load DBC files in support of the script - maybe the graphing system too.
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

    model = new CANFrameModel(this); // set parent to mainwindow to prevent canframemodel to change thread (might be done by setModel but just in case)

    QSortFilterProxyModel* proxyModel = new QSortFilterProxyModel;
    proxyModel->setSourceModel(model);

    ui->canFramesView->setModel(proxyModel);

    readSettings();

    ui->canFramesView->setColumnWidth(0, 150);
    ui->canFramesView->setColumnWidth(1, 70);
    ui->canFramesView->setColumnWidth(2, 40);
    ui->canFramesView->setColumnWidth(3, 40);
    ui->canFramesView->setColumnWidth(4, 40);
    ui->canFramesView->setColumnWidth(5, 40);
    ui->canFramesView->setColumnWidth(6, 225);
    QHeaderView *HorzHdr = ui->canFramesView->horizontalHeader();
    HorzHdr->setStretchLastSection(true); //causes the data column to automatically fill the tableview
    connect(HorzHdr, SIGNAL(sectionClicked(int)), this, SLOT(headerClicked(int)));

    graphingWindow = NULL;
    frameInfoWindow = NULL;
    playbackWindow = NULL;
    flowViewWindow = NULL;
    frameSenderWindow = NULL;
    dbcMainEditor = NULL;
    comparatorWindow = NULL;
    settingsDialog = NULL;
    firmwareUploaderWindow = NULL;
    discreteStateWindow = NULL;
    connectionWindow = NULL;
    scriptingWindow = NULL;
    rangeWindow = NULL;
    dbcFileWindow = NULL;
    fuzzingWindow = NULL;
    udsScanWindow = NULL;
    motorctrlConfigWindow = NULL;
    isoWindow = NULL;
    snifferWindow = NULL;
    bisectWindow = NULL;
    signalViewerWindow = NULL;
    dbcHandler = DBCHandler::getReference();
    bDirty = false;
    inhibitFilterUpdate = false;
    rxFrames = 0;
    framesPerSec = 0;
    continuousLogging = false;
    continuousLogFlushCounter = 0;

    connect(ui->actionSetup, SIGNAL(triggered(bool)), SLOT(showConnectionSettingsWindow()));
    connect(ui->actionOpen_Log_File, &QAction::triggered, this, &MainWindow::handleLoadFile);
    connect(ui->actionGraph_Dta, &QAction::triggered, this, &MainWindow::showGraphingWindow);
    connect(ui->actionFrame_Data_Analysis, &QAction::triggered, this, &MainWindow::showFrameDataAnalysis);
    connect(ui->btnClearFrames, &QAbstractButton::clicked, this, &MainWindow::clearFrames);
    connect(ui->actionSave_Log_File, &QAction::triggered, this, &MainWindow::handleSaveFile);
    connect(ui->actionSave_Filtered_Log_File, &QAction::triggered, this, &MainWindow::handleSaveFilteredFile);
    connect(ui->actionLoad_Filter_Definition, &QAction::triggered, this, &MainWindow::handleLoadFilters);
    connect(ui->actionSave_Filter_Definition, &QAction::triggered, this, &MainWindow::handleSaveFilters);
    connect(ui->action_Playback, &QAction::triggered, this, &MainWindow::showPlaybackWindow);
    connect(ui->actionFlow_View, &QAction::triggered, this, &MainWindow::showFlowViewWindow);
    connect(ui->action_Custom, &QAction::triggered, this, &MainWindow::showFrameSenderWindow);
    connect(ui->canFramesView, &QAbstractItemView::clicked, this, &MainWindow::gridClicked);
    connect(ui->canFramesView, &QAbstractItemView::doubleClicked, this, &MainWindow::gridDoubleClicked);
    connect(ui->cbInterpret, &QAbstractButton::toggled, this, &MainWindow::interpretToggled);
    connect(ui->cbOverwrite, &QAbstractButton::toggled, this, &MainWindow::overwriteToggled);
    connect(ui->btnCaptureToggle, &QAbstractButton::clicked, this, &MainWindow::toggleCapture);
    connect(ui->actionExit_Application, &QAction::triggered, this, &MainWindow::exitApp);
    connect(ui->actionFuzzy_Scope, &QAction::triggered, this, &MainWindow::showFuzzyScopeWindow);
    connect(ui->actionRange_State_2, &QAction::triggered, this, &MainWindow::showRangeWindow);
    connect(ui->actionSave_Decoded_Frames, &QAction::triggered, this, &MainWindow::handleSaveDecoded);
    connect(ui->actionSingle_Multi_State_2, &QAction::triggered, this, &MainWindow::showSingleMultiWindow);
    connect(ui->actionFile_Comparison, &QAction::triggered, this, &MainWindow::showComparisonWindow);
    connect(ui->actionScripting_INterface, &QAction::triggered, this, &MainWindow::showScriptingWindow);
    connect(ui->btnNormalize, &QAbstractButton::clicked, this, &MainWindow::normalizeTiming);
    connect(ui->actionPreferences, &QAction::triggered, this, &MainWindow::showSettingsDialog);
    connect(model, &CANFrameModel::updatedFiltersList, this, &MainWindow::updateFilterList);
    connect(ui->listFilters, &QListWidget::itemChanged, this, &MainWindow::filterListItemChanged);
    connect(ui->btnFilterAll, &QAbstractButton::clicked, this, &MainWindow::filterSetAll);
    connect(ui->btnFilterNone, &QAbstractButton::clicked, this, &MainWindow::filterClearAll);
    connect(ui->actionFirmware_Update, &QAction::triggered, this, &MainWindow::showFirmwareUploaderWindow);
    connect(ui->actionDBC_File_Manager, &QAction::triggered, this, &MainWindow::showDBCFileWindow);
    connect(ui->actionFuzzing, &QAction::triggered, this, &MainWindow::showFuzzingWindow);
    connect(ui->actionUDS_Scanner, &QAction::triggered, this, &MainWindow::showUDSScanWindow);
    connect(ui->actionISO_TP_Decoder, &QAction::triggered, this, &MainWindow::showISOInterpreterWindow);
    connect(ui->actionSniffer, &QAction::triggered, this, &MainWindow::showSnifferWindow);
    connect(ui->actionMotorControlConfig, &QAction::triggered, this, &MainWindow::showMCConfigWindow);
    connect(ui->actionCapture_Bisector, &QAction::triggered, this, &MainWindow::showBisectWindow);
    connect(ui->actionSignal_Viewer, &QAction::triggered, this, &MainWindow::showSignalViewer);
    connect(ui->actionSave_Continuous_Logfile, &QAction::triggered, this, &MainWindow::handleContinousLogging);

    connect(CANConManager::getInstance(), &CANConManager::framesReceived, model, &CANFrameModel::addFrames);

    lbStatusConnected.setText(tr("Connected to 0 buses"));
    updateFileStatus();
    lbStatusDatabase.setText(tr("No DBC database loaded"));
    ui->statusBar->addWidget(&lbStatusConnected);
    ui->statusBar->addWidget(&lbStatusFilename);
    ui->statusBar->addWidget(&lbStatusDatabase);

    ui->lbFPS->setText("0");
    ui->lbNumFrames->setText("0");

    connect(&updateTimer, &QTimer::timeout, this, &MainWindow::tickGUIUpdate);
    updateTimer.setInterval(250);
    updateTimer.start();

    elapsedTime = new QTime;
    elapsedTime->start();

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

    //connect(CANConManager::getInstance(), CANConManager::connectionStatusUpdated, this, MainWindow::connectionStatusUpdated);
    connect(CANConManager::getInstance(), SIGNAL(connectionStatusUpdated(int)), this, SLOT(connectionStatusUpdated(int)));

    //Automatically create the connection window so it can be updated even if we never opened it.
    connectionWindow = new ConnectionWindow();
    connect(this, SIGNAL(suspendCapturing(bool)), connectionWindow, SLOT(setSuspendAll(bool)));

}

MainWindow::~MainWindow()
{
    updateTimer.stop();
    killEmAll(); //Ride the lightning
    delete ui;
    delete model;
    delete elapsedTime;
    delete dbcHandler;

}

//kill every sub window that could be open. At the moment a hard coded list
//but eventually each window should be registered and be able to be iterated.
void MainWindow::killEmAll()
{
    killWindow(graphingWindow);
    killWindow(frameInfoWindow);
    killWindow(playbackWindow);
    killWindow(flowViewWindow);
    killWindow(frameSenderWindow);
    killWindow(comparatorWindow);
    killWindow(dbcMainEditor);
    killWindow(settingsDialog);
    killWindow(discreteStateWindow);
    killWindow(scriptingWindow);
    killWindow(rangeWindow);
    killWindow(dbcFileWindow);
    killWindow(fuzzingWindow);
    killWindow(udsScanWindow);
    killWindow(isoWindow);
    killWindow(snifferWindow);
    killWindow(bisectWindow);
    killWindow(firmwareUploaderWindow);
    killWindow(motorctrlConfigWindow);
    killWindow(signalViewerWindow);
    killWindow(connectionWindow);
}

//forcefully close the window, kill it, and salt the earth
//note, for some stupid reason this function causes a seg fault
//it seems that when it runs just before the program closes it'll
//fault out when trying to close the connection window. I assume
//this could be because that window has long running threads open and doesn't
//close quickly or maybe cleanly. Investigate.
void MainWindow::killWindow(QDialog *win)
{
    if (win)
    {
        win->close();
        delete win;
        win = NULL;
    }
}

void MainWindow::exitApp()
{
    this->close();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
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
    Utility::decimalMode = !useHex;
    secondsMode = settings.value("Main/TimeSeconds", false).toBool();
    model->setSecondsMode(secondsMode);
    useSystemClock = settings.value("Main/TimeClock", false).toBool();
    model->setSysTimeMode(useSystemClock);
    useFiltered = settings.value("Main/UseFiltered", false).toBool();
    model->setTimeFormat(settings.value("Main/TimeFormat", "MMM-dd HH:mm:ss.zzz").toString());

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

void MainWindow::updateConnectionSettings(QString connectionType, QString port, int speed0, int speed1)
{
    Q_UNUSED(connectionType);
    Q_UNUSED(port);
    Q_UNUSED(speed0);
    Q_UNUSED(speed1);
    //connType = connectionType;
    //portName = port;

    //canSpeed0 = speed0;
    //canSpeed1 = speed1;
    if (isConnected)
    {
        //emit updateBaudRates(speed0, speed1);
    }
}

void MainWindow::headerClicked(int logicalIndex)
{
    ui->canFramesView->sortByColumn(logicalIndex);
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

void MainWindow::gridDoubleClicked(QModelIndex idx)
{
    //grab ID and timestamp and send them away
    CANFrame frame = model->getListReference()->at(idx.row());
    emit sendCenterTimeID(frame.ID, frame.timestamp / 1000000.0);
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
        thisItem->setText(Utility::formatNumber(filterIter.key()));
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
    ID = Utility::ParseStringToNum(item->text());
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

void MainWindow::tickGUIUpdate()
{
    rxFrames = model->sendBulkRefresh();
    //if(rxFrames>0)
    //{
        int elapsed = elapsedTime->elapsed();
        if(elapsed) {
            framesPerSec = (framesPerSec + (rxFrames * 1000 / elapsed)) / 2;
            elapsedTime->restart();
        }
        else
            framesPerSec = 0;

        ui->lbNumFrames->setText(QString::number(model->rowCount()));
        if (allowCapture && ui->cbAutoScroll->isChecked()) ui->canFramesView->scrollToBottom();
        ui->lbFPS->setText(QString::number(framesPerSec));
        if (rxFrames > 0)
        {
            bDirty = true;
            emit framesUpdated(rxFrames); //anyone care that frames were updated?
        }

        if (model->needsFilterRefresh()) updateFilterList();

        if (continuousLogging)
        {
            const QVector<CANFrame> *modelFrames = model->getListReference();
            FrameFileIO::writeContinuousNative(modelFrames, modelFrames->count() - rxFrames);

            continuousLogFlushCounter++;
            if ((continuousLogFlushCounter % 3) == 0)
            {
                if (ui->lblContMsg->text().length() > 2)
                {
                    ui->lblContMsg->setText("");
                }
                else
                {
                    ui->lblContMsg->setText("CONTINUOUS LOGGING");
                }
            }
            if (continuousLogFlushCounter > 8)
            {
                continuousLogFlushCounter = 0;
                FrameFileIO::flushContinuousNative();
            }
        }

        rxFrames = 0;
    //}
}

void MainWindow::gotFrames(int framesSinceLastUpdate)
{
    rxFrames += framesSinceLastUpdate;
    emit frameUpdateRapid(framesSinceLastUpdate);
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

//A sub-window is sending us a center on timestamp and ID signal
//try to find the relevant frame in the list and focus on it.
void MainWindow::gotCenterTimeID(int32_t ID, double timestamp)
{
    int idx = model->getIndexFromTimeID(ID, timestamp);
    if (idx > -1)
    {
        ui->canFramesView->selectRow(idx);
    }
}

void MainWindow::clearFrames()
{
    ui->canFramesView->scrollToTop();
    model->clearFrames();
    CANConManager::getInstance()->resetTimeBasis();
    ui->lbNumFrames->setText(QString::number(model->rowCount()));
    bDirty = false;
    loadedFileName = "";
    updateFileStatus();
    emit framesUpdated(-1);
}

void MainWindow::normalizeTiming()
{
    model->normalizeTiming();
    emit framesUpdated(-2); //claim an all new set of frames because every frame was updated.
}

void MainWindow::handleLoadFile()
{
    QString filename;
    QVector<CANFrame> tempFrames;

    if (FrameFileIO::loadFrameFile(filename, &tempFrames))
    {
        ui->canFramesView->scrollToTop();
        model->clearFrames();
        model->insertFrames(tempFrames);
        loadedFileName = filename;
        model->recalcOverwrite();
        ui->lbNumFrames->setText(QString::number(model->rowCount()));
        if (ui->cbAutoScroll->isChecked()) ui->canFramesView->scrollToBottom();

        updateFileStatus();
        emit framesUpdated(-1);
    }
}

void MainWindow::handleSaveFile()
{
    QString filename;

    if (FrameFileIO::saveFrameFile(filename, model->getListReference()))
    {
        loadedFileName = filename;
        updateFileStatus();
    }
}

void MainWindow::handleContinousLogging()
{
    continuousLogging = !continuousLogging;

    if (continuousLogging)
    {
        ui->actionSave_Continuous_Logfile->setText(tr("Cease Continuous Logging"));
        FrameFileIO::openContinuousNative();
    }
    else
    {
        ui->actionSave_Continuous_Logfile->setText(tr("Start Continuous Logging"));
        ui->lblContMsg->setText("");
        FrameFileIO::closeContinuousNative();
    }
}

void MainWindow::handleSaveFilteredFile()
{
    QString filename;

    if (FrameFileIO::saveFrameFile(filename, model->getFilteredListReference()))
    {
        loadedFileName = filename;
        updateFileStatus();
    }
}

void MainWindow::handleSaveFilters()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("Filter list (*.ftl)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        if (!filename.contains('.')) filename += ".ftl";
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

//lbStatusDatabase.setText(fileList[fileList.length() - 1] + tr(" loaded."));

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
        if (!filename.contains('.')) filename += ".txt";
        saveDecodedTextFile(filename);
    }
}

void MainWindow::saveDecodedTextFile(QString filename)
{
    QFile *outFile = new QFile(filename);
    const QVector<CANFrame> *frames = model->getFilteredListReference();

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
        builderString += tr("    ID: ") + Utility::formatNumber(thisFrame.ID);
        if (thisFrame.extended) builderString += tr(" Ext ");
        else builderString += tr(" Std ");
        builderString += tr("Bus: ") + QString::number(thisFrame.bus);
        builderString += " Len: " + QString::number(thisFrame.len) + "\n";
        outFile->write(builderString.toUtf8());

        builderString = tr("Data Bytes: ");
        for (unsigned int temp = 0; temp < thisFrame.len; temp++)
        {
            builderString += Utility::formatNumber(thisFrame.data[temp]) + " ";
        }
        builderString += "\n";
        outFile->write(builderString.toUtf8());

        builderString = "";
        if (dbcHandler != NULL)
        {
            DBC_MESSAGE *msg = dbcHandler->findMessage(thisFrame);
            if (msg != NULL)
            {
                for (int j = 0; j < msg->sigHandler->getCount(); j++)
                {

                    QString temp;
                    if (msg->sigHandler->findSignalByIdx(j)->processAsText(thisFrame, temp))
                    {
                        builderString.append("\t" + temp);
                        builderString.append("\n");
                    }
                }
            }
            builderString.append("\n");
            outFile->write(builderString.toUtf8());
        }
    }
    outFile->close();
}

void MainWindow::toggleCapture()
{
    allowCapture = !allowCapture;
    if (allowCapture)
        ui->btnCaptureToggle->setText("Suspend Capturing");
    else
        ui->btnCaptureToggle->setText("Restart Capturing");

    emit suspendCapturing(!allowCapture);
}

void MainWindow::connectionStatusUpdated(int conns)
{
    lbStatusConnected.setText(tr("Connected to ") + QString::number(conns) + tr(" buses"));
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

CANFrameModel* MainWindow::getCANFrameModel()
{
    return model;
}

void MainWindow::showSettingsDialog()
{
    if (!settingsDialog)
    {
        settingsDialog = new MainSettingsDialog();
        connect (settingsDialog, SIGNAL(updatedSettings()), this, SLOT(readUpdateableSettings()));
    }
    settingsDialog->show();
}

//always gets unfiltered list. You ask for the graphs so there is no need to send filtered frames
void MainWindow::showGraphingWindow()
{
    if (!graphingWindow) {
        graphingWindow = new GraphingWindow(model->getListReference());
        connect(graphingWindow, SIGNAL(sendCenterTimeID(int32_t,double)), this, SLOT(gotCenterTimeID(int32_t,double)));
        connect(this, SIGNAL(sendCenterTimeID(int32_t,double)), graphingWindow, SLOT(gotCenterTimeID(int32_t,double)));
    }

    if (flowViewWindow) //connect the two external windows together
    {
        connect(graphingWindow, SIGNAL(sendCenterTimeID(int32_t,double)), flowViewWindow, SLOT(gotCenterTimeID(int32_t,double)));
        connect(flowViewWindow, SIGNAL(sendCenterTimeID(int32_t,double)), graphingWindow, SLOT(gotCenterTimeID(int32_t,double)));
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

void MainWindow::showISOInterpreterWindow()
{
    if (!isoWindow)
    {
        if (!useFiltered)
            isoWindow = new ISOTP_InterpreterWindow(model->getListReference());
        else
            isoWindow = new ISOTP_InterpreterWindow(model->getFilteredListReference());
    }
    isoWindow->show();
}

void MainWindow::showSnifferWindow()
{
    if (!snifferWindow)
        snifferWindow = new SnifferWindow(this);
    snifferWindow->show();
}

void MainWindow::showBisectWindow()
{
    if (!bisectWindow)
    {
        bisectWindow = new BisectWindow(model->getListReference());
    }
    bisectWindow->show();
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
            playbackWindow = new FramePlaybackWindow(model->getListReference());
        else
            playbackWindow = new FramePlaybackWindow(model->getFilteredListReference());
    }
    playbackWindow->show();
}

void MainWindow::showFirmwareUploaderWindow()
{
    if (!firmwareUploaderWindow)
    {
        firmwareUploaderWindow = new FirmwareUploaderWindow(model->getListReference());
    }
    firmwareUploaderWindow->show();
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
    if (!discreteStateWindow)
    {
        discreteStateWindow = new DiscreteStateWindow(model->getListReference());
    }
    discreteStateWindow->show();
}

void MainWindow::showFuzzingWindow()
{
    if (!fuzzingWindow)
    {
        fuzzingWindow = new FuzzingWindow(model->getListReference());
    }
    fuzzingWindow->show();
}

void MainWindow::showMCConfigWindow()
{
    if (!motorctrlConfigWindow)
    {
        motorctrlConfigWindow = new MotorControllerConfigWindow(model->getListReference());
        //connect(motorctrlConfigWindow, SIGNAL(sendCANFrame(const CANFrame*,int)), worker, SLOT(sendFrame(const CANFrame*,int)));
        //connect(motorctrlConfigWindow, SIGNAL(sendFrameBatch(const QList<CANFrame>*)), worker, SLOT(sendFrameBatch(const QList<CANFrame>*)));
    }
    motorctrlConfigWindow->show();
}

void MainWindow::showUDSScanWindow()
{
    if (!udsScanWindow)
    {
        udsScanWindow = new UDSScanWindow(model->getListReference());
    }
    udsScanWindow->show();
}

void MainWindow::showScriptingWindow()
{
    if (!scriptingWindow)
    {
        scriptingWindow = new ScriptingWindow(model->getListReference());
    }
    scriptingWindow->show();
}

void MainWindow::showRangeWindow()
{
    if (!rangeWindow)
    {
        rangeWindow = new RangeStateWindow(model->getListReference());
    }
    rangeWindow->show();
}

void MainWindow::showFuzzyScopeWindow()
{
    //not done yet
}

void MainWindow::showFlowViewWindow()
{
    if (!flowViewWindow)
    {
        if (!useFiltered)
            flowViewWindow = new FlowViewWindow(model->getListReference());
        else
            flowViewWindow = new FlowViewWindow(model->getFilteredListReference());
        connect(flowViewWindow, SIGNAL(sendCenterTimeID(int32_t,double)), this, SLOT(gotCenterTimeID(int32_t,double)));
        connect(this, SIGNAL(sendCenterTimeID(int32_t,double)), flowViewWindow, SLOT(gotCenterTimeID(int32_t,double)));
    }

    if (graphingWindow)
    {
        connect(graphingWindow, SIGNAL(sendCenterTimeID(int32_t,double)), flowViewWindow, SLOT(gotCenterTimeID(int32_t,double)));
        connect(flowViewWindow, SIGNAL(sendCenterTimeID(int32_t,double)), graphingWindow, SLOT(gotCenterTimeID(int32_t,double)));
    }

    flowViewWindow->show();
}

void MainWindow::showDBCFileWindow()
{
    if (!dbcFileWindow)
    {
        dbcFileWindow = new DBCLoadSaveWindow(model->getListReference());
    }
    dbcFileWindow->show();
}

void MainWindow::showSignalViewer()
{
    if (!signalViewerWindow)
    {
        signalViewerWindow = new SignalViewerWindow();
    }
    signalViewerWindow->show();
}

void MainWindow::showConnectionSettingsWindow()
{
    if (!connectionWindow)
    {
        connectionWindow = new ConnectionWindow();
    }
    connectionWindow->show();
}
