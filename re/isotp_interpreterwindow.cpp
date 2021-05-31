#include "isotp_interpreterwindow.h"
#include "ui_isotp_interpreterwindow.h"
#include "mainwindow.h"
#include "helpwindow.h"
#include "filterutility.h"

ISOTP_InterpreterWindow::ISOTP_InterpreterWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ISOTP_InterpreterWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);
    modelFrames = frames;

    decoder = new ISOTP_HANDLER;
    udsDecoder = new UDS_HANDLER;

    decoder->setReception(true);
    decoder->setProcessAll(true);

    udsDecoder->setReception(false);

    connect(MainWindow::getReference(), &MainWindow::framesUpdated, this, &ISOTP_InterpreterWindow::updatedFrames);
    connect(MainWindow::getReference(), &MainWindow::framesUpdated, decoder, &ISOTP_HANDLER::updatedFrames);
    connect(decoder, &ISOTP_HANDLER::newISOMessage, this, &ISOTP_InterpreterWindow::newISOMessage);
    connect(udsDecoder, &UDS_HANDLER::newUDSMessage, this, &ISOTP_InterpreterWindow::newUDSMessage);
    connect(ui->listFilter, &QListWidget::itemChanged, this, &ISOTP_InterpreterWindow::listFilterItemChanged);
    connect(ui->btnAll, &QPushButton::clicked, this, &ISOTP_InterpreterWindow::filterAll);
    connect(ui->btnNone, &QPushButton::clicked, this, &ISOTP_InterpreterWindow::filterNone);
    connect(ui->btnCaptured, &QPushButton::clicked, this, &ISOTP_InterpreterWindow::interpretCapturedFrames);

    connect(ui->tableIsoFrames, &QTableWidget::itemSelectionChanged, this, &ISOTP_InterpreterWindow::showDetailView);
    connect(ui->btnClearList, &QPushButton::clicked, this, &ISOTP_InterpreterWindow::clearList);
    connect(ui->cbUseExtendedAddressing, SIGNAL(toggled(bool)), this, SLOT(useExtendedAddressing(bool)));

    QStringList headers;
    headers << "Timestamp" << "ID" << "Bus" << "Dir" << "Length" << "Data";
    ui->tableIsoFrames->setColumnCount(6);
    ui->tableIsoFrames->setColumnWidth(0, 100);
    ui->tableIsoFrames->setColumnWidth(1, 50);
    ui->tableIsoFrames->setColumnWidth(2, 50);
    ui->tableIsoFrames->setColumnWidth(3, 50);
    ui->tableIsoFrames->setColumnWidth(4, 75);
    ui->tableIsoFrames->setColumnWidth(5, 200);
    ui->tableIsoFrames->setHorizontalHeaderLabels(headers);
    QHeaderView *HorzHdr = ui->tableIsoFrames->horizontalHeader();
    HorzHdr->setStretchLastSection(true);
    connect(HorzHdr, SIGNAL(sectionClicked(int)), this, SLOT(headerClicked(int)));

    decoder->setReception(true);
    decoder->setFlowCtrl(false);
    decoder->setProcessAll(true);
}

ISOTP_InterpreterWindow::~ISOTP_InterpreterWindow()
{
    delete decoder;
    delete ui;
}

void ISOTP_InterpreterWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    readSettings();

    QProgressDialog progress(qApp->activeWindow());
    progress.setWindowModality(Qt::WindowModal);
    progress.setLabelText("Analyzing Frames...");
    progress.setCancelButton(nullptr);
    progress.setRange(0,0);
    progress.setMinimumDuration(0);
    progress.show();

    qApp->processEvents();

    decoder->updatedFrames(-2);
    //decoder->rapidFrames(nullptr, *modelFrames);

    progress.cancel();

    installEventFilter(this);
}

void ISOTP_InterpreterWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event)
    removeEventFilter(this);
    writeSettings();
}

bool ISOTP_InterpreterWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("isotp_decoder.md");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void ISOTP_InterpreterWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("ISODecodeWindow/WindowSize", this->size()).toSize());
        move(Utility::constrainedWindowPos(settings.value("ISODecodeWindow/WindowPos", QPoint(50, 50)).toPoint()));
    }
}

void ISOTP_InterpreterWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("ISODecodeWindow/WindowSize", size());
        settings.setValue("ISODecodeWindow/WindowPos", pos());
    }
}

//erase current list then repopulate as if all the previously captured frames just came in again.
void ISOTP_InterpreterWindow::interpretCapturedFrames()
{
    clearList();
    decoder->updatedFrames(-2);
}

void ISOTP_InterpreterWindow::listFilterItemChanged(QListWidgetItem *item)
{
    if (item)
    {
        int id = FilterUtility::getIdAsInt(item);
        bool state = item->checkState();
        //qDebug() << id << "*" << state;
        idFilters[id] = state;
    }
}

void ISOTP_InterpreterWindow::filterAll()
{
    for (int i = 0 ; i < ui->listFilter->count(); i++)
    {
        ui->listFilter->item(i)->setCheckState(Qt::Checked);
        //idFilters[ui->listFilter->item(i)->text().toInt(nullptr, 16)] = true;
    }
}

void ISOTP_InterpreterWindow::filterNone()
{
    for (int i = 0 ; i < ui->listFilter->count(); i++)
    {
        ui->listFilter->item(i)->setCheckState(Qt::Unchecked);
        //idFilters[ui->listFilter->item(i)->text().toInt(nullptr, 16)] = false;
    }
}

void ISOTP_InterpreterWindow::clearList()
{
    qDebug() << "Clearing the table";
    ui->tableIsoFrames->clearContents();
    ui->tableIsoFrames->model()->removeRows(0, ui->tableIsoFrames->rowCount());
    messages.clear();
    //idFilters.clear();
}

void ISOTP_InterpreterWindow::useExtendedAddressing(bool checked)
{
    decoder->setExtendedAddressing(checked);
    this->interpretCapturedFrames();
}

void ISOTP_InterpreterWindow::updatedFrames(int numFrames)
{
    if (numFrames == -1) //all frames deleted. Kill the display
    {
        clearList();
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        clearList();
    }
    else //just got some new frames. See if they are relevant.
    {
    }
}

void ISOTP_InterpreterWindow::headerClicked(int logicalIndex)
{
    ui->tableIsoFrames->setSortingEnabled(false);
    ui->tableIsoFrames->sortByColumn(logicalIndex, Qt::SortOrder::AscendingOrder);
}

void ISOTP_InterpreterWindow::showDetailView()
{
    QString buildString;
    ISOTP_MESSAGE *msg;
    int rowNum = ui->tableIsoFrames->currentRow();

    ui->txtFrameDetails->clear();
    if (rowNum == -1) return;

    msg = &messages[rowNum];

    const unsigned char *data = reinterpret_cast<const unsigned char *>(msg->payload().constData());
    int dataLen = msg->payload().length();

    if (msg->reportedLength != dataLen)
    {
        buildString.append("Message didn't have the correct number of bytes.\rExpected "
                           + QString::number(msg->reportedLength) + " got "
                           + QString::number(dataLen) + "\r\r");
    }

    buildString.append(tr("Raw Payload: "));

    for (int i = 0; i < dataLen; i++)
    {
        buildString.append(Utility::formatNumber(data[i]));
        buildString.append(" ");
    }
    buildString.append("\r\r");

    ui->txtFrameDetails->setPlainText(buildString);

    //pass this frame to the UDS decoder to see if it feels it could be a UDS related message
    udsDecoder->gotISOTPFrame(messages[rowNum]);
}

void ISOTP_InterpreterWindow::newUDSMessage(UDS_MESSAGE msg)
{
    //qDebug() << "Got UDS message in ISOTP Interpreter";
    QString buildText;

    buildText = ui->txtFrameDetails->toPlainText();

    buildText.append(udsDecoder->getDetailedMessageAnalysis(msg));

    ui->txtFrameDetails->setPlainText(buildText);
}

void ISOTP_InterpreterWindow::newISOMessage(ISOTP_MESSAGE msg)
{
    int rowNum;
    QString tempString;

    const unsigned char *data = reinterpret_cast<const unsigned char *>(msg.payload().constData());
    int dataLen = msg.payload().length();

    if ((msg.reportedLength != dataLen) && !ui->cbShowIncomplete->isChecked()) return;

    if (idFilters.find(msg.frameId()) == idFilters.end())
    {
        idFilters.insert(msg.frameId(), true);

        FilterUtility::createCheckableFilterItem(msg.frameId(), true, ui->listFilter);
    }
    if (!idFilters[msg.frameId()]) return;
    messages.append(msg);

    rowNum = ui->tableIsoFrames->rowCount();

    ui->tableIsoFrames->insertRow(rowNum);

    QTableWidgetItem *item = new QTableWidgetItem;
    item->setData(Qt::EditRole, Utility::formatTimestamp(msg.timeStamp().microSeconds()));
    //ui->tableIsoFrames->setItem(rowNum, 0, (double)msg.timestamp, Utility::formatTimestamp(msg.timestamp)));
    ui->tableIsoFrames->setItem(rowNum, 0, item);
    ui->tableIsoFrames->setItem(rowNum, 1, new QTableWidgetItem(QString::number(msg.frameId(), 16)));
    ui->tableIsoFrames->setItem(rowNum, 2, new QTableWidgetItem(QString::number(msg.bus)));
    if (msg.isReceived) ui->tableIsoFrames->setItem(rowNum, 3, new QTableWidgetItem("Rx"));
    else ui->tableIsoFrames->setItem(rowNum, 3, new QTableWidgetItem("Tx"));
    ui->tableIsoFrames->setItem(rowNum, 4, new QTableWidgetItem(QString::number(msg.payload().length())));

    for (int i = 0; i < dataLen; i++)
    {
        tempString.append(Utility::formatNumber(data[i]));
        tempString.append(" ");
    }
    ui->tableIsoFrames->setItem(rowNum, 5, new QTableWidgetItem(tempString));
}
