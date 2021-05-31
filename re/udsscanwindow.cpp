#include "udsscanwindow.h"
#include "ui_udsscanwindow.h"
#include "mainwindow.h"
#include "connections/canconmanager.h"
#include "bus_protocols/uds_handler.h"
#include "utility.h"
#include "helpwindow.h"

UDSScanWindow::UDSScanWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UDSScanWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    modelFrames = frames;

    currentlyRunning = false;

    waitTimer = new QTimer;
    waitTimer->setInterval(100);

    udsHandler = new UDS_HANDLER;

    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    connect(udsHandler, &UDS_HANDLER::newUDSMessage, this, &UDSScanWindow::gotUDSReply);
    connect(ui->btnScan, &QPushButton::clicked, this, &UDSScanWindow::scanUDS);
    connect(waitTimer, &QTimer::timeout, this, &UDSScanWindow::timeOut);
    connect(ui->btnSaveResults, &QPushButton::clicked, this, &UDSScanWindow::saveResults);
    connect(ui->ckWildcard, &QCheckBox::toggled, this, &UDSScanWindow::wildcardToggled);
    connect(ui->ckReadByAddr, &QCheckBox::toggled, this, &UDSScanWindow::readByToggled);
    connect(ui->ckReadByID, &QCheckBox::toggled, this, &UDSScanWindow::readByToggled);
    connect(ui->cbAllowAdaptiveOffset, &QCheckBox::toggled, this, &UDSScanWindow::adaptiveToggled);
    connect(ui->spinNumBytes, SIGNAL(valueChanged(int)), this, SLOT(numBytesChanged()));
    connect(ui->spinLowerService, SIGNAL(valueChanged(int)), this, SLOT(checkServiceRange()));
    connect(ui->spinUpperService, SIGNAL(valueChanged(int)), this, SLOT(checkServiceRange()));
    connect(ui->spinLowerSubfunc, SIGNAL(valueChanged(int)), this, SLOT(checkSubFuncRange()));
    connect(ui->spinUpperSubfunc, SIGNAL(valueChanged(int)), this, SLOT(checkSubFuncRange()));
    connect(ui->spinStartID, SIGNAL(valueChanged(int)), this, SLOT(checkIDRange()));
    connect(ui->spinEndID, SIGNAL(valueChanged(int)), this, SLOT(checkIDRange()));

    int numBuses = CANConManager::getInstance()->getNumBuses();
    for (int n = 0; n < numBuses; n++) ui->cbBuses->addItem(QString::number(n));
    installEventFilter(this);
}

UDSScanWindow::~UDSScanWindow()
{
    removeEventFilter(this);
    delete ui;
    waitTimer->stop();
    delete waitTimer;
    delete udsHandler;
}

bool UDSScanWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("uds_scanner.md");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void UDSScanWindow::adaptiveToggled()
{
    if (ui->cbAllowAdaptiveOffset->isChecked()) ui->spinReplyOffset->setEnabled(false);
    else ui->spinReplyOffset->setEnabled(true);
}

void UDSScanWindow::wildcardToggled()
{
    bool state = false;
    if (ui->ckWildcard->isChecked()) state = false;
    else state = true;

    ui->ckReset->setEnabled(state);
    ui->ckSecurity->setEnabled(state);
    ui->ckSession->setEnabled(state);
    ui->ckTester->setEnabled(state);
    ui->ckReadByAddr->setEnabled(state);
    ui->ckReadByID->setEnabled(state);
    ui->ckReset->setChecked(false);
    ui->ckSecurity->setChecked(false);
    ui->ckSession->setChecked(false);
    ui->ckTester->setChecked(false);
    ui->ckReadByAddr->setChecked(false);
    ui->ckReadByID->setChecked(false);

    ui->spinLowerService->setEnabled(!state);
    ui->spinLowerSubfunc->setEnabled(!state);
    ui->spinNumBytes->setEnabled(!state);
    ui->spinUpperService->setEnabled(!state);
    ui->spinUpperSubfunc->setEnabled(!state);
}

void UDSScanWindow::readByToggled()
{
    bool state = false;
    if (ui->ckReadByAddr->isChecked() || ui->ckReadByID->isChecked()) state = true;
    else state = false;

    ui->spinLowerService->setEnabled(false);
    ui->spinLowerSubfunc->setEnabled(state);
    ui->spinNumBytes->setEnabled(state);
    ui->spinUpperService->setEnabled(false);
    ui->spinUpperSubfunc->setEnabled(state);
}

void UDSScanWindow::numBytesChanged()
{
    uint64_t upperBound =  (1ull << (8ull * ui->spinNumBytes->value())) - 1;
    if (upperBound > 0x7FFFFFFF) upperBound = 0x7FFFFFFF;
    ui->spinUpperSubfunc->setMaximum(upperBound);
}

void UDSScanWindow::checkIDRange()
{
    ui->spinStartID->setMaximum(ui->spinEndID->value());
    ui->spinEndID->setMinimum(ui->spinStartID->value());
}

void UDSScanWindow::checkServiceRange()
{
    ui->spinLowerService->setMaximum(ui->spinUpperService->value());
    ui->spinUpperService->setMinimum(ui->spinLowerService->value());
}

void UDSScanWindow::checkSubFuncRange()
{
    ui->spinLowerSubfunc->setMaximum(ui->spinUpperSubfunc->value());
    ui->spinUpperSubfunc->setMinimum(ui->spinLowerSubfunc->value());
}

void UDSScanWindow::saveResults()
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
    dialog.setDirectory(settings.value("UDSScan/LoadSaveDirectory", dialog.directory().path()).toString());

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        settings.setValue("UDSScan/LoadSaveDirectory", dialog.directory().path());

        if (!filename.contains('.')) filename += ".txt";
        if (dialog.selectedNameFilter() == filters[0])
        {
            QFile *outFile = new QFile(filename);

            if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
            {
                delete outFile;
                return;
            }
            outFile->write("UDS Scan Log:\n\n");
            dumpNode(ui->treeResults->invisibleRootItem(), outFile, 0);

            outFile->close();
            delete outFile;
        }
    }
}

void UDSScanWindow::dumpNode(QTreeWidgetItem* item, QFile *file, int indent)
{
    if (indent > 0) for (int i = 0; i < (indent - 1); i++) file->write("\t");
    file->write(item->text(0).toUtf8());
    file->write("\n");
    for( int i = 0; i < item->childCount(); ++i )
        dumpNode( item->child(i), file, indent + 1 );
    if (indent == 1) file->write("\n");
}

void UDSScanWindow::sendOnBuses(UDS_MESSAGE test, int buses)
{
    test.bus = buses;
    sendingFrames.append(test);
}

void UDSScanWindow::scanUDS()
{
    if (currentlyRunning)
    {
        waitTimer->stop();
        sendingFrames.clear();
        udsHandler->setReception(false);
        udsHandler->setProcessAllIDs(false);
        udsHandler->setFlowCtrl(false);
        currentlyRunning = false;
        ui->btnScan->setText("Start Scan");
        return;
    }

    udsHandler->setReception(true);
    udsHandler->setProcessAllIDs(true);
    udsHandler->setFlowCtrl(true);

    waitTimer->setInterval(ui->spinDelay->value());

    ui->treeResults->clear();
    sendingFrames.clear();
    nodeService = nullptr;
    nodeID = nullptr;
    nodeSubFunc = nullptr;

    UDS_MESSAGE test;
    int typ, id;
    int startID, endID;
    startID = ui->spinStartID->value();
    endID = ui->spinEndID->value();
    if (endID < startID) {
        int temp = startID;
        startID = endID;
        endID = temp;
    }

    int buses = ui->cbBuses->currentIndex();

    for (id = startID; id <= endID; id++)
    {
        test.setFrameId( id );
        test.payload().clear();

        if (ui->ckTester->isChecked())
        {
            test.service = UDS_SERVICES::TESTER_PRESENT;
            test.subFunc = 0;
            sendOnBuses(test, buses);
        }

        if (ui->ckSession->isChecked())
        {
            for (typ = 1; typ < 4; typ++) //try each type of session access
            {
                test.service = UDS_SERVICES::DIAG_CONTROL;
                test.subFunc = typ;
                sendOnBuses(test, buses);
            }
        }

        if (ui->ckReset->isChecked()) //try to command a reset of the ECU. You're likely to know if it works. ;)
        {
            for (typ = 1; typ < 4; typ++) //try each type of session access
            {
                test.service = UDS_SERVICES::ECU_RESET;
                test.subFunc = typ;
                sendOnBuses(test, buses);
            }
        }

        if (ui->ckSecurity->isChecked()) //try to enter security mode - very likely to get a response if an ECU exists.
        {
            for (typ = 1; typ < 0x42; typ = typ + 2) //try each type of session access. In practice only the first 1-3 are likely to work
            {
                test.service = UDS_SERVICES::SECURITY_ACCESS;
                test.subFunc = typ;
                sendOnBuses(test, buses);
            }
        }

        if (ui->ckReadByAddr->isChecked())
        {
            test.subFuncLen = ui->spinNumBytes->value();
            test.service = UDS_SERVICES::READ_BY_ADDR;
            for (int subf = ui->spinLowerSubfunc->value(); subf <= ui->spinUpperSubfunc->value(); subf++)
            {
                test.subFunc = subf;
                sendOnBuses(test, buses);
            }
        }

        if (ui->ckReadByID->isChecked())
        {
            test.subFuncLen = ui->spinNumBytes->value();
            test.service = UDS_SERVICES::READ_BY_ID;
            for (int subf = ui->spinLowerSubfunc->value(); subf <= ui->spinUpperSubfunc->value(); subf++)
            {
                test.subFunc = subf;
                sendOnBuses(test, buses);
            }
        }

        if (ui->ckWildcard->isChecked())
        {
            //preallocate the whole buffer so we don't have to keep updating the size and moving as we go.
            //TODO: this shows a downside to the current method - it might potentially need to create a huge
            //number of frames here. Of course, the rest of the system will do the same so I guess it's a bad idea
            //any way you go to generate a 1 billion frame test.
            int size = (endID - startID) * (ui->spinUpperService->value() - ui->spinLowerService->value());
            size *= (ui->spinUpperSubfunc->value() - ui->spinLowerSubfunc->value());
            sendingFrames.reserve(size);

            test.subFuncLen = ui->spinNumBytes->value();

            for (typ = ui->spinLowerService->value(); typ <= ui->spinUpperService->value(); typ++)
            {
                test.service = typ;
                for (int subTyp = ui->spinLowerSubfunc->value(); subTyp <= ui->spinUpperSubfunc->value(); subTyp++)
                {
                    test.subFunc = subTyp;
                    sendOnBuses(test, buses);
                }
            }
        }
    }

    waitTimer->start();
    currIdx = -1;
    currentlyRunning = true;
    ui->btnScan->setText("Abort Scan");
    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(sendingFrames.length());
    sendNextMsg();
}

//Updates here are sent about every 1/4 second. That's fine for most windows but not this one.
void UDSScanWindow::updatedFrames(int numFrames)
{
    if (numFrames == -1) //all frames deleted. We don't care
    {
    }
    else if (numFrames == -2) //all new set of frames. Don't care.
    {
    }
    else //just got some new frames. See if they are relevant.
    {
    }
}

void UDSScanWindow::gotUDSReply(UDS_MESSAGE msg)
{
    QString result;
    QString serviceShortName;
    uint32_t id;
    int offset = ui->spinReplyOffset->value();
    UDS_MESSAGE sentFrame;
    bool gotReply = false;
    const unsigned char *data = reinterpret_cast<const unsigned char *>(msg.payload().constData());
    int dataLen = msg.payload().length();

    int numSending = sendingFrames.length();
    if (numSending == 0) return;
    if (currIdx >= numSending) return;
    sentFrame = sendingFrames[currIdx];

    id = msg.frameId();

    qDebug() << "UDS message ID " << QString::number(msg.frameId(),16) << "  service: " << QString::number(msg.service, 16) << " subfunc: " << QString::number(msg.subFunc, 16);

    if ((id == (uint32_t)(sentFrame.frameId() + offset)) || ui->cbAllowAdaptiveOffset->isChecked())
    {
        serviceShortName = udsHandler->getServiceShortDesc(sentFrame.service);
        if (serviceShortName.length() < 3) serviceShortName = QString::number(sentFrame.service, 16);
        if (msg.service == (0x40 + sendingFrames[currIdx].service) )
        {
            setupNodes(id);

            QTreeWidgetItem *nodePositive = new QTreeWidgetItem();
            QString reply = "POSITIVE ";
            for (int i = 0; i < dataLen; i++)
            {
                reply.append(" ");
                reply.append(Utility::formatHexNum(data[i]));
            }
            nodePositive->setText(0, reply);
            nodePositive->setForeground(0, QBrush(Qt::darkGreen));
            nodeSubFunc->addChild(nodePositive);
            nodeSubFunc->setForeground(0, QBrush(Qt::darkGreen));
            gotReply = true;
        }
        else if ( msg.isErrorReply && (msg.service == sendingFrames[currIdx].service) )
        {
            if (dataLen)
            {
                setupNodes(id);
                QTreeWidgetItem *nodeNegative = new QTreeWidgetItem();
                qDebug() << ui->spinNumBytes->value();
                nodeNegative->setText(0, "NEGATIVE - " + udsHandler->getNegativeResponseShort(data[0]));
                nodeNegative->setForeground(0, QBrush(Qt::darkRed));
                nodeSubFunc->addChild(nodeNegative);
                nodeSubFunc->setForeground(0, QBrush(Qt::darkRed));
                gotReply = true;
            }
        }
    }
    if (gotReply)
    {
        //ui->listResults->addItem(result);
        sendNextMsg();
    }
}

void UDSScanWindow::setupNodes(uint32_t replyID)
{
    QString serviceShortName = udsHandler->getServiceShortDesc(sendingFrames[currIdx].service);
    if (serviceShortName.length() < 3) serviceShortName = QString::number(sendingFrames[currIdx].service, 16);
    QTreeWidgetItem *replyNode = nullptr;

    if (!nodeID || nodeID->text(0) != Utility::formatHexNum(sendingFrames[currIdx].frameId()))
    {
        nodeID = new QTreeWidgetItem();
        nodeID->setText(0, Utility::formatHexNum(sendingFrames[currIdx].frameId()));
        ui->treeResults->addTopLevelItem(nodeID);
        nodeService = nullptr;
    }

    bool foundReplyMatch = false;
    for (int i = 0; i < nodeID->childCount(); i++)
    {
        if (nodeID->child(i)->text(0) == Utility::formatHexNum(replyID) || ((replyID == 0xDEAD5EA1) && (nodeID->child(i)->text(0) == "NO REPLY")))
        {
            foundReplyMatch = true;
            replyNode = nodeID->child(i);
            break;
        }
    }
    if (!foundReplyMatch)
    {
        QTreeWidgetItem *replyItem = new QTreeWidgetItem();
        if (replyID != 0xDEAD5EA1) replyItem->setText(0, Utility::formatHexNum(replyID));
        else replyItem->setText(0, "NO REPLY");
        nodeID->addChild(replyItem);
        replyNode = replyItem;
        nodeService = nullptr;
    }

    bool foundServiceMatch = false;

    for (int i = 0; i < replyNode->childCount(); i++)
    {
        if ( replyNode->child(i)->text(0) == serviceShortName)
        {
            foundServiceMatch = true;
            nodeService = replyNode->child(i);
            break;
        }
    }
    if (!foundServiceMatch)
    {
        nodeService = new QTreeWidgetItem();
        nodeService->setText(0, serviceShortName);
        replyNode->addChild(nodeService);
    }

    nodeSubFunc = new QTreeWidgetItem();
    nodeSubFunc->setText(0, Utility::formatHexNum(sendingFrames[currIdx].subFunc));
    nodeService->addChild(nodeSubFunc);
}

void UDSScanWindow::timeOut()
{
    if (ui->ckShowNoReply->isChecked())
    {
        setupNodes(0xDEAD5EA1);
        nodeSubFunc->setForeground(0, QBrush(Qt::gray));
    }

    sendNextMsg();
}

void UDSScanWindow::sendNextMsg()
{
    currIdx++;
    if (currIdx < sendingFrames.count())
    {
        udsHandler->sendUDSFrame(sendingFrames[currIdx]);
        waitTimer->start();
    }
    else
    {
        waitTimer->stop();
        udsHandler->setReception(false);
        udsHandler->setProcessAllIDs(false);
        udsHandler->setFlowCtrl(false);
        ui->btnScan->setText("Start Scan");
        currentlyRunning = false;
    }
    ui->progressBar->setValue(currIdx);
}
