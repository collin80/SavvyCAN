#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "can_structs.h"
#include <QDateTime>
#include <QFileDialog>
#include <QtSerialPort/QSerialPortInfo>
#include "connections/canconmanager.h"
#include "utility.h"
#include "serialworker.h"

/*
Compile for all platforms and create release (remember to include QScintilla libs) and make Win32 binary.

Some notes on things I'd like to put into the program but haven't put on github (yet)

Single / Multi state - The goal is to find bits that change based on toggles or discrete state items (shifters, etc)

fuzzy scope - Try to find potential places where a given value might be stored - offer guesses and the program tries to find candidates for you
or, try to find things that appear to be multi-byte integers

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

    ui->canFramesView->setModel(model);

    readSettings();

    ui->canFramesView->setColumnWidth(0, 110);
    ui->canFramesView->setColumnWidth(1, 70);
    ui->canFramesView->setColumnWidth(2, 40);
    ui->canFramesView->setColumnWidth(3, 40);
    ui->canFramesView->setColumnWidth(4, 40);
    ui->canFramesView->setColumnWidth(5, 40);
    ui->canFramesView->setColumnWidth(6, 275);
    QHeaderView *HorzHdr = ui->canFramesView->horizontalHeader();
    HorzHdr->setStretchLastSection(true); //causes the data column to automatically fill the tableview

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
    dbcHandler = new DBCHandler;
    bDirty = false;
    inhibitFilterUpdate = false;
    rxFrames = 0;
    framesPerSec = 0;

    model->setDBCHandler(dbcHandler);

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

    connect(CANConManager::getInstance(), &CANConManager::framesReceived, model, &CANFrameModel::addFrames);

    lbStatusConnected.setText(tr("Not connected"));
    updateFileStatus();
    lbStatusDatabase.setText(tr("No DBC database loaded"));
    ui->statusBar->addWidget(&lbStatusConnected);
    ui->statusBar->addWidget(&lbStatusFilename);
    ui->statusBar->addWidget(&lbStatusDatabase);

    ui->lbFPS->setText("0");
    ui->lbNumFrames->setText("0");

    connect(&updateTimer, &QTimer::timeout, this, &MainWindow::tickGUIUpdate);
    updateTimer.setInterval(500); //test 250);
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

    //Automatically create the connection window so it can be updated even if we never opened it.
    connectionWindow = new ConnectionWindow();
    connect(this, SIGNAL(suspendCapturing(bool)), connectionWindow, SLOT(setSuspendAll(bool)));
}

MainWindow::~MainWindow()
{
    //serialWorkerThread.quit();
    //serialWorkerThread.wait();
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

    if (discreteStateWindow)
    {
        discreteStateWindow->close();
        delete discreteStateWindow;
    }

    if (connectionWindow)
    {
        connectionWindow->close();
        delete connectionWindow;
    }
   
    if (scriptingWindow)
    {
        scriptingWindow->close();
        delete scriptingWindow;
    }

    if (rangeWindow)
    {
        rangeWindow->close();
        delete rangeWindow;
    }

    if (dbcFileWindow)
    {
        dbcFileWindow->close();
        delete dbcFileWindow;
    }

    if (fuzzingWindow)
    {
        fuzzingWindow->close();
        delete fuzzingWindow;
    }

    if (udsScanWindow)
    {
        udsScanWindow->close();
        delete udsScanWindow;
    }

    if (isoWindow)
    {
        isoWindow->close();
        delete isoWindow;
    }
    if (snifferWindow)
    {
        snifferWindow->close();
        delete snifferWindow;
        snifferWindow = NULL;
    }

    delete model;
    delete elapsedTime;
    delete dbcHandler;
    delete ui;
}

void MainWindow::exitApp()
{
    if (graphingWindow) graphingWindow->close();
    if (frameInfoWindow) frameInfoWindow->close();
    if (playbackWindow) playbackWindow->close();
    if (flowViewWindow) flowViewWindow->close();
    if (frameSenderWindow) frameSenderWindow->close();
    if (dbcMainEditor) dbcMainEditor->close();
    if (comparatorWindow) comparatorWindow->close();
    if (connectionWindow) connectionWindow->close();
    if (settingsDialog) settingsDialog->close();
    if (discreteStateWindow) discreteStateWindow->close();
    if (scriptingWindow) scriptingWindow->close();
    if (rangeWindow) rangeWindow->close();
    if (dbcFileWindow) dbcFileWindow->close();
    if (fuzzingWindow) fuzzingWindow->close();
    if (udsScanWindow) udsScanWindow->close();
    if (isoWindow) isoWindow->close();
    if (snifferWindow) snifferWindow->close();
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

    /*
    int cType = settings.value("Main/DefaultConnectionType", 0).toInt();
    if (cType == 0) connType = "GVRET";
    if (cType == 1) connType = "KVASER";
    if (cType == 2) connType = "SOCKETCAN";
    portName = settings.value("Main/DefaultConnectionPort", "").toString();
    */

    //qDebug() << connType;
    //qDebug() << portName;

    //"Main/SingleWireMode"

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

void MainWindow::updateConnectionSettings(QString connectionType, QString port, int speed0, int speed1)
{
    //connType = connectionType;
    //portName = port;

    //canSpeed0 = speed0;
    //canSpeed1 = speed1;
    if (isConnected)
    {
        //emit updateBaudRates(speed0, speed1);
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
    if(rxFrames>0)
    {
        int elapsed = elapsedTime->elapsed();
        if(elapsed) {
            framesPerSec = rxFrames * 1000 / elapsed;
            elapsedTime->restart();
        }
        else
            framesPerSec = 0;

        ui->lbNumFrames->setText(QString::number(model->rowCount()));
        if (ui->cbAutoScroll->isChecked()) ui->canFramesView->scrollToBottom();
        ui->lbFPS->setText(QString::number(framesPerSec));
        if (rxFrames > 0)
        {
            bDirty = true;
            emit framesUpdated(rxFrames); //anyone care that frames were updated?
        }

        if (model->needsFilterRefresh()) updateFilterList();

        rxFrames = 0;
    }
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
        emit framesUpdated(-2);
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

void MainWindow::connectionSucceeded(int baud0, int baud1)
{
    lbStatusConnected.setText(tr("Connected to GVRET"));
    //finally, find the baud rate in the list of rates or
    //add it to the bottom if needed (that'll be weird though...
/*
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
    */
    if (connectionWindow) connectionWindow->setSpeed(baud0);
    //ui->btnConnect->setEnabled(false);
    ui->actionConnect->setText(tr("Disconnect"));
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

    if (connectionWindow)
    {
        if (swCAN == 1) connectionWindow->setSWMode(true);
        else connectionWindow->setSWMode(false);
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
        //connect(firmwareUploaderWindow, SIGNAL(sendCANFrame(const CANFrame*)), connectionWindow, SLOT(sendFrame(const CANFrame*)));
        //connect(worker, SIGNAL(gotTargettedFrame(int)), firmwareUploaderWindow, SLOT(gotTargettedFrame(int)));
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
        dbcFileWindow = new DBCLoadSaveWindow(dbcHandler, model->getListReference());
    }
    dbcFileWindow->show();
}

void MainWindow::showConnectionSettingsWindow()
{
    if (!connectionWindow)
    {
        connectionWindow = new ConnectionWindow();
    }
    connectionWindow->show();
}
