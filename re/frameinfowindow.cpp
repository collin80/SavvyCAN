#include "frameinfowindow.h"
#include "ui_frameinfowindow.h"
#include "mainwindow.h"
#include <QtDebug>

FrameInfoWindow::FrameInfoWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FrameInfoWindow)
{
    ui->setupUi(this);

    readSettings();

    modelFrames = frames;

    connect(ui->listFrameID, &QListWidget::currentTextChanged, this, &FrameInfoWindow::updateDetailsWindow);
    connect(MainWindow::getReference(), &MainWindow::framesUpdated, this, &FrameInfoWindow::updatedFrames);
    connect(ui->btnSave, &QAbstractButton::clicked, this, &FrameInfoWindow::saveDetails);


    ui->graphHistogram->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes |
                                    QCP::iSelectLegend | QCP::iSelectPlottables);

    ui->graphHistogram->xAxis->setRange(0, 63);
    ui->graphHistogram->yAxis->setRange(0, 100);
    ui->graphHistogram->axisRect()->setupFullAxesBox();

    ui->graphHistogram->xAxis->setLabel("Bits");
    ui->graphHistogram->yAxis->setLabel("Instances");

    ui->graphHistogram->legend->setVisible(false);

    if (useOpenGL)
    {
        ui->graphHistogram->setAntialiasedElements(QCP::aeAll);
        //ui->graphingView->setNoAntialiasingOnDrag(true);
        ui->graphHistogram->setOpenGl(true);
    }
    else
    {
        ui->graphHistogram->setOpenGl(false);
        ui->graphHistogram->setAntialiasedElements(QCP::aeNone);
    }

}

void FrameInfoWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    readSettings();
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

void FrameInfoWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
}

void FrameInfoWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("FrameInfo/WindowSize", QSize(794, 494)).toSize());
        move(settings.value("FrameInfo/WindowPos", QPoint(50, 50)).toPoint());
    }
    useOpenGL = settings.value("Main/UseOpenGL", false).toBool();
}

void FrameInfoWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("FrameInfo/WindowSize", size());
        settings.setValue("FrameInfo/WindowPos", pos());
    }
}

//remember, negative numbers are special -1 = all frames deleted, -2 = totally new set of frames.
void FrameInfoWindow::updatedFrames(int numFrames)
{
    if (numFrames == -1) //all frames deleted. Kill the display
    {
        //qDebug() << "Delete all frames in Info Window";
        ui->listFrameID->clear();
        ui->treeDetails->clear();
        foundID.clear();
        refreshIDList();
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        //qDebug() << "All new set of frames in Info Window";
        ui->listFrameID->clear();
        ui->treeDetails->clear();
        foundID.clear();
        refreshIDList();
        if (ui->listFrameID->count() > 0)
        {
            updateDetailsWindow(ui->listFrameID->item(0)->text());
            ui->listFrameID->setCurrentRow(0);
        }
    }
    else //just got some new frames. See if they are relevant.
    {
        //qDebug() << "Got frames in Info Window";
        if (numFrames > modelFrames->count()) return;

        unsigned int currID = 0;
        if (ui->listFrameID->currentItem())
            currID = (unsigned int)ui->listFrameID->currentItem()->text().toInt(NULL, 16);
        bool thisID = false;
        for (int x = modelFrames->count() - numFrames; x < modelFrames->count(); x++)
        {
            CANFrame thisFrame = modelFrames->at(x);
            unsigned int id = thisFrame.ID;
            if (!foundID.contains(id))
            {
                foundID.append(id);
                ui->listFrameID->addItem(Utility::formatCANID(id, thisFrame.extended));
            }

            if (currID == modelFrames->at(x).ID)
            {
                thisID = true;
                break;
            }
        }
        if (thisID)
        {
            //the problem here is that it'll blast us out of the details as soon as this
            //happens. The only way to do this properly is to actually traverse
            //the details structure and change the text. We don't do that yet.
            //so, the line is commented out. If people need to see the updated
            //data they can click another ID and back and it'll be OK

            //updateDetailsWindow(ui->listFrameID->currentItem()->text());
        }
        //default is to sort in ascending order
        ui->listFrameID->sortItems();
        ui->lblFrameID->setText(tr("Frame IDs: (") + QString::number(ui->listFrameID->count()) + tr(" unique ids)"));
    }
}

void FrameInfoWindow::updateDetailsWindow(QString newID)
{
    int targettedID;
    int minLen, maxLen, thisLen;
    int avgInterval;
    int minInterval;
    int maxInterval;
    int thisInterval;
    int minData[8];
    int maxData[8];
    int dataHistogram[256][8];
    int bitfieldHistogram[64];
    QVector<double> graphX, graphY;
    double maxY = -1000.0;
    uint8_t changedBits[8];
    uint8_t referenceBits[8];
    QTreeWidgetItem *baseNode, *dataBase, *histBase, *tempItem;

    targettedID = Utility::ParseStringToNum(newID);

    if (modelFrames->count() == 0) return;

    qDebug() << "Started update details window with id " << targettedID;

    avgInterval = 0;

    if (targettedID > -1)
    {

        frameCache.clear();
        for (int i = 0; i < modelFrames->count(); i++)
        {
            CANFrame thisFrame = modelFrames->at(i);
            if (thisFrame.ID == (unsigned int)targettedID) frameCache.append(thisFrame);
        }

        ui->treeDetails->clear();

        if (frameCache.count() == 0) return;

        baseNode = new QTreeWidgetItem();
        baseNode->setText(0, QString("ID: ") + newID );

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
                tempItem->setText(0, tr("Destination ID: ") + Utility::formatNumber(jid.dest));
                baseNode->addChild(tempItem);
            }
            tempItem = new QTreeWidgetItem();
            tempItem->setText(0, tr("SRC: ") + Utility::formatNumber(jid.src));
            baseNode->addChild(tempItem);

            tempItem = new QTreeWidgetItem();
            tempItem->setText(0, tr("PGN: ") + Utility::formatNumber(jid.pgn));
            baseNode->addChild(tempItem);

            tempItem = new QTreeWidgetItem();
            tempItem->setText(0, tr("PF: ") + Utility::formatNumber(jid.pf));
            baseNode->addChild(tempItem);

            tempItem = new QTreeWidgetItem();
            tempItem->setText(0, tr("PS: ") + Utility::formatNumber(jid.ps));
            baseNode->addChild(tempItem);
        }

        tempItem = new QTreeWidgetItem();
        tempItem->setText(0, tr("# of frames: ") + QString::number(frameCache.count(),10));
        baseNode->addChild(tempItem);

        //clear out all the counters and accumulators
        minLen = 8;
        maxLen = 0;
        minInterval = 0x7FFFFFFF;
        maxInterval = 0;
        for (int i = 0; i < 8; i++)
        {
            minData[i] = 256;
            maxData[i] = -1;
            for (int k = 0; k < 256; k++) dataHistogram[k][i] = 0;
        }
        for (int j = 0; j < 64; j++) bitfieldHistogram[j] = 0;

        for (int c = 0; c < 8; c++)
        {
            changedBits[c] = 0;
            referenceBits[c] = frameCache.at(0).data[c];
            qDebug() << referenceBits[c];
        }

        //then find all data points
        for (int j = 0; j < frameCache.count(); j++)
        {
            if (j != 0)
            {
                thisInterval = (frameCache[j].timestamp - frameCache[j-1].timestamp);
                if (thisInterval > maxInterval) maxInterval = thisInterval;
                if (thisInterval < minInterval) minInterval = thisInterval;
                avgInterval += thisInterval;
            }
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
                    int bit = dat & (1 << l);
                    if (bit == (1 << l))
                    {
                        bitfieldHistogram[c * 8 + l]++;
                    }
                }
                changedBits[c] |= referenceBits[c] ^ dat;
            }
        }

        if (frameCache.count() > 1)
            avgInterval = avgInterval / (frameCache.count() - 1);
        else avgInterval = 0;

        tempItem = new QTreeWidgetItem();

        if (minLen < maxLen)
            tempItem->setText(0, tr("Data Length: ") + QString::number(minLen) + tr(" to ") + QString::number(maxLen));
        else
            tempItem->setText(0, tr("Data Length: ") + QString::number(minLen));

        baseNode->addChild(tempItem);

        tempItem = new QTreeWidgetItem();
        tempItem->setText(0, tr("Average inter-frame interval: ") + QString::number(avgInterval / 1000.0f) + "ms");
        baseNode->addChild(tempItem);
        tempItem = new QTreeWidgetItem();
        tempItem->setText(0, tr("Minimum inter-frame interval: ") + QString::number(minInterval / 1000.0f) + "ms");
        baseNode->addChild(tempItem);
        tempItem = new QTreeWidgetItem();
        tempItem->setText(0, tr("Maximum inter-frame interval: ") + QString::number(maxInterval / 1000.0f) + "ms");
        baseNode->addChild(tempItem);
        tempItem = new QTreeWidgetItem();
        tempItem->setText(0, tr("Inter-frame interval variation: ") + QString::number((maxInterval - minInterval) / 1000.0f) + "ms");
        baseNode->addChild(tempItem);

        for (int c = 0; c < maxLen; c++)
        {
            dataBase = new QTreeWidgetItem();
            histBase = new QTreeWidgetItem();

            dataBase->setText(0, tr("Data Byte ") + QString::number(c));
            baseNode->addChild(dataBase);

            tempItem = new QTreeWidgetItem();
            QString builder;
            builder = tr("Changed bits: 0x") + QString::number(changedBits[c], 16) + "  (" + Utility::formatByteAsBinary(changedBits[c]) + ")";
            tempItem->setText(0, builder);
            dataBase->addChild(tempItem);

            tempItem = new QTreeWidgetItem();
            tempItem->setText(0, tr("Range: ") + Utility::formatNumber(minData[c]) + tr(" to ") + Utility::formatNumber(maxData[c]));
            dataBase->addChild(tempItem);
            histBase->setText(0, tr("Histogram"));
            dataBase->addChild(histBase);

            for (int d = 0; d < 256; d++)
            {
                if (dataHistogram[d][c] > 0)
                {
                    tempItem = new QTreeWidgetItem();
                    tempItem->setText(0, QString::number(d) + "/0x" + QString::number(d, 16) +" (" + Utility::formatByteAsBinary(d) +") -> " + QString::number(dataHistogram[d][c]));
                    histBase->addChild(tempItem);
                }
            }
        }

        dataBase = new QTreeWidgetItem();
        dataBase->setText(0, tr("Bitfield Histogram"));
        for (int c = 0; c < 8 * maxLen; c++)
        {
            tempItem = new QTreeWidgetItem();
            tempItem->setText(0, QString::number(c) + " (Byte " + QString::number(c / 8) + " Bit "
                            + QString::number(c % 8) + ") :" + QString::number(bitfieldHistogram[c]));

            dataBase->addChild(tempItem);
            graphX.append(c);
            graphY.append(bitfieldHistogram[c]);
            if (bitfieldHistogram[c] > maxY) maxY = bitfieldHistogram[c];
        }
        baseNode->addChild(dataBase);

        ui->treeDetails->insertTopLevelItem(0, baseNode);
        ui->graphHistogram->clearGraphs();
        ui->graphHistogram->addGraph();
        ui->graphHistogram->graph()->setData(graphX, graphY);
        ui->graphHistogram->graph()->setLineStyle(QCPGraph::lsStepLeft); //connect points with lines
        QBrush graphBrush;
        graphBrush.setColor(Qt::red);
        graphBrush.setStyle(Qt::SolidPattern);
        ui->graphHistogram->graph()->setPen(Qt::NoPen);
        ui->graphHistogram->graph()->setBrush(graphBrush);
        ui->graphHistogram->yAxis->setRange(0, maxY * 1.02);
        ui->graphHistogram->axisRect()->setupFullAxesBox();
        ui->graphHistogram->replot();
    }
    else
    {
    }

    QSettings settings;
    if (settings.value("InfoCompare/AutoExpand", false).toBool())
    {
        ui->treeDetails->expandAll();
    }
}

void FrameInfoWindow::refreshIDList()
{
    int id;
    for (int i = 0; i < modelFrames->count(); i++)
    {
        CANFrame thisFrame = modelFrames->at(i);
        id = (int)thisFrame.ID;
        if (!foundID.contains(id))
        {
            foundID.append(id);
            ui->listFrameID->addItem(Utility::formatCANID(id, thisFrame.extended));
        }
    }
    //default is to sort in ascending order
    ui->listFrameID->sortItems();
    ui->lblFrameID->setText(tr("Frame IDs: (") + QString::number(ui->listFrameID->count()) + tr(" unique ids)"));
}

void FrameInfoWindow::saveDetails()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("Text File (*.txt)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        if (!filename.contains('.')) filename += ".txt";
        if (dialog.selectedNameFilter() == filters[0])
        {
            QFile *outFile = new QFile(filename);

            if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
            {
                delete outFile;
                return;
            }

            //go through all IDs, recalculate the data, and then save it to file
            for (int i = 0; i < ui->listFrameID->count(); i++)
            {
                updateDetailsWindow(ui->listFrameID->item(i)->text());
                dumpNode(ui->treeDetails->invisibleRootItem(), outFile, 0);
                outFile->write("\n\n");
            }

            outFile->close();
            delete outFile;
        }
    }
}

void FrameInfoWindow::dumpNode(QTreeWidgetItem* item, QFile *file, int indent)
{
    for (int i = 0; i < indent; i++) file->write("\t");
    file->write(item->text(0).toUtf8());
    file->write("\n");
    for( int i = 0; i < item->childCount(); ++i )
        dumpNode( item->child(i), file, indent + 1 );
}


