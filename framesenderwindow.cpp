#include "framesenderwindow.h"
#include "ui_framesenderwindow.h"
#include "utility.h"

#include <QFileDialog>
#include <QDebug>
#include "mainwindow.h"
#include "helpwindow.h"
#include "connections/canconmanager.h"
#include "triggerdialog.h"

/*
 * notes: need to ensure that you grab pointers when modifying data structures and dont
 * make copies. Also, check # of ms elapsed since last tick since they don't tend to come in at 1ms
 * intervals like we asked.
 * Also, rows default to enabled which is odd because the button state does not reflect that.
*/

FrameSenderWindow::FrameSenderWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FrameSenderWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    modelFrames = frames;

    dbcHandler = DBCHandler::getReference();

    intervalTimer = new QTimer();
    intervalTimer->setTimerType(Qt::PreciseTimer);
    intervalTimer->setInterval(1);

    setupGrid();
    createBlankRow();

    connect(ui->tableSender, SIGNAL(cellChanged(int,int)), this, SLOT(onCellChanged(int,int)));
    connect(ui->tableSender, SIGNAL(cellDoubleClicked(int, int)), SLOT(onCellDoubleTap(int, int)));
    connect(intervalTimer, SIGNAL(timeout()), this, SLOT(handleTick()));
    connect(ui->btnClearGrid, SIGNAL(clicked(bool)), this, SLOT(clearGrid()));
    connect(ui->btnDisableAll, SIGNAL(clicked(bool)), this, SLOT(disableAll()));
    connect(ui->btnEnableAll, SIGNAL(clicked(bool)), this, SLOT(enableAll()));
    connect(ui->btnLoadGrid, SIGNAL(clicked(bool)), this, SLOT(loadGrid()));
    connect(ui->btnSaveGrid, SIGNAL(clicked(bool)), this, SLOT(saveGrid()));
    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));

    intervalTimer->start();
    elapsedTimer.start();
    installEventFilter(this);
}

void FrameSenderWindow::setupGrid()
{
    QStringList headers;
    headers << "En" << "Bus" << "ID" << "MsgName" << "Len" << "Ext" << "Rem" << "Data"
            << "Trigger" << "Modifications" << "Count";
    ui->tableSender->setColumnCount(11);
    ui->tableSender->setColumnWidth(ST_COLS::SENDTAB_COL_EN, 50);
    ui->tableSender->setColumnWidth(ST_COLS::SENDTAB_COL_BUS, 50);
    ui->tableSender->setColumnWidth(ST_COLS::SENDTAB_COL_ID, 50);
    ui->tableSender->setColumnWidth(ST_COLS::SENDTAB_COL_MSGNAME, 150);
    ui->tableSender->setColumnWidth(ST_COLS::SENDTAB_COL_LEN, 50);
    ui->tableSender->setColumnWidth(ST_COLS::SENDTAB_COL_EXT, 50);
    ui->tableSender->setColumnWidth(ST_COLS::SENDTAB_COL_REM, 50);
    ui->tableSender->setColumnWidth(ST_COLS::SENDTAB_COL_DATA, 220);
    ui->tableSender->setColumnWidth(ST_COLS::SENDTAB_COL_TRIGGER, 220);
    ui->tableSender->setColumnWidth(ST_COLS::SENDTAB_COL_MODS, 220);
    ui->tableSender->setColumnWidth(ST_COLS::SENDTAB_COL_COUNT, 80);
    ui->tableSender->setHorizontalHeaderLabels(headers);
}

FrameSenderWindow::~FrameSenderWindow()
{
    removeEventFilter(this);
    delete ui;

    intervalTimer->stop();
    delete intervalTimer;
}

bool FrameSenderWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("customsender.md");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void FrameSenderWindow::createBlankRow()
{
    int row = ui->tableSender->rowCount();
    ui->tableSender->insertRow(row);

    QTableWidgetItem *item = new QTableWidgetItem();
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
    inhibitChanged = true;
    ui->tableSender->setItem(row, ST_COLS::SENDTAB_COL_EN, item);

    item = new QTableWidgetItem();
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
    ui->tableSender->setItem(row, ST_COLS::SENDTAB_COL_EXT, item);

    item = new QTableWidgetItem();
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
    ui->tableSender->setItem(row, ST_COLS::SENDTAB_COL_REM, item);

    for (int i = 1; i <= ST_COLS::SENDTAB_COL_COUNT; i++)
    {
        if (i != ST_COLS::SENDTAB_COL_EXT && i != ST_COLS::SENDTAB_COL_REM) {
            item = new QTableWidgetItem("");
            //msgname is looked up via DBC interface and not able to be edited.
            //Though, it might be perhaps interesting to allow renaming it to
            //a shorter name in some cases? Jury is still out on this one...
            if (i == ST_COLS::SENDTAB_COL_MSGNAME) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui->tableSender->setItem(row, i, item);
         }
    }

    inhibitChanged = false;
}

void FrameSenderWindow::buildFrameCache()
{
    CANFrame thisFrame;
    frameCache.clear();
    for (int i = 0; i < modelFrames->count(); i++)
    {
        thisFrame = modelFrames->at(i);
        if (!frameCache.contains(thisFrame.frameId()))
        {
            frameCache.insert(thisFrame.frameId(), thisFrame);
        }
        else
        {
            frameCache[thisFrame.frameId()] = thisFrame;
        }
    }
}

//remember, negative numbers are special -1 = all frames deleted, -2 = totally new set of frames.
void FrameSenderWindow::updatedFrames(int numFrames)
{
    CANFrame thisFrame;
    if (numFrames == -1) //all frames deleted.
    {
    }
    else if (numFrames == -2) //all new set of frames.
    {
        buildFrameCache();
    }
    else //just got some new frames. See if they are relevant.
    {
        if (numFrames > modelFrames->count()) return;
        qDebug() << "New frames in sender window";
        //run through the supposedly new frames in order
        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);
            if (!frameCache.contains(thisFrame.frameId()))
            {
                frameCache.insert(thisFrame.frameId(), thisFrame);
            }
            else
            {
                frameCache[thisFrame.frameId()] = thisFrame;
            }
            processIncomingFrame(&thisFrame);
        }
    }
}

void FrameSenderWindow::processIncomingFrame(CANFrame *frame)
{
    for (int sd = 0; sd < sendingData.count(); sd++)
    {
        if (sendingData[sd].triggers.count() == 0) continue;
        bool passedChecks = true;
        for (int trig = 0; trig < sendingData[sd].triggers.count(); trig++)
        {
            Trigger *thisTrigger = &sendingData[sd].triggers[trig];
            //need to ensure that this trigger is actually related to frames incoming.
            //Otherwise ignore it here. Only frames that have BUS and/or ID set as trigger will work here.
            if (!(thisTrigger->triggerMask & (TriggerMask::TRG_BUS | TriggerMask::TRG_ID) ) ) continue;

            passedChecks = true;
            //qDebug() << "Trigger ID: " << thisTrigger->ID;
            //qDebug() << "Frame ID: " << frame->frameId();

            //Check to see if we have a bus trigger condition and if so does it match
            if (thisTrigger->bus != frame->bus && (thisTrigger->triggerMask & TriggerMask::TRG_BUS) )
                passedChecks = false;

            //check to see if we have an ID trigger condition and if so does it match
            if ((thisTrigger->triggerMask & TriggerMask::TRG_ID) && (uint32_t)thisTrigger->ID != frame->frameId() )
                passedChecks = false;

            //check to see if we're limiting the trigger by max count and have we reached that count?
            if ( (thisTrigger->triggerMask & TriggerMask::TRG_COUNT) && (thisTrigger->currCount >= thisTrigger->maxCount) )
                passedChecks = false;

            //if the above passed then are we triggering not only on ID but also signal?
            if (passedChecks && (thisTrigger->triggerMask & (TriggerMask::TRG_SIGNAL | TriggerMask::TRG_ID) ) )
            {
                bool sigCheckPassed = false;
                DBC_MESSAGE *msg = dbcHandler->findMessage(thisTrigger->ID);
                if (msg)
                {
                    DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(thisTrigger->sigName);
                    if (sig)
                    {
                        //first of all, is this signal really in this message we got?
                        if (sig->isSignalInMessage(*frame)) sigCheckPassed = true;
                        //if it was and we're also filtering on value then try that next
                        if (sigCheckPassed && (thisTrigger->triggerMask & TriggerMask::TRG_SIGVAL))
                        {
                            double sigval = 0.0;
                            if (sig->processAsDouble(*frame, sigval))
                            {
                                if (abs(sigval - thisTrigger->sigValueDbl) > 0.001)
                                {
                                    sigCheckPassed = false;
                                }
                            }
                            else sigCheckPassed = false;
                        }
                    }
                    else sigCheckPassed = false;
                }
                else sigCheckPassed = false;
                passedChecks &= sigCheckPassed; //passedChecks can only be true if both are
            }

            //if all the above says it's OK then we'll go ahead and send that message.
            //If a message that comes through here has a MS value then we use it as a delay
            //after the check passes. This allows for delaying the sending of the frame if that
            //is required. Otherwise, just send it immediately.
            if (passedChecks)
            {
                if (thisTrigger->milliseconds == 0) //immediate reply
                {
                    thisTrigger->currCount++;
                    sendingData[sd].count++;
                    doModifiers(sd);
                    updateGridRow(sd);
                    CANConManager::getInstance()->sendFrame(sendingData[sd]);
                }
                else //delayed sending frame
                {
                    thisTrigger->readyCount = true;
                }
            }
        }
    }
}

void FrameSenderWindow::enableAll()
{
    for (int i = 0; i < ui->tableSender->rowCount() - 1; i++)
    {
        ui->tableSender->item(i, ST_COLS::SENDTAB_COL_EN)->setCheckState(Qt::Checked);
        sendingData[i].enabled = true;
    }
}

void FrameSenderWindow::disableAll()
{
    for (int i = 0; i < ui->tableSender->rowCount() - 1; i++)
    {
        ui->tableSender->item(i, ST_COLS::SENDTAB_COL_EN)->setCheckState(Qt::Unchecked);
        sendingData[i].enabled = false;
    }
}

void FrameSenderWindow::clearGrid()
{
    if (ui->tableSender->rowCount() == 1) return;
    for (int i = ui->tableSender->rowCount() - 2; i >= 0; i--)
    {
        sendingData[i].enabled = false;
        sendingData.removeAt(i);
        ui->tableSender->removeRow(i);
    }
}

void FrameSenderWindow::saveGrid()
{
    QString filename;
    QFileDialog dialog(this);
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("Frame Sender Definition (*.fsd)")));

    dialog.setDirectory(settings.value("FrameSender/LoadSaveDirectory", dialog.directory().path()).toString());
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];

        if (dialog.selectedNameFilter() == filters[0])
        {
            if (!filename.contains('.')) filename += ".fsd";
            saveSenderFile(filename);
            settings.setValue("FrameSender/LoadSaveDirectory", dialog.directory().path());
        }
    }
}

void FrameSenderWindow::loadGrid()
{
    QString filename;
    QFileDialog dialog(this);
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("Frame Sender Definition (*.fsd)")));

    dialog.setDirectory(settings.value("FrameSender/LoadSaveDirectory", dialog.directory().path()).toString());
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];

        if (dialog.selectedNameFilter() == filters[0])
        {
            loadSenderFile(filename);
            settings.setValue("FrameSender/LoadSaveDirectory", dialog.directory().path());
        }
    }

    createBlankRow();

    setupGrid();
}

void FrameSenderWindow::saveSenderFile(QString filename)
{
    QFile *outFile = new QFile(filename);
    QString outString;

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete outFile;
        return;
    }

    for (int c = 0; c < sendingData.count(); c++)
    {
        outString.clear();
        if (ui->tableSender->item(c, ST_COLS::SENDTAB_COL_EN)->checkState() == Qt::Checked)
        {
            outString = "T#";
        }
        else outString = "F#";
        for (int i = 1; i < ST_COLS::SENDTAB_COL_COUNT; i++)
        {
            if (i == ST_COLS::SENDTAB_COL_EXT || i == ST_COLS::SENDTAB_COL_REM) {
                if (ui->tableSender->item(c, i)->checkState() == Qt::Checked) {
                    outString.append("T");
                } else {
                    outString.append("F");
                }
            } else {
                outString.append(ui->tableSender->item(c, i)->text());
            }
            outString.append("#");
        }
        outString.append("\n");
        outFile->write(outString.toUtf8());
    }

    outFile->close();
    delete outFile;
}

void FrameSenderWindow::loadSenderFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return;
    }

    ui->tableSender->clear();
    while (ui->tableSender->rowCount() > 0) ui->tableSender->removeRow(0);
    sendingData.clear();

    while (!inFile->atEnd()) {
        inhibitChanged = true;
        line = inFile->readLine().simplified();
        if (line.length() > 2)
        {
            QList<QByteArray> tokens = line.split('#');
            int row = ui->tableSender->rowCount();
            ui->tableSender->insertRow(row);
            QTableWidgetItem *item = new QTableWidgetItem();
            ui->tableSender->setItem(row, ST_COLS::SENDTAB_COL_EN, item);
            ui->tableSender->item(row, ST_COLS::SENDTAB_COL_EN)->setFlags(item->flags() | Qt::ItemIsUserCheckable);

            item = new QTableWidgetItem();
            ui->tableSender->setItem(row, ST_COLS::SENDTAB_COL_EXT, item);
            ui->tableSender->item(row, ST_COLS::SENDTAB_COL_EN)->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);

            item = new QTableWidgetItem();
            ui->tableSender->setItem(row, ST_COLS::SENDTAB_COL_REM, item);
            ui->tableSender->item(row, ST_COLS::SENDTAB_COL_REM)->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);

            if (tokens[0] == "T")
            {
                ui->tableSender->item(row, ST_COLS::SENDTAB_COL_EN)->setCheckState(Qt::Checked);
            }
            else ui->tableSender->item(row, ST_COLS::SENDTAB_COL_EN)->setCheckState(Qt::Unchecked);
            if (tokens.length() >= 9) {
                for (int i = 1; i < ST_COLS::SENDTAB_COL_COUNT; i++)
                {
                    if (i != ST_COLS::SENDTAB_COL_EXT && i != ST_COLS::SENDTAB_COL_REM) {
                        ui->tableSender->setItem(row, i, new QTableWidgetItem(QString(tokens[i])));
                    } else {
                        if (tokens[i] == "T") {
                            ui->tableSender->item(row, i)->setCheckState(Qt::Checked);
                        }
                    }
                }
            } else {
                ui->tableSender->setItem(row, ST_COLS::SENDTAB_COL_BUS, new QTableWidgetItem(QString(tokens[1])));
                ui->tableSender->setItem(row, ST_COLS::SENDTAB_COL_ID, new QTableWidgetItem(QString(tokens[2])));
                ui->tableSender->setItem(row, ST_COLS::SENDTAB_COL_LEN, new QTableWidgetItem(QString(tokens[3])));
                ui->tableSender->setItem(row, ST_COLS::SENDTAB_COL_DATA, new QTableWidgetItem(QString(tokens[4])));
                ui->tableSender->setItem(row, ST_COLS::SENDTAB_COL_TRIGGER, new QTableWidgetItem(QString(tokens[5])));
                ui->tableSender->setItem(row, ST_COLS::SENDTAB_COL_MODS, new QTableWidgetItem(QString(tokens[6])));
            }
            inhibitChanged = false;
            for (int k = 0; k < ST_COLS::SENDTAB_COL_COUNT; k++) processCellChange(row, k);

        }
    }
    inFile->close();
    delete inFile;
}

void FrameSenderWindow::onCellDoubleTap(int row, int column)
{
    if (column == ST_COLS::SENDTAB_COL_TRIGGER)
    {
        if (row == ui->tableSender->rowCount() - 1)
        {
            createBlankRow();
        }
        if (row >= sendingData.count())
        {
            FrameSendData tempData;
            tempData.enabled = false;
            tempData.setFrameType(QCanBusFrame::DataFrame);
            tempData.setExtendedFrameFormat(false);
            sendingData.append(tempData);
        }
        sendData = nullptr;
        sendData = &sendingData[row];
        td = new TriggerDialog(sendData->triggers);
        if (td->exec() == QDialog::Accepted)
        {
            sendData->triggers.clear();
            sendData->triggers.append(td->getUpdatedTriggers());
            //now have to generate the actual trigger text
            QString output;
            foreach (Trigger trig, sendData->triggers)
            {
                output += td->buildEntry(trig) + ",";
            }
            output.chop(1); //don't want the trailing ,
            inhibitChanged = true;
            ui->tableSender->item(row, ST_COLS::SENDTAB_COL_TRIGGER)->setText(output);
            inhibitChanged = false;
        }
        delete td;
        td = nullptr;
    }
}

void FrameSenderWindow::onCellChanged(int row, int col)
{
    if (inhibitChanged) return;
    qDebug() << "onCellChanged";
    if (row == ui->tableSender->rowCount() - 1)
    {
        createBlankRow();
    }

    processCellChange(row, col);
}

/// <summary>
/// Called every millisecond to set the system update figures and send frames if necessary.
/// </summary>
/// <param name="sender"></param>
/// <param name="timerEventArgs"></param>
void FrameSenderWindow::handleTick()
{
    FrameSendData *sendData;
    Trigger *trigger;
    if(mutex.tryLock())
    {
        int elapsed = elapsedTimer.restart();
        if (elapsed == 0) elapsed = 1;
        //Modifier modifier;
        for (int i = 0; i < sendingData.count(); i++)
        {
            sendData = &sendingData[i];
            if (!sendData->enabled)
            {
                if (sendData->triggers.count() > 0)
                {
                    for (int j = 0; j < sendData->triggers.count(); j++)    //resetting currCount when line is disabled
                    {
                      sendData->triggers[j].currCount = 0;
                    }
                }
                continue; //abort any processing on this if it is not enabled.
            }
            if (sendData->triggers.count() == 0) break;
            for (int j = 0; j < sendData->triggers.count(); j++)
            {
                trigger = &sendData->triggers[j];
                if (trigger->currCount >= trigger->maxCount) continue; //don't process if we've sent max frames we were supposed to
                if (!trigger->readyCount) continue; //don't tick if not ready to tick
                //is it time to fire?
                trigger->msCounter += elapsed; //gives proper tracking even if timer doesn't fire as fast as it should
                if (trigger->msCounter >= trigger->milliseconds)
                {
                    trigger->msCounter = 0;
                    sendData->count++;
                    trigger->currCount++;
                    doModifiers(i);
                    updateGridRow(i);
                    //qDebug() << "About to try to send a frame";
                    CANConManager::getInstance()->sendFrame(sendingData[i]);
                    if (trigger->ID > 0) trigger->readyCount = false; //reset flag if this is a timed ID trigger
                }
            }
        }

        mutex.unlock();
    }
    else
    {
        qDebug() << "framesenderwindow::handleTick() couldn't get mutex, elapsed is: " << elapsedTimer.elapsed();
    }

}

/// <summary>
/// given an index into the sendingData list we run the modifiers that it has set up
/// </summary>
/// <param name="idx">The index into the sendingData list</param>
void FrameSenderWindow::doModifiers(int idx)
{
    int shadowReg = 0; //shadow register we use to accumulate results
    int first=0, second=0;

    FrameSendData *sendData = &sendingData[idx];
    Modifier *mod;
    ModifierOp op;

    if (sendData->modifiers.count() == 0) return; //if no modifiers just leave right now

    //qDebug() << "Executing mods";

    for (int i = 0; i < sendData->modifiers.count(); i++)
    {
        mod = &sendData->modifiers[i];
        for (int j = 0; j < mod->operations.count(); j++)
        {
            op = mod->operations.at(j);
            if (op.first.ID == -1)
            {
                first = shadowReg;
            }
            else first = fetchOperand(idx, op.first);
            second = fetchOperand(idx, op.second);
            switch (op.operation)
            {
            case ADDITION:
                shadowReg = first + second;
                break;
            case AND:
                shadowReg = first & second;
                break;
            case DIVISION:
                shadowReg = first / second;
                break;
            case MULTIPLICATION:
                shadowReg = first * second;
                break;
            case OR:
                shadowReg = first | second;
                break;
            case SUBTRACTION:
                shadowReg = first - second;
                break;
            case XOR:
                shadowReg = first ^ second;
                break;
            case MOD:
                shadowReg = first % second;
            }
        }
        //Finally, drop the result into the proper data byte
        QByteArray newArr(sendData->payload());
        newArr[mod->destByte] = (char) shadowReg;
        sendData->setPayload(newArr);
    }
}

int FrameSenderWindow::fetchOperand(int idx, ModifierOperand op)
{
    CANFrame *tempFrame = nullptr;
    if (op.ID == 0) //numeric constant
    {
        if (op.notOper) return ~op.databyte;
        else return op.databyte;
    }
    else if (op.ID == -2) //fetch data from a data byte within the output frame
    {
        if (op.notOper) return ~((unsigned char)sendingData.at(idx).payload()[op.databyte]);
        else return (unsigned char)sendingData.at(idx).payload()[op.databyte];
    }
    else //look up external data byte
    {
        tempFrame = lookupFrame(op.ID, op.bus);
        if (tempFrame != nullptr)
        {
            if (op.notOper) return ~((unsigned char)tempFrame->payload()[op.databyte]);
            else return (unsigned char)tempFrame->payload()[op.databyte];
        }
        else return 0;
    }
}

/// <summary>
/// Try to find the most recent frame given the input criteria
/// </summary>
/// <param name="ID">The ID to find</param>
/// <param name="bus">Which bus to look on (-1 if you don't care)</param>
/// <returns></returns>
CANFrame* FrameSenderWindow::lookupFrame(int ID, int bus)
{
    if (!frameCache.contains(ID)) return nullptr;

    if (bus == -1 || frameCache[ID].bus == bus) return &frameCache[ID];

    return nullptr;
}

/// <summary>
/// Process a single line from the dataGrid. Right now it seems to not trigger at all after the first adding of the code but that seems to maybe
/// be because whichever field you where just in will show up as nothing to the code.
/// </summary>
/// <param name="line"></param>
void FrameSenderWindow::processModifierText(int line)
{
    qDebug() << "processModifierText";
    QString modString;
    //bool firstOp = true;
    bool abort = false;
    QString token;
    ModifierOp thisOp;

    //Example line:
    //d0 = D0 + 1,d1 = id:0x200:d3 + id:0x200:d4 AND 0xF0 - Original version
    //D0=D0+1,D1=ID:0x200:D3+ID:0x200:D4&0xF0
    //This is certainly much harder to parse than the trigger definitions.
    //the left side of the = has to be D0 to D7. After that there is a string of
    //data. Spaces used to be required but no longer are. This makes parsing harder but data entry easier

    //yeah, lots of operations on this one line but it's for a good cause. Removes the convenience English versions of the
    //logical operators and replaces them with the math equivs. Also uppercases and removes all superfluous whitespace
    modString = ui->tableSender->item(line, 8)->text().toUpper().trimmed().replace("AND", "&").replace("XOR", "^").replace("OR", "|").replace(" ", "");
    if (modString != "")
    {
        QStringList mods = modString.split(',');
        sendingData[line].modifiers.clear();
        sendingData[line].modifiers.reserve(mods.length());
        for (int i = 0; i < mods.length(); i++)
        {
            Modifier thisMod;
            thisMod.destByte = 0;

            QString leftSide = Utility::grabAlphaNumeric(mods[i]);
            if (leftSide.startsWith("D") && leftSide.length() == 2)
            {
                thisMod.destByte = leftSide.right(1).toInt();
                thisMod.operations.clear();
            }
            else
            {
                qDebug() << "Something wrong with lefthand val";
                continue;
            }
            if (!(Utility::grabOperation(mods[i]) == "="))
            {
                qDebug() << "Err: No = after lefthand val";
                continue;
            }
            abort = false;

            token = Utility::grabAlphaNumeric(mods[i]);
            if (token[0] == '~')
            {
                thisOp.first.notOper = true;
                token = token.remove(0, 1); //remove the ~ character
            }
            else thisOp.first.notOper = false;
            parseOperandString(token.split(":"), thisOp.first);

            if (mods[i].length() < 2) {
                abort = true;
                thisOp.operation = ADDITION;
                thisOp.second.ID = 0;
                thisOp.second.databyte = 0;
                thisOp.second.notOper = false;
                thisMod.operations.append(thisOp);
            }

            while (!abort)
            {
                QString operation = Utility::grabOperation(mods[i]);
                if (operation == "")
                {
                    abort = true;
                }
                else
                {
                    thisOp.operation = parseOperation(operation);
                    QString secondOp = Utility::grabAlphaNumeric(mods[i]);
                    if (secondOp.length() > 0 && secondOp[0] == '~')
                    {
                        thisOp.second.notOper = true;
                        secondOp = secondOp.remove(0, 1); //remove the ~ character
                    }
                    else thisOp.second.notOper = false;
                    thisOp.second.bus = sendingData[line].bus;
                    thisOp.second.ID = sendingData[line].frameId();
                    parseOperandString(secondOp.split(":"), thisOp.second);
                    thisMod.operations.append(thisOp);
                }

                thisOp.first.ID = -1; //shadow register
                if (mods[i].length() < 2) abort = true;
            }

            sendingData[line].modifiers.append(thisMod);
        }
    }
    //there is no else for the modifiers. We'll accept there not being any
}

void FrameSenderWindow::processTriggerText(int line)
{
    qDebug() << "processTriggerText";
    QString trigger;

    //Example line:
    //id0x200 5ms 10x bus0,1000ms,ID0x202 SIG[BMS_maxChargeCurrent;300]
    //trigger has two levels of syntactic parsing. First you split by comma to get each
    //actual trigger. Then you split by spaces to get the tokens within each trigger
    trigger = ui->tableSender->item(line, ST_COLS::SENDTAB_COL_TRIGGER)->text().toUpper();
    if (trigger != "")
    {
        QStringList triggers = trigger.split(',');
        sendingData[line].triggers.clear();
        sendingData[line].triggers.reserve(triggers.length());
        for (int k = 0; k < triggers.length(); k++)
        {
            Trigger thisTrigger;
            //start out by setting defaults - should be moved to constructor for class Trigger.
            thisTrigger.bus = -1; //-1 means we don't care which
            thisTrigger.ID = -1; //the rest of these being -1 means nothing has changed it
            thisTrigger.maxCount = -1;
            thisTrigger.milliseconds = -1;
            thisTrigger.currCount = 0;
            thisTrigger.msCounter = 0;
            thisTrigger.triggerMask = 0;
            thisTrigger.readyCount = true;

            QStringList trigToks = triggers[k].split(' ');
            for (int x = 0; x < trigToks.length(); x++)
            {
                QString tok = trigToks.at(x);
                if (tok.left(2) == "ID")
                {
                    thisTrigger.ID = Utility::ParseStringToNum(tok.right(tok.length() - 2));
                    //if (thisTrigger.maxCount == -1) thisTrigger.maxCount = 0;

                    if (thisTrigger.milliseconds == -1) thisTrigger.milliseconds = 0; //by default don't count, just send it upon trigger
                    thisTrigger.readyCount = false; //won't try counting until trigger hits
                    thisTrigger.triggerMask |= TriggerMask::TRG_ID;
                }
                else if (tok.endsWith("MS"))
                {
                    thisTrigger.milliseconds = Utility::ParseStringToNum(tok.left(tok.length()-2));
                    thisTrigger.triggerMask |= TriggerMask::TRG_MS;
                    //if (thisTrigger.maxCount == -1) thisTrigger.maxCount = 0;
                    //if (thisTrigger.ID == -1) thisTrigger.ID = 0;
                }
                else if (tok.endsWith("X"))
                {
                    thisTrigger.maxCount = Utility::ParseStringToNum(tok.left(tok.length() - 1));
                    thisTrigger.triggerMask |= TriggerMask::TRG_COUNT;
                    //if (thisTrigger.ID == -1) thisTrigger.ID = 0;
                    if (thisTrigger.milliseconds == -1)
                    {
                        thisTrigger.milliseconds = 10;
                        thisTrigger.triggerMask |= TriggerMask::TRG_ID;
                    }
                }
                else if (tok.startsWith("BUS"))
                {
                    thisTrigger.bus = Utility::ParseStringToNum(tok.right(tok.length() - 3));
                    thisTrigger.triggerMask |= TriggerMask::TRG_BUS;
                }
                else if (tok.startsWith("SIG["))
                {
                    //remove the SIG[ header
                    tok = tok.right(tok.size() - 4);
                    int semi = tok.indexOf(';');
                    if (semi == -1) semi = tok.indexOf(']');
                    QString signame = tok.left(semi);
                    thisTrigger.sigName = signame;
                    thisTrigger.triggerMask |= TriggerMask::TRG_SIGNAL;
                    semi = tok.indexOf(';');
                    if (semi > -1) //have a signal value to match too
                    {
                        thisTrigger.triggerMask |= TriggerMask::TRG_SIGVAL;
                        int end = tok.indexOf(']');
                        thisTrigger.sigValueDbl =
                                tok.mid(semi + 1, end - semi - 1)
                                .toDouble();
                    }
                }
            }
            //now, find anything that wasn't set and set it to defaults
            //if (thisTrigger.maxCount == -1) thisTrigger.maxCount = 0;
            //if (thisTrigger.milliseconds == -1) thisTrigger.milliseconds = 100;
            //if (thisTrigger.ID == -1) thisTrigger.ID = 0;
            sendingData[line].triggers.append(thisTrigger);
        }
    }
    else //setup a default single shot trigger
    {
        Trigger thisTrigger;
        thisTrigger.bus = -1;
        thisTrigger.ID = 0;
        thisTrigger.maxCount = 1;
        thisTrigger.milliseconds = 10;
        thisTrigger.triggerMask = TriggerMask::TRG_MS;
        sendingData[line].triggers.append(thisTrigger);
    }
}

//Turn a set of tokens into an operand
void FrameSenderWindow::parseOperandString(QStringList tokens, ModifierOperand &operand)
{
    qDebug() << "parseOperandString";
    //example string -> bus:0:id:200:d3

    operand.bus = -1;
    operand.ID = -2;
    operand.databyte = 0;

    for (int i = 0; i < tokens.length(); i++)
    {
        if (tokens[i] == "BUS")
        {
            operand.bus = Utility::ParseStringToNum(tokens[++i]);
        }
        else if (tokens[i] == "ID")
        {
            operand.ID = Utility::ParseStringToNum(tokens[++i]);
        }
        else if (tokens[i].length() == 2 && tokens[i].startsWith("D"))
        {
            operand.databyte = Utility::ParseStringToNum(tokens[i].right(tokens[i].length() - 1));
        }
        else
        {
            operand.databyte = Utility::ParseStringToNum(tokens[i]);
            operand.ID = 0; //special ID to show this is a number not a look up.
        }
    }
}

ModifierOperationType FrameSenderWindow::parseOperation(QString op)
{
    qDebug() << "parseOperation";
    if (op == "+") return ADDITION;
    if (op == "-") return SUBTRACTION;
    if (op == "*") return MULTIPLICATION;
    if (op == "/") return DIVISION;
    if (op == "&") return AND;
    if (op == "|") return OR;
    if (op == "^") return XOR;
    if (op == "%") return MOD;
    return ADDITION;
}

/// <summary>
/// Update the DataGridView with the newest info from sendingData
/// </summary>
/// <param name="idx"></param>
void FrameSenderWindow::updateGridRow(int idx)
{
    //qDebug() << "updateGridRow";

    inhibitChanged = true;
    FrameSendData *temp = &sendingData[idx];
    int gridLine = idx;
    QString dataString;
    QTableWidgetItem *item = ui->tableSender->item(gridLine, ST_COLS::SENDTAB_COL_COUNT);
    const unsigned char *data = reinterpret_cast<const unsigned char *>(temp->payload().constData());
    int dataLen = temp->payload().length();

    if (item == nullptr)
    {
        item = new QTableWidgetItem();
        item->setText(QString::number(temp->count));
        ui->tableSender->setItem(gridLine, ST_COLS::SENDTAB_COL_COUNT, item);
    }
    else
    {
        item->setText(QString::number(temp->count));
    }

    if (temp->frameType() != QCanBusFrame::RemoteRequestFrame)
    {
        for (int i = 0; i < dataLen; i++)
        {
            dataString.append(Utility::formatNumber(data[i]));
            dataString.append(" ");
        }
        ui->tableSender->item(gridLine, ST_COLS::SENDTAB_COL_DATA)->setText(dataString);
    }
    else
    {
        ui->tableSender->item(gridLine, ST_COLS::SENDTAB_COL_DATA)->setText("");
    }
    inhibitChanged = false;
}

void FrameSenderWindow::processCellChange(int line, int col)
{
    qDebug() << "processCellChange";
    FrameSendData tempData;
    QStringList tokens;
    int tempVal;
    DBC_MESSAGE *msg;

    //if this is a new line then create the base object for the line
    if (line >= sendingData.count())
    {
        FrameSendData tempData;
        tempData.enabled = false;
        tempData.setFrameType(QCanBusFrame::DataFrame);
        tempData.setExtendedFrameFormat(false);
        sendingData.append(tempData);
    }

    sendingData[line].count = 0;

    int numBuses = CANConManager::getInstance()->getNumBuses();
    QByteArray arr;

    switch (col)
    {
        case ST_COLS::SENDTAB_COL_EN: //Enable check box
            if (ui->tableSender->item(line, 0)->checkState() == Qt::Checked)
            {
                sendingData[line].enabled = true;
            }
            else sendingData[line].enabled = false;
            qDebug() << "Setting enabled to " << sendingData[line].enabled;
            break;
        case ST_COLS::SENDTAB_COL_BUS: //Bus designation
            tempVal = Utility::ParseStringToNum(ui->tableSender->item(line, ST_COLS::SENDTAB_COL_BUS)->text());
            if (tempVal < -1) tempVal = -1;
            if (tempVal >= numBuses) tempVal = numBuses - 1;
            sendingData[line].bus = tempVal;
            qDebug() << "Setting bus to " << tempVal;
            break;
        case ST_COLS::SENDTAB_COL_ID: //ID field
            tempVal = Utility::ParseStringToNum(ui->tableSender->item(line, ST_COLS::SENDTAB_COL_ID)->text());
            if (tempVal < 0) tempVal = 0;
            if (tempVal > 0x7FFFFFFF) tempVal = 0x7FFFFFFF;
            sendingData[line].setFrameId(tempVal);
            if (sendingData[line].frameId() > 0x7FF) {
                sendingData[line].setExtendedFrameFormat(true);
                ui->tableSender->blockSignals(true);
                ui->tableSender->item(line, ST_COLS::SENDTAB_COL_EXT)->setCheckState(Qt::Checked);
                ui->tableSender->blockSignals(false);
            }
            msg = dbcHandler->findMessage(sendingData[line].frameId());
            if (msg)
            {
                ui->tableSender->blockSignals(true);
                ui->tableSender->item(line, ST_COLS::SENDTAB_COL_MSGNAME)->setText(msg->name);
                ui->tableSender->item(line, ST_COLS::SENDTAB_COL_LEN)->setText(QString::number(msg->len));
                ui->tableSender->blockSignals(false);
            }
            qDebug() << "setting ID to " << tempVal;
            break;
        case ST_COLS::SENDTAB_COL_LEN:
            tempVal = Utility::ParseStringToNum(ui->tableSender->item(line, 3)->text());
            if (tempVal < 0) tempVal = 0;
            if (tempVal > 8) tempVal = 8;            
            arr.resize(tempVal);
            sendingData[line].setPayload(arr);
            break;
        case ST_COLS::SENDTAB_COL_EXT:
            if (ui->tableSender->item(line, 4)->checkState() == Qt::Checked) {
                sendingData[line].setExtendedFrameFormat(true);
            } else {
                sendingData[line].setExtendedFrameFormat(false);
            }
            break;
        case ST_COLS::SENDTAB_COL_REM:
            if (ui->tableSender->item(line, 5)->checkState() == Qt::Checked) {
                sendingData[line].setFrameType(QCanBusFrame::RemoteRequestFrame);
            } else {
                sendingData[line].setFrameType(QCanBusFrame::DataFrame);
            }
            break;
        case ST_COLS::SENDTAB_COL_DATA: //Data bytes
#if QT_VERSION >= QT_VERSION_CHECK( 5, 14, 0 )
            tokens = ui->tableSender->item(line, ST_COLS::SENDTAB_COL_DATA)->text().split(" ", Qt::SkipEmptyParts);
#else
            tokens = ui->tableSender->item(line, ST_COLS::SENDTAB_COL_DATA)->text().split(" ", QString::SkipEmptyParts);
#endif
            arr.clear();
            arr.reserve(tokens.count());
            for (int j = 0; j < tokens.count(); j++)
            {
                arr.append((uint8_t)Utility::ParseStringToNum(tokens[j]));
            }
            sendingData[line].setPayload(arr);
            break;
        case ST_COLS::SENDTAB_COL_TRIGGER: //triggers
            processTriggerText(line);
            break;
        case ST_COLS::SENDTAB_COL_MODS: //modifiers
            processModifierText(line);
            break;
    }
}

