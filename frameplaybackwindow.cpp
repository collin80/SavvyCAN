#include "frameplaybackwindow.h"
#include "ui_frameplaybackwindow.h"
#include <QDebug>

FramePlaybackWindow::FramePlaybackWindow(QVector<CANFrame> *frames, SerialWorker *worker, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FramePlaybackWindow)
{
    ui->setupUi(this);

    //this code intentionally creates a copy here so that we
    //don't pick up more frames coming in as we're going.
    modelFrames = new QVector<CANFrame>(*frames);
    serialWorker = worker;

    playbackTimer = new QTimer();

    currentPosition = 0;
    playbackActive = false;
    playbackForward = true;
    whichBusSend = 0; //0 = no bus, 1 = bus 0, 2 = bus 1, 4 = from file - Bitfield so you can 'or' them.

    refreshIDList();

    ui->comboCANBus->addItem(tr("None"));
    ui->comboCANBus->addItem(tr("0"));
    ui->comboCANBus->addItem(tr("1"));
    ui->comboCANBus->addItem(tr("Both"));
    ui->comboCANBus->addItem(tr("From File"));

    updateFrameLabel();

    connect(ui->btnStepBack, SIGNAL(clicked(bool)), this, SLOT(btnBackOneClick()));
    connect(ui->btnPause, SIGNAL(clicked(bool)), this, SLOT(btnPauseClick()));
    connect(ui->btnPlayReverse, SIGNAL(clicked(bool)), this, SLOT(btnReverseClick()));
    connect(ui->btnStop, SIGNAL(clicked(bool)), this, SLOT(btnStopClick()));
    connect(ui->btnPlay, SIGNAL(clicked(bool)), this, SLOT(btnPlayClick()));
    connect(ui->btnStepForward, SIGNAL(clicked(bool)), this, SLOT(btnFwdOneClick()));
    connect(ui->btnSelectAll, SIGNAL(clicked(bool)), this, SLOT(btnSelectAllClick()));
    connect(ui->btnSelectNone, SIGNAL(clicked(bool)), this, SLOT(btnSelectNoneClick()));
    connect(ui->spinPlaySpeed, SIGNAL(valueChanged(int)), this, SLOT(changePlaybackSpeed(int)));
    connect(ui->cbLoop, SIGNAL(clicked(bool)), this, SLOT(changeLooping(bool)));
    connect(ui->comboCANBus, SIGNAL(currentIndexChanged(int)), this, SLOT(changeSendingBus(int)));
    connect(ui->listID, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(changeIDFiltering(QListWidgetItem*)));
    connect(playbackTimer, SIGNAL(timeout()), this, SLOT(timerTriggered()));

    playbackTimer->setInterval(ui->spinPlaySpeed->value()); //set the timer to the default value of the control

    connect(this, SIGNAL(sendCANFrame(const CANFrame*,int)), worker, SLOT(sendFrame(const CANFrame*,int)), Qt::QueuedConnection);

}

FramePlaybackWindow::~FramePlaybackWindow()
{
    delete ui;

    playbackTimer->stop();
    delete playbackTimer;
}

void FramePlaybackWindow::refreshIDList()
{
    int id;
    for (int i = 0; i < modelFrames->count(); i++)
    {
        id = modelFrames->at(i).ID;
        if (!foundID.contains(id))
        {
            foundID.append(id);
            QListWidgetItem* item = new QListWidgetItem(QString::number(id, 16).toUpper().rightJustified(4,'0'), ui->listID);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable); // set checkable flag
            item->setCheckState(Qt::Checked);
        }
    }
    //default is to sort in ascending order
    ui->listID->sortItems();
}

void FramePlaybackWindow::updateFrameLabel()
{
    ui->lblPosition->setText(QString::number(currentPosition) + tr(" of ") + QString::number(modelFrames->count()));
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

}

void FramePlaybackWindow::changeSendingBus(int newIdx)
{
    //falls out neatly this way. 0 = no sending, 1 = bus 0, 2 = bus 1, 3 = both, 4 = from file
    //the index is exactly the same as the whichSendBus bitfield.
    whichBusSend = newIdx;
}

void FramePlaybackWindow::changeIDFiltering(QListWidgetItem *item)
{

}

void FramePlaybackWindow::btnSelectAllClick()
{
    for (int i = 0; i < ui->listID->count(); i++)
    {
        QListWidgetItem *item = ui->listID->item(i);
        item->setCheckState(Qt::Checked);
    }
}

void FramePlaybackWindow::btnSelectNoneClick()
{
    for (int i = 0; i < ui->listID->count(); i++)
    {
        QListWidgetItem *item = ui->listID->item(i);
        item->setCheckState(Qt::Unchecked);
    }
}

void FramePlaybackWindow::timerTriggered()
{
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

        if (!ui->cbLoop->isChecked())
        {
            if (currentPosition == 0) playbackActive = false;
            if (currentPosition == (modelFrames->count() - 1)) playbackActive = false;
        }
    }
}

void FramePlaybackWindow::updatePosition(bool forward)
{

    if (forward)
    {
        if (currentPosition < (modelFrames->count() - 1)) currentPosition++;
        else if (ui->cbLoop->isChecked()) currentPosition = 0;
    }
    else
    {
        if (currentPosition > 0) currentPosition--;
        else if (ui->cbLoop->isChecked()) currentPosition = modelFrames->count() - 1;
    }

    //only send frame out if its ID is checked in the list. Otherwise discard it.

    QList<QListWidgetItem *> idList = ui->listID->findItems(QString::number(modelFrames->at(currentPosition).ID, 16).toUpper().rightJustified(4,'0'), Qt::MatchExactly);
    if (idList.count() > 0)
    {
        if (idList.at(0)->checkState() == Qt::Checked)
        {
            //index 0 is none, 1 is Bus 0, 2 is bus 1, 3 is both, 4 is from file
            const CANFrame *thisFrame = &modelFrames->at(currentPosition);
            if (whichBusSend & 1) emit sendCANFrame(thisFrame, 0);
            if (whichBusSend & 2) emit sendCANFrame(thisFrame, 1);
            if (whichBusSend & 4) emit sendCANFrame(thisFrame, thisFrame->bus);
            //if (whichBusSend & 1) serialWorker->sendFrame(thisFrame, 0);
            //if (whichBusSend & 2) serialWorker->sendFrame(thisFrame, 1);
            //if (whichBusSend & 4) serialWorker->sendFrame(thisFrame, thisFrame->bus);
            updateFrameLabel();
        }
    }

}
