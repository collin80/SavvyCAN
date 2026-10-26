#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "can_structs.h"
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QtSerialPort/QSerialPortInfo>
#include "connections/canconmanager.h"
#include "bus_protocols/handler_factory.h"
#include "connections/connectionwindow.h"
#include "helpwindow.h"
#include "utility.h"
#include "filterutility.h"
#include "github/githubaccountdialog.h"
#include "github/githubbrowserdialog.h"
#include "github/githubhelp.h"
#include "lognotesdialog.h"
#include "lognotespanel.h"

#include <QClipboard>
#include <QInputDialog>
#include <QTimer>
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
    useColorsByCanId = false;
    selfRef = this;

    this->setWindowTitle("Savvy CAN V" + QString::number(VERSION) + " [Built " + QString(__DATE__) +"]");

    model = new CommFrameModel(this); // set parent to mainwindow to prevent canframemodel to change thread (might be done by setModel but just in case)

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
    udsFirmwareUploaderWindow = nullptr;
    discreteStateWindow = nullptr;
    connectionWindow = nullptr;
    scriptingWindow = nullptr;
    rangeWindow = nullptr;
    dbcFileWindow = nullptr;
    fuzzingWindow = nullptr;
    udsScanWindow = nullptr;
    obd2Window = nullptr;
    motorctrlConfigWindow = nullptr;
    isoWindow = nullptr;
    snifferWindow = nullptr;
    bisectWindow = nullptr;
    signalViewerWindow = nullptr;
    temporalGraphWindow = nullptr;
    dbcComparatorWindow = nullptr;
    canBridgeWindow = nullptr;
    searchWindow = nullptr;
    dbcHandler = DBCHandler::getReference();
    bDirty = false;
    inhibitFilterUpdate = false;
    rxFrames = 0;
    framesPerSec = 0;
    continuousLogging = false;
    continuousLogFlushCounter = 0;
    continuousFrameCount = 0;
    continuousFirstUs = continuousLastUs = 0;
    connectedBusCount = 0;

    //handlers for all menu entries
    connect(ui->actionSetup, SIGNAL(triggered(bool)), SLOT(showConnectionSettingsWindow()));
    connect(ui->actionOpen_Log_File, &QAction::triggered, this, &MainWindow::handleLoadFile);
    connect(ui->actionLoad_Log_File_from_GitHub, &QAction::triggered, this, &MainWindow::handleLoadFromGitHub);
    connect(ui->actionGitHub_Account, &QAction::triggered, this, &MainWindow::showGitHubAccount);
    connect(ui->actionSave_Log_File_to_GitHub, &QAction::triggered, this, &MainWindow::handleSaveToGitHub);
    gitHubHelp() = [](const QString &page) { HelpWindow::getRef()->showHelp(page); };
    connect(ui->actionGraph_Dta, &QAction::triggered, this, &MainWindow::showGraphingWindow);
    connect(ui->actionFrame_Data_Analysis, &QAction::triggered, this, &MainWindow::showFrameDataAnalysis);
    connect(ui->actionSave_Log_File, &QAction::triggered, this, &MainWindow::handleSaveFile);
    connect(ui->actionSave_Filtered_Log_File, &QAction::triggered, this, &MainWindow::handleSaveFilteredFile);
    connect(ui->actionLog_Notes, &QAction::triggered, this, &MainWindow::editLogNotes);
    connect(ui->actionAdd_Marker, &QAction::triggered, this, &MainWindow::addLogMarker);
    connect(ui->btnAddMarker, &QAbstractButton::clicked, this, &MainWindow::addLogMarker);
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
    connect(ui->actionCrop_Log, &QAction::triggered, this, &MainWindow::handleCropLog);
    connect(ui->actionSingle_Multi_State_2, &QAction::triggered, this, &MainWindow::showSingleMultiWindow);
    connect(ui->actionFile_Comparison, &QAction::triggered, this, &MainWindow::showComparisonWindow);
    connect(ui->actionDBC_Comparison, &QAction::triggered, this, &MainWindow::showDBCComparisonWindow);
    connect(ui->actionScripting_INterface, &QAction::triggered, this, &MainWindow::showScriptingWindow);
    connect(ui->actionPreferences, &QAction::triggered, this, &MainWindow::showSettingsDialog);
    connect(ui->actionUDS_Firmware_Update, &QAction::triggered, this, &MainWindow::showUDSFirmwareUploaderWindow);
    connect(ui->actionDBC_File_Manager, &QAction::triggered, this, &MainWindow::showDBCFileWindow);
    connect(ui->actionFuzzing, &QAction::triggered, this, &MainWindow::showFuzzingWindow);
    connect(ui->actionUDS_Scanner, &QAction::triggered, this, &MainWindow::showUDSScanWindow);
    connect(ui->actionOBD2_Poller, &QAction::triggered, this, &MainWindow::showObd2Window);
    connect(ui->actionISO_TP_Decoder, &QAction::triggered, this, &MainWindow::showISOInterpreterWindow);
    connect(ui->actionSniffer, &QAction::triggered, this, &MainWindow::showSnifferWindow);
    connect(ui->actionMotorControlConfig, &QAction::triggered, this, &MainWindow::showMCConfigWindow);
    connect(ui->actionCapture_Bisector, &QAction::triggered, this, &MainWindow::showBisectWindow);
    connect(ui->actionSignal_Viewer, &QAction::triggered, this, &MainWindow::showSignalViewer);
    connect(ui->actionSave_Continuous_Logfile, &QAction::triggered, this, &MainWindow::handleContinousLogging);
    connect(ui->actionTemporal_Graph, &QAction::triggered, this, &MainWindow::showTemporalGraphWindow);
    connect(ui->actionCAN_Bridge, &QAction::triggered, this, &MainWindow::showCANBridgeWindow);
    connect(ui->actionFrame_Search, &QAction::triggered, this, &MainWindow::showSearchWindow);

    //handlers fror interactions with the main can frame view table
    connect(ui->canFramesView, &QAbstractItemView::clicked, this, &MainWindow::gridClicked);
    connect(ui->canFramesView, &QAbstractItemView::doubleClicked, this, &MainWindow::gridDoubleClicked);
    ui->canFramesView->setContextMenuPolicy(Qt::CustomContextMenu);

    copyAct = new QAction(tr("Copy"), this);
    copyAct->setShortcut(QKeySequence::Copy);
    connect(copyAct, &QAction::triggered, this, &MainWindow::copyFromTable);
    ui->canFramesView->addAction(copyAct);
    connect(ui->canFramesView, &QAbstractItemView::customContextMenuRequested, this, &MainWindow::gridContextMenuRequest);

    connect(model, &CommFrameModel::updatedFiltersList, this, &MainWindow::updateFilterList);
    connect(CANConManager::getInstance(), &CANConManager::framesReceived, model, &CommFrameModel::addFrames);
    //new implementation for continuous logging
    connect(CANConManager::getInstance(), &CANConManager::framesReceived, this, &MainWindow::logReceivedFrame);

    connect(ui->cbAutoScroll, &QAbstractButton::toggled, this, &MainWindow::interpretAutoScroll);
    connect(ui->cbInterpret, &QAbstractButton::toggled, this, &MainWindow::interpretToggled);
    connect(ui->cbOverwrite, &QAbstractButton::toggled, this, &MainWindow::overwriteToggled);
    connect(ui->cbChangingOnly, &QAbstractButton::toggled, this, &MainWindow::changingOnlyToggled);
    connect(ui->cbPersistentFilters, &QAbstractButton::toggled, this, &MainWindow::presistentFiltersToggled);
    connect(ui->listFilters, &QListWidget::itemChanged, this, &MainWindow::filterListItemChanged);
    connect(ui->listBusFilters, &QListWidget::itemChanged, this, &MainWindow::busFilterListItemChanged);
    connect(ui->listDirFilters, &QListWidget::itemChanged, this, &MainWindow::dirFilterListItemChanged);
    connect(ui->listLengthFilters, &QListWidget::itemChanged, this, &MainWindow::lengthFilterListItemChanged);

    connect(ui->btnCaptureToggle, &QAbstractButton::clicked, this, &MainWindow::toggleCapture);
    connect(ui->btnClearFrames, &QAbstractButton::clicked, this, &MainWindow::clearFrames);
    connect(ui->btnNormalize, &QAbstractButton::clicked, this, &MainWindow::normalizeTiming);
    connect(ui->btnFilterAll, &QAbstractButton::clicked, this, &MainWindow::filterSetAll);
    connect(ui->btnFilterNone, &QAbstractButton::clicked, this, &MainWindow::filterClearAll);
    connect(ui->btnExpandAll, &QAbstractButton::clicked, this, &MainWindow::expandAllRows);
    connect(ui->btnCollapseAll, &QAbstractButton::clicked, this, &MainWindow::collapseAllRows);

    connect(ui->tableSimpleSender, SIGNAL(cellChanged(int,int)), this, SLOT(onSenderCellChanged(int,int)));

    //Frame count column should be hidden by default. Only used in overwrite mode
    ui->canFramesView->hideColumn((int)Column::FrameCount);
    //If overwrite mode was restored from settings, show the column again
    if (ui->cbOverwrite->isChecked()) {
        ui->canFramesView->showColumn((int)Column::FrameCount);
    }

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
    // Allow drag-select and Shift/Ctrl+click multi-selection in the ID filter list
    ui->listFilters->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->listFilters->installEventFilter(this);

    // Wrapping filter lists: items reflow on resize and height is recalculated via event filter
    ui->listBusFilters->setWrapping(true);
    ui->listBusFilters->setResizeMode(QListView::Adjust);
    ui->listDirFilters->setWrapping(true);
    ui->listDirFilters->setResizeMode(QListView::Adjust);
    ui->listLengthFilters->setWrapping(true);
    ui->listLengthFilters->setResizeMode(QListView::Adjust);
    ui->listBusFilters->installEventFilter(this);
    ui->listDirFilters->installEventFilter(this);
    ui->listLengthFilters->installEventFilter(this);

    // Restore or default splitterLeft after the window has real geometry.
    // QTimer::singleShot(0) defers to first event loop tick.
    QTimer::singleShot(0, this, [this]() {
        QSettings s;
        QByteArray state = s.value("Main/SplitterLeft").toByteArray();
        if (!state.isEmpty()) {
            ui->splitterLeft->restoreState(state);
        } else {
            int h = ui->splitterLeft->height();
            if (h > 0)
                ui->splitterLeft->setSizes({h * 3 / 4, h / 4}); // 75% frame table, 25% send panel
        }
    });

    ui->leSearchFilter->setClearButtonEnabled(true);
    connect(ui->leSearchFilter, &QLineEdit::textChanged, this, &MainWindow::onSearchFilterChanged);

    //Log Notes panel, hidden until a log with notes is loaded or it's turned on in the File menu
    logNotesPanel = new LogNotesPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, logNotesPanel);
    logNotesPanel->setVisible(QSettings().value("LogNotes/PanelVisible", false).toBool());
    connect(logNotesPanel, &LogNotesPanel::editRequested, this, &MainWindow::editLogNotes);
    connect(logNotesPanel, &LogNotesPanel::addMarkerRequested, this, &MainWindow::addLogMarker);
    connect(logNotesPanel, &LogNotesPanel::markerActivated, this, &MainWindow::goToLogTime);
    QAction *showNotesAction = logNotesPanel->toggleViewAction();
    showNotesAction->setText(tr("Show Log Notes Panel"));
    const QList<QAction *> fileActions = ui->menuFile->actions();
    ui->menuFile->insertAction(fileActions.value(int(fileActions.indexOf(ui->actionLog_Notes)) + 1), showNotesAction);

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
    CommFrame temp;
    temp.setBus(0);
    temp.setFrameId(0x100);
    temp.setReceived(true);
    temp.setTimeStamp(CommFrame::TimeStamp(0, 100000000));
    model->addFrame(temp, true);
    ui->canFramesView->resizeRowToContents(0);      // Resize the row to fit the contents so we get a proper height value
    qApp->processEvents();
    tickGUIUpdate(); //force a GUI refresh so that the row exists to measure
    normalRowHeight = ui->canFramesView->rowHeight(0);
    if (normalRowHeight == 0) normalRowHeight = 30; //should not be necessary but provides a sane number if something stupid happened.
    qDebug() << "normal row height = " << normalRowHeight;
    model->clearFrames();

    ui->canFramesView->verticalHeader()->setDefaultSectionSize(normalRowHeight);    // Set the default height for all rows to the height that was calculated

    //connect(CANConManager::getInstance(), CANConManager::connectionStatusUpdated, this, MainWindow::connectionStatusUpdated);
    connect(CANConManager::getInstance(), SIGNAL(connectionStatusUpdated(int)), this, SLOT(connectionStatusUpdated(int)));

    //Automatically create the connection window so it can be updated even if we never opened it.
    connectionWindow = new ConnectionWindow();
    connect(this, SIGNAL(suspendCapturing(bool)), connectionWindow, SLOT(setSuspendAll(bool)));

    //these either are unfinished/not working or are not for general use. But,they exist
    //so if you want to enable them and play with them then go for it.
    //ui->actionFirmware_Update->setVisible(false);
    ui->actionMotorControlConfig->setVisible(false);
    ui->actionSingle_Multi_State_2->setVisible(false);

    QStringList headers;
    headers << "En" << "Bus" << "ID" << "Ext" << "Rem" << "Data"
            << "Interval" << "Count";
    ui->tableSimpleSender->setColumnCount(8);
    ui->tableSimpleSender->setColumnWidth(SIMP_COL::SC_COL_EN, 70);
    ui->tableSimpleSender->setColumnWidth(SIMP_COL::SC_COL_BUS, 70);
    ui->tableSimpleSender->setColumnWidth(SIMP_COL::SC_COL_ID, 70);
    ui->tableSimpleSender->setColumnWidth(SIMP_COL::SC_COL_EXT, 70);
    ui->tableSimpleSender->setColumnWidth(SIMP_COL::SC_COL_REM, 70);
    ui->tableSimpleSender->setColumnWidth(SIMP_COL::SC_COL_DATA, 300);
    ui->tableSimpleSender->setColumnWidth(SIMP_COL::SC_COL_INTERVAL, 100);
    ui->tableSimpleSender->setColumnWidth(SIMP_COL::SC_COL_COUNT, 100);
    ui->tableSimpleSender->setHorizontalHeaderLabels(headers);

    createSenderRow();

    frameSender = new FrameSenderObject(model->getListReference());

    frameSender->initialize(); //creates the thread and sets things up
    frameSender->startSending(); //start the timer in the object so enabled things can send

    installEventFilter(this);
}

MainWindow::~MainWindow()
{
    updateTimer.stop();
    frameSender->stopSending();
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
    killWindow(obd2Window);
    killWindow(isoWindow);
    killWindow(snifferWindow);
    killWindow(bisectWindow);
    killWindow(udsFirmwareUploaderWindow);
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
    // Space key on the ID filter list: toggle checkbox state for all selected items
    if (obj == ui->listFilters && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Space) {
            QList<QListWidgetItem*> selected = ui->listFilters->selectedItems();
            if (!selected.isEmpty()) {
                Qt::CheckState newState = (selected.first()->checkState() == Qt::Checked)
                                          ? Qt::Unchecked : Qt::Checked;
                inhibitFilterUpdate = true;
                for (QListWidgetItem *item : selected)
                    item->setCheckState(newState);
                inhibitFilterUpdate = false;
                QList<QPair<int,bool>> updates;
                updates.reserve(selected.size());
                for (QListWidgetItem *item : selected)
                    updates.append({FilterUtility::getIdAsInt(item), newState == Qt::Checked});
                model->setFilterStatesBatch(updates);
                manageRowExpansion();
                return true;
            }
        }
    }
    // Recalculate height of wrapping filter lists when they are resized (panel drag etc.)
    if ((obj == ui->listBusFilters || obj == ui->listDirFilters || obj == ui->listLengthFilters)
        && event->type() == QEvent::Resize) {
        QListWidget *list = qobject_cast<QListWidget*>(obj);
        QTimer::singleShot(0, this, [this, list]() {
            if (list->count() > 0) {
                QRect r = list->visualItemRect(list->item(list->count() - 1));
                int h = r.bottom() + 1 + 2 * list->frameWidth();
                if (h > 0 && h != list->height()) list->setFixedHeight(h);
            }
        });
        return false;
    }
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

        QByteArray splitterMainState = settings.value("Main/SplitterMain").toByteArray();
        if (!splitterMainState.isEmpty())
            ui->splitterMain->restoreState(splitterMainState);
        else
            ui->splitterMain->setSizes({750, 250});

        // splitterLeft is restored/defaulted via deferred QTimer in constructor
    }
    else
    {
        ui->splitterMain->setSizes({750, 250});
        // splitterLeft is defaulted via deferred QTimer in constructor
    }

    if (settings.value("Main/AutoScroll", false).toBool())
    {
        ui->cbAutoScroll->setChecked(Qt::Checked);
    }

    if (settings.value("Main/Overwrite", false).toBool())
    {
        ui->cbOverwrite->setChecked(Qt::Checked);
        model->setOverwriteMode(true);
    }
    else
    {
        model->setOverwriteMode(false);
        rowExpansionActive = false;
    }

    if (settings.value("Main/PersistentFilters", false).toBool())
    {
        ui->cbPersistentFilters->setChecked(Qt::Checked);
        model->setClearMode(true);
    }

    if (settings.value("Main/Overwrite", false).toBool())
    {
        ui->cbOverwrite->setChecked(Qt::Checked);
        model->setOverwriteMode(true);
    }
    else
    {
        model->setOverwriteMode(false);
        rowExpansionActive = false;
    }

    if (settings.value("Main/ChangingOnly", false).toBool())
    {
        ui->cbChangingOnly->setChecked(Qt::Checked);
        model->setChangingOnly(true);
    }
    else
        model->setChangingOnly(false);

    if (settings.value("Main/PersistentFilters", false).toBool())
    {
        ui->cbPersistentFilters->setChecked(Qt::Checked);
        model->setClearMode(true);
    }
    else
        model->setClearMode(false);

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

    useColorsByCanId = settings.value("Main/ColorsByCanId", false).toBool();
    model->setUseColorsByCanId(useColorsByCanId);

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

    CSVAbsTime = settings.value("Main/CSVAbsTime", false).toBool();

    if (settings.value("Main/FilterLabeling", false).toBool())
        ui->listFilters->setMaximumWidth(250);
    else
        ui->listFilters->setMaximumWidth(175);

    bool showSendPanel = settings.value("Main/ShowSendPanel", true).toBool();
    ui->tableSimpleSender->setVisible(showSendPanel);

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
        settings.setValue("Main/SplitterMain", ui->splitterMain->saveState());
    }
    // Always save splitterLeft so send-panel height is restored on next launch
    settings.setValue("Main/SplitterLeft", ui->splitterLeft->saveState());
    settings.setValue("LogNotes/PanelVisible", logNotesPanel->isVisible());
}

void MainWindow::onSenderCellChanged(int row, int col)
{
    if (inhibitSenderChanged) return;
    qDebug() << "onCellChanged";
    if (row == ui->tableSimpleSender->rowCount() - 1)
    {
        createSenderRow();
    }

    processSenderCellChange(row, col);
}

void MainWindow::onSearchFilterChanged(const QString &text)
{
    // Apply search text to the model (filters main table rows)
    model->setSearchFilter(text);

    // Show/hide items in the filter list to match the search text
    QString stripped = text;
    if (stripped.startsWith("0x", Qt::CaseInsensitive))
        stripped = stripped.mid(2);

    for (int i = 0; i < ui->listFilters->count(); ++i) {
        QListWidgetItem *item = ui->listFilters->item(i);
        if (stripped.isEmpty())
            item->setHidden(false);
        else {
            QString idStr = FilterUtility::getId(item);
            // strip leading "0x" from the displayed id string too
            if (idStr.startsWith("0x", Qt::CaseInsensitive))
                idStr = idStr.mid(2);
            item->setHidden(!idStr.contains(stripped, Qt::CaseInsensitive));
        }
    }
}

void MainWindow::processSenderCellChange(int line, int col)
{
    qDebug() << "processSenderCellChange";
    FrameSendData *tempData;
    QStringList tokens;
    int tempVal;

    int numBuses = CANConManager::getInstance()->getNumBuses();
    QByteArray arr;

    tempData = frameSender->getSendRecordRef(line);

    if (!tempData)
    {
        qDebug() << "Need to set up a new entry in senders";
        FrameSendData dat;
        dat.enabled = false;
        dat.count = 0;
        dat.setFrameCount(0);
        dat.setBus(0);
        frameSender->addSendRecord(dat);
        tempData = frameSender->getSendRecordRef(line);
    }

    if (!tempData)
    {
        qDebug() << "No data to modify in processSenderCellChange. This is a bug!";
        return;
    }

    switch (col)
    {
    case SIMP_COL::SC_COL_EN: //Enable check box
        if (ui->tableSimpleSender->item(line, 0)->checkState() == Qt::Checked)
        {
            tempData->enabled = true;
        }
        else tempData->enabled = false;
        qDebug() << "Setting enabled to " << tempData->enabled;
        break;
    case SIMP_COL::SC_COL_BUS: //Bus designation
        tempVal = Utility::ParseStringToNum(ui->tableSimpleSender->item(line, SIMP_COL::SC_COL_BUS)->text());
        if (tempVal < -1) tempVal = -1;
        if (tempVal >= numBuses) tempVal = numBuses - 1;
        tempData->setBus(tempVal);
        qDebug() << "Setting bus to " << tempVal;
        break;
    case SIMP_COL::SC_COL_ID: //ID field
        tempVal = Utility::ParseStringToNum(ui->tableSimpleSender->item(line, SIMP_COL::SC_COL_ID)->text());
        if (tempVal < 0) tempVal = 0;
        if (tempVal > 0x7FFFFFFF) tempVal = 0x7FFFFFFF;
        tempData->setFrameId(tempVal);
        if (tempData->frameId() > 0x7FF) {
            tempData->setExtendedFrameFormat(true);
            ui->tableSimpleSender->blockSignals(true);
            ui->tableSimpleSender->item(line, ST_COLS::SENDTAB_COL_EXT)->setCheckState(Qt::Checked);
            ui->tableSimpleSender->blockSignals(false);
        }
        qDebug() << "setting ID to " << tempVal;
        break;
    case SIMP_COL::SC_COL_EXT:
        if (ui->tableSimpleSender->item(line, SIMP_COL::SC_COL_EXT)->checkState() == Qt::Checked) {
            tempData->setExtendedFrameFormat(true);
        } else {
            tempData->setExtendedFrameFormat(false);
        }
        break;
    case SIMP_COL::SC_COL_REM:
        if (ui->tableSimpleSender->item(line, SIMP_COL::SC_COL_REM)->checkState() == Qt::Checked) {
            tempData->setFrameType(CommFrame::RemoteRequestFrame);
        } else {
            tempData->setFrameType(CommFrame::CANDataFrame);
        }
        break;
    case SIMP_COL::SC_COL_DATA: //Data bytes
        //for (int i = 0; i < 8; i++) tempData->payload().data()[i] = 0;

#if QT_VERSION >= QT_VERSION_CHECK( 5, 14, 0 )
        tokens = ui->tableSimpleSender->item(line, SIMP_COL::SC_COL_DATA)->text().split(" ", Qt::SkipEmptyParts);
#else
        tokens = ui->tableSimpleSender->item(line, SIMP_COL::SC_COL_DATA)->text().split(" ", QString::SkipEmptyParts);
#endif
        arr.clear();
        arr.reserve(tokens.count());
        for (int j = 0; j < tokens.count(); j++)
        {
            arr.append((uint8_t)Utility::ParseStringToNum(tokens[j]));
        }
        tempData->setPayload(arr);
        break;
    case SIMP_COL::SC_COL_INTERVAL: //interval in ms

        QString trigger = ui->tableSimpleSender->item(line, SIMP_COL::SC_COL_INTERVAL)->text().toUpper();

        Trigger thisTrigger;
        thisTrigger.bus = -1; //-1 means we don't care which
        thisTrigger.ID = -1; //the rest of these being -1 means nothing has changed it
        thisTrigger.maxCount = -1;
        thisTrigger.milliseconds = -1;
        thisTrigger.currCount = 0;
        thisTrigger.msCounter = 0;
        thisTrigger.triggerMask = 0;
        thisTrigger.readyCount = true;

        tempData->triggers.clear();
        tempData->triggers.reserve(1);

        if (trigger != "")
        {
            thisTrigger.milliseconds = Utility::ParseStringToNum(trigger);
            thisTrigger.triggerMask |= TriggerMask::TRG_MS;
        }

        if (thisTrigger.milliseconds < 1) thisTrigger.milliseconds = 1;

        tempData->triggers.append(thisTrigger);

        break;
    }
}

void MainWindow::createSenderRow()
{
    int row = ui->tableSimpleSender->rowCount();
    ui->tableSimpleSender->insertRow(row);

    QTableWidgetItem *item = new QTableWidgetItem();
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
    inhibitSenderChanged = true;
    ui->tableSimpleSender->setItem(row, SIMP_COL::SC_COL_EN, item);

    item = new QTableWidgetItem();
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
    ui->tableSimpleSender->setItem(row, SIMP_COL::SC_COL_EXT, item);

    item = new QTableWidgetItem();
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
    ui->tableSimpleSender->setItem(row, SIMP_COL::SC_COL_REM, item);

    for (int i = 1; i <= SIMP_COL::SC_COL_COUNT; i++)
    {
        if (i != SIMP_COL::SC_COL_EXT && i != SIMP_COL::SC_COL_REM) {
            item = new QTableWidgetItem("");
            ui->tableSimpleSender->setItem(row, i, item);
        }
    }

    inhibitSenderChanged = false;
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
    CommFrame frame = model->getListReference()->at(idx.row());
    emit sendCenterTimeID(frame.frameId(), frame.timeStamp().microSeconds() / 1000000.0);
}

void MainWindow::gridContextMenuRequest(QPoint pos)
{
    QModelIndex idx = ui->canFramesView->indexAt(pos); //figure out where in the view we clicked (row, column)
    qDebug() << "Pos: " << pos << " Row :" << idx.row() << " Col: " << idx.column();

    if (!idx.isValid()) return;

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(copyAct);

    if (idx.column() == 8) //we're over the DATA column
    {
        contextMenuPosition = pos;
        menu->addSeparator();
        menu->addAction(tr("Add to a new graphing window"), this, SLOT(setupAddToNewGraph()));
        menu->addAction(tr("Add to latest graphing window"), this, SLOT(setupSendToLatestGraphWindow()));
    }

    menu->popup(ui->canFramesView->viewport()->mapToGlobal(pos));
}

void MainWindow::copyFromTable()
{
    copySelection();
}

void MainWindow::copySelection()
{
    QItemSelectionModel *selectionModel = ui->canFramesView->selectionModel();
    QModelIndexList selectedIndexes = selectionModel->selectedIndexes();

    if(selectedIndexes.isEmpty())
        return;

    // QModelIndex::operator< sorts by row and then by column.
    std::sort(selectedIndexes.begin(), selectedIndexes.end());

    QString selectedText;
    int currentRow = -1;
    int lastRow = selectedIndexes.last().row();

    for(const QModelIndex &current : selectedIndexes)
    {
        if (currentRow != -1 && current.row() != currentRow)
        {
            // remove last tab
            if (selectedText.endsWith(QLatin1Char('\t')))
                selectedText.chop(1);
            selectedText.append(QLatin1Char('\n'));
        }
        currentRow = current.row();

        QString cellText = current.data(Qt::DisplayRole).toString();

        // Replace newlines within a cell to avoid breaking the table structure in Excel
        cellText.replace(QLatin1Char('\n'), QLatin1String("  "));

        selectedText.append(cellText);

        if (current.row() != lastRow || current != selectedIndexes.last())
        {
            selectedText.append(QLatin1Char('\t'));
        }
    }

    // remove last tab if it exists
    if (selectedText.endsWith(QLatin1Char('\t')))
        selectedText.chop(1);

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(selectedText);
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

void MainWindow::interpretAutoScroll(bool state)
{
    QSettings settings;
    settings.setValue("Main/AutoScroll", state);
}

void MainWindow::interpretToggled(bool state)
{
    QSettings settings;
    settings.setValue("Main/Interpret", state);

    model->setInterpretMode(state);
    //ui->canFramesView->resizeRowsToContents();   //a VERY costly operation!
}

void MainWindow::overwriteToggled(bool state)
{
    QSettings settings;
    settings.setValue("Main/Overwrite", state);

    if (state)
    {
        QMessageBox::StandardButton confirmDialog;
        confirmDialog = QMessageBox::question(this, "Danger Will Robinson", "Enabling Overwrite mode will\ndelete your captured frames\nand replace them with one\nframe per ID.\n\nAre you ready to do that?",
                                      QMessageBox::Yes|QMessageBox::No);
        if (confirmDialog == QMessageBox::Yes)
        {
            model->setOverwriteMode(true);
            ui->canFramesView->showColumn((int)Column::FrameCount);
        }
        else ui->cbOverwrite->setCheckState(Qt::Unchecked);

    }
    else
    {
        rowExpansionActive = false;
        model->setOverwriteMode(false);
        ui->canFramesView->hideColumn((int)Column::FrameCount);
    }
}

void MainWindow::changingOnlyToggled(bool state)
{
    QSettings settings;
    settings.setValue("Main/ChangingOnly", state);
    model->setChangingOnly(state);
}

void MainWindow::presistentFiltersToggled(bool state)
{    
    QSettings settings;
    settings.setValue("Main/PersistentFilters", state);

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
    if (filters == nullptr || busFilters == nullptr) return;

    qDebug() << "updateFilterList called on MainWindow";

    inhibitFilterUpdate = true;

    ui->listFilters->clear();
    ui->listBusFilters->clear();
    ui->listDirFilters->clear();
    ui->listLengthFilters->clear();

    if (filters->isEmpty()) return;

    QMap<int, bool>::const_iterator filterIter;
    for (filterIter = filters->begin(); filterIter != filters->end(); ++filterIter)
    {
        /*QListWidgetItem *thisItem = */FilterUtility::createCheckableFilterItem(filterIter.key(), filterIter.value(), ui->listFilters);
    }

    if (!busFilters->isEmpty())
    {
        for (filterIter = busFilters->begin(); filterIter != busFilters->end(); ++filterIter)
        {
            FilterUtility::createCheckableBusFilterItem(filterIter.key(), filterIter.value(), ui->listBusFilters);
        }
    }

    // Direction filters
    const QMap<int, bool> *dirFilters = model->getDirFiltersReference();
    if (dirFilters && !dirFilters->isEmpty())
    {
        for (filterIter = dirFilters->begin(); filterIter != dirFilters->end(); ++filterIter)
        {
            QString label = (filterIter.key() == 1) ? tr("Rx") : tr("Tx");
            QListWidgetItem *item = new QListWidgetItem(label, ui->listDirFilters);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(filterIter.value() ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, filterIter.key());
        }
    }

    // Length filters
    const QMap<int, bool> *lengthFilters = model->getLengthFiltersReference();
    if (lengthFilters && !lengthFilters->isEmpty())
    {
        for (filterIter = lengthFilters->begin(); filterIter != lengthFilters->end(); ++filterIter)
        {
            FilterUtility::createCheckableBusFilterItem(filterIter.key(), filterIter.value(), ui->listLengthFilters);
        }
    }

    // Adjust height of the wrapping filter lists to fit their items (deferred so layout is done first)
    QTimer::singleShot(0, this, [this]() {
        auto fitH = [](QListWidget *list) {
            if (list->count() == 0) { list->setFixedHeight(0); return; }
            QRect r = list->visualItemRect(list->item(list->count() - 1));
            int h = r.bottom() + 1 + 2 * list->frameWidth();
            if (h > 0) list->setFixedHeight(h);
        };
        fitH(ui->listBusFilters);
        fitH(ui->listDirFilters);
        fitH(ui->listLengthFilters);
    });

    inhibitFilterUpdate = false;

    // Re-apply any active search filter text so items stay hidden/shown after the list is repopulated
    onSearchFilterChanged(ui->leSearchFilter->text());
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

    int ID = FilterUtility::getIdAsInt(item);
    bool isSet = (item->checkState() == Qt::Checked);

    model->setBusFilterState(ID, isSet);

    manageRowExpansion();
}

void MainWindow::dirFilterListItemChanged(QListWidgetItem *item)
{
    if (inhibitFilterUpdate) return;

    int dir = item->data(Qt::UserRole).toInt();
    bool isSet = (item->checkState() == Qt::Checked);

    model->setDirFilterState(dir, isSet);

    manageRowExpansion();
}

void MainWindow::lengthFilterListItemChanged(QListWidgetItem *item)
{
    if (inhibitFilterUpdate) return;

    int len = FilterUtility::getIdAsInt(item);
    bool isSet = (item->checkState() == Qt::Checked);

    model->setLengthFilterState(len, isSet);

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

void MainWindow::logReceivedFrame(CANConnection* conn, QVector<CommFrame> frames)
{
    Q_UNUSED(conn);
    if (continuousLogging)
    {
        FrameFileIO::writeContinuousNative(&frames, 0);
        //for the continuous log's notes
        for (const CommFrame &frame : frames)
        {
            const qint64 us = frame.timeStamp().seconds() * 1000000 + frame.timeStamp().microSeconds();
            if (continuousFrameCount == 0 || us < continuousFirstUs) continuousFirstUs = us;
            if (continuousFrameCount == 0 || us > continuousLastUs) continuousLastUs = us;
            continuousFrameCount++;
        }
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
//            const QVector<CommFrame> *modelFrames = model->getListReference();
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

        //refresh the count for all the frame senders
        FrameSendData *tempData;
        int numRows = ui->tableSimpleSender->rowCount();
        for (int i = 0; i < numRows; i++)
        {
            tempData = frameSender->getSendRecordRef(i);
            if (tempData)
            {
                ui->tableSimpleSender->item(i, SIMP_COL::SC_COL_COUNT)->setText(QString::number( tempData->count ));
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

void MainWindow::addFrameToDisplay(CommFrame &frame, bool autoRefresh = false)
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
    //a new capture: what the user typed carries over, what described the old log doesn't
    logNotes = logNotes.userFields();
    logNotesFile.clear();
    logNotesSource.clear();
    updateLogNotesPanel();
    updateFileStatus();
    emit framesUpdated(-1);
}

void MainWindow::normalizeTiming()
{
    model->normalizeTiming();
    emit framesUpdated(-2); //claim an all new set of frames because every frame was updated.
}

void MainWindow::handleCropLog()
{
    const QVector<CommFrame> *src = model->getListReference();
    if (src->isEmpty())
    {
        QMessageBox::information(this, tr("Crop Log"), tr("No frames loaded."));
        return;
    }

    CropLogDialog dlg(src, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    int startIdx = dlg.startIndex();
    int endIdx   = dlg.endIndex();
    QVector<CommFrame> cropped = src->mid(startIdx, endIdx - startIdx + 1);

    model->clearFrames();
    model->insertFrames(cropped);
    CANConManager::getInstance()->resetTimeBasis();
    ui->lbNumFrames->setText(QString::number(model->rowCount()));
    bDirty = true;
    emit framesUpdated(-2);
}

void MainWindow::handleLoadFile()
{
    QString filename;
    QString fullPath;
    QVector<CommFrame> tempFrames;

    QMessageBox::StandardButton confirmDialog;

    bool loadResult = FrameFileIO::loadFrameFile(filename, &tempFrames, &fullPath);

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

        loadNotesOfLog(fullPath, QFileInfo(fullPath).fileName(), true);
        updateFileStatus();
        emit framesUpdated(-1);
    }
}

void MainWindow::handleDroppedFile(const QString &filename)
{
    loadLogFile(filename, 0, filename, false);
}

void MainWindow::handleLoadFromGitHub()
{
    GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Open, FrameFileIO::loadFilters(), this);
    if (dialog.exec() != QDialog::Accepted) return;
    //notes are read from the downloaded copy, but not written back to it
    if (loadLogFile(dialog.localFile(), dialog.fileTypeIndex(), dialog.origin().displayName(), true, false))
        gitHubLogOrigin = dialog.origin();
}

void MainWindow::handleSaveToGitHub()
{
    const QVector<CommFrame> *frames = model->getListReference();
    if (frames->isEmpty())
    {
        QMessageBox::information(this, tr("Save Log File to GitHub"), tr("There are no frames to save."));
        return;
    }

    GitHubBrowserDialog dialog(GitHubBrowserDialog::Logs, GitHubBrowserDialog::Save, FrameFileIO::saveFilters(), this);
    dialog.setSaveWriter([frames](const QString &localFile, int fileType, QString &error)
    {
        QString name = localFile;
        if (FrameFileIO::saveFrameFileAs(name, fileType, frames)) return true;
        error = tr("The log couldn't be written as that file type.");
        return false;
    });
    //back where it came from, if it came from GitHub; otherwise under its own name
    if (gitHubLogOrigin.isValid() && loadedFileName == gitHubLogOrigin.displayName())
        dialog.setSuggestion(gitHubLogOrigin, QString());
    else
    {
        const QString name = QFileInfo(loadedFileName).fileName();
        dialog.setSuggestion(GitHubOrigin(), name.isEmpty() || loadedFileName.startsWith("github:") ? "log" : name);
    }
    if (dialog.exec() != QDialog::Accepted) return;

    gitHubLogOrigin = dialog.origin();
    loadedFileName = gitHubLogOrigin.displayName();
    updateFileStatus();
}

void MainWindow::showGitHubAccount()
{
    GitHubAccountDialog dialog(this);
    dialog.exec();
}

//Loads a log file into the frame list as the file type at that index of FrameFileIO::loadFilters() (0 autodetects),
//showing it as displayName. reportFailure: say so when nothing at all could be loaded. notesWritable: File > Log
//Notes... may save the log's notes back into the file (or next to it).
bool MainWindow::loadLogFile(const QString &filename, int fileType, const QString &displayName, bool reportFailure,
                             bool notesWritable)
{
    QProgressDialog progress(qApp->activeWindow());
    progress.setWindowModality(Qt::WindowModal);
    progress.setLabelText("Loading file...");
    progress.setCancelButton(nullptr);
    progress.setRange(0,0);
    progress.setMinimumDuration(0);
    progress.show();
    
    QVector<CommFrame> loadedFrames;
    bool loadResult = FrameFileIO::loadFrameFileAs(filename, fileType, &loadedFrames);
    
    progress.cancel();
    
    if (!loadResult && reportFailure && loadedFrames.isEmpty())
    {
        QMessageBox::warning(this, tr("Error Loading"), tr("No frames could be loaded from %1. Perhaps it isn't "
                                                           "that type of file?").arg(displayName));
    }

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
        loadedFileName = displayName;
        model->recalcOverwrite();
        ui->lbNumFrames->setText(QString::number(model->rowCount()));
        if (ui->cbAutoScroll->isChecked()) ui->canFramesView->scrollToBottom();

        loadNotesOfLog(filename, displayName, notesWritable);
        updateFileStatus();
        emit framesUpdated(-1);
    }
    return loadResult;
}


void MainWindow::handleSaveFile()
{
    saveLog(model->getListReference(), false);
}

//One line per bus of the current connections, for the log notes: "0: GVRET COM11, 500 kbit/s"
static QStringList connectedBuses()
{
    auto rate = [](int bitsPerSecond)
    {
        if (bitsPerSecond % 1000 == 0) return QString("%1 kbit/s").arg(bitsPerSecond / 1000);
        return QString("%1 bit/s").arg(bitsPerSecond);
    };
    QStringList buses;
    CANConManager *manager = CANConManager::getInstance();
    for (CANConnection *conn : manager->getConnections())
    {
        QString type;
        switch (conn->getType())
        {
        case CANCon::KVASER: type = "KVASER"; break;
        case CANCon::SERIALBUS: type = conn->getDriver(); break;
        case CANCon::GVRET_SERIAL: type = "GVRET"; break;
        case CANCon::REMOTE: type = "Remote"; break;
        case CANCon::KAYAK: type = "socketcand"; break;
        case CANCon::MQTT: type = "MQTT"; break;
        case CANCon::LAWICEL: type = "LAWICEL"; break;
        case CANCon::CANSERVER: type = "CANserver"; break;
        case CANCon::CANLOGSERVER: type = "CanLogServer"; break;
        default: type = "?"; break;
        }
        const int base = manager->getBusBase(conn);
        for (int i = 0; i < conn->getNumBuses(); i++)
        {
            CANBus bus;
            if (!conn->getBusSettings(i, bus) || !bus.isActive()) continue;
            QString line = QString("%1: %2 %3, %4").arg(base + i).arg(type, conn->getPort(), rate(bus.getSpeed()));
            if (bus.isCanFD()) line += QString(", CAN FD data %1").arg(rate(bus.getDataRate()));
            if (bus.isListenOnly()) line += ", listen only";
            if (conn->getStatus() != CANCon::CONNECTED) line += ", not connected";
            buses.append(line);
        }
    }
    return buses;
}

//One line per loaded DBC or LDF file: "bmw_e90.dbc (bus 0)"
static QStringList loadedDbcFiles()
{
    QStringList files;
    DBCHandler *handler = DBCHandler::getReference();
    for (int i = 0; i < handler->getFileCount(); i++)
    {
        DBCFile *file = handler->getFileByIdx(i);
        if (!file) continue;
        if (file->getAssocBus() < 0) files.append(QString("%1 (all buses)").arg(file->getFilename()));
        else files.append(QString("%1 (bus %2)").arg(file->getFilename()).arg(file->getAssocBus()));
    }
    return files;
}

//The notes to save with these frames: the current ones (or the typed fields of the last ones saved), with the fields
//SavvyCAN knows filled in. Buses and DBC files describe the current setup, unless the notes came from a log file:
//then they describe that log's capture, and are only filled in where it had none.
LogNotes MainWindow::notesForSaving(const QVector<CommFrame> *frames, bool filtered) const
{
    LogNotes notes = logNotes;
    if (!notes.hasUserFields()) notes.setUserFields(LogNotesDialog::lastNotes());
    if (notes.author.isEmpty()) notes.author = qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER"));

    const qint64 total = model->getListReference()->count();
    notes.setFrames(*frames, (filtered && frames->count() != total) ? total : -1);
    notes.saved = QDateTime::currentDateTime();
    notes.savvycanVersion = QString::number(VERSION);

    const bool fromLog = !logNotesSource.isEmpty();
    const QStringList buses = connectedBuses();
    if (!buses.isEmpty() && (!fromLog || notes.buses.isEmpty())) notes.buses = buses;
    const QStringList dbcFiles = loadedDbcFiles();
    if (!dbcFiles.isEmpty() && (!fromLog || notes.dbcFiles.isEmpty())) notes.dbcFiles = dbcFiles;
    return notes;
}

//Save Log File and Save Filtered Log File: pick the file, ask for the notes, save both
void MainWindow::saveLog(const QVector<CommFrame> *frames, bool filtered)
{
    QString path;
    int filterIndex;
    if (!FrameFileIO::pickSaveFile(path, filterIndex)) return;

    LogNotes notes = notesForSaving(frames, filtered);
    //without asking, the notes are saved if the user has typed any
    bool withNotes = notes.hasUserFields() || !notes.markers.isEmpty();
    if (LogNotesDialog::askOnSave())
    {
        LogNotesDialog dialog(LogNotesDialog::Saving, this);
        dialog.setNotes(notes);
        dialog.setTarget(QFileInfo(path).fileName(), filterIndex == 0);
        dialog.exec();
        if (dialog.choice() == LogNotesDialog::Cancelled) return;
        withNotes = dialog.choice() == LogNotesDialog::WithNotes;
        if (withNotes) notes = dialog.notes();
    }

    if (!FrameFileIO::saveFrameFileWithProgress(path, filterIndex, frames)) return;
    loadedFileName = QFileInfo(path).fileName();

    notes.logFile = QFileInfo(path).fileName();
    if (withNotes)
    {
        if (!FrameFileIO::saveLogNotes(path, filterIndex, notes))
            QMessageBox::warning(this, tr("Log Notes"), tr("The log was saved, but its notes couldn't be saved."));
        LogNotesDialog::setLastNotes(notes);
    }

    //a notes file next to the log is from an earlier log of that name: it would be read as this log's notes
    if ((!withNotes || filterIndex == 0) && QFile::exists(LogNotes::sidecarPath(path)))
    {
        if (QMessageBox::question(this, tr("Log Notes"),
                tr("There is a notes file %1 from an earlier log with this name, which would be shown as this log's "
                   "notes. Delete it?").arg(QFileInfo(LogNotes::sidecarPath(path)).fileName())) == QMessageBox::Yes)
            LogNotes::removeSidecar(path);
    }

    logNotes = withNotes ? notes : LogNotes();
    logNotesFile = path;
    logNotesSource = loadedFileName;
    updateLogNotesPanel();
    updateFileStatus();
}

//Reads the notes of a log just loaded (the empty notes if it has none)
void MainWindow::loadNotesOfLog(const QString &filename, const QString &displayName, bool writable)
{
    LogNotes notes;
    const bool found = !filename.isEmpty() && FrameFileIO::loadLogNotes(filename, notes);
    logNotes = found ? notes : LogNotes();
    logNotesFile = writable ? filename : QString();
    logNotesSource = displayName;
    updateLogNotesPanel();
    if (found) logNotesPanel->show();
}

//shows the current notes in the panel, and their markers in the graphs
void MainWindow::updateLogNotesPanel()
{
    logNotesPanel->setNotes(logNotes, logNotesSource);
    for (GraphingWindow *win : graphWindows) win->setLogMarkers(logNotes.markers);
}

bool MainWindow::anyBusConnected() const
{
    for (CANConnection *conn : CANConManager::getInstance()->getConnections())
        if (conn->getStatus() == CANCon::CONNECTED) return true;
    return false;
}

//Ctrl+M, the Add Marker buttons: marks a point of the log. While capturing that's the newest frame; on a log that
//isn't being captured (no bus connected, or capturing suspended), the frame clicked in the list (or the newest one).
void MainWindow::addLogMarker()
{
    const QVector<CommFrame> *frames = model->getFilteredListReference();
    if (frames->isEmpty())
    {
        QMessageBox::information(this, tr("Add Marker"), tr("There are no frames to mark yet."));
        return;
    }
    auto timeOf = [](const CommFrame &frame)
    {
        return frame.timeStamp().seconds() * 1000000 + frame.timeStamp().microSeconds();
    };
    LogMarker marker;
    //the frame list selects single cells, so the clicked frame is the current index's row (selectedRows() is empty)
    const QModelIndex current = ui->canFramesView->currentIndex();
    const bool capturing = anyBusConnected() && allowCapture;
    if (!capturing && current.isValid() && ui->canFramesView->selectionModel()->isSelected(current)
        && current.row() < frames->count())
        marker.timeUs = timeOf(frames->at(current.row()));
    else
    {
        //the newest frame: not always the last row (overwrite mode orders the list by ID)
        const QVector<CommFrame> *all = model->getListReference();
        marker.timeUs = timeOf(all->isEmpty() ? frames->last() : all->first());
        for (const CommFrame &frame : *all) marker.timeUs = qMax(marker.timeUs, timeOf(frame));
    }

    bool ok = false;
    marker.text = QInputDialog::getText(this, tr("Add Marker"),
                                        tr("Marker at %1 s:").arg(LogNotes::formatSeconds(marker.timeUs)),
                                        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok) return;
    if (marker.text.isEmpty()) marker.text = tr("Marker %1").arg(logNotes.markers.count() + 1);
    logNotes.addMarker(marker);

    //a loaded log's markers are saved back at once, as File > Log Notes... does
    if (!logNotesFile.isEmpty() && QFile::exists(logNotesFile))
    {
        if (logNotes.logFile.isEmpty()) logNotes.logFile = QFileInfo(logNotesFile).fileName();
        const bool saved = FrameFileIO::isNativeCSVFile(logNotesFile) ? logNotes.writeToNativeCSV(logNotesFile)
                                                                      : logNotes.writeSidecar(logNotesFile);
        if (!saved) QMessageBox::warning(this, tr("Add Marker"), tr("The marker couldn't be saved with %1.")
                                                                   .arg(QFileInfo(logNotesFile).fileName()));
    }
    updateLogNotesPanel();
    updateFileStatus();
}

//a marker double-clicked in the Log Notes panel: select the frame closest to its time
void MainWindow::goToLogTime(qint64 timeUs)
{
    const QVector<CommFrame> *frames = model->getFilteredListReference();
    int best = -1;
    qint64 bestDistance = 0;
    for (int i = 0; i < frames->count(); i++)
    {
        const CommFrame::TimeStamp stamp = frames->at(i).timeStamp();
        const qint64 distance = qAbs(stamp.seconds() * 1000000 + stamp.microSeconds() - timeUs);
        if (best < 0 || distance < bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }
    if (best < 0) return;
    ui->cbAutoScroll->setChecked(false); //or the next frames would scroll it away
    ui->canFramesView->selectRow(best);
    ui->canFramesView->scrollTo(model->index(best, 0), QAbstractItemView::PositionAtCenter);
}

//When capture starts (the first bus connects with no frames, or continuous logging starts): ask for the setup and
//scenario now, so they're written down before the test rather than after
void MainWindow::askCaptureNotes()
{
    LogNotes notes = notesForSaving(model->getListReference(), false);
    notes.frameCount = -1; //nothing captured yet
    notes.hasSpan = false;
    notes.saved = QDateTime();

    LogNotesDialog dialog(LogNotesDialog::Starting, this);
    dialog.setNotes(notes);
    dialog.setTarget(continuousLogging ? QFileInfo(FrameFileIO::continuousFileName()).fileName() : QString(), true);
    dialog.exec();
    if (dialog.choice() != LogNotesDialog::WithNotes) return;

    logNotes = dialog.notes();
    if (logNotes.hasUserFields()) LogNotesDialog::setLastNotes(logNotes);
    //these notes are for the capture, not for a log file that was open
    logNotesFile.clear();
    logNotesSource.clear();
    updateLogNotesPanel();
}

//File > Log Notes...: the notes of the loaded log, saved back into it (or next to it) straight away; with no log
//file, they're kept for the next save
void MainWindow::editLogNotes()
{
    LogNotes notes = logNotes.isEmpty() ? notesForSaving(model->getListReference(), false) : logNotes;
    const bool hasFile = !logNotesFile.isEmpty() && QFile::exists(logNotesFile);
    const bool insideLog = hasFile && FrameFileIO::isNativeCSVFile(logNotesFile);
    if (hasFile && notes.logFile.isEmpty()) notes.logFile = QFileInfo(logNotesFile).fileName();

    LogNotesDialog dialog(LogNotesDialog::Editing, this);
    dialog.setNotes(notes);
    dialog.setTarget(hasFile ? QFileInfo(logNotesFile).fileName() : QString(), insideLog);
    if (dialog.exec() != QDialog::Accepted) return;
    notes = dialog.notes();

    if (hasFile)
    {
        bool saved;
        if (insideLog) saved = notes.writeToNativeCSV(logNotesFile);
        else if (notes.isEmpty()) saved = LogNotes::removeSidecar(logNotesFile);
        else saved = notes.writeSidecar(logNotesFile);
        if (!saved) QMessageBox::warning(this, tr("Log Notes"), tr("The notes couldn't be saved with %1.")
                                                                  .arg(QFileInfo(logNotesFile).fileName()));
    }
    if (notes.hasUserFields()) LogNotesDialog::setLastNotes(notes);
    logNotes = notes;
    updateLogNotesPanel();
}

void MainWindow::handleContinousLogging()
{
    if (!continuousLogging)
    {
        //only logging once a file was picked and opened (cancelling the file dialog used to leave it "logging")
        if (!FrameFileIO::openContinuousNative()) return;
        continuousLogging = true;
        continuousFrameCount = 0;
        ui->actionSave_Continuous_Logfile->setText(tr("Cease Continuous Logging"));
        if (LogNotesDialog::askOnCapture()) askCaptureNotes();
    }
    else
    {
        continuousLogging = false;
        ui->actionSave_Continuous_Logfile->setText(tr("Start Continuous Logging"));
        ui->lblContMsg->setText("");
        FrameFileIO::closeContinuousNative();
        finishContinuousLogNotes();
    }
}

//Continuous logging stopped: the notes go at the end of the log it wrote, after asking for what was seen
void MainWindow::finishContinuousLogNotes()
{
    const QString path = FrameFileIO::continuousFileName();
    LogNotes notes = notesForSaving(model->getListReference(), false);
    notes.frameCount = continuousFrameCount;
    notes.unfilteredCount = -1;
    notes.hasSpan = continuousFrameCount > 0;
    notes.firstUs = continuousFirstUs;
    notes.lastUs = continuousLastUs;
    notes.logFile = QFileInfo(path).fileName();

    bool withNotes = notes.hasUserFields() || !notes.markers.isEmpty();
    if (LogNotesDialog::askOnSave())
    {
        LogNotesDialog dialog(LogNotesDialog::Finishing, this);
        dialog.setNotes(notes);
        dialog.setTarget(notes.logFile, true);
        dialog.exec();
        withNotes = dialog.choice() == LogNotesDialog::WithNotes;
        if (withNotes) notes = dialog.notes();
    }
    if (!withNotes) return;

    if (!notes.writeToNativeCSV(path))
        QMessageBox::warning(this, tr("Log Notes"), tr("The notes couldn't be saved in %1.").arg(notes.logFile));
    LogNotesDialog::setLastNotes(notes);
    //the frames on screen are still the capture's, so the notes (and markers) stay with them
    logNotes = notes;
    logNotes.logFile.clear();
    updateLogNotesPanel();
}

void MainWindow::handleSaveFilteredFile()
{
    saveLog(model->getFilteredListReference(), true);
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
    if (!csv) filters.append(QString(tr("Text File (*.txt *.TXT)")));
    else filters.append(QString(tr("CSV File (*.csv *.CSV)")));

    dialog.setDirectory(settings.value("FileIO/LoadSaveDirectory", dialog.directory().path()).toString());
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        if (!filename.contains('.'))
        {
            if (!csv) filename += ".txt";
            else filename += ".csv";
        }

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
    const QVector<CommFrame> *frames = model->getFilteredListReference();

    //const unsigned char *data;
    int dataLen;
    const CommFrame *frame;

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
    if (CSVAbsTime)
    {
        builderString += tr("Year") + "," + tr("Month") + "," + tr("Day") + "," + tr("Hour") + "," + tr("Minute") + "," + tr("Second") + "," + tr("Ms") + ",";
        dataStartCol += 7;
    }
    else
    {
        //time
        builderString += tr("Time") + ",";
        dataStartCol++;
    }
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
        dataLen = frame->payload().length();

        //add all column names
        if (dbcHandler != nullptr)
        {            
            DBC_MESSAGE *msg = dbcHandler->findMessage(*frame);
            if (msg != nullptr)
            {                
                QList<DBC_SIGNAL *> sigs = msg->sigHandler->getSignalsAsList();
                bool found = false;
                for (int j = 0; j < msg->sigHandler->getCount(); j++)
                {
                    if(j==0)
                    {
                        for(int m = 0; m < msgsAndColumns.count(); m++)
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
                        if (sigs[j]->processAsText(*frame, temp))
                        {
                            builderString.append(sigs[j]->name);
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
        dataLen = frame->payload().length();

        QString builderString;
        if (CSVAbsTime)
        {
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(frame->timeStamp().microSeconds() / 1000);
            builderString += QString::number(dt.date().year()) + "," + QString::number(dt.date().month()) + ",";
            builderString += QString::number(dt.date().day()) + "," + QString::number(dt.time().hour()) + ",";
            builderString += QString::number(dt.time().minute()) + "," + QString::number(dt.time().second()) + ",";
            builderString += QString::number(dt.time().msec()) + ",";
            dataColumnsAdded += 7;
        }
        else {
            builderString += QString::number((frame->timeStamp().microSeconds() / 1000000.0), 'f', 6) + ",";
            dataColumnsAdded++;
        }
        //id
        builderString += Utility::formatCANID(frame->frameId(), frame->hasExtendedFrameFormat()) + ",";
        dataColumnsAdded++;
        //if (frame->hasExtendedFrameFormat()) builderString += tr(" Ext ");
        //else builderString += tr(" Std ");
        //bus
        builderString += QString::number(frame->getBus()) + ",";
        dataColumnsAdded++;
        //len
        builderString += QString::number(dataLen) + ",";
        dataColumnsAdded++;

        if (dbcHandler != nullptr)
        {
            DBC_MESSAGE *msg = dbcHandler->findMessage(*frame);
            if (msg != nullptr)
            {
                QList<DBC_SIGNAL *> sigs = msg->sigHandler->getSignalsAsList();
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
                    if (sigs[j]->processAsText(*frame, temp))
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
    const QVector<CommFrame> *frames = model->getFilteredListReference();

    const unsigned char *data;
    int dataLen;
    const CommFrame *frame;

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
        dataLen = frame->payload().length();

        QString builderString;
        builderString += tr("Time: ") + QString::number((frame->timeStamp().microSeconds() / 1000000.0), 'f', 6);
        builderString += tr("    ID: ") + Utility::formatCANID(frame->frameId(), frame->hasExtendedFrameFormat());
        if (frame->hasExtendedFrameFormat()) builderString += tr(" Ext ");
        else builderString += tr(" Std ");
        builderString += tr("Bus: ") + QString::number(frame->getBus());
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
                QList<DBC_SIGNAL *> sigs = msg->sigHandler->getSignalsAsList();
                for (int j = 0; j < msg->sigHandler->getCount(); j++)
                {

                    QString temp;
                    if (sigs[j]->processAsText(*frame, temp))
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

    //capture starts: the first bus connected with nothing captured yet (continuous logging asks on its own)
    const bool firstConnection = connectedBusCount == 0 && conns > 0;
    connectedBusCount = conns;
    if (firstConnection && model->rowCount() == 0 && !continuousLogging && LogNotesDialog::askOnCapture())
        QTimer::singleShot(0, this, &MainWindow::askCaptureNotes); //not from inside the connection manager's signal
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
            if (!logNotesSource.isEmpty() && !logNotes.isEmpty()) output += tr(", with notes");
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

CommFrameModel* MainWindow::getCommFrameModel()
{
    return model;
}


/*
 * All functions past this point set up the various other windows that can be opened
*/

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
    const QVector<CommFrame> *frames;
    if (!useFiltered)
        frames = model->getListReference();
    else
        frames = model->getFilteredListReference();
    lastGraphingWindow = new GraphingWindow(frames);
    graphWindows.append(lastGraphingWindow);
    lastGraphingWindow->setLogMarkers(logNotes.markers);

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
        const QVector<CommFrame> *frames;
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

void MainWindow::showSearchWindow()
{
    if (!searchWindow)
    {
        searchWindow = new SearchWindow(model->getFilteredListReference(), dbcHandler);
        connect(searchWindow, &SearchWindow::jumpToFrameIndex, this, &MainWindow::jumpToSearchFrame);
        connect(this, &MainWindow::framesUpdated, searchWindow, &SearchWindow::updatedFrames);
        // Clear stale results whenever the filter changes (sendRefresh rebuilds filteredFrames)
        connect(model, &QAbstractItemModel::modelReset, searchWindow, [this]() {
            searchWindow->updatedFrames(0);
        });
    }
    searchWindow->show();
    searchWindow->raise();
}

void MainWindow::jumpToSearchFrame(int frameIndex)
{
    if (!model || frameIndex < 0 || frameIndex >= model->getFilteredListReference()->size())
        return;

    // frameIndex is a row in CommFrameModel (filteredFrames); map through the
    // sort proxy so the view scrolls to the correct visible row.
    QAbstractProxyModel *proxy = qobject_cast<QAbstractProxyModel *>(ui->canFramesView->model());
    QModelIndex srcIndex = model->index(frameIndex, 0);
    QModelIndex target = proxy ? proxy->mapFromSource(srcIndex) : srcIndex;
    if (target.isValid())
    {
        ui->canFramesView->scrollTo(target, QAbstractItemView::PositionAtCenter);
        ui->canFramesView->setCurrentIndex(target);
    }
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

void MainWindow::showUDSFirmwareUploaderWindow()
{
    if (!udsFirmwareUploaderWindow)
    {
        udsFirmwareUploaderWindow = new UDSFirmwareUploaderWindow(model->getListReference());
    }
    udsFirmwareUploaderWindow->show();
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
        //connect(motorctrlConfigWindow, SIGNAL(sendCommFrame(const CommFrame*,int)), worker, SLOT(sendFrame(const CommFrame*,int)));
        //connect(motorctrlConfigWindow, SIGNAL(sendFrameBatch(const QList<CommFrame>*)), worker, SLOT(sendFrameBatch(const QList<CommFrame>*)));
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

void MainWindow::showObd2Window()
{
    if (!obd2Window)
    {
        ISOTP_HANDLER *isotp = HandlerFactory::createISOTPHandler();
        obd2Window = new Obd2Window(new Obd2Poller(isotp), CANConManager::getInstance()->getNumBuses());
        isotp->setParent(obd2Window); //deleted with the window, after the poller that uses it
        obd2Window->setHelpCallback([]() { HelpWindow::getRef()->showHelp("obd2.md"); });
        obd2Window->setListenOnlyCheck([](int bus) {
            int busBase = 0;
            for (CANConnection *conn : CANConManager::getInstance()->getConnections())
            {
                if (bus < busBase + conn->getNumBuses())
                {
                    CANBus settings;
                    return conn->getBusSettings(bus - busBase, settings) && settings.isListenOnly();
                }
                busBase += conn->getNumBuses();
            }
            return false;
        });
        obd2Window->setClockCheck([]() {
            //each adapter stamps its frames with its own clock unless the system clock is used for all of them
            return CANConManager::getInstance()->getConnections().count() > 1
                   && !QSettings().value("Main/TimeClock", false).toBool();
        });
        obd2Window->setDbcLoader([this](const QString &path, int bus, const QList<quint32> &ecus, QString *error) {
            return attachObd2Dbc(path, bus, ecus, error);
        });
        obd2Window->setGraphOpener([this](int bus, const QList<Obd2Window::GraphSignal> &sigs, QString *error) {
            return graphObd2Signals(bus, sigs, error);
        });
    }
    obd2Window->show();
    obd2Window->raise();
}

//Loads the DBC file the OBD-II Poller wrote and attaches it to its bus. A copy already loaded from that file is used
//as it is when it has every ECU, since graphs may point at its signals; otherwise it has to be removed first.
bool MainWindow::attachObd2Dbc(const QString &path, int bus, const QList<quint32> &ecus, QString *error)
{
    DBCHandler *dbc = DBCHandler::getReference();
    DBCFile *file = nullptr;
    for (int i = 0; i < dbc->getFileCount(); i++)
    {
        if (QFileInfo(dbc->getFileByIdx(i)->getFullFilename()) == QFileInfo(path)) file = dbc->getFileByIdx(i);
    }

    bool loadedNow = false;
    if (file)
    {
        for (quint32 ecu : ecus)
        {
            if (!file->messageHandler->findMsgByID(ecu))
            {
                *error = tr("%1 is already loaded, without ECU 0x%2. Remove it in the DBC File Manager first (close the "
                            "graphs using it), or save the DBC under another name.").arg(file->getFilename()).arg(ecu, 0, 16);
                return false;
            }
        }
    }
    else
    {
        file = dbc->loadDBCFile(path);
        if (!file)
        {
            *error = tr("%1 was written but couldn't be loaded.").arg(path);
            return false;
        }
        loadedNow = true;
    }
    file->setAssocBus(bus);
    if (loadedNow && dbcFileWindow) dbcFileWindow->addLoadedFile(file);
    DBCSettingsUpdated();
    return true;
}

//Opens a graph window with the OBD-II Poller's signals, decoded by the DBC on that bus. ECUs that haven't sent a
//frame yet are left out, unless nothing has been captured at all.
bool MainWindow::graphObd2Signals(int bus, const QList<Obd2Window::GraphSignal> &sigs, QString *error)
{
    QSet<quint32> idsSeen;
    for (const CommFrame &frame : *model->getListReference())
    {
        if (frame.getBus() == bus) idsSeen.insert(frame.frameId());
    }

    DBCHandler *dbc = DBCHandler::getReference();
    QList<DBC_SIGNAL *> found;
    bool described = false;
    for (const Obd2Window::GraphSignal &wanted : sigs)
    {
        DBC_SIGNAL *sig = nullptr;
        for (int i = 0; i < dbc->getFileCount() && !sig; i++)
        {
            DBCFile *file = dbc->getFileByIdx(i);
            if (file->getAssocBus() != -1 && file->getAssocBus() != bus) continue;
            DBC_MESSAGE *msg = file->messageHandler->findMsgByID(wanted.first);
            if (msg) sig = msg->sigHandler->findSignalByName(wanted.second);
        }
        if (!sig) continue;
        described = true;
        if (idsSeen.isEmpty() || idsSeen.contains(wanted.first)) found.append(sig);
    }
    if (!described)
    {
        *error = tr("No DBC file on bus %1 describes these PIDs. Click Make DBC... first.").arg(bus);
        return false;
    }
    if (found.isEmpty())
    {
        *error = tr("None of these ECUs has answered on bus %1 yet.").arg(bus);
        return false;
    }

    static const QColor colors[] = {QColor(31, 119, 180), QColor(214, 39, 40), QColor(44, 160, 44),
                                    QColor(255, 127, 14), QColor(148, 103, 189), QColor(140, 86, 75),
                                    QColor(227, 119, 194), QColor(23, 190, 207)};
    showGraphingWindow();
    int n = 0;
    for (DBC_SIGNAL *sig : found)
    {
        GraphParams params;
        params.ID = sig->parentMessage->ID;
        params.bus = bus;
        params.startBit = sig->startBit;
        params.numBits = sig->signalSize;
        params.intelFormat = sig->intelByteOrder;
        params.isSigned = (sig->valType == SIGNED_INT);
        params.scale = sig->factor;
        params.bias = sig->bias;
        params.associatedSignal = sig;
        params.graphName = sig->name;
        params.lineColor = colors[n++ % 8];
        lastGraphingWindow->createGraph(params);
    }
    return true;
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
        connectionWindow = new ConnectionWindow();

    // Trigger a one-shot state probe on all SerialBus connections before showing the
    // window.  PCAN on macOS never emits stateChanged on USB unplug, so this is the
    // only time we can pay the cost of a brief disconnect+reconnect (user-visible action).
    for (CANConnection *conn : CANConManager::getInstance()->getConnections()) {
        if (conn->getType() == CANCon::SERIALBUS)
            QMetaObject::invokeMethod(conn, "probeConnectionState", Qt::QueuedConnection);
    }

    connectionWindow->show();
}
