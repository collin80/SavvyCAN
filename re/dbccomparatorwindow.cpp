#include "dbccomparatorwindow.h"
#include "ui_dbccomparatorwindow.h"
#include "helpwindow.h"
#include <QProgressDialog>
#include <QSettings>
#include <qevent.h>

DBCComparatorWindow::DBCComparatorWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCComparatorWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    connect(ui->btnDBCFile1, SIGNAL(clicked(bool)), this, SLOT(loadFirstFile()));
    connect(ui->btnDBCFile2, SIGNAL(clicked(bool)), this, SLOT(loadSecondFile()));
    connect(ui->btnSaveDetails, SIGNAL(clicked(bool)), this, SLOT(saveDetails()));

    ui->lblFirstFile->setText("");
    ui->lblSecondFile->setText("");

    firstDBC = nullptr;
    secondDBC = nullptr;

    installEventFilter(this);
}

DBCComparatorWindow::~DBCComparatorWindow()
{
    removeEventFilter(this);
    delete ui;
}

void DBCComparatorWindow::showEvent(QShowEvent *)
{
    readSettings();
}

void DBCComparatorWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event)
    writeSettings();
}

bool DBCComparatorWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("filecomparison.md");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void DBCComparatorWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("DBCComparator/WindowSize", QSize(720, 631)).toSize());
        move(Utility::constrainedWindowPos(settings.value("DBCComparator/WindowPos", QPoint(50, 50)).toPoint()));
    }
}

void DBCComparatorWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("DBCComparator/WindowSize", size());
        settings.setValue("DBCComparator/WindowPos", pos());
    }
}

QString DBCComparatorWindow::loadDBC(DBCFile **file)
{
    QString filename;
    QFileDialog dialog;
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("DBC Files (*.dbc *.DBC)")));

    dialog.setDirectory(settings.value("FileIO/LoadSaveDirectory", dialog.directory().path()).toString());
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];

        QProgressDialog progress(qApp->activeWindow());
        progress.setWindowModality(Qt::WindowModal);
        progress.setLabelText("Loading file...");
        progress.setCancelButton(nullptr);
        progress.setRange(0,0);
        progress.setMinimumDuration(0);
        progress.show();

        qApp->processEvents();

        if (dialog.selectedNameFilter() == filters[0])
        {
            if (file[0]) delete file[0];
            file[0] = new DBCFile;
            file[0]->loadFile(filename);
            qDebug() << "Loaded the DBC file into first slot";
        }

        progress.cancel();
        return filename;
    }
    return QString();
}

void DBCComparatorWindow::loadFirstFile()
{
    QString filename;
    filename = loadDBC(&firstDBC);
    ui->lblFirstFile->setText(filename);
    if (firstDBC && secondDBC) calculateDetails();
}

void DBCComparatorWindow::loadSecondFile()
{
    QString filename;
    filename = loadDBC(&secondDBC);
    ui->lblSecondFile->setText(filename);
    if (firstDBC && secondDBC) calculateDetails();
}

void DBCComparatorWindow::calculateDetails()
{
    QProgressDialog progress(this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setLabelText("Calculating differences");
    progress.setCancelButton(nullptr);
    progress.setRange(0,0);
    progress.setMinimumDuration(0);
    progress.show();

    qApp->processEvents();

    firstDBC->sort();
    secondDBC->sort();


    ui->treeDetails->clear();

    //Find nodes missing in one of the DBC files

    QTreeWidgetItem *nodeDiffRoot = new QTreeWidgetItem();
    nodeDiffRoot->setText(0, "Node Differences");
    QTreeWidgetItem *nodesMissingDBCFirst = new QTreeWidgetItem();
    nodesMissingDBCFirst->setText(0, "Nodes not found in first DBC");
    QTreeWidgetItem *nodesMissingDBCSecond = new QTreeWidgetItem();
    nodesMissingDBCSecond->setText(0, "Nodes not found in second DBC");
    nodeDiffRoot->addChild(nodesMissingDBCFirst);
    nodeDiffRoot->addChild(nodesMissingDBCSecond);
    ui->treeDetails->addTopLevelItem(nodeDiffRoot);

    for (int i = 0; i < firstDBC->dbc_nodes.count(); i++)
    {
        QString nodeName = firstDBC->dbc_nodes[i].name;
        DBC_NODE *node = secondDBC->findNodeByName(nodeName);
        if (!node)
        {
            QTreeWidgetItem *missingNodeItem = new QTreeWidgetItem();
            missingNodeItem->setText(0, nodeName);
            nodesMissingDBCSecond->addChild(missingNodeItem);
        }
    }

    for (int i = 0; i < secondDBC->dbc_nodes.count(); i++)
    {
        QString nodeName = secondDBC->dbc_nodes[i].name;
        DBC_NODE *node = firstDBC->findNodeByName(nodeName);
        if (!node)
        {
            QTreeWidgetItem *missingNodeItem = new QTreeWidgetItem();
            missingNodeItem->setText(0, nodeName);
            nodesMissingDBCFirst->addChild(missingNodeItem);
        }
    }



    //Find messages that are missing in one of the DBC files
    //If a message exists in both then check for missing signals

    QTreeWidgetItem *msgDiffRoot = new QTreeWidgetItem();
    msgDiffRoot->setText(0, "Message Differences");
    QTreeWidgetItem *msgMissingDBCFirst = new QTreeWidgetItem();
    msgMissingDBCFirst->setText(0, "Messages not found in first DBC");
    QTreeWidgetItem *msgMissingDBCSecond = new QTreeWidgetItem();
    msgMissingDBCSecond->setText(0, "Messages not found in second DBC");
    QTreeWidgetItem *msgSignalsDiff = new QTreeWidgetItem();
    msgSignalsDiff->setText(0, "Messages with missing signals");
    QTreeWidgetItem *sigDiffOne = new QTreeWidgetItem();
    sigDiffOne->setText(0, "Missing from first DBC");
    QTreeWidgetItem *sigDiffTwo = new QTreeWidgetItem();
    sigDiffTwo->setText(0, "Missing from second DBC");
    QTreeWidgetItem *sigModifiedRoot = new QTreeWidgetItem();
    sigModifiedRoot->setText(0, "Modified Signals");
    msgSignalsDiff->addChild(sigDiffOne);
    msgSignalsDiff->addChild(sigDiffTwo);

    msgDiffRoot->addChild(msgMissingDBCFirst);
    msgDiffRoot->addChild(msgMissingDBCSecond);
    msgDiffRoot->addChild(msgSignalsDiff);
    msgDiffRoot->addChild(sigModifiedRoot);

    ui->treeDetails->addTopLevelItem(msgDiffRoot);

    QTreeWidgetItem *msgItem;
    QTreeWidgetItem *sigTemp;

    for (int i = 0; i < firstDBC->messageHandler->getCount(); i++)
    {
        DBC_MESSAGE *thisMsg = firstDBC->messageHandler->findMsgByIdx(i);
        QString msgName = thisMsg->name;
        DBC_MESSAGE *otherMsg = secondDBC->messageHandler->findMsgByName(msgName);
        if (!otherMsg)
        {
            QTreeWidgetItem *missingMsgItem = new QTreeWidgetItem();
            missingMsgItem->setText(0, msgName+ " (" + Utility::formatCANID(thisMsg->ID) + ")");
            msgMissingDBCSecond->addChild(missingMsgItem);
        }
        else //both have Msg. Check sigs for missing
        {
            bool thisMsgHasMissing = false;
            bool thisMsgHasMods = false;

            for (int i = 0; i < thisMsg->sigHandler->getCount(); i++)
            {
                DBC_SIGNAL *thisSig = thisMsg->sigHandler->findSignalByIdx(i);
                QString sigName = thisSig->name;
                DBC_SIGNAL *otherSig = otherMsg->sigHandler->findSignalByName(sigName);
                if (!otherSig)
                {
                    QTreeWidgetItem *missingSigItem = new QTreeWidgetItem();
                    missingSigItem->setText(0, sigName);
                    if (!thisMsgHasMissing)
                    {
                        thisMsgHasMissing = true;
                        msgItem = new QTreeWidgetItem();
                        msgItem->setText(0, msgName + " (" + Utility::formatCANID(thisMsg->ID) + ")");
                        sigDiffTwo->addChild(msgItem);
                    }
                    msgItem->addChild(missingSigItem);
                }
                else //signal exists on both sides. See if it as changed position or length
                {
                    bool didChange = false;
                    QTreeWidgetItem *sigItem = new QTreeWidgetItem();
                    sigItem->setText(0, sigName);
                    if (thisSig->startBit != otherSig->startBit)
                    {
                        didChange = true;
                        QTreeWidgetItem *sigStartBit = new QTreeWidgetItem();
                        sigStartBit->setText(0, "Start Bit       First DBC: " + QString::number(thisSig->startBit) + "     Second: " + QString::number(otherSig->startBit));
                        sigItem->addChild(sigStartBit);
                    }
                    if (thisSig->signalSize != otherSig->signalSize)
                    {
                        didChange = true;
                        QTreeWidgetItem *sigSize = new QTreeWidgetItem();
                        sigSize->setText(0, "Size          First DBC: " + QString::number(thisSig->signalSize) + "      Second: " + QString::number(otherSig->signalSize));
                        sigItem->addChild(sigSize);
                    }
                    if (thisSig->bias != otherSig->bias)
                    {
                        didChange = true;
                        QTreeWidgetItem *sigBias = new QTreeWidgetItem();
                        sigBias->setText(0, "Bias          First DBC: " + QString::number(thisSig->bias) + "      Second: " + QString::number(otherSig->bias));
                        sigItem->addChild(sigBias);
                    }
                    if (thisSig->factor != otherSig->factor)
                    {
                        didChange = true;
                        QTreeWidgetItem *sigFactor = new QTreeWidgetItem();
                        sigFactor->setText(0, "Factor        First DBC: " + QString::number(thisSig->factor) + "      Second: " + QString::number(otherSig->factor));
                        sigItem->addChild(sigFactor);
                    }
                    if (didChange)
                    {
                        if (!thisMsgHasMods)
                        {
                            thisMsgHasMods = true;
                            sigTemp = new QTreeWidgetItem();
                            sigTemp->setText(0, msgName + " (" + Utility::formatCANID(thisMsg->ID) + ")");
                            sigModifiedRoot->addChild(sigTemp);
                        }
                        sigTemp->addChild(sigItem);
                    }
                }
            }
            thisMsgHasMissing = false;
            for (int i = 0; i < otherMsg->sigHandler->getCount(); i++)
            {
                DBC_SIGNAL *thisSig = otherMsg->sigHandler->findSignalByIdx(i);
                QString sigName = thisSig->name;
                DBC_SIGNAL *otherSig = thisMsg->sigHandler->findSignalByName(sigName);
                if (!otherSig)
                {
                    QTreeWidgetItem *missingSigItem = new QTreeWidgetItem();
                    missingSigItem->setText(0, sigName);
                    if (!thisMsgHasMissing)
                    {
                        thisMsgHasMissing = true;
                        msgItem = new QTreeWidgetItem();
                        msgItem->setText(0, msgName + " (" + Utility::formatCANID(thisMsg->ID) + ")");
                        sigDiffOne->addChild(msgItem);
                    }
                    msgItem->addChild(missingSigItem);
                }
            }
        }
    }

    for (int i = 0; i < secondDBC->messageHandler->getCount(); i++)
    {
        DBC_MESSAGE *origMsg = secondDBC->messageHandler->findMsgByIdx(i);
        QString msgName = origMsg->name;
        DBC_MESSAGE *msg = firstDBC->messageHandler->findMsgByName(msgName);
        if (!msg)
        {
            QTreeWidgetItem *missingMsgItem = new QTreeWidgetItem();
            missingMsgItem->setText(0, msgName + " (" + Utility::formatCANID(origMsg->ID) + ")");
            msgMissingDBCFirst->addChild(missingMsgItem);
        }
    }

    QSettings settings;
    if (settings.value("InfoCompare/AutoExpand", false).toBool())
    {
        ui->treeDetails->expandAll();
    }

    progress.cancel();

    qApp->processEvents();
}

void DBCComparatorWindow::saveDetails()
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

