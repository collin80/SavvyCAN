#include "udsscanwindow.h"
#include "ui_udsscanwindow.h"
#include "mainwindow.h"
#include "connections/canconmanager.h"
#include "bus_protocols/uds_handler.h"
#include "utility.h"

UDSScanWindow::UDSScanWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UDSScanWindow)
{
    ui->setupUi(this);

    modelFrames = frames;

    currentlyRunning = false;

    waitTimer = new QTimer;
    waitTimer->setInterval(100);

    UDS_HANDLER::getInstance()->setReception(true);

    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    connect(UDS_HANDLER::getInstance(), &UDS_HANDLER::newUDSMessage, this, &UDSScanWindow::gotUDSReply);
    connect(ui->btnScan, &QPushButton::clicked, this, &UDSScanWindow::scanUDS);
    connect(waitTimer, &QTimer::timeout, this, &UDSScanWindow::timeOut);
    connect(ui->btnSaveResults, &QPushButton::clicked, this, &UDSScanWindow::saveResults);
    connect(ui->ckWildcard, &QCheckBox::toggled, this, &UDSScanWindow::wildcardToggled);
    connect(ui->cbAllowAdaptiveOffset, &QCheckBox::toggled, this, &UDSScanWindow::adaptiveToggled);
    connect(ui->spinNumBytes, SIGNAL(valueChanged(int)), this, SLOT(numBytesChanged()));

    int numBuses = CANConManager::getInstance()->getNumBuses();
    for (int n = 0; n < numBuses; n++) ui->cbBuses->addItem(QString::number(n));
    ui->cbBuses->addItem(tr("All"));
}

UDSScanWindow::~UDSScanWindow()
{
    delete ui;
    waitTimer->stop();
    delete waitTimer;
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
    ui->ckReset->setChecked(false);
    ui->ckSecurity->setChecked(false);
    ui->ckSession->setChecked(false);
    ui->ckTester->setChecked(false);

    ui->spinLowerService->setEnabled(!state);
    ui->spinLowerSubfunc->setEnabled(!state);
    ui->spinNumBytes->setEnabled(!state);
    ui->spinUpperService->setEnabled(!state);
    ui->spinUpperSubfunc->setEnabled(!state);
}

void UDSScanWindow::numBytesChanged()
{
    uint64_t upperBound =  (1 << (8 * ui->spinNumBytes->value())) - 1;
    ui->spinUpperSubfunc->setMaximum(upperBound);
}

void UDSScanWindow::saveResults()
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
    int busList = buses;
    if (busList < ui->cbBuses->count() - 1)
    {
        test.bus = buses;
        sendingFrames.append(test);
    }
    else
    {
        for (int c = 0; c < ui->cbBuses->count() - 1; c++)
        {
            test.bus = c;
            sendingFrames.append(test);
        }
    }
}

void UDSScanWindow::scanUDS()
{
    if (currentlyRunning)
    {
        waitTimer->stop();
        sendingFrames.clear();
        currentlyRunning = false;
        ui->btnScan->setText("Start Scan");
        return;
    }

    waitTimer->setInterval(ui->spinDelay->value());

    ui->treeResults->clear();
    sendingFrames.clear();
    nodeService = NULL;
    nodeID = NULL;
    nodeSubFunc = NULL;

    UDS_MESSAGE test;
    int typ, id;
    int startID, endID;
    startID = Utility::ParseStringToNum(ui->txtStartID->text());
    endID = Utility::ParseStringToNum(ui->txtEndID->text());
    if (endID < startID) {
        int temp = startID;
        startID = endID;
        endID = temp;
    }

    int buses = ui->cbBuses->currentIndex();

    for (id = startID; id <= endID; id++)
    {
        test.ID = id;
        test.data.clear();

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
            test.service = UDS_SERVICES::ECU_RESET;
            test.subFunc = 1;
            sendOnBuses(test, buses);
        }
        if (ui->ckSecurity->isChecked()) //try to enter security mode - very likely to get a response if an ECU exists.
        {
            test.service = UDS_SERVICES::SECURITY_ACCESS;
            test.subFunc = 1;
            sendOnBuses(test, buses);
        }
        if (ui->ckWildcard->isChecked())
        {
            int numBytes = ui->spinNumBytes->value();

            //preallocate the whole buffer so we don't have to keep updating the size and moving as we go.
            //TODO: this shows a downside to the current method - it might potentially need to create a huge
            //number of frames here. Of course, the rest of the system will do the same so I guess it's a bad idea
            //any way you go to generate a 1 billion frame test.
            int size = (endID - startID) * (ui->spinUpperService->value() - ui->spinLowerService->value());
            size *= (ui->spinUpperSubfunc->value() - ui->spinLowerSubfunc->value());
            sendingFrames.reserve(size);

            for (typ = ui->spinLowerService->value(); typ <= ui->spinUpperService->value(); typ++)
            {
                test.service = typ;
                for (int subTyp = ui->spinLowerSubfunc->value(); subTyp <= ui->spinUpperSubfunc->value(); subTyp++)
                {
                    test.data.clear();
                    //multi-byte data sent big endian (largest value byte first) so plan accordingly.
                    test.subFunc = subTyp >> (8 * (numBytes - 1));
                    for (int by = 1; by < numBytes; by++)
                    {
                        test.data.append(subTyp >> (8 * (numBytes - by)));
                    }
                    sendOnBuses(test, buses);
                }
            }
        }
    }

    waitTimer->start();
    currIdx = -1;
    currentlyRunning = true;
    ui->btnScan->setText("Abort Scan");
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

void UDSScanWindow::gotUDSReply(UDS_MESSAGE &msg)
{
    QString result;
    QString serviceShortName;
    uint32_t id;
    int offset = ui->spinReplyOffset->value();
    UDS_MESSAGE sentFrame;
    bool gotReply = false;

    int numSending = sendingFrames.length();
    if (numSending == 0) return;
    if (currIdx >= numSending) return;
    sentFrame = sendingFrames[currIdx];

    id = msg.ID;

    if ((id == (uint32_t)(sentFrame.ID + offset)) || ui->cbAllowAdaptiveOffset->isChecked())
    {
        serviceShortName = UDS_HANDLER::getInstance()->getServiceShortDesc(sentFrame.service);
        if (serviceShortName.length() < 3) serviceShortName = QString::number(sentFrame.service, 16);
        if (msg.service == 0x40 + sendingFrames[currIdx].service)
        {
            setupNodes();

            QTreeWidgetItem *nodePositive = new QTreeWidgetItem();
            QString reply = "POSITIVE ";
            for (int i = 0; i < msg.data.length(); i++)
            {
                reply.append(" ");
                reply.append(Utility::formatHexNum(msg.data[i]));
            }
            nodePositive->setText(0, reply);
            nodePositive->setForeground(0, QBrush(Qt::darkGreen));
            nodeSubFunc->addChild(nodePositive);
            nodeSubFunc->setForeground(0, QBrush(Qt::darkGreen));
            gotReply = true;
        }
        else if ( msg.service == 0x7F)
        {
            setupNodes();
            QTreeWidgetItem *nodeNegative = new QTreeWidgetItem();
            nodeNegative->setText(0, "NEGATIVE - " + UDS_HANDLER::getInstance()->getNegativeResponseShort(msg.data[1]));
            nodeNegative->setForeground(0, QBrush(Qt::darkRed));
            nodeSubFunc->addChild(nodeNegative);
            nodeSubFunc->setForeground(0, QBrush(Qt::darkRed));
            gotReply = true;
        }
    }
    if (gotReply)
    {
        //ui->listResults->addItem(result);
        sendNextMsg();
    }
}

void UDSScanWindow::setupNodes()
{
    QString serviceShortName = UDS_HANDLER::getInstance()->getServiceShortDesc(sendingFrames[currIdx].service);
    if (serviceShortName.length() < 3) serviceShortName = QString::number(sendingFrames[currIdx].service, 16);

    if (!nodeID || nodeID->text(0) != Utility::formatHexNum(sendingFrames[currIdx].ID))
    {
        nodeID = new QTreeWidgetItem();
        nodeID->setText(0, Utility::formatHexNum(sendingFrames[currIdx].ID));
        ui->treeResults->addTopLevelItem(nodeID);
        nodeService = NULL;
    }
    if (!nodeService || nodeService->text(0) != serviceShortName)
    {
        nodeService = new QTreeWidgetItem();
        nodeService->setText(0, serviceShortName);
        nodeID->addChild(nodeService);
    }

    nodeSubFunc = new QTreeWidgetItem();
    nodeSubFunc->setText(0, Utility::formatHexNum(sendingFrames[currIdx].subFunc));
    nodeService->addChild(nodeSubFunc);
}

void UDSScanWindow::timeOut()
{
    if (ui->ckShowNoReply->isChecked())
    {
        setupNodes();
        QTreeWidgetItem *nodeNoReply = new QTreeWidgetItem();
        nodeNoReply->setText(0, "No Reply");
        nodeNoReply->setForeground(0, QBrush(Qt::gray));
        nodeSubFunc->addChild(nodeNoReply);
        nodeSubFunc->setForeground(0, QBrush(Qt::gray));
    }

    sendNextMsg();
}

void UDSScanWindow::sendNextMsg()
{
    QVector<unsigned char> data;

    currIdx++;
    if (currIdx < sendingFrames.count())
    {
        data.clear();
        data.append(sendingFrames[currIdx].subFunc);
        data.append(sendingFrames[currIdx].data);
        UDS_HANDLER::getInstance()->sendUDSFrame(sendingFrames[currIdx].bus, sendingFrames[currIdx].ID, sendingFrames[currIdx].service, data);
        waitTimer->start();
    }
    else
    {
        waitTimer->stop();
        ui->btnScan->setText("Start Scan");
        currentlyRunning = false;
    }
}
