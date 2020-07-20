#include "dbcmaineditor.h"
#include "ui_dbcmaineditor.h"

#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QColorDialog>
#include <QTableWidgetItem>
#include <qevent.h>
#include "helpwindow.h"

DBCMainEditor::DBCMainEditor( const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCMainEditor)
{
    ui->setupUi(this);

    readSettings();

    dbcHandler = DBCHandler::getReference();
    referenceFrames = frames;

    connect(ui->btnSearch, &QAbstractButton::clicked, this, &DBCMainEditor::handleSearch);
    connect(ui->lineSearch, &QLineEdit::returnPressed, this, &DBCMainEditor::handleSearch);
    connect(ui->treeDBC, &QTreeWidget::doubleClicked, this, &DBCMainEditor::onTreeDoubleClicked);

    sigEditor = new DBCSignalEditor(this);
    msgEditor = new DBCMessageEditor(this);

    nodeIcon = QIcon(":/icons/images/node.png");
    messageIcon = QIcon(":/icons/images/message.png");
    signalIcon = QIcon(":/icons/images/signal.png");
    multiplexedSignalIcon = QIcon(":/icons/images/multiplexed-signal.png");
    multiplexorSignalIcon = QIcon(":/icons/images/multiplexor-signal.png");

    //ui->btnDelete->setFixedSize(32,32);
    ui->btnDelete->setIconSize(QSize(32, 32));

    //ui->btnNewNode->setFixedSize(32,32);
    ui->btnNewNode->setIconSize(QSize(32, 32));

    //ui->btnNewMessage->setFixedSize(32,32);
    ui->btnNewMessage->setIconSize(QSize(32, 32));

    //ui->btnNewSignal->setFixedSize(32,32);
    ui->btnNewSignal->setIconSize(QSize(32, 32));

    installEventFilter(this);
}

void DBCMainEditor::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    refreshTree();
}

DBCMainEditor::~DBCMainEditor()
{
    removeEventFilter(this);
    delete ui;
    delete sigEditor;
}

bool DBCMainEditor::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("dbc_editor.html");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void DBCMainEditor::setFileIdx(int idx)
{
    if (idx < 0 || idx > dbcHandler->getFileCount() - 1) return;
    dbcFile = dbcHandler->getFileByIdx(idx);
    fileIdx = idx;
}

void DBCMainEditor::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event)
    writeSettings();
    sigEditor->close();
}

void DBCMainEditor::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("DBCMainEditor/WindowSize", QSize(1103, 571)).toSize());
        move(settings.value("DBCMainEditor/WindowPos", QPoint(50, 50)).toPoint());
    }
}

void DBCMainEditor::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("DBCMainEditor/WindowSize", size());
        settings.setValue("DBCMainEditor/WindowPos", pos());
    }
}

void DBCMainEditor::onCustomMenuTree(QPoint point)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(tr("Delete currently selected message"), this, SLOT(deleteCurrentMessage()));

    //menu->popup(ui->MessagesTable->mapToGlobal(point));

}

void DBCMainEditor::deleteCurrentTreeItem()
{
    /*
    int thisRow = ui->NodesTable->currentRow();
    QTableWidgetItem* thisItem = ui->NodesTable->item(thisRow, 0);
    if (!thisItem) return;
    QString nodeName = thisItem->text();
    if (nodeName.length() > 0 && nodeName.compare("Vector__XXX", Qt::CaseInsensitive) != 0)
    {        
        ui->NodesTable->removeRow(thisRow);
        dbcFile->dbc_nodes.removeAt(thisRow);
        inhibitCellChanged = true;
        refreshMessagesTable(&dbcFile->dbc_nodes[0]);
        ui->NodesTable->selectRow(0);
        inhibitCellChanged = false;
    } */
}

//void DBCMainEditor::deleteCurrentMessage()
//{
    /*
    int thisRow = ui->MessagesTable->currentRow();
    QTableWidgetItem* thisItem = ui->MessagesTable->item(thisRow, 0);
    if (!thisItem) return;
    if (thisItem->text().length() > 0)
    {
        ui->MessagesTable->removeRow(thisRow);
        dbcFile->messageHandler->removeMessageByIndex(thisRow);
    }
    */
//}

void DBCMainEditor::handleSearch()
{
    /*
    uint32_t msgId = Utility::ParseStringToNum(ui->lineSearch->text());
    if (ui->radioID->isChecked())
    {
        DBC_MESSAGE *msg = dbcFile->messageHandler->findMsgByID(msgId);
        if (msg)
        {
            DBC_NODE *node = msg->sender;
            if (node)
            {
                QString nodeName = node->name;
                for (int i = 0; i < ui->NodesTable->rowCount(); i++)
                {
                    QTableWidgetItem *item = ui->NodesTable->item(i, 0);
                    if (item->text() == nodeName)
                    {
                        item->setSelected(true);
                        ui->NodesTable->scrollToItem(item);
                        onCellClickedNode(i, 0);
                        for (int j = 0; j < ui->MessagesTable->rowCount(); j++)
                        {
                            QTableWidgetItem *msgItem = ui->MessagesTable->item(j, 0);
                            if (!msgItem) break;
                            if (Utility::ParseStringToNum(msgItem->text()) == msgId)
                            {
                                msgItem->setSelected(true);
                                ui->MessagesTable->scrollToItem(msgItem);
                                onCellClickedMessage(j, 0);
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    if (ui->radioName->isChecked())
    {
        DBC_MESSAGE *msg = dbcFile->messageHandler->findMsgByPartialName(ui->lineSearch->text());
        if (msg)
        {
            DBC_NODE *node = msg->sender;
            if (node)
            {
                QString nodeName = node->name;
                for (int i = 0; i < ui->NodesTable->rowCount(); i++)
                {
                    QTableWidgetItem *item = ui->NodesTable->item(i, 0);
                    if (item->text() == nodeName)
                    {
                        item->setSelected(true);
                        ui->NodesTable->scrollToItem(item);
                        onCellClickedNode(i, 0);
                        for (int j = 0; j < ui->MessagesTable->rowCount(); j++)
                        {
                            QTableWidgetItem *msgItem = ui->MessagesTable->item(j, 1);
                            if (!msgItem) break;
                            if (msgItem->text().contains(ui->lineSearch->text(), Qt::CaseInsensitive))
                            {
                                msgItem->setSelected(true);
                                ui->MessagesTable->scrollToItem(msgItem);
                                onCellClickedMessage(j, 0);
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
    } */
}

//Double clicking is interpreted as a desire to edit the given item.
void DBCMainEditor::onTreeDoubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index)

    QTreeWidgetItem* firstCol = ui->treeDBC->currentItem();
    bool ret = false;
    DBC_MESSAGE *msg;
    DBC_SIGNAL *sig;
    uint32_t msgID;
    QString idString;

    qDebug() << firstCol->data(0, Qt::UserRole) << " - " << firstCol->text(0);

    switch (firstCol->data(0, Qt::UserRole).toInt())
    {
    case 1: //a node
        break;
    case 2: //a message
        idString = firstCol->text(0).split(" ")[0];
        msgID = static_cast<uint32_t>(Utility::ParseStringToNum(idString));
        msg = dbcFile->messageHandler->findMsgByID(msgID);
        msgEditor->setMessageRef(msg);
        msgEditor->setFileIdx(fileIdx);
        msgEditor->setWindowModality(Qt::WindowModal);
        msgEditor->exec(); //blocks this window from being active until we're done
        break;
    case 3: //a signal
        idString = firstCol->parent()->text(0).split(" ")[0];
        msgID = static_cast<uint32_t>(Utility::ParseStringToNum(idString));
        msg = dbcFile->messageHandler->findMsgByID(msgID);
        sig = msg->sigHandler->findSignalByName(firstCol->text(0).split(" ")[0]);
        sigEditor->setSignalRef(sig);
        sigEditor->setMessageRef(msg);
        sigEditor->setFileIdx(fileIdx);
        sigEditor->setWindowModality(Qt::WindowModal);
        sigEditor->exec(); //blocks this window from being active until we're done
        break;
    }

/*    if (col == 3) //3 is the signals field. If clicked we go to the signals dialog
    {
        QTableWidgetItem* msg = ui->MessagesTable->item(row, 0);
        if(msg) {
            QString idString = msg->text();
            DBC_MESSAGE *message = dbcFile->messageHandler->findMsgByID(static_cast<uint32_t>(Utility::ParseStringToNum(idString)));
            sigEditor->setMessageRef(message);
            sigEditor->setFileIdx(fileIdx);
            sigEditor->setWindowModality(Qt::WindowModal);
            sigEditor->exec(); //blocks this window from being active until we're done
            //now update the displayed # of signals
            inhibitCellChanged = true;
            QTableWidgetItem *replacement = new QTableWidgetItem(QString::number(message->sigHandler->getCount()));
            ui->MessagesTable->setItem(row, col, replacement);
            inhibitCellChanged = false;
        }
    }
    if (col == 4)
    {
        QColor newColor = QColorDialog::getColor(thisItem->background().color());
        thisItem->setBackground(newColor);

        if(!firstCol) return;

        msgID = Utility::ParseStringToNum2(firstCol->text(), &ret);
        msg = dbcFile->messageHandler->findMsgByID(msgID);

        if (msg)
        {
            msg->fgColor = newColor;
            DBC_ATTRIBUTE_VALUE *val = msg->findAttrValByName("GenMsgForegroundColor");
            if (val)
            {
                val->value = newColor.name();
            }
            else
            {
                DBC_ATTRIBUTE_VALUE newVal;
                newVal.attrName = "GenMsgForegroundColor";
                newVal.value = newColor.name();
                msg->attributes.append(newVal);
            }
        }
    }
    if (col == 5)
    {
        QColor newColor = QColorDialog::getColor(thisItem->background().color());
        thisItem->setBackground(newColor);

        if(!firstCol) return;

        msgID = Utility::ParseStringToNum2(firstCol->text(), &ret);
        msg = dbcFile->messageHandler->findMsgByID(msgID);

        if (msg)
        {
            msg->bgColor = newColor;
            DBC_ATTRIBUTE_VALUE *val = msg->findAttrValByName("GenMsgBackgroundColor");
            if (val)
            {
                val->value = newColor.name();
            }
            else
            {
                DBC_ATTRIBUTE_VALUE newVal;
                newVal.attrName = "GenMsgBackgroundColor";
                newVal.value = newColor.name();
                msg->attributes.append(newVal);
            }
        }
    } */
}


/*
 * Recreate the whole tree with pretty icons and custom user roles that give the rest of code an easy way to figure out whether a given tree node
 * is a node, message, or signal.
*/
void DBCMainEditor::refreshTree()
{
    ui->treeDBC->clear();

    if (dbcFile->findNodeByName("Vector__XXX") == nullptr)
    {
        DBC_NODE newNode;
        newNode.name = "Vector__XXX";
        newNode.comment = "Default node if no other node is specified";
        dbcFile->dbc_nodes.append(newNode);
    }

    foreach (DBC_NODE node, dbcFile->dbc_nodes)
    {
        QTreeWidgetItem *nodeItem = new QTreeWidgetItem();
        QString nodeInfo = node.name;
        if (node.comment.count() > 0) nodeInfo.append(" - ").append(node.comment);
        nodeItem->setText(0, nodeInfo);
        nodeItem->setIcon(0, nodeIcon);
        nodeItem->setData(0, Qt::UserRole, 1);
        for (int x = 0; x < dbcFile->messageHandler->getCount(); x++)
        {
            DBC_MESSAGE *msg = dbcFile->messageHandler->findMsgByIdx(x);
            if (msg->sender->name == node.name)
            {
                QTreeWidgetItem *msgItem = new QTreeWidgetItem(nodeItem);
                QString msgInfo = Utility::formatCANID(msg->ID) + " " + msg->name;
                if (msg->comment.count() > 0) msgInfo.append(" - ").append(msg->comment);
                msgItem->setText(0, msgInfo);
                msgItem->setIcon(0, messageIcon);
                msgItem->setData(0, Qt::UserRole, 2);
                for (int i = 0; i < msg->sigHandler->getCount(); i++)
                {
                    DBC_SIGNAL *sig = msg->sigHandler->findSignalByIdx(i);
                    QTreeWidgetItem *sigItem = new QTreeWidgetItem(msgItem);
                    QString sigInfo = sig->name;
                    if (sig->comment.count() > 0) sigInfo.append(" - ").append(sig->comment);
                    sigItem->setText(0, sigInfo);
                    if (sig->isMultiplexed) sigItem->setIcon(0, multiplexedSignalIcon);
                    else if (sig->isMultiplexor) sigItem->setIcon(0, multiplexorSignalIcon);
                    else sigItem->setIcon(0, signalIcon);
                    sigItem->setData(0, Qt::UserRole, 3);
                }
            }
        }
        ui->treeDBC->addTopLevelItem(nodeItem);
    }
    ui->treeDBC->sortItems(0, Qt::SortOrder::AscendingOrder); //sort the display list for ease in viewing by mere mortals, helps me a lot.
}
