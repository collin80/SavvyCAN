#include "frameinfowindow.h"
#include "ui_frameinfowindow.h"
#include "mainwindow.h"
#include <QtDebug>

FrameInfoWindow::FrameInfoWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FrameInfoWindow)
{
    ui->setupUi(this);
    modelFrames = frames;

    connect(ui->listFrameID, SIGNAL(currentTextChanged(QString)), this, SLOT(updateDetailsWindow(QString)));
    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
}

void FrameInfoWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    refreshIDList();
    if (ui->listFrameID->count() > 0)
    {
        updateDetailsWindow(ui->listFrameID->item(0)->text());
        ui->listFrameID->setCurrentRow(0);
    }
}

FrameInfoWindow::~FrameInfoWindow()
{
    delete ui;
}

//remember, negative numbers are special -1 = all frames deleted, -2 = totally new set of frames.
void FrameInfoWindow::updatedFrames(int numFrames)
{
    if (numFrames == -1) //all frames deleted. Kill the display
    {
        ui->listFrameID->clear();
        ui->treeDetails->clear();
        refreshIDList();
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        refreshIDList();
        if (ui->listFrameID->count() > 0)
        {
            updateDetailsWindow(ui->listFrameID->item(0)->text());
            ui->listFrameID->setCurrentRow(0);
        }
    }
    else //just got some new frames. See if they are relevant.
    {
        int currID = ui->listFrameID->currentItem()->text().toInt(NULL, 16);
        bool foundID = false;
        for (int x = modelFrames->count() - numFrames; x < modelFrames->count(); x++)
        {
            if (currID == modelFrames->at(x).ID)
            {
                foundID = true;
                break;
            }
        }
        if (foundID)
        {
            //the problem here is that it'll blast us out of the details as soon as this
            //happens. The only way to do this properly is to actually traverse
            //the details structure and change the text. We don't do that yet.
            //so, the line is commented out. If people need to see the updated
            //data they can click another ID and back and it'll be OK

            //updateDetailsWindow(ui->listFrameID->currentItem()->text());
        }
    }
}

void FrameInfoWindow::updateDetailsWindow(QString newID)
{
    int idx, numFrames, targettedID;
    int minLen, maxLen, thisLen;
    int avgInterval;
    int minData[8];
    int maxData[8];
    int dataHistogram[256][8];
    int bitfieldHistogram[64];
    QTreeWidgetItem *baseNode, *dataBase, *histBase, *numBase, *tempItem;

    targettedID = newID.toInt(NULL, 16);

    if (modelFrames->count() == 0) return;

    qDebug() << "Started update details window with id " << targettedID;

    avgInterval = 0;

    if (targettedID > -1)
    {

        frameCache.clear();
        for (int i = 0; i < modelFrames->count(); i++)
        {
            CANFrame thisFrame = modelFrames->at(i);
            if (thisFrame.ID == targettedID) frameCache.append(thisFrame);
        }

        ui->treeDetails->clear();

        baseNode = new QTreeWidgetItem();
        baseNode->setText(0, QString("ID: 0x") + newID );

        if (frameCache[0].extended) //if these frames seem to be extended then try for J1939 decoding
        {
            J1939ID jid;
            jid.src = targettedID & 0xFF;
            jid.priority = targettedID >> 26;
            jid.pgn = (targettedID >> 8) & 0x3FFFF; //18 bits
            jid.pf = (targettedID >> 16) & 0xFF;
            jid.ps = (targettedID >> 8) & 0xFF;

            if (jid.pf > 0xEF)
            {
                jid.isBroadcast = true;
                jid.dest = 0xFFFF;
                tempItem = new QTreeWidgetItem();
                tempItem->setText(0, tr("Broadcast Frame"));
                baseNode->addChild(tempItem);
            }
            else
            {
                jid.dest = jid.ps;
                tempItem = new QTreeWidgetItem();
                tempItem->setText(0, tr("Destination ID: 0x") + QString::number(jid.dest,16));
                baseNode->addChild(tempItem);
            }
            tempItem = new QTreeWidgetItem();
            tempItem->setText(0, tr("SRC: 0x") + QString::number(jid.src,16));
            baseNode->addChild(tempItem);

            tempItem = new QTreeWidgetItem();
            tempItem->setText(0, tr("PGN: ") + QString::number(jid.pgn,10));
            baseNode->addChild(tempItem);

            tempItem = new QTreeWidgetItem();
            tempItem->setText(0, tr("PF: 0x") + QString::number(jid.pf,16));
            baseNode->addChild(tempItem);

            tempItem = new QTreeWidgetItem();
            tempItem->setText(0, tr("PS: 0x") + QString::number(jid.ps,16));
            baseNode->addChild(tempItem);
        }

        tempItem = new QTreeWidgetItem();
        tempItem->setText(0, tr("# of frames: ") + QString::number(frameCache.count(),10));
        baseNode->addChild(tempItem);

        //clear out all the counters and accumulators
        minLen = 8;
        maxLen = 0;
        for (int i = 0; i < 8; i++)
        {
            minData[i] = 256;
            maxData[i] = -1;
            for (int k = 0; k < 256; k++) dataHistogram[k][i] = 0;
        }
        for (int j = 0; j < 64; j++) bitfieldHistogram[j] = 0;

        //then find all data points
        for (int j = 0; j < frameCache.count(); j++)
        {
            if (j != 0) avgInterval += (frameCache[j].timestamp - frameCache[j-1].timestamp);
            thisLen = frameCache.at(j).len;
            if (thisLen > maxLen) maxLen = thisLen;
            if (thisLen < minLen) minLen = thisLen;
            for (int c = 0; c < thisLen; c++)
            {
                unsigned char dat = frameCache.at(j).data[c];
                if (minData[c] > dat) minData[c] = dat;
                if (maxData[c] < dat) maxData[c] = dat;
                dataHistogram[dat][c]++; //add one to count for this
                for (int l = 0; l < 8; l++)
                {
                    int bit =  dat & (1 << l);
                    if (bit == (1 << l))
                    {
                        bitfieldHistogram[c * 8 + l]++;
                    }
                }
            }
        }

        avgInterval = avgInterval / (frameCache.count() - 1);

        tempItem = new QTreeWidgetItem();

        if (minLen < maxLen)
            tempItem->setText(0, tr("Data Length: ") + QString::number(minLen) + tr(" to ") + QString::number(maxLen));
        else
            tempItem->setText(0, tr("Data Length: ") + QString::number(minLen));

        baseNode->addChild(tempItem);

        tempItem = new QTreeWidgetItem();
        tempItem->setText(0, tr("Average inter-frame interval: ") + QString::number(avgInterval) + "us");
        baseNode->addChild(tempItem);

        for (int c = 0; c < maxLen; c++)
        {
            dataBase = new QTreeWidgetItem();
            histBase = new QTreeWidgetItem();

            dataBase->setText(0, tr("Data Byte ") + QString::number(c));
            baseNode->addChild(dataBase);
            tempItem = new QTreeWidgetItem();
            tempItem->setText(0, tr("Range: ") + QString::number(minData[c]) + tr(" to ") + QString::number(maxData[c]));
            dataBase->addChild(tempItem);
            histBase->setText(0, tr("Histogram"));
            dataBase->addChild(histBase);

            for (int d = 0; d < 256; d++)
            {
                if (dataHistogram[d][c] > 0)
                {
                    tempItem = new QTreeWidgetItem();
                    tempItem->setText(0, QString::number(d) + "/0x" + QString::number(d, 16) +": " + QString::number(dataHistogram[d][c]));
                    histBase->addChild(tempItem);
                }
            }            
        }

        dataBase = new QTreeWidgetItem();
        dataBase->setText(0, tr("Bitfield Histogram"));
        for (int c = 0; c < 64; c++)
        {
            tempItem = new QTreeWidgetItem();
            tempItem->setText(0, QString::number(c) + " (Byte " + QString::number(c / 8) + " Bit "
                            + QString::number(c % 8) + ") :" + QString::number(bitfieldHistogram[c]));

            dataBase->addChild(tempItem);
        }
        baseNode->addChild(dataBase);

        ui->treeDetails->insertTopLevelItem(0, baseNode);
    }
    else
    {
    }
}

void FrameInfoWindow::refreshIDList()
{
    int id;
    for (int i = 0; i < modelFrames->count(); i++)
    {
        id = modelFrames->at(i).ID;
        if (!foundID.contains(id))
        {
            foundID.append(id);
            ui->listFrameID->addItem(QString::number(id, 16).toUpper().rightJustified(4,'0'));
        }
    }
    //default is to sort in ascending order
    ui->listFrameID->sortItems();
    ui->lblFrameID->setText(tr("Frame IDs: (") + QString::number(ui->listFrameID->count()) + tr(" unique ids)"));
}
