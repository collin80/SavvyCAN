#include "fuzzingwindow.h"
#include "ui_fuzzingwindow.h"
#include "utility.h"
#include <QDebug>
#include <QRandomGenerator>
#include "mainwindow.h"
#include "helpwindow.h"
#include "connections/canconmanager.h"
#include "filterutility.h"

FuzzingWindow::FuzzingWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FuzzingWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    modelFrames = frames;

    fuzzTimer = new QTimer();

    connect(ui->btnStartStop, &QPushButton::clicked, this, &FuzzingWindow::toggleFuzzing);
    connect(ui->btnAllFilters, &QPushButton::clicked, this, &FuzzingWindow::setAllFilters);
    connect(ui->btnNoFilters, &QPushButton::clicked, this, &FuzzingWindow::clearAllFilters);
    connect(fuzzTimer, &QTimer::timeout, this, &FuzzingWindow::timerTriggered);
    connect(ui->spinTiming, SIGNAL(valueChanged(int)), this, SLOT(changePlaybackSpeed(int)));
    connect(ui->listID, &QListWidget::itemChanged, this, &FuzzingWindow::idListChanged);
    connect(ui->spinBytes, SIGNAL(valueChanged(int)), this, SLOT(changedNumDataBytes(int)));
    connect(ui->bitfield, SIGNAL(gridClicked(int,int)), this, SLOT(bitfieldClicked(int,int)));
    connect(ui->txtByte0, &QLineEdit::returnPressed, this, [=](){changedDataByteText(0, ui->txtByte0->text());});
    connect(ui->txtByte1, &QLineEdit::returnPressed, this, [=](){changedDataByteText(1, ui->txtByte1->text());});
    connect(ui->txtByte2, &QLineEdit::returnPressed, this, [=](){changedDataByteText(2, ui->txtByte2->text());});
    connect(ui->txtByte3, &QLineEdit::returnPressed, this, [=](){changedDataByteText(3, ui->txtByte3->text());});
    connect(ui->txtByte4, &QLineEdit::returnPressed, this, [=](){changedDataByteText(4, ui->txtByte4->text());});
    connect(ui->txtByte5, &QLineEdit::returnPressed, this, [=](){changedDataByteText(5, ui->txtByte5->text());});
    connect(ui->txtByte6, &QLineEdit::returnPressed, this, [=](){changedDataByteText(6, ui->txtByte6->text());});
    connect(ui->txtByte7, &QLineEdit::returnPressed, this, [=](){changedDataByteText(7, ui->txtByte7->text());});


    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));

    refreshIDList();

    currentlyFuzzing = false;

    for (int j = 0; j < 64; j++) bitGrid[j] = 1;
    numBits = 64;
    bitAccum = 0;
    redrawGrid();

    fuzzTimer->setInterval(ui->spinTiming->value());

    int numBuses = CANConManager::getInstance()->getNumBuses();
    for (int n = 0; n < numBuses; n++) ui->cbBuses->addItem(QString::number(n));
    ui->cbBuses->addItem(tr("All"));

    // Prevent annoying accidental horizontal scrolling when filter list is populated with long interpreted message names
    ui->listID->horizontalScrollBar()->setEnabled(false);    

    installEventFilter(this);
}

FuzzingWindow::~FuzzingWindow()
{
    removeEventFilter(this);
    delete ui;
}

bool FuzzingWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("fuzzingwindow.md");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void FuzzingWindow::updatedFrames(int numFrames)
{
    int id;
    if (numFrames == -1) //all frames deleted. Kill the display
    {
        ui->listID->clear();
        foundIDs.clear();
        refreshIDList();
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        ui->listID->clear();
        foundIDs.clear();
        refreshIDList();
    }
    else //just got some new frames. See if they are relevant.
    {
        if (numFrames > modelFrames->count()) return;
        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            id = modelFrames->at(i).frameId();
            if (!foundIDs.contains(id))
            {
                foundIDs.append(id);
                selectedIDs.append(id);
                FilterUtility::createCheckableFilterItem(id, true, ui->listID);
            }
        }
    }
}

void FuzzingWindow::changePlaybackSpeed(int newSpeed)
{
    fuzzTimer->setInterval(newSpeed);
}

void FuzzingWindow::changedDataByteText(int which, QString valu)
{
    int startBit = which * 8;
    int byt = valu.toInt(nullptr, 16);

    for (int i = 0; i < 8; i++)
    {
        bitGrid[startBit + i] = (byt & (1 << i)) ? 2 : 0;
    }

    redrawGrid();
}

void FuzzingWindow::changedNumDataBytes(int newVal)
{
    qDebug() << "new num bytes: " << newVal;

    ui->txtByte0->setEnabled((newVal > 0) ? true : false);
    ui->txtByte1->setEnabled((newVal > 1) ? true : false);
    ui->txtByte2->setEnabled((newVal > 2) ? true : false);
    ui->txtByte3->setEnabled((newVal > 3) ? true : false);
    ui->txtByte4->setEnabled((newVal > 4) ? true : false);
    ui->txtByte5->setEnabled((newVal > 5) ? true : false);
    ui->txtByte6->setEnabled((newVal > 6) ? true : false);
    ui->txtByte7->setEnabled((newVal > 7) ? true : false);

    int byt;
    for (int i = 0; i < 64; i++)
    {
        byt = i / 8;
        if (byt >= newVal)
        {
            bitGrid[i] = 3;
        }
        else
        {
            if (bitGrid[i] == 3) bitGrid[i] = 1;
        }
    }

    redrawGrid();
}

void FuzzingWindow::timerTriggered()
{
    static uint64_t lastByteUpdate = 0;
    CANFrame thisFrame;
    sendingBuffer.clear();
    //Every 250ms update the text fields to show our progress and what's going on.
    if (QDateTime::currentMSecsSinceEpoch() - lastByteUpdate > 250)
    {
        ui->txtByte0->setText(QString::number(currentBytes[0], 16));
        ui->txtByte1->setText(QString::number(currentBytes[1], 16));
        ui->txtByte2->setText(QString::number(currentBytes[2], 16));
        ui->txtByte3->setText(QString::number(currentBytes[3], 16));
        ui->txtByte4->setText(QString::number(currentBytes[4], 16));
        ui->txtByte5->setText(QString::number(currentBytes[5], 16));
        ui->txtByte6->setText(QString::number(currentBytes[6], 16));
        ui->txtByte7->setText(QString::number(currentBytes[7], 16));
    }
    int buses = ui->cbBuses->currentIndex();
    for (int count = 0; count < ui->spinBurst->value(); count++)
    {
        thisFrame.setFrameId(currentID);
        QByteArray bytes(ui->spinBytes->value(), 0);
        for (int i = 0; i < bytes.length(); i++) bytes[i] = currentBytes[i];
        thisFrame.setPayload(bytes);
        if (currentID > 0x7FF) thisFrame.setExtendedFrameFormat(true);
        else thisFrame.setExtendedFrameFormat(false);
        thisFrame.bus = 0; //hard coded for now. TODO: do not hard code        

        if (buses < (ui->cbBuses->count() - 1))
        {
            thisFrame.bus = buses;
            sendingBuffer.append(thisFrame);
        }
        else //fuzz all the buses! HACK THE PLANET! Er, something...
        {
            for (int j = 0; j < ui->cbBuses->count() - 1; j++)
            {
                thisFrame.bus = j;
                sendingBuffer.append(thisFrame);
            }
        }

        calcNextID();
        calcNextBitPattern();
        numSentFrames++;
    }
    CANConManager::getInstance()->sendFrames(sendingBuffer);
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
        if (rangeIDSelect)
        {
            currentID++;
            if (currentID > endID) currentID = startID;
        }
        else //IDs by filter. So, select the first filter
        {
            currentIdx++;
            if (currentIdx >= selectedIDs.length()) currentIdx = 0;
            currentID = selectedIDs[currentIdx];
            qDebug() << "idx id: " << currentID;
        }
    }
    else //random IDs
    {
        if (rangeIDSelect)
        {
            int range = endID - startID + 1;
            if (range != 1) currentID = startID + QRandomGenerator::global()->bounded(range);
            else currentID = startID;
        }
        else //IDs by filter so pick a random selected ID from the filter list
        {
            currentIdx = QRandomGenerator::global()->bounded(selectedIDs.length());
            currentID = selectedIDs[currentIdx];
        }
    }
}

void FuzzingWindow::calcNextBitPattern()
{
    uint64_t accum;

    switch (bitSequenceType)
    {
    case BitSequenceType::Random:
        int thisBit;
        for (int byt = 0; byt < ui->spinBytes->value(); byt++)
        {
            currentBytes[byt] = 0;
            for (int bit = 0; bit < 8; bit++)
            {
                thisBit = bitGrid[byt * 8 + bit];
                if (thisBit == 1)
                {
                    if ((QRandomGenerator::global()->bounded(2)) == 1) currentBytes[byt] |= (1 << bit);
                }
                if (thisBit == 2) currentBytes[byt] |= (1 << bit);
            }
        }
        break;
    case BitSequenceType::Sequential:
        bitAccum++;
        bitAccum &= ((1 << numBits) - 1);
        accum = bitAccum;
        for (int byt = 0; byt < ui->spinBytes->value(); byt++)
        {
            currentBytes[byt] = 0;
            for (int bit = 0; bit < 8; bit++)
            {
                thisBit = bitGrid[byt * 8 + bit];
                if (thisBit == 1)
                {
                    if (accum & 1) currentBytes[byt] |= (1 << bit);
                    accum >>= 1;
                }
                if (thisBit == 2) currentBytes[byt] |= (1 << bit);
            }
        }
        break;
    case BitSequenceType::Sweeping:
        qDebug() << "Start " << bitAccum;
        accum = bitAccum;

        int offset;
        for (int i = 1; i < 64; i++)
        {
            offset = (i + bitAccum) % 64;
            if (bitGrid[offset] == 1)
            {
                bitAccum = offset;
                qDebug() << "End " << bitAccum;
                break;
            }
        }

        for (int byt = 0; byt < ui->spinBytes->value(); byt++)
        {
            currentBytes[byt] = 0;
            for (int bit = 0; bit < 8; bit++)
            {
                thisBit = bitGrid[byt * 8 + bit];
                if ( (thisBit == 1) && (unsigned int)(byt * 8 + bit) == bitAccum)
                {
                    currentBytes[byt] |= (1 << bit);
                }
                if (thisBit == 2) currentBytes[byt] |= (1 << bit);
            }
        }
        break;
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
        if (ui->rbSequentialBits->isChecked()) bitSequenceType = BitSequenceType::Sequential;
        if (ui->rbRandomBits->isChecked())
        {
            bitSequenceType = BitSequenceType::Random;
            bitAccum = 0;
        }
        if (ui->rbSweep->isChecked())
        {
            bitSequenceType = BitSequenceType::Sweeping;
            for (int i = 0; i < 64; i++)
            {
                if (bitGrid[i] == 1)
                {
                    bitAccum = i;
                    break;
                }
            }
        }

        numSentFrames = 0;

        if (seqIDScan)
        {
            if (rangeIDSelect)
            {
                currentID = startID;
            }
            else //IDs by filter. So, select the first filter
            {
                currentIdx = 0;
                currentID = selectedIDs[currentIdx];
            }
        }
        else //random IDs
        {
            if (rangeIDSelect)
            {
                int range = endID - startID;
                if (range != 0) currentID = startID + QRandomGenerator::global()->bounded(range);
                else currentID = startID;
            }
            else //IDs by filter so pick a random selected ID from the filter list
            {
                currentIdx = QRandomGenerator::global()->bounded(selectedIDs.length());
                currentID = selectedIDs[currentIdx];
            }
        }

        calcNextBitPattern();

        fuzzTimer->start();
    }
}

void FuzzingWindow::refreshIDList()
{
    ui->listID->clear();
    foundIDs.clear();

    int id;
    for (int i = 0; i < modelFrames->count(); i++)
    {
        CANFrame thisFrame = modelFrames->at(i);
        id = thisFrame.frameId();
        if (!foundIDs.contains(id))
        {
            foundIDs.append(id);
            selectedIDs.append(id);
            FilterUtility::createCheckableFilterItem(id, true, ui->listID);
        }
    }
    //default is to sort in ascending order
    ui->listID->sortItems();
}

void FuzzingWindow::idListChanged(QListWidgetItem *item)
{
    int id = FilterUtility::getIdAsInt(item);
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

/*
bitGrid stores the state of all 64 bits.
The grid is capable of showing the following colors:
White = not used (left as 0)
Gray = past the end of the valid bits (because of # of data bytes requested)
Green = fuzz it
black = always keep it set to 1
*/
void FuzzingWindow::bitfieldClicked(int x, int y)
{
    qDebug() << "X: " << x << " Y: " << y;
    int bit = (7 - x) + (y * 8);
    if (bitGrid[bit] == 3) return; //naughty!
    bitGrid[bit]++;
    if (bitGrid[bit] > 2) bitGrid[bit] = 0;

    redrawGrid();
}

void FuzzingWindow::redrawGrid()
{
    //now update the bits in the bitfield control
    uint8_t refBytes[8];
    uint8_t dataBytes[8];
    uint8_t usedBytes[8];

    for (int j = 0; j < 8; j++)
    {
        refBytes[j] = 0;
        dataBytes[j] = 0;
        usedBytes[j] = 0;
    }

    numBits = 0;

    for (int i = 0; i < 64; i++)
    {
        int byt = i / 8;
        int bit = i % 8;
        switch (bitGrid[i])
        {
        case 0: //white, keep this bit off always
            break;
        case 1: //Green, fuzz this bit
            dataBytes[byt] |= (1 << bit);
            numBits++;
            break;
        case 2: //black, bit always set
            dataBytes[byt] |= (1 << bit);
            refBytes[byt] |= (1 << bit);
            break;
        case 3: //gray, this bit doesn't exist
            usedBytes[byt] |= (1 << bit);
            break;
        }
    }

    ui->bitfield->setUsed(usedBytes, false);
    ui->bitfield->setReference(refBytes, false);
    ui->bitfield->updateData(dataBytes, true);
}
