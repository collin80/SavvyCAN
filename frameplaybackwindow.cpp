#include "frameplaybackwindow.h"
#include "mainwindow.h"
#include "ui_frameplaybackwindow.h"
#include <QDebug>
#include <QFileDialog>
#include <QMenu>
#include <QSettings>
#include <qevent.h>
#include <QScrollBar>
#include "connections/canconmanager.h"
#include "helpwindow.h"
#include "filterutility.h"

/*
 * Notes about new functionality:
 * The system has a sequence of files or captured data to play back. It starts at the first item in the list
 * and goes through all of the frames. When it gets to the end it sees if there is a loop count for this file.
 * If so it increments the loop count and goes back to the beginning. If not it goes to the next file in the list.
 * At the end of the list it either stops (if looping is off) or goes back to the first item in the list.
 * All items in the list have their own frame cache and filters
 *
 * So, to do this we'll have to track which item we're on and which frame within that item. As we go through
 * items it should highlight the row we're on
 *
*/

FramePlaybackWindow::FramePlaybackWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FramePlaybackWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    int numBuses = CANConManager::getInstance()->getNumBuses();
    for (int n = 0; n < numBuses; n++) ui->comboCANBus->addItem(QString::number(n));
    ui->comboCANBus->addItem(tr("All"));
    ui->comboCANBus->addItem(tr("From File"));
    ui->comboCANBus->setCurrentIndex(0);

    playbackObject.initialize();
    playbackObject.setNumBuses(numBuses);

    readSettings();

    modelFrames = frames;

    currentSeqItem = nullptr;
    currentSeqNum = -1;
    currentPosition = 0;
    forward = true;
    isPlaying = false;
    wantPlaying = false;
    haveIncomingTraffic = false;

    updateFrameLabel();

    connect(ui->btnStepBack, &QAbstractButton::clicked, this, &FramePlaybackWindow::btnBackOneClick);
    connect(ui->btnPause, &QAbstractButton::clicked, this, &FramePlaybackWindow::btnPauseClick);
    connect(ui->btnPlayReverse, &QAbstractButton::clicked, this, &FramePlaybackWindow::btnReverseClick);
    connect(ui->btnStop, &QAbstractButton::clicked, this, &FramePlaybackWindow::btnStopClick);
    connect(ui->btnPlay, &QAbstractButton::clicked, this, &FramePlaybackWindow::btnPlayClick);
    connect(ui->btnStepForward, &QAbstractButton::clicked, this, &FramePlaybackWindow::btnFwdOneClick);
    connect(ui->btnSelectAll, &QAbstractButton::clicked, this, &FramePlaybackWindow::btnSelectAllClick);
    connect(ui->btnSelectNone, &QAbstractButton::clicked, this, &FramePlaybackWindow::btnSelectNoneClick);
    connect(ui->btnDelete, &QAbstractButton::clicked, this, &FramePlaybackWindow::btnDeleteCurrSeq);
    connect(ui->spinPlaySpeed, SIGNAL(valueChanged(int)), this, SLOT(changePlaybackSpeed(int)));
    connect(ui->spinBurstSpeed, SIGNAL(valueChanged(int)), this, SLOT(changeBurstRate(int)));
    //connect(ui->cbLoop, SIGNAL(clicked(bool)), this, SLOT(changeLooping(bool)));
    connect(ui->comboCANBus, SIGNAL(currentIndexChanged(int)), this, SLOT(changeSendingBus(int)));
    connect(ui->listID, &QListWidget::itemChanged, this, &FramePlaybackWindow::changeIDFiltering);
    connect(ui->btnLoadFile, &QAbstractButton::clicked, this, &FramePlaybackWindow::btnLoadFile);
    connect(ui->btnLoadLive, &QAbstractButton::clicked, this, &FramePlaybackWindow::btnLoadLive);
    connect(ui->tblSequence, &QTableWidget::cellPressed, this, &FramePlaybackWindow::seqTableCellClicked);
    connect(ui->tblSequence, &QTableWidget::cellChanged, this, &FramePlaybackWindow::seqTableCellChanged);
    connect(ui->btnLoadFilters, &QAbstractButton::clicked, this, &FramePlaybackWindow::loadFilters);
    connect(ui->btnSaveFilters, &QAbstractButton::clicked, this, &FramePlaybackWindow::saveFilters);
    connect(ui->cbOriginalTiming, &QCheckBox::toggled, this, &FramePlaybackWindow::useOrigTimingClicked);
    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));

    connect(&playbackObject, &FramePlaybackObject::EndOfFrameCache, this, &FramePlaybackWindow::EndOfFrameCache);
    connect(&playbackObject, &FramePlaybackObject::statusUpdate, this, &FramePlaybackWindow::getStatusUpdate);

    ui->listID->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listID, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenuFilters(QPoint)));

    playbackObject.setPlaybackInterval(ui->spinPlaySpeed->value());

    // Prevent annoying accidental horizontal scrolling when filter list is populated with long interpreted message names
    ui->listID->horizontalScrollBar()->setEnabled(false);    

    QStringList headers;
    headers << "Source" << "Loops";
    ui->tblSequence->setColumnCount(2);
    ui->tblSequence->setColumnWidth(0, 260);
    ui->tblSequence->setColumnWidth(1, 80);
    ui->tblSequence->setHorizontalHeaderLabels(headers);
}

FramePlaybackWindow::~FramePlaybackWindow()
{
    delete ui;
}

void FramePlaybackWindow::showEvent(QShowEvent *)
{
    readSettings();
    installEventFilter(this);

    //any time the view is shown we'll go see how many buses are registered and redo the combobox to match
    int numBuses = CANConManager::getInstance()->getNumBuses();
    ui->comboCANBus->clear();
    for (int n = 0; n < numBuses; n++) ui->comboCANBus->addItem(QString::number(n));
    ui->comboCANBus->addItem(tr("All"));
    ui->comboCANBus->addItem(tr("From File"));
    ui->comboCANBus->setCurrentIndex(0);

    playbackObject.initialize();
    playbackObject.setNumBuses(numBuses);
}

void FramePlaybackWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    removeEventFilter(this);
    writeSettings();
}

bool FramePlaybackWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("playbackwindow.md");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void FramePlaybackWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("Playback/WindowSize", QSize(742, 606)).toSize());
        move(Utility::constrainedWindowPos(settings.value("Playback/WindowPos", QPoint(50, 50)).toPoint()));
    }
    if (settings.value("Playback/AutoLoop", false).toBool())
    {
        ui->cbLoop->setChecked(true);
    }
    ui->spinPlaySpeed->setValue(settings.value("Playback/DefSpeed", 5).toInt());
    ui->comboCANBus->setCurrentIndex(settings.value("Playback/SendingBus", 0).toInt());

    calculateWhichBus();
}

void FramePlaybackWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("Playback/WindowSize", size());
        settings.setValue("Playback/WindowPos", pos());
    }
}

void FramePlaybackWindow::contextMenuFilters(QPoint pos)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addAction(tr("Save filter definition to file"), this, SLOT(saveFilters()));
    menu->addAction(tr("Load filter definition from file"), this, SLOT(loadFilters()));
    menu->popup(ui->listID->mapToGlobal(pos));
}

void FramePlaybackWindow::saveFilters()
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
        if (dialog.selectedNameFilter() == filters[0])
        {
            QFile *outFile = new QFile(filename);

            if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
                return;

            for (int c = 0; c < ui->listID->count(); c++)
            {
                outFile->write(QString::number(FilterUtility::getId(ui->listID->item(c)).toInt(nullptr, 16), 16).toUtf8());
                outFile->putChar(',');
                if (ui->listID->item(c)->checkState() == Qt::Checked) outFile->putChar('T');
                else outFile->putChar('F');
                outFile->write("\n");
            }

            outFile->close();
            dialog.setDirectory(settings.value("Filters/LoadSaveDirectory", dialog.directory().path()).toString());
        }
    }

}

void FramePlaybackWindow::loadFilters()
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
        QFile *inFile = new QFile(filename);
        QByteArray line;
        int ID;
        bool checked = false;

        if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
            return;

        btnSelectNoneClick();

        while (!inFile->atEnd()) {
            line = inFile->readLine().simplified();
            if (line.length() > 2)
            {
                QList<QByteArray> tokens = line.split(',');
                ID = tokens[0].toInt(nullptr, 16);
                if (tokens[1].toUpper() == "T") checked = true;
                    else checked = false;
                if (checked)
                {
                    QHash<int,bool>::iterator it;
                    for (it = currentSeqItem->idFilters.begin(); it != currentSeqItem->idFilters.end(); ++it)
                    {
                        if (it.key() == ID)
                        {
                            it.value() = true;
                        }
                    }
                    for (int c = 0; c < ui->listID->count(); c++)
                    {
                        QListWidgetItem *item = ui->listID->item(c);
                        if (FilterUtility::getId(item).toInt(nullptr, 16) == ID)
                        {
                            item->setCheckState(Qt::Checked);
                        }
                    }
                }
            }
        }
        inFile->close();
        dialog.setDirectory(settings.value("Filters/LoadSaveDirectory", dialog.directory().path()).toString());
    }
}

void FramePlaybackWindow::updatedFrames(int numFrames)
{
    CANFrame thisFrame;

    if (numFrames == -1) //all frames deleted. Don't care
    {
    }
    else if (numFrames == -2) //all new set of frames. Don't care
    {
    }
    else //just got some new frames.
    {
        if (numFrames > 0)
        {
            haveIncomingTraffic = true;
            if (wantPlaying && !isPlaying)
            {
                isPlaying = true;
                updateFrameLabel();
                if (forward) playbackObject.startPlaybackForward();
                else playbackObject.startPlaybackBackward();
            }
        }
    }
}

void FramePlaybackWindow::refreshIDList()
{
    if (currentSeqNum < 0 || currentSeqItem == nullptr)
    {
        ui->listID->clear();
        return;
    }

    ui->tblSequence->setCurrentCell(currentSeqNum, 0);

    //this signal would otherwise be called constantly as we do this routine. Turn it off temporarily
    disconnect(ui->listID, &QListWidget::itemChanged, this, &FramePlaybackWindow::changeIDFiltering);

    ui->listID->clear();

    QHash<int, bool>::Iterator filterIter;
    for (filterIter = currentSeqItem->idFilters.begin(); filterIter != currentSeqItem->idFilters.end(); ++filterIter)
    {
        /*QListWidgetItem* listItem = */ FilterUtility::createCheckableFilterItem(filterIter.key(), filterIter.value(), ui->listID);
    }
    //default is to sort in ascending order
    ui->listID->sortItems();

    connect(ui->listID, &QListWidget::itemChanged, this, &FramePlaybackWindow::changeIDFiltering);
}

void FramePlaybackWindow::calculateWhichBus()
{
    int idx = ui->comboCANBus->currentIndex();
    int maxIdx = ui->comboCANBus->count() - 1;
    if (maxIdx == 0) maxIdx = 2;
    int out = idx;

    qDebug() << idx << "***" << maxIdx;

    if (idx == (maxIdx - 1) ) out = -1;
    if (idx == maxIdx) out = -2;
    if (idx < 0) out = 0;

    playbackObject.setSendingBus(out);
}

void FramePlaybackWindow::EndOfFrameCache()
{
    if (forward)
    {
        currentSeqNum++; //go forward in the sequence
        if (currentSeqNum == seqItems.count()) //are we at the end of the sequence?
        {
            //reset the loop figures for each sequence entry
            for (int i = 0; i < seqItems.count(); i++) seqItems[i].currentLoopCount = 0;
            currentSeqNum = 0;
            if (ui->cbLoop->isChecked()) //go back to beginning if we're looping the sequence
            {

            }
            else //not looping so stop playback entirely
            {
                isPlaying = false;
                wantPlaying = false;
                haveIncomingTraffic = false;
                playbackObject.stopPlayback();
            }
        }
        currentSeqItem = &seqItems[currentSeqNum];
        playbackObject.setSequenceObject(currentSeqItem);
        if (isPlaying) playbackObject.startPlaybackForward();
        ui->tblSequence->setCurrentCell(currentSeqNum, 0);
    }
    else
    {
        currentSeqNum--; //go backward in the sequence
        if (currentSeqNum == -1) //are we trying to go past the beginning?
        {
            //reset the loop figures for each sequence entry
            for (int i = 0; i < seqItems.count(); i++) seqItems[i].currentLoopCount = 0;
            currentSeqNum = seqItems.count() - 1;
            if (ui->cbLoop->isChecked()) //go back to the last sequence entry if we're looping
            {

            }
            else //not looping so stop playback entirely
            {
                isPlaying = false;
                wantPlaying = false;
                haveIncomingTraffic = false;
                playbackObject.stopPlayback();
            }
        }
        currentSeqItem = &seqItems[currentSeqNum];
        playbackObject.setSequenceObject(currentSeqItem);
        if (isPlaying) playbackObject.startPlaybackBackward();
        ui->tblSequence->setCurrentCell(currentSeqNum, 0);
    }
}

void FramePlaybackWindow::getStatusUpdate(int frameNum)
{
    currentPosition = frameNum;
    updateFrameLabel();
}

void FramePlaybackWindow::updateFrameLabel()
{
    int row = currentSeqNum;

    if (row != ui->tblSequence->currentRow())
    {
        ui->tblSequence->setCurrentCell(row, 0);
    }

    if (row == -1)
    {
        ui->lblCurrPlayback->setText("");
        ui->lblPosition->setText("");
        return;
    }

    ui->lblCurrPlayback->setText(currentSeqItem->filename);

    if (wantPlaying && !isPlaying)
        ui->lblPosition->setText(QString::number(currentPosition) + tr(" of ") + QString::number(seqItems[row].data.count()) + "  (WAITING)");
    else
        ui->lblPosition->setText(QString::number(currentPosition) + tr(" of ") + QString::number(seqItems[row].data.count()));
}

void FramePlaybackWindow::seqTableCellClicked(int row, int col)
{
    qDebug() << "Row: " << QString::number(row) << " Col: " << QString::number(col);
    if (currentSeqNum != row)
    {
        currentSeqNum = row;
        currentSeqItem = &seqItems[row];
        refreshIDList();
    }
}

void FramePlaybackWindow::seqTableCellChanged(int row,int col)
{
    if (col == 1) //don't save any changes to the first column (0)
    {
        seqItems[row].maxLoops = Utility::ParseStringToNum(ui->tblSequence->item(row, col)->text());
        if (seqItems[row].maxLoops < 1) seqItems[row].maxLoops = 1;
    }
}

void FramePlaybackWindow::fillIDHash(SequenceItem &item)
{
    int id;

    item.idFilters.clear();

    for (int i = 0; i < item.data.count(); i++)
    {
        id = item.data[i].frameId();
        if (!item.idFilters.contains(id))
        {
            item.idFilters.insert(id, true);
        }
    }
}

void FramePlaybackWindow::useOrigTimingClicked()
{
    if (ui->cbOriginalTiming->isChecked())
    {
        ui->spinBurstSpeed->setEnabled(false);
        ui->spinPlaySpeed->setEnabled(false);
        playbackObject.setUseOriginalTiming(true);
    }
    else
    {
        ui->spinBurstSpeed->setEnabled(true);
        ui->spinPlaySpeed->setEnabled(true);
        playbackObject.setUseOriginalTiming(false);
    }
}

void FramePlaybackWindow::btnDeleteCurrSeq()
{
    if (currentSeqNum == -1) return;

    playbackObject.stopPlayback();

    seqItems.removeAt(currentSeqNum);
    ui->tblSequence->removeRow(currentSeqNum);
    if (seqItems.count() > 0)
    {
        currentSeqNum = 0;
        currentSeqItem = &seqItems[currentSeqNum];
    }
    else
    {
        currentSeqNum = -1;
        currentSeqItem = nullptr;
    }
    refreshIDList();
    updateFrameLabel();
}

void FramePlaybackWindow::btnLoadFile()
{
    QString filename;
    SequenceItem item;

    if (FrameFileIO::loadFrameFile(filename, &item.data))
    {
        std::sort(item.data.begin(), item.data.end()); //sort by timestamp to be sure it's in order
        QStringList fileList = filename.split('/');
        item.filename = fileList[fileList.length() - 1];
        item.currentLoopCount = 0;
        item.maxLoops = 1;
        fillIDHash(item);
        if (ui->tblSequence->currentRow() == -1)
        {
            ui->tblSequence->setCurrentCell(0,0);
        }
        seqItems.append(item);
        int row = ui->tblSequence->rowCount();
        ui->tblSequence->insertRow(row);
        ui->tblSequence->setItem(row, 0, new QTableWidgetItem(item.filename));
        ui->tblSequence->setItem(row, 1, new QTableWidgetItem(QString::number(item.maxLoops)));
        qDebug() << currentSeqNum;
        if (currentSeqNum == -1)
        {
            currentSeqNum = 0;
            currentSeqItem = &seqItems[0];
            playbackObject.setSequenceObject(currentSeqItem);
        }
        refreshIDList();
        updateFrameLabel();
    }
}

void FramePlaybackWindow::btnLoadLive()
{
    SequenceItem item;
    item.filename = "<CAPTURED DATA>";
    item.currentLoopCount = 0;
    item.maxLoops = 1;
    item.data = QVector<CANFrame>(*modelFrames); //create a copy of the current frames from the main view
    std::sort(item.data.begin(), item.data.end()); //be sure it's all in time based order
    fillIDHash(item);
    if (ui->tblSequence->currentRow() == -1)
    {
        ui->tblSequence->setCurrentCell(0,0);
    }
    seqItems.append(item);
    int row = ui->tblSequence->rowCount();
    ui->tblSequence->insertRow(row);
    ui->tblSequence->setItem(row, 0, new QTableWidgetItem(item.filename));
    ui->tblSequence->setItem(row, 1, new QTableWidgetItem(QString::number(item.maxLoops)));
    if (currentSeqNum == -1)
    {
        currentSeqNum = 0;
        currentSeqItem = &seqItems[0];
        playbackObject.setSequenceObject(currentSeqItem);
    }
    refreshIDList();
    updateFrameLabel();
    btnStopClick();
}

void FramePlaybackWindow::btnBackOneClick()
{
    if (!checkNoSeqLoaded()) return;
    forward = false;
    isPlaying = false;
    wantPlaying = false;
    playbackObject.stepPlaybackBackward();
    updateFrameLabel();
}

void FramePlaybackWindow::btnPauseClick()
{
    isPlaying = false;
    wantPlaying = false;
    playbackObject.pausePlayback();
    updateFrameLabel();
}

void FramePlaybackWindow::btnReverseClick()
{
    if (!checkNoSeqLoaded()) return;
    forward = false;
    wantPlaying = true;
    if (!ui->ckWaitForTraffic->isChecked())
    {
        playbackObject.startPlaybackBackward();
        isPlaying = true;
    }
    updateFrameLabel();
}

void FramePlaybackWindow::btnStopClick()
{
    isPlaying = false;
    wantPlaying = false;
    haveIncomingTraffic = false;
    playbackObject.stopPlayback();
    if (seqItems.count() > 0)
    {
        currentSeqNum = 0;
        currentSeqItem = &seqItems[currentSeqNum];
    }
    else {
        currentSeqNum = -1;
        currentSeqItem = nullptr;
    }
    if (ui->tblSequence->rowCount() > 0)
    {
        ui->tblSequence->setCurrentCell(0, 0);
        refreshIDList();
    }
    updateFrameLabel();
}

void FramePlaybackWindow::btnPlayClick()
{
    if (!checkNoSeqLoaded()) return;
    forward = true;
    wantPlaying = true;
    if (!ui->ckWaitForTraffic->isChecked())
    {
        playbackObject.startPlaybackForward();
        isPlaying = true;
    }
    updateFrameLabel();
}

void FramePlaybackWindow::btnFwdOneClick()
{
    if (!checkNoSeqLoaded()) return;
    forward = true;
    isPlaying = false;
    wantPlaying = false;
    playbackObject.stepPlaybackForward();
    updateFrameLabel();
}

bool FramePlaybackWindow::checkNoSeqLoaded()
{
    if (seqItems.count() == 0)
    {
        QMessageBox::warning(this, "Warning", "Cannot begin playback until at\nleast one playback source is loaded.");
        return false;
    }
    return true;
}

void FramePlaybackWindow::changePlaybackSpeed(int newSpeed)
{
    playbackObject.setPlaybackInterval(newSpeed);
}

void FramePlaybackWindow::changeBurstRate(int burst)
{
    playbackObject.setPlaybackBurst(burst);
}

void FramePlaybackWindow::changeLooping(bool check)
{
    Q_UNUSED(check);
}

void FramePlaybackWindow::changeSendingBus(int newIdx)
{
    Q_UNUSED(newIdx);
    calculateWhichBus();
}

void FramePlaybackWindow::changeIDFiltering(QListWidgetItem *item)
{
    qDebug() << "Changed ID filter " << item->text() << " : " << item->checkState();
    int ID = FilterUtility::getIdAsInt(item);
    currentSeqItem->idFilters[ID] = (item->checkState() == Qt::Checked) ? true : false;
}

void FramePlaybackWindow::btnSelectAllClick()
{
    for (int i = 0; i < ui->listID->count(); i++)
    {
        QListWidgetItem *item = ui->listID->item(i);
        item->setCheckState(Qt::Checked);
        currentSeqItem->idFilters[Utility::ParseStringToNum(item->text())] = true;
    }
}

void FramePlaybackWindow::btnSelectNoneClick()
{
    for (int i = 0; i < ui->listID->count(); i++)
    {
        QListWidgetItem *item = ui->listID->item(i);
        item->setCheckState(Qt::Unchecked);
        currentSeqItem->idFilters[Utility::ParseStringToNum(item->text())] = false;
    }
}
