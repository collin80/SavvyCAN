#include "udsfirmwareuploaderwindow.h"
#include "ui_udsfirmwareuploaderwindow.h"
#include "mainwindow.h"
#include "connections/canconmanager.h"
#include "utility.h"

#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

UDSFirmwareUploaderWindow::UDSFirmwareUploaderWindow(const QVector<CommFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UDSFirmwareUploaderWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    modelFrames = frames;
    currentState = STATE_IDLE;
    firmwareSize = 0;
    firmwareSize2 = 0;
    blockSequenceCounter = 0;
    maxBlockLength = 0;
    currentDataPos = 0;
    busId = 0;
    targetId = 0x7E0;
    responseId = 0x7E8;
    sessionType = 0x02;
    securityLevel = 1;
    flashAddr = 0x08000000;
    flashAddr2 = 0x08000000;
    useSecurity = true;
    uploadingSecondCpu = false;

    udsHandler = HandlerFactory::createUDSHandler();
    udsHandler->setReception(true);

    timeoutTimer = new QTimer();
    timeoutTimer->setInterval(1000);
    testerTimer = new QTimer();
    testerTimer->setInterval(2000);
    progressTimer = new QElapsedTimer();

    int numBuses = CANConManager::getInstance()->getNumBuses();
    ui->spinBus->setMaximum(numBuses > 0 ? numBuses - 1 : 0);

    connect(ui->btnLoadFile, &QPushButton::clicked, this, &UDSFirmwareUploaderWindow::handleLoadFile);
    connect(ui->btnStartStop, &QPushButton::clicked, this, &UDSFirmwareUploaderWindow::handleStartStop);
    connect(ui->btnAbort, &QPushButton::clicked, this, &UDSFirmwareUploaderWindow::handleAbort);
    connect(ui->btnReadBuildID, &QPushButton::clicked, this, &UDSFirmwareUploaderWindow::handleReadBuildID);
    connect(ui->btnLookupVersion, &QPushButton::clicked, this, &UDSFirmwareUploaderWindow::handleLookupVersion);
    connect(ui->btnBrowseTeslaDir, &QPushButton::clicked, this, &UDSFirmwareUploaderWindow::handleBrowseTeslaDir);
    connect(ui->btnReloadNodes, &QPushButton::clicked, this, &UDSFirmwareUploaderWindow::handleReloadNodes);
    connect(udsHandler, &UDS_HANDLER::newUDSMessage, this, &UDSFirmwareUploaderWindow::gotUDSReply);
    connect(timeoutTimer, &QTimer::timeout, this, &UDSFirmwareUploaderWindow::timeoutTriggered);
    connect(testerTimer, &QTimer::timeout, this, &UDSFirmwareUploaderWindow::testerPresentTick);
    connect(ui->cmbTeslaNode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UDSFirmwareUploaderWindow::handleNodeChanged);

    loadTeslaNodes();
}

UDSFirmwareUploaderWindow::~UDSFirmwareUploaderWindow()
{
    stopUpload();
    delete testerTimer;
    delete timeoutTimer;
    delete udsHandler;
    delete progressTimer;
    delete ui;
}

void UDSFirmwareUploaderWindow::logMessage(const QString &msg)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    ui->txtLog->append("[" + timestamp + "] " + msg);
}

void UDSFirmwareUploaderWindow::setState(UPLOAD_STATE newState)
{
    currentState = newState;
    if (newState == STATE_ERROR || newState == STATE_COMPLETE || newState == STATE_IDLE)
    {
        timeoutTimer->stop();
        testerTimer->stop();
        ui->btnStartStop->setEnabled(true);
        ui->btnAbort->setEnabled(false);
        ui->btnLoadFile->setEnabled(true);
        ui->btnReadBuildID->setEnabled(true);
    }
}

void UDSFirmwareUploaderWindow::handleLoadFile()
{
    QFileDialog dialog;
    QStringList filters;
    filters.append("Binary Firmware (*.bin)");
    filters.append("All Files (*)");
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);

    if (dialog.exec() == QDialog::Accepted)
    {
        QString filename = dialog.selectedFiles().constFirst();
        QFile inFile(filename);
        if (!inFile.open(QIODevice::ReadOnly))
        {
            logMessage("ERROR: Could not open file: " + filename);
            return;
        }
        firmwareData = inFile.readAll();
        firmwareSize = firmwareData.length();
        inFile.close();
        ui->lblFilename->setText(filename + QString(" (%1 bytes)").arg(firmwareSize));
        logMessage("Loaded firmware: " + filename + " (" + QString::number(firmwareSize) + " bytes)");
    }
}

void UDSFirmwareUploaderWindow::handleStartStop()
{
    if (currentState != STATE_IDLE && currentState != STATE_COMPLETE && currentState != STATE_ERROR)
    {
        stopUpload();
        return;
    }
    startUpload();
}

void UDSFirmwareUploaderWindow::handleAbort()
{
    stopUpload();
    logMessage("Upload aborted by user");
    setState(STATE_IDLE);
}

void UDSFirmwareUploaderWindow::startUpload()
{
    if (firmwareData.isEmpty())
    {
        logMessage("ERROR: No firmware file loaded");
        return;
    }

    busId = ui->spinBus->value();
    targetId = Utility::ParseStringToNum(ui->txtTargetID->text());
    responseId = Utility::ParseStringToNum(ui->txtResponseID->text());
    flashAddr = Utility::ParseStringToNum(ui->txtFlashAddr->text());
    securityLevel = ui->spinSecurityLevel->value();
    useSecurity = ui->ckUseSecurity->isChecked();

    if (ui->cbSessionType->currentIndex() == 0) sessionType = 0x02;
    else sessionType = 0x03;

    blockSequenceCounter = 0;
    currentDataPos = 0;
    uploadingSecondCpu = false;

    ui->progressBar->setValue(0);
    ui->btnStartStop->setEnabled(false);
    ui->btnAbort->setEnabled(true);
    ui->btnLoadFile->setEnabled(false);
    ui->btnReadBuildID->setEnabled(false);

    udsHandler->setReception(true);
    udsHandler->addFilter(busId, responseId, 0x7FF);

    logMessage("Starting UDS firmware upload...");
    logMessage(QString("Target: 0x%1, Response: 0x%2, Bus: %3")
               .arg(targetId, 0, 16).arg(responseId, 0, 16).arg(busId));
    logMessage(QString("Flash Address: 0x%1, Size: %2 bytes")
               .arg(flashAddr, 0, 16).arg(firmwareSize));

    sendDiagSession();
}

void UDSFirmwareUploaderWindow::stopUpload(bool resetState)
{
    timeoutTimer->stop();
    testerTimer->stop();
    udsHandler->clearAllFilters();
    udsHandler->setReception(false);

    ui->btnStartStop->setEnabled(true);
    ui->btnAbort->setEnabled(false);
    ui->btnLoadFile->setEnabled(true);

    if (resetState)
    {
        currentState = STATE_IDLE;
        ui->btnStartStop->setText("Start Upload");
    }
}

void UDSFirmwareUploaderWindow::sendDiagSession()
{
    UDS_MESSAGE msg;
    msg.setBus(busId);
    msg.setFrameId(targetId);
    msg.service = UDS_SERVICES::DIAG_CONTROL;
    msg.subFuncLen = 1;
    msg.subFunc = sessionType;

    logMessage("Sending Diagnostic Session Control (0x10) - Sub: 0x" + QString::number(sessionType, 16));
    udsHandler->sendUDSFrame(msg);
    setState(STATE_WAIT_DIAG_RESP);
    timeoutTimer->start();
}

void UDSFirmwareUploaderWindow::sendSecuritySeed()
{
    UDS_MESSAGE msg;
    msg.setBus(busId);
    msg.setFrameId(targetId);
    msg.service = UDS_SERVICES::SECURITY_ACCESS;
    msg.subFuncLen = 1;
    msg.subFunc = (securityLevel * 2) - 1;

    logMessage("Requesting Security Seed (0x27) - Level: " + QString::number(securityLevel));
    udsHandler->sendUDSFrame(msg);
    setState(STATE_WAIT_SEED);
    timeoutTimer->start();
}

void UDSFirmwareUploaderWindow::sendSecurityKey(const QByteArray &seed)
{
    UDS_MESSAGE msg;
    msg.setBus(busId);
    msg.setFrameId(targetId);
    msg.service = UDS_SERVICES::SECURITY_ACCESS;
    msg.subFuncLen = 1;
    msg.subFunc = securityLevel * 2;
    msg.payload() = seed;

    logMessage("Sending Security Key (0x27) - " + seed.toHex(' '));
    udsHandler->sendUDSFrame(msg);
    setState(STATE_WAIT_KEY_RESP);
    timeoutTimer->start();
}

void UDSFirmwareUploaderWindow::sendRequestDownload()
{
    UDS_MESSAGE msg;
    msg.setBus(busId);
    msg.setFrameId(targetId);
    msg.service = UDS_SERVICES::REQUEST_DOWNLOAD;
    msg.subFuncLen = 0;
    msg.subFunc = 0;

    QByteArray data;
    data.append(static_cast<char>(0x00));
    uint32_t addr = flashAddr;
    uint32_t size = firmwareSize;

    int addrLen = 4;
    int sizeLen = 4;
    data[0] = (addrLen << 4) | sizeLen;

    for (int i = addrLen - 1; i >= 0; i--)
        data.append((addr >> (i * 8)) & 0xFF);
    for (int i = sizeLen - 1; i >= 0; i--)
        data.append((size >> (i * 8)) & 0xFF);

    msg.payload() = data;

    logMessage(QString("Sending Request Download (0x34) - Addr: 0x%1, Size: %2")
               .arg(addr, 0, 16).arg(size));
    udsHandler->sendUDSFrame(msg);
    setState(STATE_WAIT_DOWNLOAD_RESP);
    timeoutTimer->start();
}

void UDSFirmwareUploaderWindow::sendDataBlock()
{
    UDS_MESSAGE msg;
    msg.setBus(busId);
    msg.setFrameId(targetId);
    msg.service = UDS_SERVICES::TRANSFER_DATA;
    msg.subFuncLen = 0;
    msg.subFunc = 0;

    QByteArray data;
    data.append(blockSequenceCounter & 0xFF);

    int bytesToSend = firmwareSize - currentDataPos;
    if (maxBlockLength > 0 && bytesToSend > maxBlockLength)
        bytesToSend = maxBlockLength;

    data.append(firmwareData.constData() + currentDataPos, bytesToSend);
    msg.payload() = data;

    udsHandler->sendUDSFrame(msg);
    setState(STATE_WAIT_DATA_RESP);
    timeoutTimer->start();
}

void UDSFirmwareUploaderWindow::sendTransferExit()
{
    UDS_MESSAGE msg;
    msg.setBus(busId);
    msg.setFrameId(targetId);
    msg.service = UDS_SERVICES::REQ_TRANS_EXIT;
    msg.subFuncLen = 0;
    msg.subFunc = 0;

    logMessage("Sending Request Transfer Exit (0x37)");
    udsHandler->sendUDSFrame(msg);
    setState(STATE_WAIT_EXIT_RESP);
    timeoutTimer->start();
}

void UDSFirmwareUploaderWindow::sendECUReset()
{
    UDS_MESSAGE msg;
    msg.setBus(busId);
    msg.setFrameId(targetId);
    msg.service = UDS_SERVICES::ECU_RESET;
    msg.subFuncLen = 1;
    msg.subFunc = 0x01;

    logMessage("Sending ECU Reset (0x11)");
    udsHandler->sendUDSFrame(msg);
    setState(STATE_WAIT_RESET_RESP);
    timeoutTimer->start();
}

void UDSFirmwareUploaderWindow::sendTesterPresent()
{
    if (currentState != STATE_WAIT_DATA_RESP && currentState != STATE_WAIT_DOWNLOAD_RESP)
        return;

    UDS_MESSAGE msg;
    msg.setBus(busId);
    msg.setFrameId(targetId);
    msg.service = UDS_SERVICES::TESTER_PRESENT;
    msg.subFuncLen = 1;
    msg.subFunc = 0x80;

    udsHandler->sendUDSFrame(msg);
}

void UDSFirmwareUploaderWindow::handleReadBuildID()
{
    if (currentState != STATE_IDLE && currentState != STATE_COMPLETE && currentState != STATE_ERROR)
        return;

    busId = ui->spinBus->value();
    targetId = Utility::ParseStringToNum(ui->txtTargetID->text());
    responseId = Utility::ParseStringToNum(ui->txtResponseID->text());

    ui->btnReadBuildID->setEnabled(false);

    udsHandler->setReception(true);
    udsHandler->addFilter(busId, responseId, 0x7FF);

    logMessage(QString("Reading build ID from 0x%1 (response 0x%2, bus %3)...")
               .arg(targetId, 0, 16).arg(responseId, 0, 16).arg(busId));

    sendReadBuildID();
}

void UDSFirmwareUploaderWindow::sendReadBuildID()
{
    UDS_MESSAGE msg;
    msg.bus = busId;
    msg.setFrameId(targetId);
    msg.service = UDS_SERVICES::READ_BY_ID;
    msg.subFuncLen = 2;
    msg.subFunc = 0xF180;

    udsHandler->setFlowCtrl(true); //we need to generate flow control messages for this read request.
    udsHandler->sendUDSFrame(msg);
    setState(STATE_WAIT_BUILD_ID_RESP);
    timeoutTimer->start();
}

void UDSFirmwareUploaderWindow::handleNRC(int nrc)
{
    QString desc = udsHandler->getNegativeResponseLong(nrc);
    logMessage(QString("NEGATIVE RESPONSE: 0x%1 - %2").arg(nrc, 0, 16).arg(desc));
    setState(STATE_ERROR);
    stopUpload();
}

void UDSFirmwareUploaderWindow::gotUDSReply(UDS_MESSAGE msg)
{
    if (msg.frameId() != responseId) return;
    if (msg.getBus() != busId) return;

    const unsigned char *data = reinterpret_cast<const unsigned char *>(msg.payload().constData());
    int dataLen = msg.payload().size();

    if (msg.isErrorReply)
    {
        qDebug() << "UDS FW Upload: NRC service=" << msg.service << " subfunc=" << msg.subFunc;
        handleNRC(msg.subFunc);
        return;
    }

    timeoutTimer->stop();

    switch (currentState)
    {
    case STATE_WAIT_DIAG_RESP:
    {
        logMessage("Diagnostic Session Control response received");
        if (useSecurity)
        {
            sendSecuritySeed();
        }
        else
        {
            sendRequestDownload();
        }
        break;
    }

    case STATE_WAIT_SEED:
    {
        logMessage("Security seed received (" + QString::number(dataLen) + " bytes)");
        QByteArray seed;
        seed.append(reinterpret_cast<const char *>(data), dataLen);
        QByteArray key = seed;
        for (int i = 0; i < key.size(); i++)
            key[i] = ~key[i];
        sendSecurityKey(key);
        break;
    }

    case STATE_WAIT_KEY_RESP:
    {
        logMessage("Security access granted");
        sendRequestDownload();
        break;
    }

    case STATE_WAIT_DOWNLOAD_RESP:
    {
        if (dataLen > 1)
        {
            int sizeLen = data[0] >> 4;
            maxBlockLength = 0;
            for (int i = 0; i < sizeLen && (i + 1) < dataLen; i++)
            {
                maxBlockLength = (maxBlockLength << 8) | data[1 + i];
            }
            logMessage(QString("Download accepted. Max block size: %1 bytes").arg(maxBlockLength));
        }
        else
        {
            maxBlockLength = 4096;
            logMessage("Download accepted (no block size limit advertised)");
        }
        testerTimer->start();
        sendDataBlock();
        break;
    }

    case STATE_WAIT_DATA_RESP:
    {
        progressTimer->start();
        currentDataPos += maxBlockLength > 0 ?
            qMin(maxBlockLength, firmwareSize - currentDataPos) :
            (firmwareSize - currentDataPos);
        blockSequenceCounter++;

        int progress = (currentDataPos * 100) / firmwareSize;
        ui->progressBar->setValue(progress);

        if (currentDataPos >= firmwareSize)
        {
            logMessage("All data blocks transferred");
            testerTimer->stop();
            sendTransferExit();
        }
        else
        {
            sendDataBlock();
        }
        break;
    }

    case STATE_WAIT_EXIT_RESP:
    {
        if (!uploadingSecondCpu && !firmwareData2.isEmpty())
        {
            logMessage("CPU1 transfer complete, starting CPU2 upload...");
            firmwareData = firmwareData2;
            firmwareSize = firmwareSize2;
            flashAddr = flashAddr2;
            blockSequenceCounter = 0;
            currentDataPos = 0;
            uploadingSecondCpu = true;
            sendRequestDownload();
        }
        else
        {
            logMessage("Transfer exit confirmed");
            sendECUReset();
        }
        break;
    }

    case STATE_WAIT_RESET_RESP:
    {
        logMessage("ECU reset acknowledged");
        setState(STATE_COMPLETE);
        stopUpload();
        logMessage("*** Firmware upload complete! ***");
        ui->progressBar->setValue(100);
        break;
    }

    case STATE_WAIT_BUILD_ID_RESP:
    {
        // READ_BY_ID (0x22) positive response (0x62): service byte stripped by UDS handler.
        // msg.payload() = [0xF1, 0x80, ..., buildId at indices 6-9]
        // This corresponds to bytes 6-10 of the raw ISOTP assembled frame.
        if (dataLen >= 10)
        {
            quint32 buildId = (data[6] << 24) + (data[7] << 16) + (data[8] << 8) + data[9];
            QString buildIdStr = QString::number(buildId);
            lastBuildId = buildIdStr;
            logMessage("Build ID: " + buildIdStr);
            ui->lblBuildID->setText(buildIdStr);
        }
        else
        {
            logMessage(QString("Build ID response too short (%1 bytes in payload)").arg(dataLen));
        }
        udsHandler->clearAllFilters();
        udsHandler->setReception(false);
        setState(STATE_IDLE);
        break;
    }

    default:
        break;
    }
}

void UDSFirmwareUploaderWindow::loadTeslaNodes()
{
    messageIdMap.clear();
    teslaNodes.clear();

    QStringList searchPaths;

    if (!teslaDir.isEmpty())
    {
        searchPaths.append(teslaDir + "/opt/odin/data/Model3/dej");
        searchPaths.append(teslaDir + "/opt/odin/data/Model3");
    }

    searchPaths.append(QCoreApplication::applicationDirPath());
    searchPaths.append(QDir::currentPath());
    searchPaths.append(QCoreApplication::applicationDirPath() + "/../");
    searchPaths.append(QDir::currentPath() + "/../");

    QString compactPath;
    QString nodesPath;
    for (const QString &path : searchPaths)
    {
        if (compactPath.isEmpty())
        {
            QString test = path + "/Model3_ETH.compact.json";
            if (QFile::exists(test)) compactPath = test;
        }
        if (nodesPath.isEmpty())
        {
            QString test = path + "/nodes.json";
            if (QFile::exists(test)) nodesPath = test;
        }
    }

    // Load Model3_ETH.compact.json first to build message name -> ID map
    if (!compactPath.isEmpty())
    {
        QFile compactFile(compactPath);
        if (compactFile.open(QIODevice::ReadOnly))
        {
            QByteArray data = compactFile.readAll();
            compactFile.close();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull())
            {
                QJsonObject root = doc.object();
                QJsonObject messages = root["messages"].toObject();
                for (auto it = messages.begin(); it != messages.end(); it++)
                {
                    QJsonObject msgObj = it->toObject();
                    int msgId = msgObj["message_id"].toInt(-1);
                    if (msgId >= 0)
                        messageIdMap[it.key()] = static_cast<uint32_t>(msgId);
                }
                logMessage("Loaded " + QString::number(messageIdMap.size()) +
                           " message IDs from Model3_ETH.compact.json");
            }
            else
                logMessage("ERROR: Failed to parse Model3_ETH.compact.json");
        }
        else
            logMessage("ERROR: Could not open Model3_ETH.compact.json");
    }
    else
        logMessage("WARNING: Model3_ETH.compact.json not found");

    // Load nodes.json
    if (!nodesPath.isEmpty())
    {
        QFile nodesFile(nodesPath);
        if (nodesFile.open(QIODevice::ReadOnly))
        {
            QByteArray data = nodesFile.readAll();
            nodesFile.close();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull())
            {
                QJsonObject root = doc.object();
                for (auto it = root.begin(); it != root.end(); it++)
                {
                    TeslaNodeInfo node;
                    node.name = it.key();
                    QJsonObject obj = it->toObject();

                    QString reqName = obj["request_message_name"].toString();
                    QString respName = obj["response_message_name"].toString();

                    node.requestId = messageIdMap.value(reqName, 0x7E0);
                    node.responseId = messageIdMap.value(respName, 0x7E8);
                    node.securityLevel = 1;
                    node.useSecurity = true;

                    QJsonObject sec = obj["security"].toObject();
                    if (!sec.isEmpty())
                    {
                        node.securityAlgorithm = sec["algorithm"].toString();
                        node.securityBufferSize = sec["buffer_size"].toInt();
                    }

                    node.dataUpload = obj["data_upload"].toString();

                    teslaNodes.append(node);
                }
                logMessage("Loaded " + QString::number(teslaNodes.size()) +
                           " Tesla nodes from nodes.json");
                populateNodeCombo();
            }
            else
                logMessage("ERROR: Failed to parse nodes.json");
        }
        else
            logMessage("ERROR: Could not open nodes.json");
    }
    else
        logMessage("WARNING: nodes.json not found");

    if (!teslaDir.isEmpty())
        ui->txtTeslaDir->setText(teslaDir);
    else if (!nodesPath.isEmpty())
    {
        QFileInfo info(nodesPath);
        teslaDir = info.absolutePath();
        ui->txtTeslaDir->setText(teslaDir);
    }
}

void UDSFirmwareUploaderWindow::populateNodeCombo()
{
    ui->cmbTeslaNode->clear();
    ui->cmbTeslaNode->addItem("-- Manual --");
    for (const TeslaNodeInfo &node : teslaNodes)
        ui->cmbTeslaNode->addItem(node.name);
}

void UDSFirmwareUploaderWindow::handleBrowseTeslaDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Tesla Data Directory", teslaDir);
    if (!dir.isEmpty())
    {
        teslaDir = dir;
        ui->txtTeslaDir->setText(teslaDir);
        handleReloadNodes();
    }
}

void UDSFirmwareUploaderWindow::handleReloadNodes()
{
    teslaDir = ui->txtTeslaDir->text().trimmed();
    loadTeslaNodes();
}

void UDSFirmwareUploaderWindow::handleNodeChanged(int index)
{
    if (index <= 0)
        return;
    autoFillFromNode(index - 1);
}

void UDSFirmwareUploaderWindow::autoFillFromNode(int idx)
{
    if (idx < 0 || idx >= teslaNodes.size())
        return;

    const TeslaNodeInfo &node = teslaNodes[idx];
    ui->txtTargetID->setText("0x" + QString::number(node.requestId, 16).toUpper());
    ui->txtResponseID->setText("0x" + QString::number(node.responseId, 16).toUpper());
    if (!node.securityAlgorithm.isEmpty() && node.securityAlgorithm != "tesla_hash")
    {
        ui->ckUseSecurity->setChecked(false);
        logMessage("Node " + node.name + " uses " + node.securityAlgorithm +
                   " security; security disabled (unsupported)");
    }
    else
    {
        ui->ckUseSecurity->setChecked(node.useSecurity);
    }
    logMessage("Selected node: " + node.name +
               " (Req: 0x" + QString::number(node.requestId, 16) +
               ", Resp: 0x" + QString::number(node.responseId, 16) + ")");
}

void UDSFirmwareUploaderWindow::timeoutTriggered()
{
    logMessage("TIMEOUT - No response received");
    handleNRC(0x10);
}

void UDSFirmwareUploaderWindow::testerPresentTick()
{
    sendTesterPresent();
    logMessage("Tester Present keep-alive sent");
}

void UDSFirmwareUploaderWindow::handleLookupVersion()
{
    lookupVersionMap();
}

void UDSFirmwareUploaderWindow::lookupVersionMap()
{
    int nodeIdx = ui->cmbTeslaNode->currentIndex();
    if (nodeIdx <= 0)
    {
        logMessage("Version lookup: select a target ECU node first");
        return;
    }
    if (lastBuildId.isEmpty())
    {
        logMessage("Version lookup: read the build ID first");
        return;
    }

    QString ecuName = ui->cmbTeslaNode->currentText().toLower();

    QStringList searchPaths;
    if (!teslaDir.isEmpty()) searchPaths.append(teslaDir + "/deploy/seed_artifacts_v2");
    searchPaths.append(QCoreApplication::applicationDirPath());
    searchPaths.append(QDir::currentPath());

    QString tsvPath;
    for (const QString &path : searchPaths)
    {
        QString candidate = path + "/version_map2.tsv";
        if (QFile::exists(candidate)) { tsvPath = candidate; break; }
    }

    if (tsvPath.isEmpty())
    {
        logMessage("Version lookup: version_map2.tsv not found");
        return;
    }

    QFile f(tsvPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        logMessage("Version lookup: could not open " + tsvPath);
        return;
    }

    logMessage(QString("Version lookup: ECU='%1' build='%2'").arg(ecuName).arg(lastBuildId));
    QString searchTerm = ecuName + ":" + lastBuildId.trimmed();

    //the 4th column has the firmware type. if it ends in bu it's a bootloader updater.
    //If it ends in bl it's the bootloader itself. Otherwise it is likely the main firmware
    //but pcs and some others have two cpus so then you get cpu2 at the end for the second processor
    QString tsvDir = QFileInfo(tsvPath).absolutePath();
    int matchCount = 0;
    bool cpu1Loaded = false;
    bool cpu2Loaded = false;
    QTextStream in(&f);
    while (!in.atEnd())
    {
        QString line = in.readLine();
        QStringList cols = line.split('\t');
        if (cols.size() < 4) continue;
        if (cols[0] != searchTerm) continue;

        QString fwType = cols[3];
        if (fwType.endsWith("bu") || fwType.endsWith("bl"))
            continue;

        bool isCpu2 = fwType.contains("cpu2");
        matchCount++;
        logMessage(QString("Match (%1): %2").arg(isCpu2 ? "cpu2" : "cpu1").arg(line));

        if (isCpu2 && !cpu2Loaded)
        {
            QString fwPath = tsvDir + "/" + cols[1];
            QFile fwFile(fwPath);
            if (!fwFile.open(QIODevice::ReadOnly))
            {
                logMessage("ERROR: Could not open CPU2 firmware file: " + fwPath);
                continue;
            }
            firmwareData2 = fwFile.readAll();
            firmwareSize2 = firmwareData2.length();
            fwFile.close();
            logMessage("Loaded CPU2 firmware: " + fwPath + " (" + QString::number(firmwareSize2) + " bytes)");
            cpu2Loaded = true;
        }
        else if (!isCpu2 && !cpu1Loaded)
        {
            QString fwPath = tsvDir + "/" + cols[1];
            QFile fwFile(fwPath);
            if (!fwFile.open(QIODevice::ReadOnly))
            {
                logMessage("ERROR: Could not open firmware file: " + fwPath);
                continue;
            }
            firmwareData = fwFile.readAll();
            firmwareSize = firmwareData.length();
            fwFile.close();
            ui->lblFilename->setText(fwPath + QString(" (%1 bytes)").arg(firmwareSize));
            logMessage("Loaded firmware: " + fwPath + " (" + QString::number(firmwareSize) + " bytes)");
            cpu1Loaded = true;
        }
    }
    f.close();

    if (matchCount == 0)
        logMessage("Version lookup: no main firmware matches found");
    else
        logMessage(QString("Version lookup: %1 main firmware match(es) found").arg(matchCount));
}

void UDSFirmwareUploaderWindow::advanceState()
{
}
