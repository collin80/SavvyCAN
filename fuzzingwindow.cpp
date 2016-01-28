#include "fuzzingwindow.h"
#include "ui_fuzzingwindow.h"
#include "utility.h"
#include <QDebug>

FuzzingWindow::FuzzingWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FuzzingWindow)
{
    ui->setupUi(this);

    modelFrames = frames;

    fuzzTimer = new QTimer();

    connect(ui->btnStartStop, &QPushButton::clicked, this, &FuzzingWindow::toggleFuzzing);
    connect(ui->btnAllFilters, &QPushButton::clicked, this, &FuzzingWindow::setAllFilters);
    connect(ui->btnNoFilters, &QPushButton::clicked, this, &FuzzingWindow::clearAllFilters);
    connect(fuzzTimer, &QTimer::timeout, this, &FuzzingWindow::timerTriggered);
    connect(ui->spinTiming, SIGNAL(valueChanged(int)), this, SLOT(changePlaybackSpeed(int)));
    connect(ui->listID, &QListWidget::itemChanged, this, &FuzzingWindow::idListChanged);

    refreshIDList();

    currentlyFuzzing = false;
}

FuzzingWindow::~FuzzingWindow()
{
    delete ui;
}

void FuzzingWindow::changePlaybackSpeed(int newSpeed)
{
    fuzzTimer->setInterval(newSpeed);
}

void FuzzingWindow::timerTriggered()
{
    CANFrame thisFrame;
    sendingBuffer.clear();
    for (int count = 0; count < ui->spinBurst->value(); count++)
    {
        thisFrame.ID = currentID;
        for (int i = 0; i < 8; i++) thisFrame.data[i] = currentBytes[i];
        if (currentID > 0x7FF) thisFrame.extended = true;
        else thisFrame.extended = false;
        thisFrame.bus = 0; //hard coded for now. TODO: do not hard code
        thisFrame.len = 8;
        sendingBuffer.append(thisFrame);
        calcNextID();
        calcNextBitPattern();
        numSentFrames++;
    }
    emit sendFrameBatch(&sendingBuffer);
    ui->lblNumFrames->setText("# of sent frames: " + QString::number(numSentFrames));
}

void FuzzingWindow::clearAllFilters()
{
    for (int i = 0; i < ui->listID->count(); i++)
    {
        ui->listID->item(i)->setCheckState(Qt::Unchecked);
    }
}

void FuzzingWindow::setAllFilters()
{
    for (int i = 0; i < ui->listID->count(); i++)
    {
        ui->listID->item(i)->setCheckState(Qt::Checked);
    }
}

void FuzzingWindow::calcNextID()
{
    if (seqIDScan)
    {
        currentID++;
        if (rangeIDSelect)
        {
            if (currentID > endID) currentID = startID;
        }
        else //IDs by filter. So, select the first filter
        {
            if (currentID > selectedIDs.length()) currentID = 0;
        }
    }
    else //random IDs
    {
        if (rangeIDSelect)
        {
            int range = endID - startID;
            currentID = startID + qrand() % range;
        }
        else //IDs by filter so pick a random selected ID from the filter list
        {
            currentID = qrand() % selectedIDs.length();
        }
    }
}

void FuzzingWindow::calcNextBitPattern()
{
    //simple, quick random data for now
    for (int i = 0; i < 8; i++)
    {
        currentBytes[i] = qrand() % 256;
    }
}

void FuzzingWindow::toggleFuzzing()
{
    if (currentlyFuzzing) //stop it then
    {
        ui->btnStartStop->setText("Start Fuzzing");
        currentlyFuzzing = false;
        fuzzTimer->stop();
    }
    else //start it then
    {
        ui->btnStartStop->setText("Stop Fuzzing");
        currentlyFuzzing = true;

        startID = Utility::ParseStringToNum(ui->txtStartID->text());
        endID = Utility::ParseStringToNum(ui->txtEndID->text());

        seqIDScan = ui->rbSequentialID->isChecked();
        rangeIDSelect = ui->rbRangeIDSel->isChecked();
        seqBitSelect = ui->rbSequentialBits->isChecked();

        numSentFrames = 0;

        if (seqIDScan)
        {
            if (rangeIDSelect)
            {
                currentID = startID;
            }
            else //IDs by filter. So, select the first filter
            {
                currentID = 0;
            }
        }
        else //random IDs
        {
            if (rangeIDSelect)
            {
                int range = endID - startID;
                currentID = startID + qrand() % range;
            }
            else //IDs by filter so pick a random selected ID from the filter list
            {
                currentID = qrand() % selectedIDs.length();
            }
        }

        calcNextBitPattern();

        fuzzTimer->start();
    }
}

void FuzzingWindow::refreshIDList()
{
    ui->listID->clear();

    int id;
    for (int i = 0; i < modelFrames->count(); i++)
    {
        id = modelFrames->at(i).ID;
        if (!foundIDs.contains(id))
        {
            foundIDs.append(id);
            selectedIDs.append(id);
            QListWidgetItem *thisItem = new QListWidgetItem();
            thisItem->setText(Utility::formatNumber(id));
            thisItem->setFlags(thisItem->flags() | Qt::ItemIsUserCheckable);
            thisItem->setCheckState(Qt::Checked);
            ui->listID->addItem(thisItem);
        }
    }
    //default is to sort in ascending order
    ui->listID->sortItems();
}

void FuzzingWindow::idListChanged(QListWidgetItem *item)
{
    int id = Utility::ParseStringToNum(item->text());
    if (item->checkState() == Qt::Checked)
    {
        if (!selectedIDs.contains(id))
        {
            qDebug() << "adding " << id << " to list of selected IDs";
            selectedIDs.append(id);
        }
    }
    else
    {
        qDebug() << "removing " << id << " from the list of selected ids";
        selectedIDs.removeOne(id);
    }
}
