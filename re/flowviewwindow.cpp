#include "flowviewwindow.h"
#include "ui_flowviewwindow.h"
#include "mainwindow.h"
#include "helpwindow.h"
#include "filterutility.h"
#include "qcpaxistickerhex.h"

const QColor FlowViewWindow::graphColors[8] = {Qt::blue, Qt::green, Qt::black, Qt::red, //0 1 2 3
                                               Qt::gray, Qt::yellow, Qt::cyan, Qt::darkMagenta}; //4 5 6 7


FlowViewWindow::FlowViewWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FlowViewWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    readSettings();

    modelFrames = frames;

    playbackTimer = new QTimer();

    currentPosition = 0;
    playbackActive = false;
    playbackForward = true;

    memset(refBytes, 0, 8);
    memset(currBytes, 0, 8);
    memset(triggerValues, -1, sizeof(int) * 8);
    triggerBits = 0;

    //ui->graphView->setInteractions();

    ui->graphView->xAxis->setRange(0, 8);
    ui->graphView->yAxis->setRange(-10, 265); //run range a bit outside possible number so they aren't plotted in a hard to see place
    if (useHexTicker)
    {
        QSharedPointer<QCPAxisTickerHex> hexTicker(new QCPAxisTickerHex);
        ui->graphView->yAxis->setTicker(hexTicker);
    }
    ui->graphView->axisRect()->setupFullAxesBox();

    QCPItemText *textLabel = new QCPItemText(ui->graphView);
    //ui->graphView->addItem(textLabel);
    textLabel->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
    textLabel->position->setType(QCPItemPosition::ptAxisRectRatio);
    textLabel->position->setCoords(0.5, .5);
    textLabel->setText("+");
    textLabel->setFont(QFont(font().family(), 16)); // make font a bit larger
    textLabel->setPen(QPen(Qt::black)); // show black border around text

    ui->graphView->xAxis->setLabel("Time Axis");
    if (useHexTicker) ui->graphView->yAxis->setLabel("Value Axis (HEX)");
    else ui->graphView->yAxis->setLabel("Value Axis (dec)");

    QFont legendFont = font();
    legendFont.setPointSize(10);
    QFont legendSelectedFont = font();
    legendSelectedFont.setPointSize(12);
    legendSelectedFont.setBold(true);
    ui->graphView->legend->setFont(legendFont);
    ui->graphView->legend->setSelectedFont(legendSelectedFont);
    ui->graphView->legend->setSelectableParts(QCPLegend::spItems); // legend box shall not be selectable, only legend items
    //ui->graphView->xAxis->setAutoSubTicks(false);
    //ui->graphView->xAxis->setAutoTicks(false);
    QCPAxisTicker *xTicker = new QCPAxisTicker();
    xTicker->setTickCount(5);
    xTicker->setTickStepStrategy(QCPAxisTicker::tssReadability);
    ui->graphView->xAxis->setTicker(QSharedPointer<QCPAxisTicker>(xTicker));
    //ui->graphView->xAxis->setAutoTickStep(false);
    //ui->graphView->xAxis->setAutoSubTicks(false);
    //ui->graphView->xAxis->setNumberFormat("gb");
    //ui->graphView->xAxis->setTickStep(5.0);
    //ui->graphView->xAxis->setSubTickCount(0);

    if (openGLMode)
    {
        ui->graphView->setAntialiasedElements(QCP::aeAll);
        ui->graphView->setOpenGl(true);
    }
    else
    {
        ui->graphView->setOpenGl(false);
        ui->graphView->setAntialiasedElements(QCP::aeNone);
    }

    connect(ui->btnBackOne, SIGNAL(clicked(bool)), this, SLOT(btnBackOneClick()));
    connect(ui->btnPause, SIGNAL(clicked(bool)), this, SLOT(btnPauseClick()));
    connect(ui->btnReverse, SIGNAL(clicked(bool)), this, SLOT(btnReverseClick()));
    connect(ui->btnStop, SIGNAL(clicked(bool)), this, SLOT(btnStopClick()));
    connect(ui->btnPlay, SIGNAL(clicked(bool)), this, SLOT(btnPlayClick()));
    connect(ui->btnForwardOne, SIGNAL(clicked(bool)), this, SLOT(btnFwdOneClick()));
    connect(ui->spinPlayback, SIGNAL(valueChanged(int)), this, SLOT(changePlaybackSpeed(int)));
    connect(ui->cbLoopPlayback, SIGNAL(clicked(bool)), this, SLOT(changeLooping(bool)));
    connect(playbackTimer, SIGNAL(timeout()), this, SLOT(timerTriggered()));
    connect(ui->graphView, SIGNAL(plottableDoubleClick(QCPAbstractPlottable*,QMouseEvent*)), this, SLOT(plottableDoubleClick(QCPAbstractPlottable*,QMouseEvent*)));
    connect(ui->txtTrigger0, SIGNAL(textEdited(QString)), this, SLOT(updateTriggerValues()));
    connect(ui->txtTrigger1, SIGNAL(textEdited(QString)), this, SLOT(updateTriggerValues()));
    connect(ui->txtTrigger2, SIGNAL(textEdited(QString)), this, SLOT(updateTriggerValues()));
    connect(ui->txtTrigger3, SIGNAL(textEdited(QString)), this, SLOT(updateTriggerValues()));
    connect(ui->txtTrigger4, SIGNAL(textEdited(QString)), this, SLOT(updateTriggerValues()));
    connect(ui->txtTrigger5, SIGNAL(textEdited(QString)), this, SLOT(updateTriggerValues()));
    connect(ui->txtTrigger6, SIGNAL(textEdited(QString)), this, SLOT(updateTriggerValues()));
    connect(ui->txtTrigger7, SIGNAL(textEdited(QString)), this, SLOT(updateTriggerValues()));
    connect(ui->graphRangeSlider, &QSlider::valueChanged, this, &FlowViewWindow::graphRangeChanged);
    connect(ui->check_0, &QCheckBox::stateChanged, this, &FlowViewWindow::changeGraphVisibility);
    connect(ui->check_1, &QCheckBox::stateChanged, this, &FlowViewWindow::changeGraphVisibility);
    connect(ui->check_2, &QCheckBox::stateChanged, this, &FlowViewWindow::changeGraphVisibility);
    connect(ui->check_3, &QCheckBox::stateChanged, this, &FlowViewWindow::changeGraphVisibility);
    connect(ui->check_4, &QCheckBox::stateChanged, this, &FlowViewWindow::changeGraphVisibility);
    connect(ui->check_5, &QCheckBox::stateChanged, this, &FlowViewWindow::changeGraphVisibility);
    connect(ui->check_6, &QCheckBox::stateChanged, this, &FlowViewWindow::changeGraphVisibility);
    connect(ui->check_7, &QCheckBox::stateChanged, this, &FlowViewWindow::changeGraphVisibility);

//    ui->timelineSlider->setTracking(true);
    connect(ui->timelineSlider, &QSlider::sliderPressed, this, &FlowViewWindow::btnPauseClick);
    connect(ui->timelineSlider, &QSlider::valueChanged, this, &FlowViewWindow::gotoFrame);

    // Using lambda expression to strip away the possible filter label before passing the ID to updateDetailsWindow
    connect(ui->listFrameID, &QListWidget::currentTextChanged, 
        [this](QString itemText)
            {
            changeID(FilterUtility::getId(itemText));
            } );

    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));

    ui->graphView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->graphView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenuRequestGraph(QPoint)));
    ui->flowView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->flowView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenuRequestFlow(QPoint)));
    connect(ui->flowView, SIGNAL(gridClicked(int,int)), this, SLOT(gotCellClick(int,int)));

    // Prevent annoying accidental horizontal scrolling when filter list is populated with long interpreted message names
    ui->listFrameID->horizontalScrollBar()->setEnabled(false);

    playbackTimer->setInterval(ui->spinPlayback->value()); //set the timer to the default value of the control
}

void FlowViewWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    installEventFilter(this);

    readSettings();

    refreshIDList();
    if (ui->listFrameID->count() > 0)
    {
        changeID(FilterUtility::getId(ui->listFrameID->item(0)));
        ui->listFrameID->setCurrentRow(0);
    }
    updateFrameLabel();
    qDebug() << "FlowView show event was processed";
}

FlowViewWindow::~FlowViewWindow()
{
    removeEventFilter(this);

    delete ui;

    playbackTimer->stop();
    delete playbackTimer;
}

void FlowViewWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
    emit rejected(); //can be picked up by main window if needed
}

void FlowViewWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("FlowView/WindowSize", QSize(1078, 621)).toSize());
        move(Utility::constrainedWindowPos(settings.value("FlowView/WindowPos", QPoint(50, 50)).toPoint()));
    }

    if (settings.value("FlowView/AutoRef", false).toBool())
    {
        ui->cbAutoRef->setChecked(true);
    }

    if (settings.value("FlowView/UseTimestamp", false).toBool())
    {
        ui->cbTimeGraph->setChecked(true);
    }

    if (settings.value("Main/FilterLabeling", false).toBool())
        ui->listFrameID->setMaximumWidth(250);
    else
        ui->listFrameID->setMaximumWidth(120);    

    secondsMode = settings.value("Main/TimeSeconds", false).toBool();
    openGLMode = settings.value("Main/UseOpenGL", false).toBool();
    useHexTicker = settings.value("FlowView/GraphHex", false).toBool();
}

void FlowViewWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("FlowView/WindowSize", size());
        settings.setValue("FlowView/WindowPos", pos());
    }
}

/*
 * Keyboard shortcuts to allow for quick work without needing to move around a mouse.
 * R = resume or pause playback
 * T = go back one frame
 * Y = go forward one frame
*/
bool FlowViewWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_R:
            if (playbackActive) btnPauseClick();
            else btnPlayClick();
            break;
        case Qt::Key_T:
            btnBackOneClick();
            break;
        case Qt::Key_Y:
            btnFwdOneClick();
            break;
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("flowview.md");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
}

void FlowViewWindow::changeGraphVisibility(int state){
    QCheckBox *sender = qobject_cast<QCheckBox *>(QObject::sender());
    if(sender){
        sender->objectName();
        int graphId = sender->objectName().right(1).toInt();
        for (int k = 0; k < 8; k++)
        {
            if (k == graphId && graphRef[k] && graphRef[k]->data()){
                graphRef[k]->setVisible(state);
            }
        }

        ui->graphView->replot();
    }
}
void FlowViewWindow::gotCellClick(int x, int y)
{
    int bitnum = (7-x) + (8 * y);
    triggerBits = triggerBits ^ (1ull << bitnum);
    if (triggerBits & (1ull << bitnum)) ui->flowView->setCellTextState(x, y, GridTextState::BOLD_BLUE);
    else ui->flowView->setCellTextState(x, y, GridTextState::NORMAL);
    qDebug() << "Bit Num: " << bitnum << "   Hex of trigger bits: " << QString::number(triggerBits, 16);
}

void FlowViewWindow::graphRangeChanged(int range) {
    ui->rangeValue->setText(QString::number(range));
    updateGraphLocation();
}

void FlowViewWindow::updateTriggerValues()
{
    triggerValues[0] = Utility::ParseStringToNum(ui->txtTrigger0->text());
    triggerValues[1] = Utility::ParseStringToNum(ui->txtTrigger1->text());
    triggerValues[2] = Utility::ParseStringToNum(ui->txtTrigger2->text());
    triggerValues[3] = Utility::ParseStringToNum(ui->txtTrigger3->text());
    triggerValues[4] = Utility::ParseStringToNum(ui->txtTrigger4->text());
    triggerValues[5] = Utility::ParseStringToNum(ui->txtTrigger5->text());
    triggerValues[6] = Utility::ParseStringToNum(ui->txtTrigger6->text());
    triggerValues[7] = Utility::ParseStringToNum(ui->txtTrigger7->text());
}

void FlowViewWindow::plottableDoubleClick(QCPAbstractPlottable* plottable, QMouseEvent* event)
{
    int id = 0;
    //apply transforms to get the X axis value where we double clicked
    double coord = plottable->keyAxis()->pixelToCoord(event->localPos().x());
    if (frameCache.count() > 0) id = frameCache[0].frameId();
    if (secondsMode) emit sendCenterTimeID(id, coord);
    else emit sendCenterTimeID(id, coord / 1000000.0);
}

void FlowViewWindow::gotCenterTimeID(int32_t ID, double timestamp)
{
    int64_t t_stamp;

    t_stamp = timestamp * 1000000l;

    qDebug() << "timestamp: " << t_stamp;

    changeID(QString::number(ID)); //to be sure we're focused on the proper ID

    for (int j = 0; j < ui->listFrameID->count(); j++)
    {
        int thisNum = FilterUtility::getIdAsInt(ui->listFrameID->item(j));
        if (thisNum == ID)
        {
            ui->listFrameID->setCurrentRow(j);
            break;
        }
    }

    int bestIdx = -1;
    for (int i = 0; i < frameCache.count(); i++)
    {
        if (frameCache[i].timeStamp().microSeconds() > t_stamp)
        {
            bestIdx = i - 1;
            break;
        }
    }
    qDebug() << "Best index " << bestIdx;
    if (bestIdx > -1)
    {
        currentPosition = bestIdx;
        if (ui->cbAutoRef->isChecked())
        {
            memcpy(refBytes, currBytes, 8);
        }

        memset(currBytes, 0, 8); //first zero out all 8 bytes

        memcpy(currBytes, frameCache.at(currentPosition).payload().data(), frameCache.at(currentPosition).payload().length());

        updateDataView();
    }
}

void FlowViewWindow::contextMenuRequestFlow(QPoint pos)
{
  QMenu *menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose);

  menu->addAction(tr("Save image to file"), this, SLOT(saveFileFlow()));

  menu->popup(ui->flowView->mapToGlobal(pos));
}

void FlowViewWindow::contextMenuRequestGraph(QPoint pos)
{
  QMenu *menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose);

  menu->addAction(tr("Save image to file"), this, SLOT(saveFileGraph()));

  menu->popup(ui->graphView->mapToGlobal(pos));
}

void FlowViewWindow::saveFileGraph()
{
    QString filename;
    QFileDialog dialog(this);
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("PDF Files (*.pdf)")));
    filters.append(QString(tr("PNG Files (*.png)")));
    filters.append(QString(tr("JPEG Files (*.jpg)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDirectory(settings.value("FlowView/LoadSaveDirectory", dialog.directory().path()).toString());

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];

        if (dialog.selectedNameFilter() == filters[0])
        {
            if (!filename.contains('.')) filename += ".pdf";
            ui->graphView->savePdf(filename, 0, 0);
        }
        if (dialog.selectedNameFilter() == filters[1])
        {
            if (!filename.contains('.')) filename += ".png";
            ui->graphView->savePng(filename, 1024, 768);
        }
        if (dialog.selectedNameFilter() == filters[2])
        {
            if (!filename.contains('.')) filename += ".jpg";
            ui->graphView->saveJpg(filename, 1024, 768);
        }
        settings.setValue("FlowView/LoadSaveDirectory", dialog.directory().path());
    }
}

void FlowViewWindow::saveFileFlow()
{
    QString filename;
    QFileDialog dialog(this);
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("PNG Files (*.png)")));
    filters.append(QString(tr("JPEG Files (*.jpg)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDirectory(settings.value("FlowView/LoadSaveDirectory", dialog.directory().path()).toString());

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];

        if (dialog.selectedNameFilter() == filters[0])
        {
            if (!filename.contains('.')) filename += ".png";
            ui->flowView->saveImage(filename, 1024, 768);
        }
        if (dialog.selectedNameFilter() == filters[1])
        {
            if (!filename.contains('.')) filename += ".jpg";
            ui->flowView->saveImage(filename, 1024, 768);
        }
        settings.setValue("FlowView/LoadSaveDirectory", dialog.directory().path());
    }
}

void FlowViewWindow::updatedFrames(int numFrames)
{
    QVector<double>newX[8];
    QVector<double>newY[8];
    const unsigned char *data;
    int dataLen = 0;

    const CANFrame *thisFrame;
    if (numFrames == -1) //all frames deleted. Kill the display
    {
        ui->listFrameID->clear();
        foundID.clear();
        currentPosition = 0;
        refreshIDList();
        updateFrameLabel();
        removeAllGraphs();
        memset(refBytes, 0, 8);
        memset(currBytes, 0, 8);
        updateDataView();
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        ui->listFrameID->clear();
        foundID.clear();
        currentPosition = 0;
        refreshIDList();
        //if (ui->listFrameID->count() > 0)
        //{
         //   changeID(ui->listFrameID->item(0)->text());
        //    ui->listFrameID->setCurrentRow(0);
        //}
        //updateFrameLabel();
    }
    else //just got some new frames. See if they are relevant.
    {
        if (numFrames > modelFrames->count()) return;
        unsigned int refID;
        if (frameCache.count() > 0) refID = frameCache[0].frameId();
            else refID = 0;
        bool needRefresh = false;
        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            thisFrame = &modelFrames->at(i);
            data = reinterpret_cast<const unsigned char *>(thisFrame->payload().constData());
            dataLen = thisFrame->payload().length();

            if (!foundID.contains(thisFrame->frameId()))
            {
                foundID.append(thisFrame->frameId());
                FilterUtility::createFilterItem(thisFrame->frameId(), ui->listFrameID);
            }

            if (thisFrame->frameId() == refID)
            {
                frameCache.append(*thisFrame);

                for (int k = 0; k < dataLen; k++)
                {
                    if (ui->cbTimeGraph->isChecked())
                    {
                        if (secondsMode){
                            newX[k].append((double)(thisFrame->timeStamp().microSeconds()) / 1000000.0);
                        }
                        else
                        {
                            newX[k].append(thisFrame->timeStamp().microSeconds());
                        }
                    }
                    else
                    {
                        newX[k].append(x[k].count());
                    }
                    newY[k].append(data[k]);
                    needRefresh = true;
                }
            }
        }
        if (ui->cbLiveMode->checkState() == Qt::Checked)
        {
            currentPosition = frameCache.count() - 1;
            memset(currBytes, 0, 8);
            memcpy(currBytes, frameCache.at(currentPosition).payload().data(), frameCache.at(currentPosition).payload().length());
            memcpy(refBytes, currBytes, 8);

        }
        if (needRefresh)
        {
            for (int k = 0; k < dataLen; k++)
            {
                if (graphRef[k] && graphRef[k]->data())
                    graphRef[k]->addData(newX[k], newY[k]);
            }
            ui->graphView->replot();
            updateDataView();
            if (ui->cbSync->checkState() == Qt::Checked) emit sendCenterTimeID(frameCache[currentPosition].frameId(), frameCache[currentPosition].timeStamp().microSeconds() / 1000000.0);
        }
    }
    updateFrameLabel();
}

void FlowViewWindow::removeAllGraphs()
{
  ui->graphView->clearGraphs();
  ui->graphView->replot();
}

void FlowViewWindow::createGraph(int byteNum)
{
    int tempVal;
    double minval = 1000000.0, maxval = -100000.0;
    const unsigned char *data;
    const CANFrame *frame;

    qDebug() << "Create Graph " << byteNum;

    bool graphByTime = ui->cbTimeGraph->isChecked();

    int numEntries = frameCache.count();

    x[byteNum].clear();
    y[byteNum].clear();
    x[byteNum].resize(numEntries);
    y[byteNum].resize(numEntries);

    for (int j = 0; j < numEntries; j++)
    {
        frame = &frameCache[j];
        data = reinterpret_cast<const unsigned char *>(frame->payload().constData());
        if (byteNum < frameCache[j].payload().length())
            tempVal = data[byteNum];
        else
            tempVal = 0;

        if (graphByTime)
        {
            if (secondsMode){
                x[byteNum][j] = frame->timeStamp().microSeconds() / 1000000.0;
            }
            else
            {
                x[byteNum][j] = frame->timeStamp().microSeconds();
            }
        }
        else
        {
            x[byteNum][j] = j;
        }

        y[byteNum][j] = tempVal;
        if (y[byteNum][j] < minval) minval = y[byteNum][j];
        if (y[byteNum][j] > maxval) maxval = y[byteNum][j];
    }

    graphRef[byteNum] = ui->graphView->addGraph();
    ui->graphView->graph()->setName(QString("Graph %1").arg(ui->graphView->graphCount()-1));
    ui->graphView->graph()->setData(x[byteNum],y[byteNum]);
    ui->graphView->graph()->setLineStyle(QCPGraph::lsLine); //connect points with lines
    QPen graphPen;
    graphPen.setColor(graphColors[byteNum]);
    graphPen.setWidth(1);
    ui->graphView->graph()->setPen(graphPen);
    ui->graphView->axisRect()->setupFullAxesBox();
}

void FlowViewWindow::refreshIDList()
{
    int id;
    for (int i = 0; i < modelFrames->count(); i++)
    {
        CANFrame thisFrame = modelFrames->at(i);
        id = thisFrame.frameId();
        if (!foundID.contains(id))
        {
            foundID.append(id);
            FilterUtility::createFilterItem(id, ui->listFrameID);
        }
    }
    //default is to sort in ascending order
    ui->listFrameID->sortItems();
}

void FlowViewWindow::updateFrameLabel()
{
    ui->lblNumFrames->setText(QString::number(currentPosition) + tr(" of ") + QString::number(frameCache.count()));
}

void FlowViewWindow::changeID(QString newID)
{
    qDebug() << "change id " << newID;
    //parse the ID and then load up the frame cache with just messages with that ID.
    uint32_t id = (uint32_t)Utility::ParseStringToNum(newID);
    frameCache.clear();

    if (modelFrames->count() == 0) return;

    playbackTimer->stop();
    playbackActive = false;
    for (int x = 0; x < modelFrames->count(); x++)
    {
        CANFrame thisFrame = modelFrames->at(x);
        if (thisFrame.frameId() == id)
        {
            thisFrame.payload().clear();
            frameCache.append(thisFrame);
        }
    }
    currentPosition = 0;

    if (frameCache.count() == 0) return;

    removeAllGraphs();
    //for (uint32_t c = 0; c < frameCache.at(0).len; c++)
    for (uint32_t c = 0; c < 8; c++)
    {
        createGraph(c);
    }

    updateGraphLocation();

    memset(currBytes, 0, 8);
    memcpy(currBytes, frameCache.at(currentPosition).payload().constData(), frameCache.at(currentPosition).payload().length());
    memcpy(refBytes, currBytes, 8);

    updateDataView();
    ui->check_0->setChecked(true);
    ui->check_1->setChecked(true);
    ui->check_2->setChecked(true);
    ui->check_3->setChecked(true);
    ui->check_4->setChecked(true);
    ui->check_5->setChecked(true);
    ui->check_6->setChecked(true);
    ui->check_7->setChecked(true);
}

void FlowViewWindow::btnBackOneClick()
{
    ui->cbLiveMode->setChecked(false);
    playbackTimer->stop(); //pushing this button halts automatic playback
    playbackActive = false;

    updatePosition(false);
    updateDataView();
}

void FlowViewWindow::btnPauseClick()
{
    ui->cbLiveMode->setChecked(false);
    playbackActive = false;
    playbackTimer->stop();
    updateDataView();
}

void FlowViewWindow::btnReverseClick()
{
    ui->cbLiveMode->setChecked(false);
    playbackActive = true;
    playbackForward = false;
    playbackTimer->start();
}

void FlowViewWindow::btnStopClick()
{
    ui->cbLiveMode->setChecked(false);
    playbackTimer->stop(); //pushing this button halts automatic playback
    playbackActive = false;
    currentPosition = 0;

    memset(currBytes, 0, 8);
    memcpy(currBytes, frameCache.at(currentPosition).payload().constData(), frameCache.at(currentPosition).payload().length());
    memcpy(refBytes, currBytes, 8);

    updateFrameLabel();
    updateDataView();
}

void FlowViewWindow::btnPlayClick()
{
    ui->cbLiveMode->setChecked(false);
    playbackActive = true;
    playbackForward = true;
    playbackTimer->start();
}

void FlowViewWindow::btnFwdOneClick()
{
    ui->cbLiveMode->setChecked(false);
    playbackTimer->stop();
    playbackActive = false;
    updatePosition(true);
    updateDataView();
}

void FlowViewWindow::changePlaybackSpeed(int newSpeed)
{
    playbackTimer->setInterval(newSpeed);
}

void FlowViewWindow::changeLooping(bool check)
{
    Q_UNUSED(check);
}

void FlowViewWindow::timerTriggered()
{
    if (!playbackActive || (ui->cbLiveMode->checkState() == Qt::Checked))
    {
        playbackTimer->stop();
        return;
    }
    if (playbackForward)
    {
        updatePosition(true);
    }
    else
    {
        updatePosition(false);
    }

    updateDataView();

    if (!ui->cbLoopPlayback->isChecked())
    {
        if (currentPosition == 0) playbackActive = false;
        if (currentPosition == (frameCache.count() - 1)) playbackActive = false;
    }
}

void FlowViewWindow::updateDataView()
{

    ui->txtCurr1->setText(Utility::formatNumber((unsigned char)currBytes[0]));
    ui->txtCurr2->setText(Utility::formatNumber((unsigned char)currBytes[1]));
    ui->txtCurr3->setText(Utility::formatNumber((unsigned char)currBytes[2]));
    ui->txtCurr4->setText(Utility::formatNumber((unsigned char)currBytes[3]));
    ui->txtCurr5->setText(Utility::formatNumber((unsigned char)currBytes[4]));
    ui->txtCurr6->setText(Utility::formatNumber((unsigned char)currBytes[5]));
    ui->txtCurr7->setText(Utility::formatNumber((unsigned char)currBytes[6]));
    ui->txtCurr8->setText(Utility::formatNumber((unsigned char)currBytes[7]));

    ui->txtRef1->setText(Utility::formatNumber((unsigned char)refBytes[0]));
    ui->txtRef2->setText(Utility::formatNumber((unsigned char)refBytes[1]));
    ui->txtRef3->setText(Utility::formatNumber((unsigned char)refBytes[2]));
    ui->txtRef4->setText(Utility::formatNumber((unsigned char)refBytes[3]));
    ui->txtRef5->setText(Utility::formatNumber((unsigned char)refBytes[4]));
    ui->txtRef6->setText(Utility::formatNumber((unsigned char)refBytes[5]));
    ui->txtRef7->setText(Utility::formatNumber((unsigned char)refBytes[6]));
    ui->txtRef8->setText(Utility::formatNumber((unsigned char)refBytes[7]));

    ui->flowView->setReference(refBytes, false);
    ui->flowView->updateData(currBytes, true);

    ui->timelineSlider->setMaximum(frameCache.count() - 1);
    ui->timelineSlider->setValue(currentPosition);

    for (int i = 0; i < 8; i++)
    {
        if (currBytes[i] == triggerValues[i])
        {
            playbackActive = false;
            playbackTimer->stop();
            break; //once any trigger is hit we can stop looking
        }
    }

    updateGraphLocation();

    updateFrameLabel();

}

void FlowViewWindow::gotoFrame(int frame) {
    if (frameCache.count() >= frame) currentPosition = frame;
    else currentPosition = 0;

    if (ui->cbSync->checkState() == Qt::Checked) emit sendCenterTimeID(frameCache[currentPosition].frameId(), frameCache[currentPosition].timeStamp().microSeconds() / 1000000.0);
}

void FlowViewWindow::updatePosition(bool forward)
{

    if (forward)
    {
        if (currentPosition < (frameCache.count() - 1)) currentPosition++;
        else if (ui->cbLoopPlayback->isChecked()) currentPosition = 0;
    }
    else
    {
        if (currentPosition > 0) currentPosition--;
        else if (ui->cbLoopPlayback->isChecked()) currentPosition = frameCache.count() - 1;
    }

    if (ui->cbAutoRef->isChecked())
    {
        memcpy(refBytes, currBytes, 8);
    }

    //figure out which bits changed since the previous frame and then AND that with the trigger bits. If any bits
    //get through that then they're changed and a trigger so we stop playback at this frame.
    uint64_t changedBits = 0;
    uint8_t cngByte;
    for (int i = 0; i < frameCache.at(currentPosition).payload().length(); i++)
    {
        unsigned char thisByte = static_cast<unsigned char>(frameCache.at(currentPosition).payload().data()[i]);
        cngByte = currBytes[i] ^ thisByte;
        changedBits |= (uint64_t)cngByte << (8ull * i);
    }

    qDebug() << "ChangedBits: " << QString::number(changedBits, 16);
    qDebug() << "TriggerBits: " << QString::number(triggerBits, 16);
    changedBits &= triggerBits;
    qDebug() << "Final ChangedBits: " << QString::number(changedBits, 16);
    if (changedBits)
    {
        playbackActive = false;
        playbackTimer->stop();
    }

    memset(currBytes, 0, 8);
    memcpy(currBytes, frameCache.at(currentPosition).payload().constData(), frameCache.at(currentPosition).payload().length());

    if (ui->cbSync->checkState() == Qt::Checked) emit sendCenterTimeID(frameCache[currentPosition].frameId(), frameCache[currentPosition].timeStamp().microSeconds() / 1000000.0);
    ui->timelineSlider->setValue(currentPosition);
}

void FlowViewWindow::updateGraphLocation()
{
    if (frameCache.count() == 0) return;
    int start = currentPosition - ui->graphRangeSlider->value();
    if (start < 0) start = 0;
    int end = currentPosition + ui->graphRangeSlider->value();
    if (end >= frameCache.count()) end = frameCache.count() - 1;
    if (ui->cbTimeGraph->isChecked())
    {
        if (secondsMode)
        {
            ui->graphView->xAxis->setRange(frameCache[start].timeStamp().microSeconds() / 1000000.0, frameCache[end].timeStamp().microSeconds() / 1000000.0);
            /*
            ui->graphView->xAxis->setTickStep((frameCache[end].timeStamp().microSeconds() - frameCache[start].timeStamp().microSeconds())/ 3000000.0);
            ui->graphView->xAxis->setSubTickCount(0);
            ui->graphView->xAxis->setNumberFormat("f");
            ui->graphView->xAxis->setNumberPrecision(6);
            */
        }
        else
        {
            ui->graphView->xAxis->setRange(frameCache[start].timeStamp().microSeconds(), frameCache[end].timeStamp().microSeconds());
            /*
            ui->graphView->xAxis->setTickStep((frameCache[end].timeStamp().microSeconds() - frameCache[start].timeStamp().microSeconds())/ 3.0);
            ui->graphView->xAxis->setSubTickCount(0);
            ui->graphView->xAxis->setNumberFormat("f");
            ui->graphView->xAxis->setNumberPrecision(0); */
        }
    }
    else
    {
        ui->graphView->xAxis->setRange(start, end);
        //ui->graphView->xAxis->setTickStep(5.0);
        //ui->graphView->xAxis->setSubTickCount(0);
        ui->graphView->xAxis->setNumberFormat("gb");
    }

    ui->graphView->replot();
}

