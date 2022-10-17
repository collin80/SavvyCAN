#include "signalviewerwindow.h"
#include "ui_signalviewerwindow.h"
#include "helpwindow.h"
#include "mainwindow.h"
#include <QDebug>

#define MSG_COL     1
#define VALUE_COL   2

SignalViewerWindow::SignalViewerWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SignalViewerWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    modelFrames = frames;

    QStringList headers;
    headers << "Node" << "Signal" << "Value";
    ui->tableViewer->setHorizontalHeaderLabels(headers);
    ui->tableViewer->setColumnWidth(0, 100);
    ui->tableViewer->setColumnWidth(1, 150);

    QSettings settings;
    QFont sysFont;
    int fontSize = settings.value("Main/FontSize", 9).toUInt();
    if(settings.value("Main/FontFixedWidth", false).toBool())
        sysFont = QFontDatabase::systemFont(QFontDatabase::FixedFont); //get default fixed width font
    else
        sysFont = QFont();  //get default font
    sysFont.setPointSize(fontSize);
    ui->tableViewer->setFont(sysFont);

    QHeaderView *HorzHdr = ui->tableViewer->horizontalHeader();
    HorzHdr->setStretchLastSection(true); //causes the data column to automatically fill the tableview
    HorzHdr->setFont(QFont());

    QHeaderView *verticalHeader = ui->tableViewer->verticalHeader();
    verticalHeader->setFont(QFont());

    dbcHandler = DBCHandler::getReference();

    connect(ui->cbNodes, SIGNAL(currentIndexChanged(int)), this, SLOT(loadMessages(int)));
    connect(ui->cbMessages, SIGNAL(currentIndexChanged(int)), this, SLOT(loadSignals(int)));
    connect(ui->btnAdd, SIGNAL(clicked(bool)), this, SLOT(addSignal()));
    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    connect(ui->btnRemove, SIGNAL(clicked(bool)), this, SLOT(removeSelectedSignal()));
    connect(ui->btnSave, SIGNAL(clicked(bool)), this, SLOT(saveSignalsFile()));
    connect(ui->btnLoad, SIGNAL(clicked(bool)), this, SLOT(loadSignalsFile()));
    connect(ui->btnAppend, SIGNAL(clicked(bool)), this, SLOT(appendSignalsFile()));
    connect(ui->btnClear, SIGNAL(clicked(bool)), this, SLOT(clearSignalsTable()));

    loadNodes();
}

SignalViewerWindow::~SignalViewerWindow()
{
    delete ui;
}

void SignalViewerWindow::updatedFrames(int numFrames)
{
    CANFrame thisFrame;

    if (numFrames == -1) //all frames deleted. Don't care
    {
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        for (int i = 0; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);
            processFrame(thisFrame);
        }
    }
    else //just got some new frames. See if they are relevant.
    {
        if (numFrames > modelFrames->count()) return;

        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);
            processFrame(thisFrame);
        }
    }
}

void SignalViewerWindow::processFrame(CANFrame &frame)
{
    QString sigString;
    DBC_SIGNAL *sig;
    for (int i = 0; i < signalList.count(); i++)
    {
        sig = signalList.at(i);
        if (!sig) return;
        if (sig->parentMessage->ID == frame.frameId())
        {
            if (sig->processAsText(frame, sigString, false)) //if true we could interpret the signal so update it in the list
            {
                QTableWidgetItem *item = ui->tableViewer->item(i, VALUE_COL);
                if (!item)
                {
                    item = new QTableWidgetItem(sigString);
                    ui->tableViewer->setItem(i, VALUE_COL, item);
                }
                else item->setText(sigString);
            }
        }
    }
}

void SignalViewerWindow::removeSelectedSignal()
{
    int selRow = ui->tableViewer->currentRow();
    if (selRow < 0) return; //no selected row
    signalList.removeAt(selRow);
    ui->tableViewer->removeRow(selRow);
}

void SetComboBoxItemEnabled(QComboBox * comboBox, int index, bool enabled)
{
    auto * model = qobject_cast<QStandardItemModel*>(comboBox->model());
    assert(model);
    if(!model) return;

    auto * item = model->item(index);
    assert(item);
    if(!item) return;
    item->setEnabled(enabled);
}

void SignalViewerWindow::loadNodes()
{
    int numFiles;
    ui->cbNodes->clear();
    if (dbcHandler == nullptr) return;
    if ((numFiles = dbcHandler->getFileCount()) == 0) return;
    qDebug() << numFiles;
    for (int f = 0; f < numFiles; f++)
    {
        qDebug() << dbcHandler->getFileByIdx(f)->messageHandler->getCount();

        QList<QString> names;

        for (int x = 0; x < dbcHandler->getFileByIdx(f)->dbc_nodes.count(); x++)
        {
            QString name = dbcHandler->getFileByIdx(f)->dbc_nodes[x].name;
            if(name != "Vector__XXX")
                names.append(name);
        }

        if(names.count() > 0)
        {
            names.sort();
            ui->cbNodes->addItem("----" + dbcHandler->getFileByIdx(f)->getFilename());
            SetComboBoxItemEnabled(ui->cbNodes, ui->cbNodes->count() -1, false);
            for(int i=0; i<names.count(); i++)
                ui->cbNodes->addItem(names[i]);
        }
    }
}

void SignalViewerWindow::loadMessages(int idx)
{
    int numFiles;
    ui->cbMessages->clear();
    if (dbcHandler == nullptr) return;
    if ((numFiles = dbcHandler->getFileCount()) == 0) return;
    qDebug() << numFiles;

    QString nodeName = ui->cbNodes->itemText(idx);

    for (int f = 0; f < numFiles; f++)
    {
        qDebug() << dbcHandler->getFileByIdx(f)->messageHandler->getCount();

        for (int x = 0; x < dbcHandler->getFileByIdx(f)->messageHandler->getCount(); x++)
        {
            if(dbcHandler->getFileByIdx(f)->messageHandler->findMsgByIdx(x)->sender->name == nodeName)
                ui->cbMessages->addItem(dbcHandler->getFileByIdx(f)->messageHandler->findMsgByIdx(x)->name);
        }
    }
}

void SignalViewerWindow::loadSignals(int idx)
{
    Q_UNUSED(idx);
    //messages were placed into the list in the same order as they exist
    //in the data structure so it should have been possible to just
    //look it up based on index but by name is probably safer and this operation
    //is not time critical at all.
    DBC_MESSAGE *msg = dbcHandler->findMessage(ui->cbMessages->currentText());

    if (msg == nullptr) return;
    ui->cbSignals->clear();
    for (int x = 0; x < msg->sigHandler->getCount(); x++)
    {
        ui->cbSignals->addItem(msg->sigHandler->findSignalByIdx(x)->name);
    }
}

void SignalViewerWindow::addSignal()
{
    DBC_MESSAGE *msg = dbcHandler->findMessage(ui->cbMessages->currentText());
    if (!msg) return;
    DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(ui->cbSignals->currentText());
    if (!sig) return;

    addSignal(sig);
}

void SignalViewerWindow::addSignal(DBC_SIGNAL *sig)
{
    signalList.append(sig);

    int rowIdx = ui->tableViewer->rowCount();
    ui->tableViewer->insertRow(rowIdx);
    QTableWidgetItem *nodeitem = new QTableWidgetItem(sig->parentMessage->sender->name);
    ui->tableViewer->setItem(rowIdx, 0, nodeitem);
    QTableWidgetItem *msgitem = new QTableWidgetItem(sig->name);
    ui->tableViewer->setItem(rowIdx, 1, msgitem);
}

void SignalViewerWindow::saveSignalsFile()
{
    saveDefinitions();
}

void SignalViewerWindow::loadSignalsFile()
{
    loadDefinitions(false);
}

void SignalViewerWindow::appendSignalsFile()
{
    loadDefinitions(true);
}

void SignalViewerWindow::clearSignalsTable()
{
    clearSignalsTable(true);
}

void SignalViewerWindow::clearSignalsTable(bool askForConfirmation)
{
    if(askForConfirmation)
    {
        QMessageBox::StandardButton confirmDialog;
        confirmDialog = QMessageBox::question(this, "Danger Will Robinson", "Are you sure you want to clear all of your signals?",
                                      QMessageBox::Yes|QMessageBox::No);
        if (confirmDialog == QMessageBox::No)
        {
            return;
        }
    }

    signalList.clear();
    ui->tableViewer->setRowCount(0);
}

void SignalViewerWindow::saveDefinitions()
{
    QString filename;
    QFileDialog dialog(this);
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("SignalViewer definition (*.sdf)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDirectory(settings.value("SignalViewer/LoadSaveDirectory", dialog.directory().path()).toString());

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        settings.setValue("SignalViewer/LoadSaveDirectory", dialog.directory().path());

        if (!filename.contains('.')) filename += ".sdf";

        QFile *outFile = new QFile(filename);

        if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
            return;

        DBC_SIGNAL *sig;
        for (int i = 0; i < signalList.count(); i++)
        {
            sig = signalList.at(i);

            outFile->write("SV1");
            outFile->putChar(',');
            outFile->write(QString::number(sig->parentMessage->ID, 16).toUtf8());
            outFile->putChar(',');
            outFile->write(sig->parentMessage->name.toUtf8());
            outFile->putChar(',');
            outFile->write(sig->name.toUtf8());

            outFile->write("\n");
        }
        outFile->close();
    }
}

void SignalViewerWindow::loadDefinitions(bool append)
{
    QString filename;
    QFileDialog dialog;
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("SignalViewer definition (*.sdf)")));

    QList<DBC_SIGNAL *> loadedSignals;

    if (dbcHandler == nullptr) return;
    if (dbcHandler->getFileCount() == 0) dbcHandler->createBlankFile();

    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setDirectory(settings.value("SignalViewer/LoadSaveDirectory", dialog.directory().path()).toString());

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        settings.setValue("SignalViewer/LoadSaveDirectory", dialog.directory().path());

        QFile *inFile = new QFile(filename);
        QByteArray line;

        if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
            return;

        while (!inFile->atEnd()) {
            line = inFile->readLine().simplified();
            if (line.length() > 2)
            {
                QList<QByteArray> tokens = line.split(',');

                DBC_SIGNAL *sig;

                if (tokens[0] == "SV1") //signal viewer save format v1
                {
                    // = tokens[1].toUInt(nullptr, 16);

                    int msgId = tokens[1].toUInt(nullptr, 16);
                    QString msgName = QString(tokens[2]);
                    QString sigName = QString(tokens[3]);
                    DBC_MESSAGE *msg;;
                    if(msg = dbcHandler->findMessage(msgId))
                    {
                        sig = msg->sigHandler->findSignalByName(sigName);
                        if(sig)
                            loadedSignals.append(sig);
                    }
                    else if (msg  = dbcHandler->findMessage(msgName))
                    {
                        //this is not a very safe way to match since messages names can be duplicated
                        sig = msg->sigHandler->findSignalByName(sigName);
                        if(sig)
                            loadedSignals.append(sig);
                    }
                    else
                    {
                        qDebug() << "Couldn't find the message by name! " << msgName << "  " << sigName;
                    }
                }
            }
        }
        inFile->close();

        if(loadedSignals.count() > 0)
        {
            if(append == false)
            {
                clearSignalsTable(false);
            }

            for (int i=0; i<loadedSignals.count(); i++)
            {
                addSignal(loadedSignals[i]);
            }
        }
    }
}
