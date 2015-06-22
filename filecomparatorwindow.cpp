#include "filecomparatorwindow.h"
#include "ui_filecomparatorwindow.h"

FileComparatorWindow::FileComparatorWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FileComparatorWindow)
{
    ui->setupUi(this);

    connect(ui->btnFirstFile, SIGNAL(clicked(bool)), this, SLOT(loadFirstFile()));
    connect(ui->btnSecondFile, SIGNAL(clicked(bool)), this, SLOT(loadSecondFile()));
    connect(ui->btnSaveDetails, SIGNAL(clicked(bool)), this, SLOT(saveDetails()));

    ui->lblFirstFile->setText("");
    ui->lblSecondFile->setText("");
}

FileComparatorWindow::~FileComparatorWindow()
{
    delete ui;
}

void FileComparatorWindow::loadFirstFile()
{
    firstFileFrames.clear();
    QString result = FrameFileIO::loadFrameFile(&firstFileFrames);
    if (result.length() > 0)
    {
        ui->lblFirstFile->setText(result);
        firstFilename = result;
        if (firstFileFrames.count() > 0 && secondFileFrames.count() > 0) calculateDetails();
    }
}

void FileComparatorWindow::loadSecondFile()
{
    secondFileFrames.clear();
    QString result = FrameFileIO::loadFrameFile(&secondFileFrames);
    if (result.length() > 0)
    {
        ui->lblSecondFile->setText(result);
        secondFilename = result;
        if (firstFileFrames.count() > 0 && secondFileFrames.count() > 0) calculateDetails();
    }
}

void FileComparatorWindow::calculateDetails()
{
    QHash<int, FrameData> firstFileIDs;
    QHash<int, FrameData> secondFileIDs;
    uint64_t shiftBase = 1; //stupid hack to ensure that 64 bit shifting is used.
    QTreeWidgetItem *firstOnlyBase, *secondOnlyBase, *sharedBase, *bitmapBaseFirst, *bitmapBaseSecond;
    QTreeWidgetItem *valuesBase, *detail, *sharedItem, *valuesFirst, *valuesSecond;

    int idx = 0;

    ui->treeDetails->clear();

    firstOnlyBase = new QTreeWidgetItem();
    firstOnlyBase->setText(0,"IDs found only in " + firstFilename);
    secondOnlyBase = new QTreeWidgetItem();
    secondOnlyBase->setText(0, "IDs found only in " + secondFilename);
    sharedBase = new QTreeWidgetItem();
    sharedBase->setText(0,"IDs found in both files");

    //first we have to fill out the data structures to get ready to do the report
    for (int x = 0; x < firstFileFrames.count(); x++)
    {
        CANFrame frame = firstFileFrames.at(x);
        if (firstFileIDs.contains(frame.ID)) //if we saw this ID before then add to the QList in there
        {
            for (int y = 0; y < frame.len; y++)
            {
                firstFileIDs[frame.ID].values[y][frame.data[y]]++;
                firstFileIDs[frame.ID].bitmap |= frame.data[y] << (8 * y);
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
            //C++ arrays take the form of a single memory block so zap the whole block at once
            memset(newData->values, 0, 256 * 8);
            for (int y = 0; y < frame.len; y++)
            {
                newData->values[y][frame.data[y]] = 1;
                newData->bitmap |= frame.data[y] << (8 * y);
            }
            firstFileIDs.insert(frame.ID, *newData);
        }        
    }

    for (int x = 0; x < secondFileFrames.count(); x++)
    {
        CANFrame frame = secondFileFrames.at(x);
        if (secondFileIDs.contains(frame.ID)) //if we saw this ID before then add to the QList in there
        {
            for (int y = 0; y < frame.len; y++)
            {
                secondFileIDs[frame.ID].values[y][frame.data[y]]++;
                secondFileIDs[frame.ID].bitmap |= frame.data[y] << (8 * y);
            }
        }
        else //never seen this ID before so add one
        {
            FrameData *newData = new FrameData();
            newData->ID = frame.ID;
            newData->dataLen = frame.len;
            newData->bitmap = 0;
            memset(newData->values, 0, 256 * 8);
            for (int y = 0; y < frame.len; y++)
            {
                newData->values[y][frame.data[y]] = 1;
                newData->bitmap |= frame.data[y] << (8 * y);
            }
            secondFileIDs.insert(frame.ID, *newData);
        }
    }

    //now we iterate through the IDs within both files and see which are unique to one file and which
    //are shared
    QHash<int, FrameData>::iterator i;
    for (i = firstFileIDs.begin(); i != firstFileIDs.end(); ++i)
    {
        int keyone = i.key();
        if (!secondFileIDs.contains(keyone))
        {
            valuesBase = new QTreeWidgetItem();
            valuesBase->setText(0, QString::number(keyone, 16));
            firstOnlyBase->addChild(valuesBase);
        }
        else //ID was in both files
        {
            sharedItem = new QTreeWidgetItem();
            sharedItem->setText(0, Utility::formatHexNum(keyone));
            sharedBase->addChild(sharedItem);
            //if the ID was in both files then we can use the data accumulated above in bitmap
            //and values to figure out what has changed between the two files

            FrameData first = firstFileIDs[keyone];
            FrameData second = secondFileIDs[keyone];

            bitmapBaseFirst = new QTreeWidgetItem();
            bitmapBaseFirst->setText(0, "Bits set only in " + firstFilename);
            bitmapBaseSecond = new QTreeWidgetItem();
            bitmapBaseSecond->setText(0, "Bits set only in " + secondFilename);
            sharedItem->addChild(bitmapBaseFirst);
            sharedItem->addChild(bitmapBaseSecond);

            //first up, which bits were set in one file but not the other
            for (int b = 0; b < 64; b++)
            {
                detail = new QTreeWidgetItem();
                detail->setText(0, QString::number(b) + " (" + QString::number(b / 8) + ":" + QString::number(b % 8) + ")");
                if ( (first.bitmap & (shiftBase<<b)) && !(second.bitmap & (shiftBase<<b)) )
                {
                    bitmapBaseFirst->addChild(detail);
                }
                if ( !(first.bitmap & (shiftBase<<b)) && (second.bitmap & (shiftBase<<b)) )
                {
                    bitmapBaseSecond->addChild(detail);
                }
            }

            for (int i = 0; i < qMax(first.dataLen, second.dataLen); i++)
            {
                valuesBase = new QTreeWidgetItem();
                valuesBase->setText(0, "Byte " + QString::number(i));
                sharedItem->addChild(valuesBase);
                valuesFirst = new QTreeWidgetItem();
                valuesFirst->setText(0, "Values found only in " + firstFilename);
                valuesSecond = new QTreeWidgetItem();
                valuesSecond->setText(0, "Values found only in " + secondFilename);
                valuesBase->addChild(valuesFirst);
                valuesBase->addChild(valuesSecond);
                for (int j = 0; j < 256; j++)
                {
                    detail = new QTreeWidgetItem();
                    detail->setText(0, Utility::formatHexNum(j));
                    if (first.values[i][j] > 0 && second.values[i][j] == 0)
                    {
                        valuesFirst->addChild(detail);
                    }
                    if (second.values[i][j] > 0 && first.values[i][j] == 0)
                    {
                        valuesSecond->addChild(detail);
                    }
                }
            }
        }
    }

    QHash<int, FrameData>::iterator itwo;
    for (itwo = secondFileIDs.begin(); itwo != secondFileIDs.end(); ++itwo)
    {
        int keytwo = itwo.key();
        if (!firstFileIDs.contains(keytwo))
        {
            valuesBase = new QTreeWidgetItem();
            valuesBase->setText(0, Utility::formatHexNum(keytwo));
            secondOnlyBase->addChild(valuesBase);
        }
    }

    ui->treeDetails->addTopLevelItem(firstOnlyBase);
    ui->treeDetails->addTopLevelItem(secondOnlyBase);
    ui->treeDetails->addTopLevelItem(sharedBase);

    ui->treeDetails->setSortingEnabled(true);
    ui->treeDetails->sortByColumn(0, Qt::AscendingOrder);

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

