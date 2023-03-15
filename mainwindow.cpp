#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "can_structs.h"
#include <QDateTime>
#include <QFileDialog>
#include <QtSerialPort/QSerialPortInfo>
#include "connections/canconmanager.h"
#include "connections/connectionwindow.h"
#include "helpwindow.h"
#include "utility.h"
#include "filterutility.h"

/*
Some notes on things I'd like to put into the program but haven't put on github (yet)

Allow scripts to read/write signals from DBC files
allow scripts to load DBC files in support of the script - maybe the graphing system too.
*/

QString MainWindow::loadedFileName = "";
MainWindow *MainWindow::selfRef = nullptr;

MainWindow *MainWindow::getReference()
{
    return selfRef;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
#if QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 )
    qRegisterMetaTypeStreamOperators<QVector<QString>>();
    qRegisterMetaTypeStreamOperators<QVector<int>>();
#endif

    useHex = true;

    selfRef = this;

    this->setWindowTitle("Savvy CAN V" + QString::number(VERSION) + " [Built " + QString(__DATE__) +"]");

    model = new CANFrameModel(this); // set parent to mainwindow to prevent canframemodel to change thread (might be done by setModel but just in case)

    QSortFilterProxyModel* proxyModel = new QSortFilterProxyModel;
    proxyModel->setSourceModel(model);

    ui->canFramesView->setModel(proxyModel);

    settingsDialog = new MainSettingsDialog(); //instantiate the settings dialog so it can initialize settings if this is the first run or the config file was deleted.
    settingsDialog->updateSettings(); //write out all the settings. If this is the first run it'll write defaults out.

    readSettings();

    QHeaderView *verticalHeader = ui->canFramesView->verticalHeader();
    verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
    QSettings settings;
    int fontSize = settings.value("Main/FontSize", 9).toUInt();
    QFont sysFont;
    if(settings.value("Main/FontFixedWidth", false).toBool())
        sysFont = QFontDatabase::systemFont(QFontDatabase::FixedFont); //get default fixed width font
    else
        sysFont = QFont();  //get default font
    sysFont.setPointSize(fontSize);
    verticalHeader->setDefaultSectionSize(sysFont.pixelSize());
    verticalHeader->setFont(QFont());
    ui->canFramesView->setFont(sysFont);

    QHeaderView *HorzHdr = ui->canFramesView->horizontalHeader();
    HorzHdr->setFont(QFont());
    HorzHdr->setStretchLastSection(true); //causes the data column to automatically fill the tableview
    connect(HorzHdr, SIGNAL(sectionClicked(int)), this, SLOT(headerClicked(int)));

    lastGraphingWindow = nullptr;
    frameInfoWindow = nullptr;
    playbackWindow = nullptr;
    flowViewWindow = nullptr;
    frameSenderWindow = nullptr;
    dbcMainEditor = nullptr;
    comparatorWindow = nullptr;
    settingsDialog = nullptr;
    firmwareUploaderWindow = nullptr;
    discreteStateWindow = nullptr;
    connectionWindow = nullptr;
    scriptingWindow = nullptr;
    rangeWindow = nullptr;
    dbcFileWindow = nullptr;
    fuzzingWindow = nullptr;
    udsScanWindow = nullptr;
    motorctrlConfigWindow = nullptr;
    isoWindow = nullptr;
    snifferWindow = nullptr;
    bisectWindow = nullptr;
    signalViewerWindow = nullptr;
    temporalGraphWindow = nullptr;
    dbcComparatorWindow = nullptr;
    canBridgeWindow = nullptr;
    dbcHandler = DBCHandler::getReference();
    bDirty = false;
    inhibitFilterUpdate = false;
    rxFrames = 0;
    framesPerSec = 0;
    continuousLogging = false;
    continuousLogFlushCounter = 0;

    //handlers for all menu entries
    connect(ui->actionSetup, SIGNAL(triggered(bool)), SLOT(showConnectionSettingsWindow()));
    connect(ui->actionOpen_Log_File, &QAction::triggered, this, &MainWindow::handleLoadFile);
    connect(ui->actionGraph_Dta, &QAction::triggered, this, &MainWindow::showGraphingWindow);
    connect(ui->actionFrame_Data_Analysis, &QAction::triggered, this, &MainWindow::showFrameDataAnalysis);
    connect(ui->actionSave_Log_File, &QAction::triggered, this, &MainWindow::handleSaveFile);
    connect(ui->actionSave_Filtered_Log_File, &QAction::triggered, this, &MainWindow::handleSaveFilteredFile);
    connect(ui->actionLoad_Filter_Definition, &QAction::triggered, this, &MainWindow::handleLoadFilters);
    connect(ui->actionSave_Filter_Definition, &QAction::triggered, this, &MainWindow::handleSaveFilters);
    connect(ui->action_Playback, &QAction::triggered, this, &MainWindow::showPlaybackWindow);
    connect(ui->actionFlow_View, &QAction::triggered, this, &MainWindow::showFlowViewWindow);
    connect(ui->action_Custom, &QAction::triggered, this, &MainWindow::showFrameSenderWindow);
    connect(ui->actionExit_Application, &QAction::triggered, this, &MainWindow::exitApp);
    connect(ui->actionFuzzy_Scope, &QAction::triggered, this, &MainWindow::showFuzzyScopeWindow);
    connect(ui->actionRange_State_2, &QAction::triggered, this, &MainWindow::showRangeWindow);
    connect(ui->actionSave_Decoded_Frames, &QAction::triggered, this, &MainWindow::handleSaveDecoded);
    connect(ui->actionSave_Decoded_Frames_CSV, &QAction::triggered, this, &MainWindow::handleSaveDecodedCsv);
    connect(ui->actionSingle_Multi_State_2, &QAction::triggered, this, &MainWindow::showSingleMultiWindow);
    connect(ui->actionFile_Comparison, &QAction::triggered, this, &MainWindow::showComparisonWindow);
    connect(ui->actionDBC_Comparison, &QAction::triggered, this, &MainWindow::showDBCComparisonWindow);
    connect(ui->actionScripting_INterface, &QAction::triggered, this, &MainWindow::showScriptingWindow);
    connect(ui->actionPreferences, &QAction::triggered, this, &MainWindow::showSettingsDialog);
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
    connect(ui->actionTemporal_Graph, &QAction::triggered, this, &MainWindow::showTemporalGraphWindow);
    connect(ui->actionCAN_Bridge, &QAction::triggered, this, &MainWindow::showCANBridgeWindow);

    //handlers fror interactions with the main can frame view table
    connect(ui->canFramesView, &QAbstractItemView::clicked, this, &MainWindow::gridClicked);
    connect(ui->canFramesView, &QAbstractItemView::doubleClicked, this, &MainWindow::gridDoubleClicked);
    ui->canFramesView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->canFramesView, &QAbstractItemView::customContextMenuRequested, this, &MainWindow::gridContextMenuRequest);

    connect(model, &CANFrameModel::updatedFiltersList, this, &MainWindow::updateFilterList);
    connect(CANConManager::getInstance(), &CANConManager::framesReceived, model, &CANFrameModel::addFrames);
    //new implementation for continuous logging
    connect(CANConManager::getInstance(), &CANConManager::framesReceived, this, &MainWindow::logReceivedFrame);

    connect(ui->cbInterpret, &QAbstractButton::toggled, this, &MainWindow::interpretToggled);
    connect(ui->cbOverwrite, &QAbstractButton::toggled, this, &MainWindow::overwriteToggled);
    connect(ui->cbPersistentFilters, &QAbstractButton::toggled, this, &MainWindow::presistentFiltersToggled);
    connect(ui->listFilters, &QListWidget::itemChanged, this, &MainWindow::filterListItemChanged);
    connect(ui->listBusFilters, &QListWidget::itemChanged, this, &MainWindow::busFilterListItemChanged);

    connect(ui->btnCaptureToggle, &QAbstractButton::clicked, this, &MainWindow::toggleCapture);
    connect(ui->btnClearFrames, &QAbstractButton::clicked, this, &MainWindow::clearFrames);
    connect(ui->btnNormalize, &QAbstractButton::clicked, this, &MainWindow::normalizeTiming);
    connect(ui->btnFilterAll, &QAbstractButton::clicked, this, &MainWindow::filterSetAll);
    connect(ui->btnFilterNone, &QAbstractButton::clicked, this, &MainWindow::filterClearAll);
    connect(ui->btnExpandAll, &QAbstractButton::clicked, this, &MainWindow::expandAllRows);
    connect(ui->btnCollapseAll, &QAbstractButton::clicked, this, &MainWindow::collapseAllRows);

    lbStatusConnected.setText(tr("Connected to 0 buses"));
    lbHelp.setText(tr("Press F1 on any screen for help"));
    lbHelp.setAlignment(Qt::AlignCenter);
    QFont boldFont;
    boldFont.setBold(true);
    lbHelp.setFont(boldFont);
    updateFileStatus();
    //lbStatusDatabase.setText(tr("No DBC database loaded"));
    ui->statusBar->insertWidget(0, &lbStatusConnected, 1);
    ui->statusBar->insertWidget(1, &lbStatusFilename, 1);
    ui->statusBar->insertWidget(2, &lbHelp, 1);
    //ui->statusBar->addWidget(&lbStatusDatabase);
    ui->lblRemoteConn->setVisible(false);
    ui->lineRemoteKey->setVisible(false);

    ui->lbFPS->setText("0");
    ui->lbNumFrames->setText("0");

    // Prevent annoying accidental horizontal scrolling when filter list is populated with long interpreted message names
    ui->listFilters->horizontalScrollBar()->setEnabled(false);

    connect(&updateTimer, &QTimer::timeout, this, &MainWindow::tickGUIUpdate);
    updateTimer.setInterval(250);
    updateTimer.start();

    elapsedTime = new QElapsedTimer;
    elapsedTime->start();

    isConnected = false;
    allowCapture = true;

    //create a temporary frame to be able to capture the correct
    //default height of an item in the table. Need to do this in case
    //of scaling or font differences between different computers.
    CANFrame temp;
    temp.bus = 0;
    temp.setFrameId(0x100);
    temp.isReceived = true;
    temp.setTimeStamp(QCanBusFrame::TimeStamp(0, 100000000));
    model->addFrame(temp, true);
    qApp->processEvents();
    tickGUIUpdate(); //force a GUI refresh so that the row exists to measure
    normalRowHeight = ui->canFramesView->rowHeight(0);
    if (normalRowHeight == 0) normalRowHeight = 30; //should not be necessary but provides a sane number if something stupid happened.
    qDebug() << "normal row height = " << normalRowHeight;
    model->clearFrames();

    //connect(CANConManager::getInstance(), CANConManager::connectionStatusUpdated, this, MainWindow::connectionStatusUpdated);
    connect(CANConManager::getInstance(), SIGNAL(connectionStatusUpdated(int)), this, SLOT(connectionStatusUpdated(int)));

    //Automatically create the connection window so it can be updated even if we never opened it.
    connectionWindow = new ConnectionWindow();
    connect(this, SIGNAL(suspendCapturing(bool)), connectionWindow, SLOT(setSuspendAll(bool)));

    //these either are unfinished/not working or are not for general use. But,they exist
    //so if you want to enable them and play with them then go for it.
    ui->actionFirmware_Update->setVisible(false);
    ui->actionMotorControlConfig->setVisible(false);
    ui->actionSingle_Multi_State_2->setVisible(false);

    installEventFilter(this);
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
    foreach (GraphingWindow *win, graphWindows)
    {
        killWindow(win);
    }
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
    killWindow(temporalGraphWindow);
    killWindow(canBridgeWindow);

    //trying to kill this window can cause a fault to happen. It's closed last just in case.
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
        win = nullptr;
    }
}

void MainWindow::exitApp()
{
    this->close();
    QApplication::quit(); //forces the whole application to terminate when the main window is closed
}


//the close event can be trapped and ignored so put unsaved warnings in here so the user can abort the program closing if they forgot to save things.
void MainWindow::closeEvent(QCloseEvent *event)
{

    QMessageBox::StandardButton confirmDialog;

    for (int i = 0; i < dbcHandler->getFileCount(); i++)
    {
        DBCFile *file = dbcHandler->getFileByIdx(i);
        if (file->getDirtyFlag())
        {
            confirmDialog = QMessageBox::question(this, "Unsaved DBC", "DBC File:\n" + file->getFilename() + "\nAppears to have unsaved changes\nReally close without saving?", QMessageBox::Yes|QMessageBox::No);
            if (confirmDialog != QMessageBox::Yes)
            {
                event->ignore();
                return;
            }
        }
    }

    removeEventFilter(this);
    writeSettings();
    exitApp();
    event->accept();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("mainscreen.md");
            return true;
            break;
        }
        return false;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
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
        move(Utility::constrainedWindowPos(settings.value("Main/WindowPos", QPoint(100, 100)).toPoint()));

        ui->canFramesView->setColumnWidth(0, settings.value("Main/TimeColumn", 150).toUInt()); //time stamp
        ui->canFramesView->setColumnWidth(1, settings.value("Main/IDColumn", 70).toUInt()); //frame ID
        ui->canFramesView->setColumnWidth(2, settings.value("Main/ExtColumn", 40).toUInt()); //extended
        ui->canFramesView->setColumnWidth(3, settings.value("Main/RemColumn", 40).toUInt()); //remote
        ui->canFramesView->setColumnWidth(4, settings.value("Main/DirColumn", 40).toUInt()); //direction
        ui->canFramesView->setColumnWidth(5, settings.value("Main/BusColumn", 40).toUInt()); //bus
        ui->canFramesView->setColumnWidth(6, settings.value("Main/LengthColumn", 40).toUInt()); //length
        ui->canFramesView->setColumnWidth(7, settings.value("Main/AsciiColumn", 50).toUInt()); //ascii
        //ui->canFramesView->setColumnWidth(8, settings.value("Main/DataColumn", 225).toUInt()); //data
    }
    if (settings.value("Main/AutoScroll", false).toBool())
    {
        ui->cbAutoScroll->setChecked(true);
    }
    int fontSize = settings.value("Main/FontSize", 9).toUInt();
    QFont newFont = QFont(ui->canFramesView->font());
    newFont.setPointSize(fontSize);
    ui->canFramesView->setFont(newFont);

    readUpdateableSettings();
}


/*
 * TODO: The way the frame timing mode is specified is DEAD STUPID. There shouldn't be three boolean values
 * for this. Instead switch it all to an ENUM or something sane.
*/
void MainWindow::readUpdateableSettings()
{
    QSettings settings;
    useHex = settings.value("Main/UseHex", true).toBool();
    model->setHexMode(useHex);
    Utility::decimalMode = !useHex;

    bool tempBool;
    TimeStyle ts = TS_MICROS;
    tempBool = settings.value("Main/TimeSeconds", false).toBool();
    if (tempBool) ts = TS_SECONDS;
    tempBool = settings.value("Main/TimeClock", false).toBool();
    if (tempBool) ts = TS_CLOCK;
    tempBool = settings.value("Main/TimeMillis", false).toBool();
    if (tempBool) ts = TS_MILLIS;
    model->setTimeStyle(ts);

    useFiltered = settings.value("Main/UseFiltered", false).toBool();
    model->setTimeFormat(settings.value("Main/TimeFormat", "MMM-dd HH:mm:ss.zzz").toString());
    ignoreDBCColors = settings.value("Main/IgnoreDBCColors", false).toBool();
    model->setIgnoreDBCColors(ignoreDBCColors);
    int bpl = settings.value("Main/BytesPerLine", 8).toInt();
    model->setBytesPerLine(bpl);

    if (settings.value("Main/FilterLabeling", false).toBool())
        ui->listFilters->setMaximumWidth(250);
    else
        ui->listFilters->setMaximumWidth(175);
    updateFilterList();    
}    


void MainWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("Main/WindowSize", size());
        settings.setValue("Main/WindowPos", pos());
        settings.setValue("Main/TimeColumn", ui->canFramesView->columnWidth(0));
        settings.setValue("Main/IDColumn", ui->canFramesView->columnWidth(1));
        settings.setValue("Main/ExtColumn", ui->canFramesView->columnWidth(2));
        settings.setValue("Main/RemColumn", ui->canFramesView->columnWidth(3));
        settings.setValue("Main/DirColumn", ui->canFramesView->columnWidth(4));
        settings.setValue("Main/BusColumn", ui->canFramesView->columnWidth(5));
        settings.setValue("Main/LengthColumn", ui->canFramesView->columnWidth(6));
        settings.setValue("Main/AsciiColumn", ui->canFramesView->columnWidth(7));
        //settings.setValue("Main/DataColumn", ui->canFramesView->columnWidth(8));
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
    //ui->canFramesView->sortByColumn(logicalIndex);
    model->sortByColumn(logicalIndex);

    manageRowExpansion();
}

void MainWindow::expandAllRows()
{
    bool goAhead = false;
    int numRows = ui->canFramesView->model()->rowCount();

    if (numRows > 20000)
    {
        QMessageBox::StandardButton confirmDialog;
        confirmDialog = QMessageBox::question(this, "Really?", "It's not recommended to use this\non more than 20000 frames.\nIt can take a long time.\n\nYou have been warned!\nStill do it?",
                                  QMessageBox::Yes|QMessageBox::No);

        if (confirmDialog == QMessageBox::Yes) goAhead = true;
    }
    else goAhead = true;

    if (goAhead)
    {
        ui->canFramesView->resizeRowsToContents();

        rowExpansionActive = true;
    }
}

void MainWindow::manageRowExpansion()
{
    int numRows = ui->canFramesView->model()->rowCount();
    if(numRows < 20000)
    {
        if(rowExpansionActive && model->getInterpretMode())
            ui->canFramesView->resizeRowsToContents();
    }
    else
    {
        disableAutoRowExpansion();
    }
}

void MainWindow::disableAutoRowExpansion()
{
    rowExpansionActive = false;
}

void MainWindow::collapseAllRows()
{
    bool goAhead = false;
    int numRows = ui->canFramesView->model()->rowCount();

    if (numRows > 50000)
    {
        QMessageBox::StandardButton confirmDialog;
        confirmDialog = QMessageBox::question(this, "Really?", "It's not recommended to use this\non more than 50000 frames.\nIt can take a long time.\n\nYou have been warned!\nStill do it?",
                                  QMessageBox::Yes|QMessageBox::No);

        if (confirmDialog == QMessageBox::Yes) goAhead = true;
    }
    else goAhead = true;

    if (goAhead)
    {
        for (int i = 0; i < numRows; i++) ui->canFramesView->setRowHeight(i, normalRowHeight);

        rowExpansionActive = false;
    }
}

void MainWindow::gridClicked(QModelIndex idx)
{
    //qDebug() << "Grid Clicked";
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
    qDebug() << "Grid double clicked";
    //grab ID and timestamp and send them away
    CANFrame frame = model->getListReference()->at(idx.row());
    emit sendCenterTimeID(frame.frameId(), frame.timeStamp().microSeconds() / 1000000.0);
}

void MainWindow::gridContextMenuRequest(QPoint pos)
{
    QModelIndex idx = ui->canFramesView->indexAt(pos); //figure out where in the view we clicked (row, column)
    qDebug() << "Pos: " << pos << " Row :" << idx.row() << " Col: " << idx.column();
    qDebug() << "Data: " << idx.data();
    if (idx.column() == 8) //we're over the DATA column
    {
        contextMenuPosition = pos;

        QMenu *menu = new QMenu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        menu->addAction(tr("Add to a new graphing window"), this, SLOT(setupAddToNewGraph()));
        menu->addSeparator();
        menu->addAction(tr("Add to latest graphing window"), this, SLOT(setupSendToLatestGraphWindow()));

        menu->popup(ui->canFramesView->mapToGlobal(pos));
    }
}

QString MainWindow::getSignalNameFromPosition(QPoint pos)
{
    //there's a bit of an issue to solve. The data column is one big string even if there are a number
    //of signals in there. So, the basic idea is to find out how tall the font is and where the user
    //clicked within the cell. Then find out which line that puts us over.
    QModelIndex idx = ui->canFramesView->indexAt(pos); //figure out where in the view we clicked (row, column)
    int fontHeight = ui->canFramesView->fontMetrics().height();
    int cellBaseY = ui->canFramesView->rowViewportPosition(idx.row());
    int lineOffset = (pos.y() - cellBaseY) / fontHeight;
    qDebug() << "Offset: " << lineOffset;
    QString lineText = idx.data().toString().split("\n")[lineOffset];
    qDebug() << "Line Text: " << lineText;
    return lineText.split(":")[0];
}

uint32_t MainWindow::getMessageIDFromPosition(QPoint pos)
{
    QModelIndex idx = ui->canFramesView->indexAt(pos); //figure out where in the view we clicked (row, column)
    QString idText = ui->canFramesView->model()->index(idx.row(), 1).data().toString();
    return Utility::ParseStringToNum(idText);
}

void MainWindow::setupAddToNewGraph()
{
    showGraphingWindow(); //creates a new window and sets it as latest
    setupSendToLatestGraphWindow(); //then call the other function to finish
}


void MainWindow::setupSendToLatestGraphWindow()
{
    if (!lastGraphingWindow) showGraphingWindow();
    GraphParams param;
    QString signalName = getSignalNameFromPosition(contextMenuPosition);
    param.ID = getMessageIDFromPosition(contextMenuPosition);
    DBC_MESSAGE *msg = dbcHandler->findMessage(param.ID);
    if(msg)
    {
        DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(signalName);
        if(sig)
        {
            param.associatedSignal = sig;
            param.bias = sig->bias;
            param.intelFormat = sig->intelByteOrder;
            param.isSigned = sig->valType == SIGNED_INT ? true : false;
            param.numBits = sig->signalSize;
            param.scale = sig->factor;
            param.startBit = sig->startBit;
            param.stride = 1;
            param.graphName = sig->name;
            param.lineColor = QColor(QRandomGenerator::global()->bounded(160), QRandomGenerator::global()->bounded(160), QRandomGenerator::global()->bounded(160));
            param.lineWidth = 1;
            param.fillColor = QColor(128, 128, 128, 0);
            param.mask = 0xFFFFFFFFFFFFFFFFull;
            param.drawOnlyPoints = false;
            param.pointType = 0;

            lastGraphingWindow->createGraph(param); //add the new graph to the window
        }
        else
        {
            QMessageBox msgbox;
            QString boxmsg = "Cannot find ID 0x" + QStringLiteral("%1").arg(param.ID, 3, 16, QLatin1Char('0')) + " in DBC message " + msg->name + ". Not adding graph.";
            msgbox.setText(boxmsg);
            msgbox.exec();
        }
    }
    else
    {
        QMessageBox msgbox;
        QString boxmsg = "Cannot find ID 0x" + QStringLiteral("%1").arg(param.ID, 3, 16, QLatin1Char('0')) + " in DBC file(s). Not adding graph.";
        msgbox.setText(boxmsg);
        msgbox.exec();
    }
}
void MainWindow::interpretToggled(bool state)
{
    model->setInterpretMode(state);
    //ui->canFramesView->resizeRowsToContents();   //a VERY costly operation!
}

void MainWindow::overwriteToggled(bool state)
{
    if (state)
    {
        QMessageBox::StandardButton confirmDialog;
        confirmDialog = QMessageBox::question(this, "Danger Will Robinson", "Enabling Overwrite mode will\ndelete your captured frames\nand replace them with one\nframe per ID.\n\nAre you ready to do that?",
                                      QMessageBox::Yes|QMessageBox::No);
        if (confirmDialog == QMessageBox::Yes)
        {
            model->setOverwriteMode(true);
        }
        else ui->cbOverwrite->setCheckState(Qt::Unchecked);
    }
    else
    {
        rowExpansionActive = false;
        model->setOverwriteMode(false);
    }
}

void MainWindow::presistentFiltersToggled(bool state)
{
    if (state)
    {
        model->setClearMode(true);
    }
    else
    {
        model->setClearMode(false);
    }
}

void MainWindow::updateFilterList()
{
    if (model == nullptr) return;
    const QMap<int, bool> *filters = model->getFiltersReference();
    const QMap<int, bool> *busFilters = model->getBusFiltersReference();
    if (filters == nullptr && busFilters == nullptr) return;

    qDebug() << "updateFilterList called on MainWindow";

    inhibitFilterUpdate = true;

    ui->listFilters->clear();
    ui->listBusFilters->clear();

    if (filters->isEmpty()) return;

    QMap<int, bool>::const_iterator filterIter;
    for (filterIter = filters->begin(); filterIter != filters->end(); ++filterIter)
    {
        /*QListWidgetItem *thisItem = */FilterUtility::createCheckableFilterItem(filterIter.key(), filterIter.value(), ui->listFilters);
    }

    if (busFilters->isEmpty()) return;

    for (filterIter = busFilters->begin(); filterIter != busFilters->end(); ++filterIter)
    {
        /*QListWidgetItem *thisItem = */ FilterUtility::createCheckableBusFilterItem(filterIter.key(), filterIter.value(), ui->listBusFilters);
    }
    inhibitFilterUpdate = false;
}

void MainWindow::filterListItemChanged(QListWidgetItem *item)
{
    if (inhibitFilterUpdate) return;
    //qDebug() << item->text();

    // strip away possible filter label
    int ID = FilterUtility::getIdAsInt(item);
    bool isSet = false;
    if (item->checkState() == Qt::Checked) isSet = true;

    model->setFilterState(ID, isSet);

    manageRowExpansion();
}

void MainWindow::busFilterListItemChanged(QListWidgetItem *item)
{
    if (inhibitFilterUpdate) return;
    //qDebug() << item->text();

    // strip away possible filter label
    int ID = FilterUtility::getIdAsInt(item);
    bool isSet = false;
    if (item->checkState() == Qt::Checked) isSet = true;

    model->setBusFilterState(ID, isSet);

    manageRowExpansion();
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

    manageRowExpansion();
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

void MainWindow::logReceivedFrame(CANConnection* conn, QVector<CANFrame> frames)
{
    Q_UNUSED(conn);
    if (continuousLogging)
    {
        FrameFileIO::writeContinuousNative(&frames, 0);
    }
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
        if (rxFrames > 0 && /*allowCapture && */ ui->cbAutoScroll->isChecked())
                ui->canFramesView->scrollToBottom();
        ui->lbFPS->setText(QString::number(framesPerSec));
        if (rxFrames > 0)
        {
            bDirty = true;
            emit framesUpdated(rxFrames); //anyone care that frames were updated?
            manageRowExpansion();
        }

        if (model->needsFilterRefresh()) updateFilterList();

        if (continuousLogging)
        {
//            const QVector<CANFrame> *modelFrames = model->getListReference();
//            FrameFileIO::writeContinuousNative(modelFrames, modelFrames->count() - rxFrames);

            continuousLogFlushCounter++;
            if ((continuousLogFlushCounter % 3) == 0)
            {
                if (ui->lblContMsg->text().length() > 2)
                {
                    ui->lblContMsg->setText("");
                }
                else
                {
                    ui->lblContMsg->setText("LOGGING");
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
void MainWindow::gotCenterTimeID(uint32_t ID, double timestamp)
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

    QMessageBox::StandardButton confirmDialog;

    bool loadResult = FrameFileIO::loadFrameFile(filename, &tempFrames);

    if (!loadResult)
    {
        if (tempFrames.count() > 0) //only ask if at least one frame was decoded.
        {
            confirmDialog = QMessageBox::question(this, "Error Loading", "Do you want to salvage what could be loaded?",
                                      QMessageBox::Yes|QMessageBox::No);
            if (confirmDialog == QMessageBox::Yes) {
                loadResult = true;
            }
        }
    }

    if (loadResult)
    {
        disableAutoRowExpansion();
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

void MainWindow::handleDroppedFile(const QString &filename)
{
    QProgressDialog progress(qApp->activeWindow());
    progress.setWindowModality(Qt::WindowModal);
    progress.setLabelText("Loading file...");
    progress.setCancelButton(nullptr);
    progress.setRange(0,0);
    progress.setMinimumDuration(0);
    progress.show();
    
    QVector<CANFrame> loadedFrames;
    bool loadResult = FrameFileIO::autoDetectLoadFile(filename, &loadedFrames);
    
    progress.cancel();
    
    if (!loadResult)
    {
        if (loadedFrames.count() > 0) //only ask if at least one frame was decoded.
        {
            QMessageBox::StandardButton confirmDialog = QMessageBox::question(this, "Error Loading", "Do you want to salvage what could be loaded?",
                                      QMessageBox::Yes|QMessageBox::No);
            if (confirmDialog == QMessageBox::Yes)
            {
                loadResult = true;
            }
        }
    }

    if (loadResult)
    {
        disableAutoRowExpansion();
        ui->canFramesView->scrollToTop();
        model->clearFrames();
        model->insertFrames(loadedFrames);
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
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("Filter list (*.ftl)")));

    dialog.setDirectory(settings.value("Filters/LoadSaveDirectory", dialog.directory().path()).toString());
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        if (!filename.contains('.')) filename += ".ftl";
        if (dialog.selectedNameFilter() == filters[0]) model->saveFilterFile(filename);
        settings.setValue("Filters/LoadSaveDirectory", dialog.directory().path());
    }
}

void MainWindow::handleLoadFilters()
{
    QString filename;
    QFileDialog dialog(this);
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("Filter List (*.ftl)")));

    dialog.setDirectory(settings.value("Filters/LoadSaveDirectory", dialog.directory().path()).toString());
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        //right now there is only one file type that can be loaded here so just do it.
        model->loadFilterFile(filename);
        settings.setValue("Filters/LoadSaveDirectory", dialog.directory().path());
    }
}

void MainWindow::handleSaveDecoded()
{
    handleSaveDecodedMethod(false);
}

void MainWindow::handleSaveDecodedCsv()
{
    handleSaveDecodedMethod(true);
}

void MainWindow::handleSaveDecodedMethod(bool csv)
{
    QString filename;
    QFileDialog dialog(this);
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("Text File (*.txt)")));

    dialog.setDirectory(settings.value("FileIO/LoadSaveDirectory", dialog.directory().path()).toString());
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        if (!filename.contains('.')) filename += ".txt";

        if(csv)
            saveDecodedTextFileAsColumns(filename);
        else
            saveDecodedTextFile(filename);

        settings.setValue("FileIO/LoadSaveDirectory", dialog.directory().path());
    }
}

void MainWindow::saveDecodedTextFileAsColumns(QString filename)
{
    QFile *outFile = new QFile(filename);
    const QVector<CANFrame> *frames = model->getFilteredListReference();

    //const unsigned char *data;
    int dataLen;
    const CANFrame *frame;

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
        return;
/*
Time: 205.173000   ID: 0x20E Std Bus: 0 Len: 8
Data Bytes: 88 10 00 13 BB 00 06 00
    SignalName	Value
*/
    QList<QPair<uint32_t, int>> msgsAndColumns;
    int columnsAdded = 0;
    int dataStartCol = 0;

    QString builderString;
    //time
    builderString += tr("Time") + ",";
    dataStartCol++;
    //id
    builderString += tr("ID") + ",";
    dataStartCol++;
    //if (frame->hasExtendedFrameFormat()) builderString += tr(" Ext ");
    //else builderString += tr(" Std ");
    //bus
    builderString += tr("Bus") + ",";
    dataStartCol++;
    //len
    builderString += tr("DataLen") + ",";
    dataStartCol++;

    columnsAdded = dataStartCol;

    //loop through all the frames and the message data therein
    for (int c = 0; c < frames->count(); c++)
    {
        frame = &frames->at(c);
        //data = reinterpret_cast<const unsigned char *>(frame->payload().constData());
        dataLen = frame->payload().count();

        //add all column names
        if (dbcHandler != nullptr)
        {
            DBC_MESSAGE *msg = dbcHandler->findMessage(*frame);
            if (msg != nullptr)
            {
                bool found = false;
                for (int j = 0; j < msg->sigHandler->getCount(); j++)
                {
                    if(j==0)
                    {
                        for(int m=0; m<msgsAndColumns.count(); m++)
                        {
                            if(msgsAndColumns[m].first == msg->ID)
                                found = true;
                        }
                        if(found == false)
                            msgsAndColumns.append(QPair<uint32_t,int>(msg->ID, columnsAdded));
                    }

                    if(found == false)
                    {
                        QString temp;
                        if (msg->sigHandler->findSignalByIdx(j)->processAsText(*frame, temp))
                        {
                            builderString.append(msg->sigHandler->findSignalByIdx(j)->name);
                            builderString.append(",");
                            columnsAdded++;
                        }
                    }
                }
            }
        }
    }

    //add EOL
    builderString += "\n";
    //write out the header row
    outFile->write(builderString.toUtf8());

        //builderString = tr("Data Bytes: ");
        //for (int temp = 0; temp < dataLen; temp++)
        //{
        //    builderString += Utility::formatNumber(data[temp]) + " ";
        //}
        //builderString += "\n";
        //outFile->write(builderString.toUtf8());

    int dataColumnsAdded = 0;
    builderString = "";
    for (int c = 0; c < frames->count(); c++)
    {
        dataColumnsAdded = 0;
        frame = &frames->at(c);
        //data = reinterpret_cast<const unsigned char *>(frame->payload().constData());
        dataLen = frame->payload().count();

        QString builderString;
        builderString += QString::number((frame->timeStamp().microSeconds() / 1000000.0), 'f', 6) + ",";
        dataColumnsAdded++;
        //id
        builderString += Utility::formatCANID(frame->frameId(), frame->hasExtendedFrameFormat()) + ",";
        dataColumnsAdded++;
        //if (frame->hasExtendedFrameFormat()) builderString += tr(" Ext ");
        //else builderString += tr(" Std ");
        //bus
        builderString += QString::number(frame->bus) + ",";
        dataColumnsAdded++;
        //len
        builderString += QString::number(dataLen) + ",";
        dataColumnsAdded++;

        if (dbcHandler != nullptr)
        {
            DBC_MESSAGE *msg = dbcHandler->findMessage(*frame);
            if (msg != nullptr)
            {
                for (int j = 0; j < msg->sigHandler->getCount(); j++)
                {
                    if(j==0)
                    {
                        for(int i = 0; i<msgsAndColumns.count(); i++)
                        {
                            if(msgsAndColumns[i].first == msg->ID)
                            {
                                int startCol = msgsAndColumns[i].second;
                                while(dataColumnsAdded < startCol)
                                {
                                    builderString += ",";
                                    dataColumnsAdded++;
                                }
                            }
                        }
                    }

                    QString temp;
                    if (msg->sigHandler->findSignalByIdx(j)->processAsText(*frame, temp, false))
                    {
                        builderString.append(temp);
                        builderString.append(",");
                        dataColumnsAdded++;
                    }
                }
            }
            builderString.append("\n");
            outFile->write(builderString.toUtf8());
        }
    }
    outFile->close();
}

void MainWindow::saveDecodedTextFile(QString filename)
{
    QFile *outFile = new QFile(filename);
    const QVector<CANFrame> *frames = model->getFilteredListReference();

    const unsigned char *data;
    int dataLen;
    const CANFrame *frame;

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
        return;
/*
Time: 205.173000   ID: 0x20E Std Bus: 0 Len: 8
Data Bytes: 88 10 00 13 BB 00 06 00
    SignalName	Value
*/
    for (int c = 0; c < frames->count(); c++)
    {
        frame = &frames->at(c);
        data = reinterpret_cast<const unsigned char *>(frame->payload().constData());
        dataLen = frame->payload().count();

        QString builderString;
        builderString += tr("Time: ") + QString::number((frame->timeStamp().microSeconds() / 1000000.0), 'f', 6);
        builderString += tr("    ID: ") + Utility::formatCANID(frame->frameId(), frame->hasExtendedFrameFormat());
        if (frame->hasExtendedFrameFormat()) builderString += tr(" Ext ");
        else builderString += tr(" Std ");
        builderString += tr("Bus: ") + QString::number(frame->bus);
        builderString += " Len: " + QString::number(dataLen) + "\n";
        outFile->write(builderString.toUtf8());

        builderString = tr("Data Bytes: ");
        for (int temp = 0; temp < dataLen; temp++)
        {
            builderString += Utility::formatNumber(data[temp]) + " ";
        }
        builderString += "\n";
        outFile->write(builderString.toUtf8());

        builderString = "";
        if (dbcHandler != nullptr)
        {
            DBC_MESSAGE *msg = dbcHandler->findMessage(*frame);
            if (msg != nullptr)
            {
                for (int j = 0; j < msg->sigHandler->getCount(); j++)
                {

                    QString temp;
                    if (msg->sigHandler->findSignalByIdx(j)->processAsText(*frame, temp))
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
        output = tr("No packets loaded");
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
//now always creates a new window. This allows for multiple independent graphing windows
void MainWindow::showGraphingWindow()
{
/* could only allow the latest window to have these centering signals.
   if (lastGraphingWindow)
    {
        disconnect(lastGraphingWindow, SIGNAL(sendCenterTimeID(uint32_t,double)), this, SLOT(gotCenterTimeID(int32_t,double)));
        disconnect(this, SIGNAL(sendCenterTimeID(uint32_t,double)), lastGraphingWindow, SLOT(gotCenterTimeID(int32_t,double)));
        if (flowViewWindow)
        {
            disconnect(lastGraphingWindow, SIGNAL(sendCenterTimeID(uint32_t,double)), flowViewWindow, SLOT(gotCenterTimeID(int32_t,double)));
            disconnect(flowViewWindow, SIGNAL(sendCenterTimeID(uint32_t,double)), lastGraphingWindow, SLOT(gotCenterTimeID(int32_t,double)));
        }
    }
*/
    lastGraphingWindow = new GraphingWindow(model->getListReference());
    graphWindows.append(lastGraphingWindow);

    connect(lastGraphingWindow, SIGNAL(sendCenterTimeID(uint32_t,double)), this, SLOT(gotCenterTimeID(uint32_t,double)));
    connect(this, SIGNAL(sendCenterTimeID(uint32_t,double)), lastGraphingWindow, SLOT(gotCenterTimeID(uint32_t,double)));

    if (flowViewWindow) //connect the two external windows together
    {
        connect(lastGraphingWindow, SIGNAL(sendCenterTimeID(uint32_t,double)), flowViewWindow, SLOT(gotCenterTimeID(uint32_t,double)));
        connect(flowViewWindow, SIGNAL(sendCenterTimeID(uint32_t,double)), lastGraphingWindow, SLOT(gotCenterTimeID(uint32_t,double)));
    }

    lastGraphingWindow->show();
}

void MainWindow::showTemporalGraphWindow()
{
    //only create an instance of the object if we dont have one. Otherwise just display the existing one.
    if (!temporalGraphWindow)
    {
        const QVector<CANFrame> *frames;
        if (!useFiltered)
            frames = model->getListReference();
        else
            frames = model->getFilteredListReference();

        if(frames->count() > 2000)
        {
            QMessageBox::StandardButton confirmDialog;
            confirmDialog = QMessageBox::question(this, "Danger Will Robinson", "There are a lot of frames (>2000) to plot, this may take a while or crash the app. Crash likely with more than 10k frames. Continue?",
                                          QMessageBox::Yes|QMessageBox::No);
            if (confirmDialog == QMessageBox::No)
            {
                return;
            }
        }

        temporalGraphWindow = new TemporalGraphWindow(frames);
    }

    temporalGraphWindow->show();
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

void MainWindow::showCANBridgeWindow()
{
    if (!canBridgeWindow)
    {
        canBridgeWindow = new CANBridgeWindow(model->getListReference());
    }
    canBridgeWindow->show();
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

void MainWindow::showDBCComparisonWindow()
{
    if (!dbcComparatorWindow)
    {
        dbcComparatorWindow = new DBCComparatorWindow();
    }
    dbcComparatorWindow->show();
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
        connect(flowViewWindow, SIGNAL(sendCenterTimeID(uint32_t,double)), this, SLOT(gotCenterTimeID(int32_t,double)));
        connect(this, SIGNAL(sendCenterTimeID(uint32_t,double)), flowViewWindow, SLOT(gotCenterTimeID(int32_t,double)));
    }

    if (lastGraphingWindow)
    {
        connect(lastGraphingWindow, SIGNAL(sendCenterTimeID(uint32_t,double)), flowViewWindow, SLOT(gotCenterTimeID(int32_t,double)));
        connect(flowViewWindow, SIGNAL(sendCenterTimeID(uint32_t,double)), lastGraphingWindow, SLOT(gotCenterTimeID(int32_t,double)));
    }

    flowViewWindow->show();
}


void MainWindow::DBCSettingsUpdated()
    {
    updateFilterList();
    model->sendRefresh();
    }

void MainWindow::showDBCFileWindow()
{
    if (!dbcFileWindow)
    {
        dbcFileWindow = new DBCLoadSaveWindow(model->getListReference());
        connect(dbcFileWindow, &DBCLoadSaveWindow::updatedDBCSettings, this, &MainWindow::DBCSettingsUpdated);
    }
    dbcFileWindow->show();
}

void MainWindow::showSignalViewer()
{
    if (!signalViewerWindow)
    {
        if (!useFiltered)
            signalViewerWindow = new SignalViewerWindow(model->getListReference());
        else
            signalViewerWindow = new SignalViewerWindow(model->getFilteredListReference());
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
