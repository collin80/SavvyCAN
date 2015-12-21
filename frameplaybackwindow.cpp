#include "frameplaybackwindow.h"
#include "ui_frameplaybackwindow.h"
#include <QDebug>
#include <QFileDialog>
#include <QMenu>
#include <QSettings>

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

FramePlaybackWindow::FramePlaybackWindow(const QVector<CANFrame> *frames, SerialWorker *worker, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FramePlaybackWindow)
{
    ui->setupUi(this);

    ui->comboCANBus->addItem(tr("None"));
    ui->comboCANBus->addItem(tr("0"));
    ui->comboCANBus->addItem(tr("1"));
    ui->comboCANBus->addItem(tr("Both"));
    ui->comboCANBus->addItem(tr("From File"));

    readSettings();

    modelFrames = frames;
    serialWorker = worker;

    playbackTimer = new QTimer();

    currentPosition = 0;
    playbackActive = false;
    playbackForward = true;
    whichBusSend = 0; //0 = no bus, 1 = bus 0, 2 = bus 1, 4 = from file - Bitfield so you can 'or' them.
    currentSeqItem = NULL;
    currentSeqNum = -1;

    updateFrameLabel();

    connect(ui->btnStepBack, SIGNAL(clicked(bool)), this, SLOT(btnBackOneClick()));
    connect(ui->btnPause, SIGNAL(clicked(bool)), this, SLOT(btnPauseClick()));
    connect(ui->btnPlayReverse, SIGNAL(clicked(bool)), this, SLOT(btnReverseClick()));
    connect(ui->btnStop, SIGNAL(clicked(bool)), this, SLOT(btnStopClick()));
    connect(ui->btnPlay, SIGNAL(clicked(bool)), this, SLOT(btnPlayClick()));
    connect(ui->btnStepForward, SIGNAL(clicked(bool)), this, SLOT(btnFwdOneClick()));
    connect(ui->btnSelectAll, SIGNAL(clicked(bool)), this, SLOT(btnSelectAllClick()));
    connect(ui->btnSelectNone, SIGNAL(clicked(bool)), this, SLOT(btnSelectNoneClick()));
    connect(ui->btnDelete, SIGNAL(clicked(bool)), this, SLOT(btnDeleteCurrSeq()));
    connect(ui->spinPlaySpeed, SIGNAL(valueChanged(int)), this, SLOT(changePlaybackSpeed(int)));
    //connect(ui->cbLoop, SIGNAL(clicked(bool)), this, SLOT(changeLooping(bool)));
    connect(ui->comboCANBus, SIGNAL(currentIndexChanged(int)), this, SLOT(changeSendingBus(int)));
    connect(ui->listID, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(changeIDFiltering(QListWidgetItem*)));
    connect(playbackTimer, SIGNAL(timeout()), this, SLOT(timerTriggered()));
    connect(ui->btnLoadFile, SIGNAL(clicked(bool)), this, SLOT(btnLoadFile()));
    connect(ui->btnLoadLive, SIGNAL(clicked(bool)), this, SLOT(btnLoadLive()));
    connect(ui->tblSequence, SIGNAL(cellPressed(int,int)), this, SLOT(seqTableCellClicked(int,int)));
    connect(ui->tblSequence, SIGNAL(cellChanged(int,int)), this, SLOT(seqTableCellChanged(int,int)));

    ui->listID->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listID, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenuFilters(QPoint)));

    playbackTimer->setInterval(ui->spinPlaySpeed->value()); //set the timer to the default value of the control

    connect(this, SIGNAL(sendCANFrame(const CANFrame*,int)), worker, SLOT(sendFrame(const CANFrame*,int)), Qt::QueuedConnection);
    connect(this, SIGNAL(sendFrameBatch(const QList<CANFrame>*)), worker, SLOT(sendFrameBatch(const QList<CANFrame>*)), Qt::QueuedConnection);

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

    disconnect(serialWorker);

    playbackTimer->stop();
    delete playbackTimer;
}

void FramePlaybackWindow::showEvent(QShowEvent *)
{
    readSettings();
}

void FramePlaybackWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
}

void FramePlaybackWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("Playback/WindowSize", QSize(742, 606)).toSize());
        move(settings.value("Playback/WindowPos", QPoint(50, 50)).toPoint());
    }
    if (settings.value("Playback/AutoLoop", false).toBool())
    {
        ui->cbLoop->setChecked(true);
    }
    ui->spinPlaySpeed->setValue(settings.value("Playback/DefSpeed", 5).toInt());
    ui->comboCANBus->setCurrentIndex(settings.value("Playback/SendingBus", 4).toInt());
    whichBusSend = ui->comboCANBus->currentIndex();
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
        if (dialog.selectedNameFilter() == filters[0])
        {
            QFile *outFile = new QFile(filename);

            if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
                return;

            for (int c = 0; c < ui->listID->count(); c++)
            {
                outFile->write(QString::number(ui->listID->item(c)->text().toInt(NULL, 16), 16).toUtf8());
                outFile->putChar(',');
                if (ui->listID->item(c)->checkState() == Qt::Checked) outFile->putChar('T');
                else outFile->putChar('F');
                outFile->write("\n");
            }

            outFile->close();
        }
    }

}

void FramePlaybackWindow::loadFilters()
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
                ID = tokens[0].toInt(NULL, 16);
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
                        if (item->text().toInt(NULL, 16) == ID)
                        {
                            item->setCheckState(Qt::Checked);
                        }
                    }
                }
            }
        }
        inFile->close();
    }
}

void FramePlaybackWindow::refreshIDList()
{
    if (currentSeqNum < 0 || currentSeqItem == NULL) return;

    ui->tblSequence->setCurrentCell(currentSeqNum, 0);

    ui->listID->clear();

    QHash<int, bool>::Iterator filterIter;
    for (filterIter = currentSeqItem->idFilters.begin(); filterIter != currentSeqItem->idFilters.end(); ++filterIter)
    {
        QListWidgetItem* listItem = new QListWidgetItem(Utility::formatNumber(filterIter.key()), ui->listID);
        listItem->setFlags(listItem->flags() | Qt::ItemIsUserCheckable); // set checkable flag
        if (filterIter.value()) listItem->setCheckState(Qt::Checked);
        else listItem->setCheckState(Qt::Unchecked);
    }
    //default is to sort in ascending order
    ui->listID->sortItems();
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
        id = item.data[i].ID;
        if (!item.idFilters.contains(id))
        {
            item.idFilters.insert(id, true);
        }
    }
}

void FramePlaybackWindow::btnDeleteCurrSeq()
{
    if (currentSeqNum == -1) return;

    if (playbackActive)
    {
        playbackActive = false;
        playbackTimer->stop();
    }

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
        currentSeqItem = NULL;
    }
    updateFrameLabel();
}

void FramePlaybackWindow::btnLoadFile()
{
    QString filename;
    QFileDialog dialog(this);
    bool result = false;
    SequenceItem item;

    QStringList filters;
    filters.append(QString(tr("CRTD Logs (*.txt)")));
    filters.append(QString(tr("GVRET Logs (*.csv)")));
    filters.append(QString(tr("Generic ID/Data CSV (*.csv)")));
    filters.append(QString(tr("BusMaster Log (*.log)")));
    filters.append(QString(tr("Microchip Log (*.log)")));

    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];

        if (dialog.selectedNameFilter() == filters[0]) result = FrameFileIO::loadCRTDFile(filename, &item.data);
        if (dialog.selectedNameFilter() == filters[1]) result = FrameFileIO::loadNativeCSVFile(filename, &item.data);
        if (dialog.selectedNameFilter() == filters[2]) result = FrameFileIO::loadGenericCSVFile(filename, &item.data);
        if (dialog.selectedNameFilter() == filters[3]) result = FrameFileIO::loadLogFile(filename, &item.data);
        if (dialog.selectedNameFilter() == filters[4]) result = FrameFileIO::loadMicrochipFile(filename, &item.data);
        if (result)
        {
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
            }
            refreshIDList();
            updateFrameLabel();
        }
    }

}

void FramePlaybackWindow::btnLoadLive()
{
    SequenceItem item;
    item.filename = "<CAPTURED DATA>";
    item.currentLoopCount = 0;
    item.maxLoops = 1;
    item.data = QVector<CANFrame>(*modelFrames); //create a copy of the current frames from the main view
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
    }
    refreshIDList();
    updateFrameLabel();
    btnStopClick();
}

void FramePlaybackWindow::btnBackOneClick()
{
    playbackTimer->stop(); //pushing this button halts automatic playback
    playbackActive = false;

    updatePosition(false);
}

void FramePlaybackWindow::btnPauseClick()
{
    playbackActive = false;
    playbackTimer->stop();
}

void FramePlaybackWindow::btnReverseClick()
{
    playbackActive = true;
    playbackForward = false;
    playbackTimer->start();
}

void FramePlaybackWindow::btnStopClick()
{
    playbackTimer->stop(); //pushing this button halts automatic playback
    playbackActive = false;
    currentPosition = 0;
    if (seqItems.count() > 0)
    {
        currentSeqNum = 0;
        currentSeqItem = &seqItems[currentSeqNum];
    }
    else {
        currentSeqNum = -1;
        currentSeqItem = NULL;
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
    playbackActive = true;
    playbackForward = true;
    playbackTimer->start();
}

void FramePlaybackWindow::btnFwdOneClick()
{
    playbackTimer->stop();
    playbackActive = false;
    updatePosition(true);
}

void FramePlaybackWindow::changePlaybackSpeed(int newSpeed)
{
    playbackTimer->setInterval(newSpeed);
}

void FramePlaybackWindow::changeLooping(bool check)
{
    Q_UNUSED(check);
}

void FramePlaybackWindow::changeSendingBus(int newIdx)
{
    //falls out neatly this way. 0 = no sending, 1 = bus 0, 2 = bus 1, 3 = both, 4 = from file
    //the index is exactly the same as the whichSendBus bitfield.
    whichBusSend = newIdx;
}

void FramePlaybackWindow::changeIDFiltering(QListWidgetItem *item)
{
    qDebug() << "Changed ID filter";
    int ID = Utility::ParseStringToNum(item->text());
    currentSeqItem->idFilters[ID] = item->checkState();
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

void FramePlaybackWindow::timerTriggered()
{    
    sendingBuffer.clear();
    for (int count = 0; count < ui->spinBurstSpeed->value(); count++)
    {
        if (!playbackActive)
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
    }
    emit sendFrameBatch(&sendingBuffer);
}

void FramePlaybackWindow::updatePosition(bool forward)
{

    if (forward)
    {
        if (currentPosition < (currentSeqItem->data.count() - 1)) currentPosition++; //still in same file so keep going
        else //hit the end of the current file
        {
            qDebug() << "hit end of current sequence";
            currentSeqItem->currentLoopCount++;
            currentPosition = 0;
            if (currentSeqItem->currentLoopCount == currentSeqItem->maxLoops) //have we looped enough times?
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
                        playbackActive = false;
                        playbackTimer->stop();
                    }
                }
                currentSeqItem = &seqItems[currentSeqNum];
                ui->tblSequence->setCurrentCell(currentSeqNum, 0);
            }
        }
    }
    else
    {
        if (currentPosition > 0) currentPosition--;
        else //hit the beginning of the current sequence
        {
            qDebug() << "hit start of current sequence";
            currentSeqItem->currentLoopCount++;
            currentPosition = currentSeqItem->data.count() - 1;
            if (currentSeqItem->currentLoopCount == currentSeqItem->maxLoops) //have we looped enough times?
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
                        playbackActive = false;
                        playbackTimer->stop();
                    }
                }
                currentSeqItem = &seqItems[currentSeqNum];
                ui->tblSequence->setCurrentCell(currentSeqNum, 0);
            }
        }
    }

    //only send frame out if its ID is checked in the list. Otherwise discard it.
    CANFrame *thisFrame = &currentSeqItem->data[currentPosition];
    int originalBus = thisFrame->bus;
    if (currentSeqItem->idFilters.find(thisFrame->ID).value())
    {
        //index 0 is none, 1 is Bus 0, 2 is bus 1, 3 is both, 4 is from file
        if (whichBusSend & 4)
        {
            sendingBuffer.append(*thisFrame);
        }
        if (whichBusSend & 1)
        {
            thisFrame->bus = 0;
            sendingBuffer.append(*thisFrame);
        }
        if (whichBusSend & 2)
        {
            thisFrame->bus = 1;
            sendingBuffer.append(*thisFrame);
        }
        thisFrame->bus = originalBus;
        updateFrameLabel();
    }

}
