#include "flowviewwindow.h"
#include "ui_flowviewwindow.h"
#include "mainwindow.h"
#include "helpwindow.h"

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

    //ui->graphView->setInteractions();

    ui->graphView->xAxis->setRange(0, 8);
    ui->graphView->yAxis->setRange(-10, 265); //run range a bit outside possible number so they aren't plotted in a hard to see place
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
    ui->graphView->yAxis->setLabel("Value Axis");
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
    connect(ui->listFrameID, SIGNAL(currentTextChanged(QString)), this, SLOT(changeID(QString)));
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

    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));

    ui->graphView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->graphView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenuRequestGraph(QPoint)));
    ui->flowView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->flowView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenuRequestFlow(QPoint)));

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
        changeID(ui->listFrameID->item(0)->text());
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
}

void FlowViewWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("FlowView/WindowSize", QSize(1078, 621)).toSize());
        move(settings.value("FlowView/WindowPos", QPoint(50, 50)).toPoint());
    }

    if (settings.value("FlowView/AutoRef", false).toBool())
    {
        ui->cbAutoRef->setChecked(true);
    }

    if (settings.value("FlowView/UseTimestamp", false).toBool())
    {
        ui->cbTimeGraph->setChecked(true);
    }

    secondsMode = settings.value("Main/TimeSeconds", false).toBool();
    openGLMode = settings.value("Main/UseOpenGL", false).toBool();
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
            HelpWindow::getRef()->showHelp("flowview.html");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
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
    if (frameCache.count() > 0) id = frameCache[0].ID;
    if (secondsMode) emit sendCenterTimeID(id, coord);
    else emit sendCenterTimeID(id, coord / 1000000.0);
}

void FlowViewWindow::gotCenterTimeID(int32_t ID, double timestamp)
{
    uint64_t t_stamp;

    t_stamp = timestamp * 1000000l;

    qDebug() << "timestamp: " << t_stamp;

    changeID(QString::number(ID)); //to be sure we're focused on the proper ID

    for (int j = 0; j < ui->listFrameID->count(); j++)
    {
        int thisNum = Utility::ParseStringToNum(ui->listFrameID->item(j)->text());
        if (thisNum == ID)
        {
            ui->listFrameID->setCurrentRow(j);
            break;
        }
    }

    int bestIdx = -1;
    for (int i = 0; i < frameCache.count(); i++)
    {
        if (frameCache[i].timestamp > t_stamp)
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

        memcpy(currBytes, frameCache.at(currentPosition).data, 8);

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

    QStringList filters;
    filters.append(QString(tr("PDF Files (*.pdf)")));
    filters.append(QString(tr("PNG Files (*.png)")));
    filters.append(QString(tr("JPEG Files (*.jpg)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

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
    }
}

void FlowViewWindow::saveFileFlow()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("PNG Files (*.png)")));
    filters.append(QString(tr("JPEG Files (*.jpg)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

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
    }
}

void FlowViewWindow::updatedFrames(int numFrames)
{
    QVector<double>newX[8];
    QVector<double>newY[8];

    CANFrame thisFrame;
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
        if (frameCache.count() > 0) refID = frameCache[0].ID;
            else refID = 0;
        bool needRefresh = false;
        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);

            if (!foundID.contains(thisFrame.ID))
            {
                foundID.append(thisFrame.ID);
                /*QListWidgetItem* item =*/ new QListWidgetItem(Utility::formatCANID(thisFrame.ID, thisFrame.extended), ui->listFrameID);
            }

            if (thisFrame.ID == refID)
            {
                frameCache.append(thisFrame);

                for (int k = 0; k < 8; k++)
                {
                    if (ui->cbTimeGraph->isChecked())
                    {
                        if (secondsMode){
                            newX[k].append((double)(thisFrame.timestamp) / 1000000.0);
                        }
                        else
                        {
                            newX[k].append(thisFrame.timestamp);
                        }
                    }
                    else
                    {
                        newX[k].append(x[k].count());
                    }
                    newY[k].append(thisFrame.data[k]);
                    needRefresh = true;
                }
            }
        }
        if (ui->cbLiveMode->checkState() == Qt::Checked)
        {
            currentPosition = frameCache.count() - 1;
            memcpy(currBytes, frameCache.at(currentPosition).data, 8);
            memcpy(refBytes, currBytes, 8);

        }
        if (needRefresh)
        {
            for (int k = 0; k < 8; k++)
            {
                if (graphRef[k] && graphRef[k]->data())
                    graphRef[k]->addData(newX[k], newY[k]);
            }
            ui->graphView->replot();
            updateDataView();
            if (ui->cbSync->checkState() == Qt::Checked) emit sendCenterTimeID(frameCache[currentPosition].ID, frameCache[currentPosition].timestamp / 1000000.0);
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
    float minval=1000000, maxval = -100000;

    qDebug() << "Create Graph " << byteNum;

    bool graphByTime = ui->cbTimeGraph->isChecked();

    int numEntries = frameCache.count();

    x[byteNum].clear();
    y[byteNum].clear();
    x[byteNum].resize(numEntries);
    y[byteNum].resize(numEntries);

    for (int j = 0; j < numEntries; j++)
    {
        tempVal = frameCache[j].data[byteNum];

        if (graphByTime)
        {
            if (secondsMode){
                x[byteNum][j] = (double)(frameCache[j].timestamp) / 1000000.0;
            }
            else
            {
                x[byteNum][j] = frameCache[j].timestamp;
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
        id = thisFrame.ID;
        if (!foundID.contains(id))
        {
            foundID.append(id);
            /*QListWidgetItem* item = */ new QListWidgetItem(Utility::formatCANID(id, thisFrame.extended), ui->listFrameID);
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
        if (thisFrame.ID == id)
        {
            for (int j = thisFrame.len; j < 8; j++) thisFrame.data[j] = 0;
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

    memcpy(currBytes, frameCache.at(currentPosition).data, 8);
    memcpy(refBytes, currBytes, 8);

    updateDataView();
}

void FlowViewWindow::btnBackOneClick()
{
    playbackTimer->stop(); //pushing this button halts automatic playback
    playbackActive = false;

    updatePosition(false);
    updateDataView();
}

void FlowViewWindow::btnPauseClick()
{
    playbackActive = false;
    playbackTimer->stop();
    updateDataView();
}

void FlowViewWindow::btnReverseClick()
{
    playbackActive = true;
    playbackForward = false;
    playbackTimer->start();
}

void FlowViewWindow::btnStopClick()
{
    playbackTimer->stop(); //pushing this button halts automatic playback
    playbackActive = false;
    currentPosition = 0;

    memcpy(currBytes, frameCache.at(currentPosition).data, 8);
    memcpy(refBytes, currBytes, 8);

    updateFrameLabel();
    updateDataView();
}

void FlowViewWindow::btnPlayClick()
{
    playbackActive = true;
    playbackForward = true;
    playbackTimer->start();
}

void FlowViewWindow::btnFwdOneClick()
{
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

    ui->txtCurr1->setText(Utility::formatNumber(currBytes[0]));
    ui->txtCurr2->setText(Utility::formatNumber(currBytes[1]));
    ui->txtCurr3->setText(Utility::formatNumber(currBytes[2]));
    ui->txtCurr4->setText(Utility::formatNumber(currBytes[3]));
    ui->txtCurr5->setText(Utility::formatNumber(currBytes[4]));
    ui->txtCurr6->setText(Utility::formatNumber(currBytes[5]));
    ui->txtCurr7->setText(Utility::formatNumber(currBytes[6]));
    ui->txtCurr8->setText(Utility::formatNumber(currBytes[7]));

    ui->txtRef1->setText(Utility::formatNumber(refBytes[0]));
    ui->txtRef2->setText(Utility::formatNumber(refBytes[1]));
    ui->txtRef3->setText(Utility::formatNumber(refBytes[2]));
    ui->txtRef4->setText(Utility::formatNumber(refBytes[3]));
    ui->txtRef5->setText(Utility::formatNumber(refBytes[4]));
    ui->txtRef6->setText(Utility::formatNumber(refBytes[5]));
    ui->txtRef7->setText(Utility::formatNumber(refBytes[6]));
    ui->txtRef8->setText(Utility::formatNumber(refBytes[7]));

    ui->flowView->setReference(refBytes, false);
    ui->flowView->updateData(currBytes, true);

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

    memcpy(currBytes, frameCache.at(currentPosition).data, 8);

    if (ui->cbSync->checkState() == Qt::Checked) emit sendCenterTimeID(frameCache[currentPosition].ID, frameCache[currentPosition].timestamp / 1000000.0);
}

void FlowViewWindow::updateGraphLocation()
{
    if (frameCache.count() == 0) return;
    int start = currentPosition - 5;
    if (start < 0) start = 0;
    int end = currentPosition + 5;
    if (end >= frameCache.count()) end = frameCache.count() - 1;
    if (ui->cbTimeGraph->isChecked())
    {
        if (secondsMode)
        {
            ui->graphView->xAxis->setRange(frameCache[start].timestamp / 1000000.0, frameCache[end].timestamp / 1000000.0);
            /*
            ui->graphView->xAxis->setTickStep((frameCache[end].timestamp - frameCache[start].timestamp)/ 3000000.0);
            ui->graphView->xAxis->setSubTickCount(0);
            ui->graphView->xAxis->setNumberFormat("f");
            ui->graphView->xAxis->setNumberPrecision(6);
            */
        }
        else
        {
            ui->graphView->xAxis->setRange(frameCache[start].timestamp, frameCache[end].timestamp);
            /*
            ui->graphView->xAxis->setTickStep((frameCache[end].timestamp - frameCache[start].timestamp)/ 3.0);
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

