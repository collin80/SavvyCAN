#include "flowviewwindow.h"
#include "ui_flowviewwindow.h"

FlowViewWindow::FlowViewWindow(QList<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FlowViewWindow)
{
    ui->setupUi(this);

    modelFrames = frames;

    playbackTimer = new QTimer();

    currentPosition = 0;
    playbackActive = false;
    playbackForward = true;

    memset(refBytes, 0, 8);
    memset(currBytes, 0, 8);

    refreshIDList();

    updateFrameLabel();

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

    playbackTimer->setInterval(ui->spinPlayback->value()); //set the timer to the default value of the control

}

FlowViewWindow::~FlowViewWindow()
{
    delete ui;

    playbackTimer->stop();
    delete playbackTimer;
}

void FlowViewWindow::refreshIDList()
{
    int id;
    for (int i = 0; i < modelFrames->count(); i++)
    {
        id = modelFrames->at(i).ID;
        if (!foundID.contains(id))
        {
            foundID.append(id);
            QListWidgetItem* item = new QListWidgetItem(QString::number(id, 16).toUpper().rightJustified(4,'0'), ui->listFrameID);
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
    //parse the ID and then load up the frame cache with just messages with that ID.
    int id = newID.toInt(NULL, 16);
    frameCache.clear();
    playbackTimer->stop();
    playbackActive = false;
    for (int x = 0; x < modelFrames->count(); x++)
    {
        CANFrame thisFrame = modelFrames->at(x);
        if (thisFrame.ID == id)
        {
            frameCache.append(thisFrame);
        }
    }
    currentPosition = 0;

    updateFrameLabel();
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
    updateFrameLabel();
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

}

void FlowViewWindow::timerTriggered()
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

    updateDataView();

    if (!ui->cbLoopPlayback->isChecked())
    {
        if (currentPosition == 0) playbackActive = false;
        if (currentPosition == (frameCache.count() - 1)) playbackActive = false;
    }
}

void FlowViewWindow::updateDataView()
{
    ui->txtCurr1->setText(QString::number(currBytes[0], 16).toUpper().rightJustified(2, '0'));
    ui->txtCurr2->setText(QString::number(currBytes[1], 16).toUpper().rightJustified(2, '0'));
    ui->txtCurr3->setText(QString::number(currBytes[2], 16).toUpper().rightJustified(2, '0'));
    ui->txtCurr4->setText(QString::number(currBytes[3], 16).toUpper().rightJustified(2, '0'));
    ui->txtCurr5->setText(QString::number(currBytes[4], 16).toUpper().rightJustified(2, '0'));
    ui->txtCurr6->setText(QString::number(currBytes[5], 16).toUpper().rightJustified(2, '0'));
    ui->txtCurr7->setText(QString::number(currBytes[6], 16).toUpper().rightJustified(2, '0'));
    ui->txtCurr8->setText(QString::number(currBytes[7], 16).toUpper().rightJustified(2, '0'));

    ui->txtRef1->setText(QString::number(refBytes[0], 16).toUpper().rightJustified(2, '0'));
    ui->txtRef2->setText(QString::number(refBytes[1], 16).toUpper().rightJustified(2, '0'));
    ui->txtRef3->setText(QString::number(refBytes[2], 16).toUpper().rightJustified(2, '0'));
    ui->txtRef4->setText(QString::number(refBytes[3], 16).toUpper().rightJustified(2, '0'));
    ui->txtRef5->setText(QString::number(refBytes[4], 16).toUpper().rightJustified(2, '0'));
    ui->txtRef6->setText(QString::number(refBytes[5], 16).toUpper().rightJustified(2, '0'));
    ui->txtRef7->setText(QString::number(refBytes[6], 16).toUpper().rightJustified(2, '0'));
    ui->txtRef8->setText(QString::number(refBytes[7], 16).toUpper().rightJustified(2, '0'));

    ui->flowView->setReference(refBytes, false);
    ui->flowView->updateData(currBytes, true);

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

}


