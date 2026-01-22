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
    rawFrames = frames;

    decoder = new ISOTP_HANDLER;
    udsDecoder = new UDS_HANDLER;

    decoder->setReception(true);
    decoder->setProcessAll(true);

    udsDecoder->setReception(false);

    ui->tableIsoFrames->setModel(&msgModel);

    connect(MainWindow::getReference(), &MainWindow::framesUpdated, this, &ISOTP_InterpreterWindow::updatedFrames);
    connect(MainWindow::getReference(), &MainWindow::framesUpdated, decoder, &ISOTP_HANDLER::updatedFrames);
    connect(decoder, &ISOTP_HANDLER::newISOMessage, this, &ISOTP_InterpreterWindow::newISOMessage);
    connect(udsDecoder, &UDS_HANDLER::newUDSMessage, this, &ISOTP_InterpreterWindow::newUDSMessage);
    connect(ui->listFilter, &QListWidget::itemChanged, this, &ISOTP_InterpreterWindow::listFilterItemChanged);
    connect(ui->btnAll, &QPushButton::clicked, this, &ISOTP_InterpreterWindow::filterAll);
    connect(ui->btnNone, &QPushButton::clicked, this, &ISOTP_InterpreterWindow::filterNone);
    connect(ui->btnCaptured, &QPushButton::clicked, this, &ISOTP_InterpreterWindow::interpretCapturedFrames);

    connect(ui->tableIsoFrames->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ISOTP_InterpreterWindow::showDetailView);
    connect(ui->btnClearList, &QPushButton::clicked, this, &ISOTP_InterpreterWindow::clearList);
    connect(ui->btnSaveList, &QPushButton::clicked, this, &ISOTP_InterpreterWindow::saveList);
    connect(ui->cbUseExtendedAddressing, &QCheckBox::toggled, this, &ISOTP_InterpreterWindow::useExtendedAddressing);

    ui->tableIsoFrames->setColumnWidth(0, 100);
    ui->tableIsoFrames->setColumnWidth(1, 50);
    ui->tableIsoFrames->setColumnWidth(2, 50);
    ui->tableIsoFrames->setColumnWidth(3, 50);
    ui->tableIsoFrames->setColumnWidth(4, 75);
    ui->tableIsoFrames->setColumnWidth(5, 200);

    ui->tableIsoFrames->setSelectionBehavior(QAbstractItemView::SelectRows);

    QHeaderView *HorzHdr = ui->tableIsoFrames->horizontalHeader();
    HorzHdr->setStretchLastSection(true);

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
    //decoder->rapidFrames(nullptr, *rawFrames);

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
        idFilters[id] = state;
    }
}

void ISOTP_InterpreterWindow::filterAll()
{
    for (int i = 0 ; i < ui->listFilter->count(); i++)
    {
        ui->listFilter->item(i)->setCheckState(Qt::Checked);
    }
}

void ISOTP_InterpreterWindow::filterNone()
{
    for (int i = 0 ; i < ui->listFilter->count(); i++)
    {
        ui->listFilter->item(i)->setCheckState(Qt::Unchecked);
    }
}

void ISOTP_InterpreterWindow::clearList()
{
    qDebug() << "Clearing the table";
    msgModel.clear();
    //idFilters.clear();
}

/*
 * A bit complicated as the list doesn't have the detailed analysis in it. Have to take each list entry
 * and process it to get the details then save those details to the file as well.
 */
void ISOTP_InterpreterWindow::saveList()
{
    QFileDialog dialog(this);
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("Text File (*.txt)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDirectory(settings.value("FrameInfo/LoadSaveDirectory", dialog.directory().path()).toString());

    if (dialog.exec() == QDialog::Accepted)
    {
        settings.setValue("FrameInfo/LoadSaveDirectory", dialog.directory().path());
        QString filename = dialog.selectedFiles().constFirst();
        if (!filename.contains('.')) filename += ".txt";
        if (dialog.selectedNameFilter() == filters[0])
        {
            QFile outFile(filename);

            if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                return;
            }

            int rows = msgModel.rowCount();
            for (int r = 0 ; r < rows; r++)
            {
                ISOTP_MESSAGE msg = msgModel.getMessage(r);
                QString buildString;
                buildString.append(QString::number(msg.timeStamp().microSeconds()) + " " + QString::number(msg.frameId(), 16) + " ");
                buildString.append(msgModel.getMessageVerbose(r));

                bool result;
                UDS_MESSAGE udsMsg = udsDecoder->tryISOtoUDS(msg, &result);
                if (result)
                    buildString.append(udsDecoder->getDetailedMessageAnalysis(udsMsg));
                buildString.append("\n*********************************************************\n");
                outFile.write(buildString.toUtf8());
            }
        }
    }
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


void ISOTP_InterpreterWindow::showDetailView()
{
    QModelIndexList selection = ui->tableIsoFrames->selectionModel()->selectedRows();
    if (selection.empty()) return;

    int idx = selection[0].row();
    ui->txtFrameDetails->clear();

    ui->txtFrameDetails->setPlainText(msgModel.getMessageVerbose(idx));

    //pass this frame to the UDS decoder to see if it feels it could be a UDS related message
    udsDecoder->gotISOTPFrame(msgModel.getMessage(idx));
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
    if ((msg.reportedLength != msg.payload().length()) && !ui->cbShowIncomplete->isChecked()) return;

    if (idFilters.find(msg.frameId()) == idFilters.end())
    {
        idFilters.insert(msg.frameId(), true);

        FilterUtility::createCheckableFilterItem(msg.frameId(), true, ui->listFilter);
    }
    if (!idFilters[msg.frameId()]) return;

    msgModel.addMessage(msg);
}



int ISOTP_InterpreterModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return messages.count();
}
int ISOTP_InterpreterModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 6;
}
QVariant ISOTP_InterpreterModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole) return {};

    const ISOTP_MESSAGE & msg = messages[index.row()];

    switch (index.column())
    {
    case Column::Time:
        return Utility::formatTimestamp(msg.timeStamp().microSeconds());
    case Column::ID:
        return QString::number(msg.frameId(), 16);
    case Column::Bus:
        return msg.bus;
    case Column::Dir:
        return msg.isReceived ? "Rx" : "Tx";
    case Column::Len:
        return msg.payload().length();
    case Column::Data:
        return getDataString(msg);
    }
    return {};
}

ISOTP_MESSAGE ISOTP_InterpreterModel::getMessage(int idx) const
{
    return messages[idx];
}
QString ISOTP_InterpreterModel::getMessageVerbose(int idx) const
{
    const ISOTP_MESSAGE msg = messages[idx];
    int dataLen = msg.payload().length();

    QString buildString;
    if (msg.reportedLength != dataLen)
    {
        buildString.append("Message didn't have the correct number of bytes.\rExpected "
                           + QString::number(msg.reportedLength) + " got "
                           + QString::number(dataLen) + "\r\r");
    }

    buildString.append(tr("Raw Payload: "));
    buildString.append(getDataString(msg));
    buildString.append("\r\r");

    return buildString;
}


QString ISOTP_InterpreterModel::getDataString(const ISOTP_MESSAGE & msg)
{
    const unsigned char *data = reinterpret_cast<const unsigned char *>(msg.payload().constData());
    int dataLen = msg.payload().length();

    QString tempString;
    for (int i = 0; i < dataLen; i++)
    {
        tempString.append(Utility::formatNumber(data[i]));
        tempString.append(" ");
    }
    return tempString;
}

void ISOTP_InterpreterModel::clear()
{
    emit beginResetModel();
    messages.clear();
    emit endResetModel();
}

void ISOTP_InterpreterModel::addMessage(ISOTP_MESSAGE msg)
{
    int newIdx = messages.size();
    emit beginInsertRows(QModelIndex(), newIdx, newIdx);
    messages.append(msg);
    emit endInsertRows();
}

QVariant ISOTP_InterpreterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        switch (section)
        {
        case Column::Time: return tr("Timestamp");
        case Column::ID: return tr("ID");
        case Column::Bus: return tr("Bus");
        case Column::Dir: return tr("Dir");
        case Column::Len: return tr("Length");
        case Column::Data: return tr("Data");
        }
    }
    return {};
}
