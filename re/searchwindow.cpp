#include "searchwindow.h"
#include "ui_searchwindow.h"
#include "../utility.h"
#include <QDebug>
#include <cmath>
#include <cstring>

SearchWindow::SearchWindow(const QVector<CommFrame> *frames, DBCHandler *handler, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SearchWindow)
    , modelFrames(frames)
    , dbcHandler(handler)
    , assocSignal(nullptr)
    , startBit(0)
    , dataLen(8)
    , currentMatchIndex(-1)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    // DBC signal pickers
    connect(ui->cbNodes,    SIGNAL(currentIndexChanged(int)), this, SLOT(loadMessages(int)));
    connect(ui->cbMessages, SIGNAL(currentIndexChanged(int)), this, SLOT(loadSignals(int)));
    connect(ui->btnCopySignal, SIGNAL(clicked(bool)), this, SLOT(copySignalToParamsUI()));

    // Bit-field grid
    connect(ui->gridData,    SIGNAL(gridClicked(int)), this, SLOT(bitfieldClicked(int)));
    connect(ui->txtStartBit, SIGNAL(textChanged(QString)), this, SLOT(handleStartBitUpdate()));
    connect(ui->txtDataLen,  SIGNAL(textChanged(QString)), this, SLOT(handleDataLenUpdate()));
    connect(ui->cbIntel,     SIGNAL(toggled(bool)),  this, SLOT(drawBitfield()));

    // Value mode toggle
    connect(ui->rbValueAny,      SIGNAL(toggled(bool)), this, SLOT(onValueModeChanged()));
    connect(ui->rbValueSpecific, SIGNAL(toggled(bool)), this, SLOT(onValueModeChanged()));

    // Navigation
    connect(ui->btnSearch, SIGNAL(clicked(bool)), this, SLOT(onSearchClicked()));
    connect(ui->btnNext,   SIGNAL(clicked(bool)), this, SLOT(onNextClicked()));
    connect(ui->btnPrev,   SIGNAL(clicked(bool)), this, SLOT(onPreviousClicked()));

    loadNodes();
    drawBitfield();
}

SearchWindow::~SearchWindow()
{
    delete ui;
}

// Called by MainWindow when the frame list changes
void SearchWindow::updatedFrames(int /*numFrames*/)
{
    // Clear stale results when frames change
    if (!matchingIndices.isEmpty())
    {
        matchingIndices.clear();
        currentMatchIndex = -1;
        ui->lblMatchInfo->setText("Frames updated – press Search again.");
        updateNavigationState();
    }
}

// ──────────────────────────────────────────────
//  DBC hierarchy loading  (mirrors NewGraphDialog)
// ──────────────────────────────────────────────
void SearchWindow::loadNodes()
{
    ui->cbNodes->clear();
    if (!dbcHandler) return;
    int numFiles = dbcHandler->getFileCount();
    if (numFiles == 0) return;

    for (int f = 0; f < numFiles; f++)
    {
        DBCFile *thisFile = dbcHandler->getFileByIdx(f);
        QList<QString> names;
        QList<DBC_MESSAGE *> msgs = thisFile->messageHandler->getMsgsAsList();

        for (int x = 0; x < thisFile->dbc_nodes.count(); x++)
        {
            bool hasMessages = false;
            for (int m = 0; m < msgs.count(); m++)
            {
                if (msgs[m]->sender->name == thisFile->dbc_nodes[x]->name)
                {
                    hasMessages = true;
                    break;
                }
            }
            if (hasMessages)
            {
                QString fqName = thisFile->getFilenameNoExt()
                               + Utility::fullyQualifiedNameSeperator
                               + thisFile->dbc_nodes[x]->name;
                names.append(fqName);
            }
        }

        if (!names.isEmpty())
        {
            names.sort();
            ui->cbNodes->addItem("----" + thisFile->getFilename());
            Utility::SetComboBoxItemEnabled(ui->cbNodes, ui->cbNodes->count() - 1, false);
            for (const QString &n : names)
                ui->cbNodes->addItem(n);
        }
    }
}

void SearchWindow::loadMessages(int idx)
{
    ui->cbMessages->clear();
    if (!dbcHandler) return;
    if (dbcHandler->getFileCount() == 0) return;

    QString displayedNode = ui->cbNodes->itemText(idx);

    for (int f = 0; f < dbcHandler->getFileCount(); f++)
    {
        QList<DBC_MESSAGE *> msgs = dbcHandler->getFileByIdx(f)->messageHandler->getMsgsAsList();
        for (int x = 0; x < msgs.count(); x++)
        {
            QString fqName = dbcHandler->getFileByIdx(f)->getFilenameNoExt()
                           + Utility::fullyQualifiedNameSeperator
                           + msgs[x]->sender->name;
            if (fqName == displayedNode)
                ui->cbMessages->addItem(msgs[x]->name);
        }
    }
}

void SearchWindow::loadSignals(int /*idx*/)
{
    ui->cbSignals->clear();
    if (!dbcHandler) return;
    if (dbcHandler->getFileCount() == 0) return;

    QString fqNode = ui->cbNodes->itemText(ui->cbNodes->currentIndex());
    DBC_MESSAGE *msg = dbcHandler->findMessage(ui->cbMessages->currentText(), fqNode);
    if (!msg) return;

    QList<DBC_SIGNAL *> sigs = msg->sigHandler->getSignalsAsList();
    for (DBC_SIGNAL *sig : sigs)
    {
        if (sig)
            ui->cbSignals->addItem(sig->name);
    }
    ui->cbSignals->model()->sort(0);
}

void SearchWindow::copySignalToParamsUI()
{
    assocSignal = nullptr;
    if (!dbcHandler) return;

    QString fqNode  = ui->cbNodes->itemText(ui->cbNodes->currentIndex());
    QString msgName = ui->cbMessages->itemText(ui->cbMessages->currentIndex());
    DBC_MESSAGE *msg = dbcHandler->findMessage(msgName, fqNode);
    if (!msg) return;

    DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(ui->cbSignals->currentText());
    if (!sig) return;

    // Populate manual fields from DBC signal
    startBit = sig->startBit;
    dataLen  = sig->signalSize;

    ui->txtStartBit->blockSignals(true);
    ui->txtStartBit->setText(QString::number(startBit));
    ui->txtStartBit->blockSignals(false);

    ui->txtDataLen->setText(QString::number(dataLen));
    ui->txtID->setText(Utility::formatCANID(msg->ID));
    ui->cbUseID->setChecked(true);
    ui->txtScale->setText(QString::number(sig->factor));
    ui->txtBias->setText(QString::number(sig->bias));
    ui->cbIntel->setChecked(sig->intelByteOrder);
    ui->cbSigned->setChecked(sig->valType == SIGNED_INT);

    assocSignal = sig;
    ui->lblSignalStatus->setText("Params loaded from: " + sig->name);

    drawBitfield();
}

// ──────────────────────────────────────────────
//  Bit-field visualisation (mirrors NewGraphDialog)
// ──────────────────────────────────────────────
void SearchWindow::bitfieldClicked(int bit)
{
    startBit = bit;
    ui->txtStartBit->blockSignals(true);
    ui->txtStartBit->setText(QString::number(startBit));
    ui->txtStartBit->blockSignals(false);
    drawBitfield();
}

void SearchWindow::handleStartBitUpdate()
{
    startBit = ui->txtStartBit->text().toInt();
    if (startBit < 0)   startBit = 0;
    if (startBit > 511) startBit = 511;
    drawBitfield();
}

void SearchWindow::handleDataLenUpdate()
{
    dataLen = ui->txtDataLen->text().toInt();
    if (dataLen < 1)  dataLen = 1;
    if (dataLen > 512) dataLen = 512;
    drawBitfield();
}

void SearchWindow::drawBitfield()
{
    uint8_t bitField[64];
    memset(bitField, 0, sizeof(bitField));

    // Mark the start bit distinctly
    bitField[Utility::getByteFromBitPosition(startBit)] |=
        static_cast<uint8_t>(1 << Utility::getBitFromBitPosition(startBit));

    ui->gridData->setReference(reinterpret_cast<unsigned char *>(bitField), false);

    if (ui->cbIntel->isChecked())
    {
        int endBit = startBit + dataLen - 1;
        if (endBit > 511) endBit = 511;
        for (int y = startBit; y <= endBit; y++)
            bitField[Utility::getByteFromBitPosition(y)] |=
                static_cast<uint8_t>(1 << Utility::getBitFromBitPosition(y));
    }
    else
    {
        int size = dataLen;
        int sBit = startBit;
        while (size > 0)
        {
            bitField[Utility::getByteFromBitPosition(sBit)] |=
                static_cast<uint8_t>(1 << Utility::getBitFromBitPosition(sBit));
            size--;
            if ((sBit % 8) == 0) sBit += 15;
            else sBit--;
            if (sBit > 511) sBit = 511;
        }
    }

    ui->gridData->updateData(reinterpret_cast<unsigned char *>(bitField), true);
}

// ──────────────────────────────────────────────
//  Value mode
// ──────────────────────────────────────────────
void SearchWindow::onValueModeChanged()
{
    ui->txtValue->setEnabled(ui->rbValueSpecific->isChecked());
}

// ──────────────────────────────────────────────
//  Core decode helper
// ──────────────────────────────────────────────
double SearchWindow::decodeValue(const CommFrame &frame) const
{
    double scale = ui->txtScale->text().toDouble();
    double bias  = ui->txtBias->text().toDouble();
    if (qFuzzyIsNull(scale)) scale = 1.0;

    // If we have an associated DBC signal, use its decoder
    if (assocSignal)
    {
        double val = 0.0;
        if (assocSignal->processAsDouble(frame, val))
            return val;
    }

    int64_t raw = Utility::processIntegerSignal(
        frame.payload(), startBit, dataLen,
        ui->cbIntel->isChecked(), ui->cbSigned->isChecked());

    return static_cast<double>(raw) * scale + bias;
}

// ──────────────────────────────────────────────
//  Search
// ──────────────────────────────────────────────
void SearchWindow::onSearchClicked()
{
    performSearch();
}

void SearchWindow::performSearch()
{
    matchingIndices.clear();
    currentMatchIndex = -1;

    if (!modelFrames || modelFrames->isEmpty())
    {
        ui->lblMatchInfo->setText("No frames loaded.");
        updateNavigationState();
        return;
    }

    // ── Collect filter parameters ────────────────────────────
    bool   useID  = ui->cbUseID->isChecked();
    uint32_t filterID = useID ? Utility::ParseStringToNum(ui->txtID->text()) : 0;

    bool   useLen  = ui->cbUseLen->isChecked();
    int    filterLen = ui->spinFrameLen->value();

    // Direction: 0 = both, 1 = RX only, 2 = TX only
    int dirMode = 0;
    if (ui->rbDirRX->isChecked())  dirMode = 1;
    if (ui->rbDirTX->isChecked())  dirMode = 2;

    bool specificValue = ui->rbValueSpecific->isChecked();
    double targetValue = ui->txtValue->text().toDouble();

    // For "any change" mode we need to track the last decoded value per CAN ID
    QMap<uint32_t, double> lastSeen;
    const double NO_PREV = std::numeric_limits<double>::max();

    for (int i = 0; i < modelFrames->size(); i++)
    {
        const CommFrame &f = modelFrames->at(i);

        // ── ID filter ──────────────────────────────────────────
        if (useID && f.frameId() != filterID)
            continue;

        // ── Length filter ──────────────────────────────────────
        if (useLen && f.payload().size() != filterLen)
            continue;

        // ── Direction filter ───────────────────────────────────
        if (dirMode == 1 && !f.isReceived())   continue; // want RX, frame is TX
        if (dirMode == 2 &&  f.isReceived())   continue; // want TX, frame is RX

        // ── Value filter ───────────────────────────────────────
        // Only apply value filter when there's at least one data byte covered
        if (startBit / 8 < f.payload().size())
        {
            double val = decodeValue(f);

            if (specificValue)
            {
                if (!qFuzzyCompare(val, targetValue))
                    continue;
            }
            else // any change
            {
                uint32_t key = f.frameId();
                double prev  = lastSeen.value(key, NO_PREV);
                if (prev != NO_PREV && qFuzzyCompare(val, prev))
                    continue;          // same as last time → skip
                lastSeen[key] = val;
            }
        }

        matchingIndices.append(i);
    }

    if (matchingIndices.isEmpty())
    {
        ui->lblMatchInfo->setText("No matches found.");
    }
    else
    {
        currentMatchIndex = 0;
        ui->lblMatchInfo->setText(
            QString("Match 1 of %1").arg(matchingIndices.size()));
        emit jumpToFrameIndex(matchingIndices.at(0));
    }

    updateNavigationState();
}

// ──────────────────────────────────────────────
//  Navigation
// ──────────────────────────────────────────────
void SearchWindow::onNextClicked()
{
    if (matchingIndices.isEmpty()) return;
    currentMatchIndex = (currentMatchIndex + 1) % matchingIndices.size();
    ui->lblMatchInfo->setText(
        QString("Match %1 of %2").arg(currentMatchIndex + 1).arg(matchingIndices.size()));
    emit jumpToFrameIndex(matchingIndices.at(currentMatchIndex));
}

void SearchWindow::onPreviousClicked()
{
    if (matchingIndices.isEmpty()) return;
    currentMatchIndex = (currentMatchIndex - 1 + matchingIndices.size()) % matchingIndices.size();
    ui->lblMatchInfo->setText(
        QString("Match %1 of %2").arg(currentMatchIndex + 1).arg(matchingIndices.size()));
    emit jumpToFrameIndex(matchingIndices.at(currentMatchIndex));
}

void SearchWindow::updateNavigationState()
{
    bool hasResults = !matchingIndices.isEmpty();
    ui->btnNext->setEnabled(hasResults);
    ui->btnPrev->setEnabled(hasResults);
}
