#include "filecomparatorwindow.h"
#include "ui_filecomparatorwindow.h"

#include <QSettings>

FileComparatorWindow::FileComparatorWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FileComparatorWindow)
{
    ui->setupUi(this);

    connect(ui->btnInterestedFile, SIGNAL(clicked(bool)), this, SLOT(loadInterestedFile()));
    connect(ui->btnLoadRefFile, SIGNAL(clicked(bool)), this, SLOT(loadReferenceFile()));
    connect(ui->btnSaveDetails, SIGNAL(clicked(bool)), this, SLOT(saveDetails()));
    connect(ui->btnClear, SIGNAL(clicked(bool)), this, SLOT(clearReference()));

    ui->lblFirstFile->setText("");
    ui->lblRefFrames->setText("0");
}

FileComparatorWindow::~FileComparatorWindow()
{
    delete ui;
}

void FileComparatorWindow::showEvent(QShowEvent *)
{
    readSettings();
}

void FileComparatorWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
}

void FileComparatorWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("FileComparator/WindowSize", QSize(720, 631)).toSize());
        move(settings.value("FileComparator/WindowPos", QPoint(50, 50)).toPoint());
    }
}

void FileComparatorWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("FileComparator/WindowSize", size());
        settings.setValue("FileComparator/WindowPos", pos());
    }
}

void FileComparatorWindow::loadInterestedFile()
{
    interestedFrames.clear();
    QString result = FrameFileIO::loadFrameFile(&interestedFrames);
    if (result.length() > 0)
    {
        ui->lblFirstFile->setText(result);
        interestedFilename = result;
        if (interestedFrames.count() > 0 && referenceFrames.count() > 0) calculateDetails();
    }
}

void FileComparatorWindow::loadReferenceFile()
{
    //secondFileFrames.clear();
    QString result = FrameFileIO::loadFrameFile(&referenceFrames);
    if (result.length() > 0)
    {
        ui->lblRefFrames->setText(QString::number(referenceFrames.length()));
        if (interestedFrames.count() > 0 && referenceFrames.count() > 0) calculateDetails();
    }
}

void FileComparatorWindow::clearReference()
{
    referenceFrames.clear();
    ui->treeDetails->clear();
}

void FileComparatorWindow::calculateDetails()
{
    QMap<int, FrameData> interestedIDs;
    QMap<int, FrameData> referenceIDs;
    QTreeWidgetItem *interestedOnlyBase, *referenceOnlyBase = NULL, *sharedBase, *bitmapBaseInterested, *bitmapBaseReference = NULL;
    QTreeWidgetItem *valuesBase, *detail, *sharedItem, *valuesInterested, *valuesReference = NULL;
    uint64_t tmp;

    bool uniqueInterested = ui->ckUniqueToInterested->isChecked();

    ui->treeDetails->clear();

    interestedOnlyBase = new QTreeWidgetItem();
    interestedOnlyBase->setText(0,"IDs found only in " + interestedFilename);
    if (!uniqueInterested)
    {
        referenceOnlyBase = new QTreeWidgetItem();
        referenceOnlyBase->setText(0, "IDs found only in reference frames");
    }
    sharedBase = new QTreeWidgetItem();
    sharedBase->setText(0,"IDs found in both places");

    //first we have to fill out the data structures to get ready to do the report
    for (int x = 0; x < interestedFrames.count(); x++)
    {
        CANFrame frame = interestedFrames.at(x);
        if (interestedIDs.contains(frame.ID)) //if we saw this ID before then add to the QList in there
        {
            for (int y = 0; y < frame.len; y++)
            {
                interestedIDs[frame.ID].values[y][frame.data[y]]++;
                tmp = frame.data[y];
                tmp = tmp << (8 * y);
                interestedIDs[frame.ID].bitmap |= tmp;
                //qDebug() << "bitmap: " << QString::number(interestedIDs[frame.ID].bitmap, 16);
            }
        }
        else //never seen this ID before so add one
        {
            FrameData *newData = new FrameData();
            newData->ID = frame.ID;
            newData->dataLen = frame.len;
            //it would be possible to implement a constructor for FrameData
            //that sets the bitmap and values to zero. That would be cleaner and better.
            newData->bitmap = 0;
            for (int x = 0; x < 8; x++)
            {
                for (int y = 0; y < 256; y++)
                {
                    newData->values[x][y] = 0;
                }
            }
            //memset(newData->values, 0, 256 * 8);
            for (int y = 0; y < frame.len; y++)
            {
                newData->values[y][frame.data[y]] = 1;
                tmp = frame.data[y];
                tmp = tmp << (8 * y);
                newData->bitmap |= tmp;
                //qDebug() << "bitmap: " << QString::number(newData->bitmap, 16);
            }
            interestedIDs.insert(frame.ID, *newData);
        }        
    }    

    for (int x = 0; x < referenceFrames.count(); x++)
    {
        CANFrame frame = referenceFrames.at(x);
        if (referenceIDs.contains(frame.ID)) //if we saw this ID before then add to the QList in there
        {
            for (int y = 0; y < frame.len; y++)
            {
                referenceIDs[frame.ID].values[y][frame.data[y]]++;
                tmp = frame.data[y];
                tmp = tmp << (8 * y);
                referenceIDs[frame.ID].bitmap |= tmp;
                //qDebug() << "bitmap: " << QString::number(referenceIDs[frame.ID].bitmap, 16);
            }
        }
        else //never seen this ID before so add one
        {
            FrameData *newData = new FrameData();
            newData->ID = frame.ID;
            newData->dataLen = frame.len;
            newData->bitmap = 0;
            for (int x = 0; x < 8; x++)
            {
                for (int y = 0; y < 256; y++)
                {
                    newData->values[x][y] = 0;
                }
            }
            //memset(newData->values, 0, 256 * 8);
            for (int y = 0; y < frame.len; y++)
            {
                newData->values[y][frame.data[y]] = 1;
                tmp = frame.data[y];
                tmp = tmp << (8 * y);
                newData->bitmap |= tmp;
                //qDebug() << "bitmap: " << QString::number(newData->bitmap, 16);
            }
            referenceIDs.insert(frame.ID, *newData);
        }
    }

    //now we iterate through the IDs within both files and see which are unique to one file and which
    //are shared
    bool interestedHadUnique = false;
    QMap<int, FrameData>::iterator i;
    for (i = interestedIDs.begin(); i != interestedIDs.end(); ++i)
    {
        int keyone = i.key();
        if (!referenceIDs.contains(keyone))
        {
            valuesBase = new QTreeWidgetItem();
            valuesBase->setText(0, QString::number(keyone, 16));
            interestedOnlyBase->addChild(valuesBase);
        }
        else //ID was in both files
        {
            interestedHadUnique = false;
            sharedItem = new QTreeWidgetItem();
            sharedItem->setText(0, Utility::formatHexNum(keyone));            
            //if the ID was in both files then we can use the data accumulated above in bitmap
            //and values to figure out what has changed between the two files

            FrameData interested = interestedIDs[keyone];
            FrameData reference = referenceIDs[keyone];

            bitmapBaseInterested = new QTreeWidgetItem();
            bitmapBaseInterested->setText(0, "Bits set only in " + interestedFilename);
            if (!uniqueInterested)
            {
                bitmapBaseReference = new QTreeWidgetItem();
                bitmapBaseReference->setText(0, "Bits set only in reference frames");
            }
            sharedItem->addChild(bitmapBaseInterested);
            if (!uniqueInterested) sharedItem->addChild(bitmapBaseReference);

            uint64_t interestedBits = interested.bitmap;
            uint64_t referenceBits = reference.bitmap;

            //first up, which bits were set in one file but not the other
            for (int b = 0; b < (8 * interested.dataLen); b++)
            {
                detail = new QTreeWidgetItem();
                detail->setText(0, QString::number(b) + " (" + QString::number(b / 8) + ":" + QString::number(b % 8) + ")");
                if ( (interestedBits & 1) && !(referenceBits & 1) )
                {
                    bitmapBaseInterested->addChild(detail);
                    interestedHadUnique = true;
                }
                else if ( !(interestedBits & 1) && (referenceBits & 1) )
                {
                    if (!uniqueInterested) bitmapBaseReference->addChild(detail);
                }
                //qDebug() << b << "  " << QString::number(interestedBits, 16) << "  " << QString::number(referenceBits, 16);
                interestedBits = interestedBits >> 1;
                referenceBits = referenceBits >> 1;
            }

            for (int i = 0; i < qMax(interested.dataLen, reference.dataLen); i++)
            {
                valuesBase = new QTreeWidgetItem();
                valuesBase->setText(0, "Byte " + QString::number(i));
                sharedItem->addChild(valuesBase);
                valuesInterested = new QTreeWidgetItem();
                valuesInterested->setText(0, "Values found only in " + interestedFilename);
                if (!uniqueInterested)
                {
                    valuesReference = new QTreeWidgetItem();
                    valuesReference->setText(0, "Values found only in reference frames");
                }
                valuesBase->addChild(valuesInterested);
                if (!uniqueInterested) valuesBase->addChild(valuesReference);
                for (int j = 0; j < 256; j++)
                {
                    detail = new QTreeWidgetItem();
                    detail->setText(0, Utility::formatHexNum(j));
                    if ((interested.values[i][j] > 0) && (reference.values[i][j] == 0) )
                    {
                        valuesInterested->addChild(detail);
                        interestedHadUnique = true;
                    }
                    if ((reference.values[i][j] > 0) && (interested.values[i][j] == 0) )
                    {
                        if (!uniqueInterested) valuesReference->addChild(detail);
                    }
                }
            }
            if (interestedHadUnique || !uniqueInterested) sharedBase->addChild(sharedItem);
        }
    }

    if (!uniqueInterested)
    {
        QMap<int, FrameData>::iterator itwo;
        for (itwo = referenceIDs.begin(); itwo != referenceIDs.end(); ++itwo)
        {
            int keytwo = itwo.key();
            if (!interestedIDs.contains(keytwo))
            {
                valuesBase = new QTreeWidgetItem();
                valuesBase->setText(0, Utility::formatHexNum(keytwo));
                referenceOnlyBase->addChild(valuesBase);
            }
        }
    }

    ui->treeDetails->addTopLevelItem(interestedOnlyBase);
    if (!uniqueInterested) ui->treeDetails->addTopLevelItem(referenceOnlyBase);
    ui->treeDetails->addTopLevelItem(sharedBase);

    //ui->treeDetails->setSortingEnabled(true);
    //ui->treeDetails->sortByColumn(0, Qt::AscendingOrder);

    QSettings settings;
    if (settings.value("InfoCompare/AutoExpand", false).toBool())
    {
        ui->treeDetails->expandAll();
    }
}

void FileComparatorWindow::saveDetails()
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
        QFile *outFile = new QFile(filename);

        if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
            return;

        QTreeWidget *tree = ui->treeDetails;


        QTreeWidgetItemIterator it(tree);
        while (*it) {
          QTreeWidgetItem *item = *it;
          QString itemText = item->text(0);
          while (item->parent())
          {
              outFile->write("   ");
              item = item->parent();
          }
          outFile->write(itemText.toUtf8() + "\n");
          ++it;
        }

        outFile->close();

    }
}

