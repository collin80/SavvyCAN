#include "filecomparatorwindow.h"
#include "ui_filecomparatorwindow.h"
#include "helpwindow.h"
#include <QProgressDialog>
#include <QSettings>
#include <qevent.h>

FileComparatorWindow::FileComparatorWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FileComparatorWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    connect(ui->btnInterestedFile, SIGNAL(clicked(bool)), this, SLOT(loadInterestedFile()));
    connect(ui->btnLoadRefFile, SIGNAL(clicked(bool)), this, SLOT(loadReferenceFile()));
    connect(ui->btnSaveDetails, SIGNAL(clicked(bool)), this, SLOT(saveDetails()));
    connect(ui->btnClear, SIGNAL(clicked(bool)), this, SLOT(clearReference()));

    ui->lblFirstFile->setText("");
    ui->lblRefFrames->setText("Loaded frames: 0");

    installEventFilter(this);
}

FileComparatorWindow::~FileComparatorWindow()
{
    removeEventFilter(this);
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

bool FileComparatorWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("filecomparison.html");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
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
    QString resultingFileName;

    qApp->processEvents();

    if (FrameFileIO::loadFrameFile(resultingFileName, &interestedFrames))
    {
        ui->lblFirstFile->setText(resultingFileName);
        interestedFilename = resultingFileName;
        if (interestedFrames.count() > 0 && referenceFrames.count() > 0) calculateDetails();
    }

}

void FileComparatorWindow::loadReferenceFile()
{
    //secondFileFrames.clear();
    QString resultingFileName;

    qApp->processEvents();

    if (FrameFileIO::loadFrameFile(resultingFileName, &referenceFrames))
    {
        ui->lblRefFrames->setText("Loaded frames: " + QString::number(referenceFrames.length()));
        if (interestedFrames.count() > 0 && referenceFrames.count() > 0) calculateDetails();
    }
}

void FileComparatorWindow::clearReference()
{
    referenceFrames.clear();
    ui->treeDetails->clear();
    ui->lblRefFrames->setText("Loaded frames: " + QString::number(referenceFrames.length()));
}

void FileComparatorWindow::calculateDetails()
{
    QMap<int, FrameData> interestedIDs;
    QMap<int, FrameData> referenceIDs;
    QTreeWidgetItem *interestedOnlyBase, *referenceOnlyBase = nullptr, *sharedBase, *bitmapBaseInterested, *bitmapBaseReference = nullptr;
    QTreeWidgetItem *valuesBase, *detail, *sharedItem, *valuesInterested, *valuesReference = nullptr;
    uint64_t tmp;

    bool uniqueInterested = ui->ckUniqueToInterested->isChecked();

    QProgressDialog progress(this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setLabelText("Calculating differences");
    progress.setCancelButton(0);
    progress.setRange(0,0);
    progress.setMinimumDuration(0);
    progress.show();

    qApp->processEvents();

    ui->treeDetails->clear();

    interestedOnlyBase = new QTreeWidgetItem();
    interestedOnlyBase->setText(0,"IDs found only in " + interestedFilename);
    if (!uniqueInterested)
    {
        referenceOnlyBase = new QTreeWidgetItem();
        referenceOnlyBase->setText(0, "IDs found only in Side 2 - Reference frames");
    }
    sharedBase = new QTreeWidgetItem();
    sharedBase->setText(0,"IDs found on both sides");

    //first we have to fill out the data structures to get ready to do the report
    for (int x = 0; x < interestedFrames.count(); x++)
    {
        CANFrame frame = interestedFrames.at(x);
        if (interestedIDs.contains(frame.frameId())) //if we saw this ID before then add to the QList in there
        {
            for (unsigned int y = 0; y < frame.payload().length(); y++)
            {
                interestedIDs[frame.frameId()].values[y][frame.payload()[y]]++;
                tmp = frame.payload()[y];
                tmp = tmp << (8 * y);
                interestedIDs[frame.frameId()].bitmap |= tmp;
                //qDebug() << "bitmap: " << QString::number(interestedIDs[frame.frameId()].bitmap, 16);
            }
        }
        else //never seen this ID before so add one
        {
            FrameData *newData = new FrameData();
            newData->ID = frame.frameId();
            newData->dataLen = frame.payload().length();
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
            for (unsigned int y = 0; y < frame.payload().length(); y++)
            {
                newData->values[y][frame.payload()[y]] = 1;
                tmp = frame.payload()[y];
                tmp = tmp << (8 * y);
                newData->bitmap |= tmp;
                //qDebug() << "bitmap: " << QString::number(newData->bitmap, 16);
            }
            interestedIDs.insert(frame.frameId(), *newData);
        }
    }

    qApp->processEvents();

    for (int x = 0; x < referenceFrames.count(); x++)
    {
        CANFrame frame = referenceFrames.at(x);
        if (referenceIDs.contains(frame.frameId())) //if we saw this ID before then add to the QList in there
        {
            for (unsigned int y = 0; y < frame.payload().length(); y++)
            {
                referenceIDs[frame.frameId()].values[y][frame.payload()[y]]++;
                tmp = frame.payload()[y];
                tmp = tmp << (8 * y);
                referenceIDs[frame.frameId()].bitmap |= tmp;
                //qDebug() << "bitmap: " << QString::number(referenceIDs[frame.frameId()].bitmap, 16);
            }
        }
        else //never seen this ID before so add one
        {
            FrameData *newData = new FrameData();
            newData->ID = frame.frameId();
            newData->dataLen = frame.payload().length();
            newData->bitmap = 0;
            for (int x = 0; x < 8; x++)
            {
                for (int y = 0; y < 256; y++)
                {
                    newData->values[x][y] = 0;
                }
            }
            //memset(newData->values, 0, 256 * 8);
            for (unsigned int y = 0; y < frame.payload().length(); y++)
            {
                newData->values[y][frame.payload()[y]] = 1;
                tmp = frame.payload()[y];
                tmp = tmp << (8 * y);
                newData->bitmap |= tmp;
                //qDebug() << "bitmap: " << QString::number(newData->bitmap, 16);
            }
            referenceIDs.insert(frame.frameId(), *newData);
        }
    }

    qApp->processEvents();

    //now we iterate through the IDs within both files and see which are unique to one file and which
    //are shared
    bool interestedHadUnique = false;
    QMap<int, FrameData>::iterator i;
    int framesCounter = 0;
    for (i = interestedIDs.begin(); i != interestedIDs.end(); ++i)
    {
        framesCounter++;
        if (framesCounter > 10000)
        {
            framesCounter = 0;
            qApp->processEvents();
        }

        int keyone = i.key();
        if (!referenceIDs.contains(keyone))
        {
            valuesBase = new QTreeWidgetItem();
            valuesBase->setText(0, Utility::formatHexNum(keyone));
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
                bitmapBaseReference->setText(0, "Bits set only in Side 2 - Reference frames");
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
                    valuesReference->setText(0, "Values found only in Side 2 - Reference frames");
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

    qApp->processEvents();

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

    progress.cancel();
}

void FileComparatorWindow::saveDetails()
{
    QString filename;
    QFileDialog dialog(this);
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("Text File (*.txt)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDirectory(settings.value("FileComparator/LoadSaveDirectory", dialog.directory().path()).toString());

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        settings.setValue("FileComparator/LoadSaveDirectory", dialog.directory().path());
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

