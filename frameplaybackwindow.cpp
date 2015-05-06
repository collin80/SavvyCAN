#include "frameplaybackwindow.h"
#include "ui_frameplaybackwindow.h"

FramePlaybackWindow::FramePlaybackWindow(QList<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FramePlaybackWindow)
{
    ui->setupUi(this);

    modelFrames = frames;

    currentPosition = 0;

    refreshIDList();

    ui->comboCANBus->addItem(tr("None"));
    ui->comboCANBus->addItem(tr("0"));
    ui->comboCANBus->addItem(tr("1"));
    ui->comboCANBus->addItem(tr("From File"));

    updateFrameLabel();
}

FramePlaybackWindow::~FramePlaybackWindow()
{
    delete ui;
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
