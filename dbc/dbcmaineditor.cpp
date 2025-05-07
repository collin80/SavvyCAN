#include "dbcmaineditor.h"
#include "ui_dbcmaineditor.h"

#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QColorDialog>
#include <QTableWidgetItem>
#include <QRandomGenerator>
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

    ui->treeDBC->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->btnSearch, &QAbstractButton::clicked, this, &DBCMainEditor::handleSearch);
    connect(ui->lineSearch, &QLineEdit::returnPressed, this, &DBCMainEditor::handleSearch);
    connect(ui->btnSearchNext, &QAbstractButton::clicked, this, &DBCMainEditor::handleSearchForward);
    connect(ui->btnSearchPrev, &QAbstractButton::clicked, this, &DBCMainEditor::handleSearchBackward);
    connect(ui->treeDBC, &QTreeWidget::doubleClicked, this, &DBCMainEditor::onTreeDoubleClicked);
    connect(ui->treeDBC, &QTreeWidget::customContextMenuRequested, this, &DBCMainEditor::onTreeContextMenu);
    connect(ui->treeDBC, &QTreeWidget::currentItemChanged, this, &DBCMainEditor::currentItemChanged);
    connect(ui->btnDelete, &QAbstractButton::clicked, this, &DBCMainEditor::deleteCurrentTreeItem);
    connect(ui->btnNewNode, &QAbstractButton::clicked, this, QOverload<>::of(&DBCMainEditor::newNode));
    connect(ui->btnNewMessage, &QAbstractButton::clicked, this, &DBCMainEditor::newMessage);
    connect(ui->btnNewSignal, &QAbstractButton::clicked, this, &DBCMainEditor::newSignal);

    sigEditor = new DBCSignalEditor(this);
    msgEditor = new DBCMessageEditor(this);
    nodeEditor = new DBCNodeEditor(this);
    nodeRebaseEditor = new DBCNodeRebaseEditor(this);
    nodeDuplicateEditor = new DBCNodeDuplicateEditor(this);

    //all three might potentially change the data stored and force the tree to be updated
    connect(sigEditor, &DBCSignalEditor::updatedTreeInfo, this, &DBCMainEditor::updatedSignal);
    connect(msgEditor, &DBCMessageEditor::updatedTreeInfo, this, &DBCMainEditor::updatedMessage);
    connect(nodeEditor, &DBCNodeEditor::updatedTreeInfo, this, &DBCMainEditor::updatedNode);
    connect(nodeRebaseEditor, &DBCNodeRebaseEditor::updatedTreeInfo, this, &DBCMainEditor::updatedMessage);
    connect(nodeDuplicateEditor, &DBCNodeDuplicateEditor::updatedTreeInfo, this, &DBCMainEditor::updatedMessage);
    connect(nodeDuplicateEditor, &DBCNodeDuplicateEditor::createNode, this, QOverload<QString>::of(&DBCMainEditor::newNode));
    connect(nodeDuplicateEditor, &DBCNodeDuplicateEditor::cloneMessageToNode, this, &DBCMainEditor::copyMessageToNode);
    connect(nodeDuplicateEditor, &DBCNodeDuplicateEditor::nodeAdded, this, &DBCMainEditor::refreshTree);

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
    delete msgEditor;
    delete nodeEditor;
}

bool DBCMainEditor::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("dbc_editor.md");
            break;
        case Qt::Key_F3:
            handleSearchForward();
            break;
        case Qt::Key_F4:
            handleSearchBackward();
            break;
        case Qt::Key_F5:
            newNode();
            break;
        case Qt::Key_F6:
            newMessage();
            break;
        case Qt::Key_F7:
            newSignal();
            break;
        case Qt::Key_Delete:
            deleteCurrentTreeItem();
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
        move(Utility::constrainedWindowPos(settings.value("DBCMainEditor/WindowPos", QPoint(50, 50)).toPoint()));
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
    Q_UNUSED(point);
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(tr("Delete currently selected message"), this, SLOT(deleteCurrentMessage()));

    //menu->popup(ui->MessagesTable->mapToGlobal(point));

}

void DBCMainEditor::handleSearch()
{
    searchItems = ui->treeDBC->findItems(ui->lineSearch->text(),Qt::MatchContains | Qt::MatchRecursive);
    qDebug() << "Search returned " << searchItems.count() << "items.";
    if (searchItems.count() > 0)
    {
        ui->treeDBC->setCurrentItem(searchItems[0]);
        searchItemPos = 0;
        ui->lblSearchPos->setText("Search Results: " + QString::number(searchItemPos + 1) + " of " + QString::number(searchItems.count()));
    }
    else
    {
        ui->lblSearchPos->setText("Search Results: 0 of 0");
    }
}

void DBCMainEditor::handleSearchForward()
{
    if (searchItems.count() == 0) return;
    if (searchItemPos < searchItems.count() - 1) searchItemPos++;
    else searchItemPos = 0;
    ui->treeDBC->setCurrentItem(searchItems[searchItemPos]);
    ui->lblSearchPos->setText("Search Results: " + QString::number(searchItemPos + 1) + " of " + QString::number(searchItems.count()));
}

void DBCMainEditor::handleSearchBackward()
{
    if (searchItems.count() == 0) return;
    if (searchItemPos > 0) searchItemPos--;
    else searchItemPos = searchItems.count() - 1;
    ui->treeDBC->setCurrentItem(searchItems[searchItemPos]);
    ui->lblSearchPos->setText("Search Results: " + QString::number(searchItemPos + 1) + " of " + QString::number(searchItems.count()));
}

void DBCMainEditor::currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *prev)
{
    Q_UNUSED(prev)
    if (!current)
    {
        ui->btnNewMessage->setEnabled(false);
        ui->btnNewSignal->setEnabled(false);
        ui->btnDelete->setEnabled(false);
        return;
    }
    switch (current->data(0, Qt::UserRole).toInt())
    {
    case DBCItemTypes::NODE: //node
        ui->btnNewNode->setEnabled(true);
        ui->btnNewMessage->setEnabled(true);
        ui->btnNewSignal->setEnabled(false);
        ui->btnDelete->setEnabled(true);
        break;
    case DBCItemTypes::MESG: //message
        ui->btnNewNode->setEnabled(true);
        ui->btnNewMessage->setEnabled(true);
        ui->btnNewSignal->setEnabled(true);
        ui->btnDelete->setEnabled(true);
        break;
    case DBCItemTypes::SIG: //signal
        ui->btnNewNode->setEnabled(true);
        ui->btnNewMessage->setEnabled(true);
        ui->btnNewSignal->setEnabled(true);
        ui->btnDelete->setEnabled(true);
        break;
    default:
        ui->btnNewMessage->setEnabled(false);
        ui->btnNewSignal->setEnabled(false);
        ui->btnDelete->setEnabled(false);
    }
}

uint32_t DBCMainEditor::getParentMessageID(QTreeWidgetItem *cell)
{
    if (cell->data(0, Qt::UserRole) == DBCItemTypes::MESG)
    {
        return static_cast<uint32_t>(Utility::ParseStringToNum(cell->text(0).split(" ")[0]));
    }
    else
    {
        if (cell->parent()) return getParentMessageID(cell->parent());
        else return 0;
    }
}

//Double clicking is interpreted as a desire to edit the given item.
void DBCMainEditor::onTreeDoubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index)

    QTreeWidgetItem* firstCol = ui->treeDBC->currentItem();
    //bool ret = false;
    DBC_MESSAGE *msg;
    DBC_SIGNAL *sig;
    DBC_NODE *node;
    uint32_t msgID;
    QString idString;

    qDebug() << firstCol->data(0, Qt::UserRole) << " - " << firstCol->text(0);

    switch (firstCol->data(0, Qt::UserRole).toInt())
    {
    case DBCItemTypes::NODE: //a node
        idString = firstCol->text(0).split(" ")[0];
        node = dbcFile->findNodeByName(idString);
        nodeEditor->setFileIdx(fileIdx);
        nodeEditor->setNodeRef(node);
        nodeEditor->refreshView();
        nodeEditor->show();
        break;
    case DBCItemTypes::MESG: //a message
        idString = firstCol->text(0).split(" ")[0];
        msgID = static_cast<uint32_t>(Utility::ParseStringToNum(idString));
        msg = dbcFile->messageHandler->findMsgByID(msgID);
        //msg = itemToMessage[firstCol];
        msgEditor->setMessageRef(msg);
        msgEditor->setFileIdx(fileIdx);
        //msgEditor->setWindowModality(Qt::WindowModal);
        msgEditor->refreshView();
        msgEditor->show(); //show allows the rest of the forms to keep going
        break;
    case DBCItemTypes::SIG: //a signal
        msgID = getParentMessageID(firstCol);
        msg = dbcFile->messageHandler->findMsgByID(msgID);
        QString nameString = firstCol->text(0);
        if (nameString.contains("("))
        {
            nameString = nameString.split(")")[1].trimmed(); //remove (1-2) type stuff from beginning of string
        }
        nameString = nameString.split(" ")[0]; //get rid of [32m 8] type stuff after the name

        sig = msg->sigHandler->findSignalByName(nameString);

        //sig = itemToSignal[firstCol];
        if (sig)
        {
            sigEditor->setSignalRef(sig);
            sigEditor->setMessageRef(msg);
            sigEditor->setFileIdx(fileIdx);
            //sigEditor->setWindowModality(Qt::WindowModal);
            sigEditor->refreshView();
            sigEditor->show();
        }
        break;
    }
}

void DBCMainEditor::onTreeContextMenu(const QPoint & pos)
{
    QTreeWidgetItem* firstCol = ui->treeDBC->currentItem();
    QString idString;

    qDebug() << firstCol->data(0, Qt::UserRole) << " - " << firstCol->text(0);

    switch (firstCol->data(0, Qt::UserRole).toInt())
    {
    case DBCItemTypes::NODE: //a node
        idString = firstCol->text(0).split(" ")[0];
        //node = dbcFile->findNodeByName(idString);

        QAction *actionRebase = new QAction(QIcon(":/Resource/warning32.ico"), tr("Rebase all messages"), this);
        actionRebase->setStatusTip(tr("Rebase all messages in node"));
        connect(actionRebase, SIGNAL(triggered()), this, SLOT(onRebaseMessages()));

        QAction *actionDupe = new QAction(QIcon(":/Resource/warning32.ico"), tr("Duplicate node"), this);
        actionDupe->setStatusTip(tr("Duplicate node and messages"));
        connect(actionDupe, SIGNAL(triggered()), this, SLOT(onDuplicateNode()));

        QMenu menu(this);
        menu.addAction(actionRebase);
        menu.addAction(actionDupe);

        //QPoint pt(pos);
        menu.exec( ui->treeDBC->mapToGlobal(pos) );
        break;
    }
}

void DBCMainEditor::onRebaseMessages()
{
    QTreeWidgetItem* firstCol = ui->treeDBC->currentItem();
    DBC_NODE *node;
    QString idString;

    idString = firstCol->text(0).split(" ")[0];
    node = dbcFile->findNodeByName(idString);
    nodeRebaseEditor->setFileIdx(fileIdx);
    nodeRebaseEditor->setNodeRef(node);
    if(nodeRebaseEditor->refreshView())
    {
        nodeRebaseEditor->setModal(true);
        nodeRebaseEditor->show();
    }
}

void DBCMainEditor::onDuplicateNode()
{
    QTreeWidgetItem* firstCol = ui->treeDBC->currentItem();
    //bool ret = false;
    //DBC_MESSAGE *msg;
    //DBC_SIGNAL *sig;
    //uint32_t msgID;
    DBC_NODE *node;
    QString idString;

    idString = firstCol->text(0).split(" ")[0];
    node = dbcFile->findNodeByName(idString);
    nodeDuplicateEditor->setFileIdx(fileIdx);
    nodeDuplicateEditor->setNodeRef(node);    
    if(nodeDuplicateEditor->refreshView())
    {
        nodeDuplicateEditor->setModal(true);
        nodeDuplicateEditor->show();
    }
}

/*
 * Recreate the whole tree with pretty icons and custom user roles that give the rest of code an easy way to figure out whether a given tree node
 * is a node, message, or signal.
*/
void DBCMainEditor::refreshTree()
{
    ui->treeDBC->clear();
    nodeToItem.clear();
    messageToItem.clear();
    signalToItem.clear();
    itemToNode.clear();
    itemToMessage.clear();
    itemToSignal.clear();

    if (dbcFile->findNodeByName("Vector__XXX") == nullptr)
    {
        DBC_NODE *newNode = new DBC_NODE;
        newNode->name = "Vector__XXX";
        newNode->comment = "Default node if no other node is specified";
        dbcFile->dbc_nodes.append(newNode);
    }

    for (int n = 0; n < dbcFile->dbc_nodes.count(); n++)
    {
        DBC_NODE *node = dbcFile->dbc_nodes[n];
        QTreeWidgetItem *nodeItem = new QTreeWidgetItem();
        QString nodeInfo = node->name;
        if (node->comment.length() > 0) nodeInfo.append(" - ").append(node->comment);
        nodeItem->setText(0, nodeInfo);
        nodeItem->setIcon(0, nodeIcon);
        nodeItem->setData(0, Qt::UserRole, DBCItemTypes::NODE);
        nodeToItem.insert(node, nodeItem);
        itemToNode.insert(nodeItem, node);
        QList<DBC_MESSAGE *>  msgs = dbcFile->messageHandler->getMsgsAsList();
        qDebug() << msgs;
        for (int x = 0; x < dbcFile->messageHandler->getCount(); x++)
        {
            DBC_MESSAGE *msg = msgs[x];
            qDebug() << msg->sender->name << " | " << node->name;
            if (msg->sender->name == node->name)
            {
                QTreeWidgetItem *msgItem = new QTreeWidgetItem(nodeItem);
                QString msgInfo = Utility::formatCANID(msg->ID) + " " + msg->name;
                if (msg->comment.length() > 0) msgInfo.append(" - ").append(msg->comment);
                msgItem->setText(0, msgInfo);
                msgItem->setIcon(0, messageIcon);
                msgItem->setData(0, Qt::UserRole, DBCItemTypes::MESG);
                messageToItem.insert(msg, msgItem);
                itemToMessage.insert(msgItem, msg);
                QList<DBC_SIGNAL *> sigs = msg->sigHandler->getSignalsAsList();
                for (int i = 0; i < msg->sigHandler->getCount(); i++)
                {
                    DBC_SIGNAL *sig = sigs[i];
                    //only process signals here which are "top" level
                    if (sig->multiplexParent == nullptr) processSignalToTree(msgItem, sig);
                }
            }
        }
        ui->treeDBC->addTopLevelItem(nodeItem);
    }
    ui->treeDBC->sortItems(0, Qt::SortOrder::AscendingOrder); //sort the display list for ease in viewing by mere mortals, helps me a lot.
}

QString DBCMainEditor::createSignalText(DBC_SIGNAL *sig)
{
    QString sigInfo;
    if (sig->isMultiplexed)
    {
        sigInfo = "(" + sig->multiplexDbcString() + ") ";
    }
    sigInfo.append(sig->name);

    if (sig->intelByteOrder)
        sigInfo.append(" [" + QString::number(sig->startBit) + "i " + QString::number(sig->signalSize) + "]");
    else
        sigInfo.append(" [" + QString::number(sig->startBit) + "m " + QString::number(sig->signalSize) + "]");

    if (sig->comment.length() > 0) sigInfo.append(" - ").append(sig->comment);
    return sigInfo;
}

//Signals can have a hierarchial relationship with other signals so this function is separate and calls itself recursively to build the tree
void DBCMainEditor::processSignalToTree(QTreeWidgetItem *parent, DBC_SIGNAL *sig)
{
    QTreeWidgetItem *sigItem = new QTreeWidgetItem(parent);
    QString sigInfo = createSignalText(sig);
    sigItem->setText(0, sigInfo);
    if (sig->isMultiplexor) sigItem->setIcon(0, multiplexorSignalIcon);
    else if (sig->isMultiplexed) sigItem->setIcon(0, multiplexedSignalIcon);
    else sigItem->setIcon(0, signalIcon);
    sigItem->setData(0, Qt::UserRole, DBCItemTypes::SIG);
    signalToItem.insert(sig, sigItem);
    itemToSignal.insert(sigItem, sig);
    if (sig->multiplexedChildren.count() > 0)
    {
        for (int i = 0; i < sig->multiplexedChildren.count(); i++)
        {
            processSignalToTree(sigItem, sig->multiplexedChildren[i]);
        }
    }
}

void DBCMainEditor::updatedNode(DBC_NODE *node)
{
    if (nodeToItem.contains(node))
    {
        QTreeWidgetItem *item = nodeToItem[node];
        QString nodeInfo = node->name;
        if (node->comment.length() > 0) nodeInfo.append(" - ").append(node->comment);
        item->setText(0, nodeInfo);
    }
    else qDebug() << "That node doesn't exist. That's a bug dude.";
}

void DBCMainEditor::updatedMessage(DBC_MESSAGE *msg, quint32 orig_ID)
{
    //if ID was changed then the messagehandler must be told to update its QMap as well.
    if (msg->ID != orig_ID)
    {
        dbcFile->messageHandler->changeMessageID(msg, orig_ID);
    }

    if (messageToItem.contains(msg))
    {
        QTreeWidgetItem *item = messageToItem.value(msg);
        QString msgInfo = Utility::formatCANID(msg->ID) + " " + msg->name;
        if (msg->comment.length() > 0) msgInfo.append(" - ").append(msg->comment);
        item->setText(0, msgInfo);
        //editor could have changed the parent Node too. Have to figure out which node
        //is parent in the GUI and compare that to parent in the data.
        DBC_NODE *oldParent = dbcFile->findNodeByName(item->parent()->text(0).split(" - ")[0]);
        if (oldParent != msg->sender && oldParent)
        {
            qDebug() << "Changed parent of message. Trying to rehome it.";
            QTreeWidgetItem *newParent = nullptr;
            if (nodeToItem.contains(msg->sender)) newParent = nodeToItem.value(msg->sender);
            else //new node created within message editor. Create the item and then use it
            {
                QTreeWidgetItem *newNodeItem = new QTreeWidgetItem();
                newNodeItem->setText(0, msg->sender->name);
                newNodeItem->setIcon(0, nodeIcon);
                newNodeItem->setData(0, Qt::UserRole, DBCItemTypes::NODE);
                ui->treeDBC->addTopLevelItem(newNodeItem);
                nodeToItem.insert(msg->sender, newNodeItem);
                itemToNode.insert(newNodeItem, msg->sender);
                newParent = newNodeItem;
            }

            QTreeWidgetItem *prevParent = nodeToItem.value(oldParent);
            prevParent->removeChild(item);
            newParent->addChild(item);
            ui->treeDBC->setCurrentItem(item);
            ui->treeDBC->sortItems(0, Qt::AscendingOrder); //resort because we just moved an item
        }
    }
    else qDebug() << "That mesage doesn't exist. That's a bug dude.";
}

void DBCMainEditor::updatedSignal(DBC_SIGNAL *sig)
{
    if (signalToItem.contains(sig))
    {
        QTreeWidgetItem *item = signalToItem.value(sig);
        QString sigInfo = createSignalText(sig);
        item->setText(0, sigInfo);
        if (sig->isMultiplexed)
        {
            if (item->parent()->data(0, Qt::UserRole).toInt() == DBCItemTypes::SIG) //if our parent is another signal
            {
                QString nameString = item->parent()->text(0);
                if (nameString.contains("(")) nameString = nameString.split(" ")[1];
                else nameString = nameString.split(" ")[0];
                DBC_SIGNAL *oldParent = sig->parentMessage->sigHandler->findSignalByName(nameString);
                if (oldParent && (oldParent != sig->multiplexParent))
                {
                    qDebug() << "You changed the signal's parent";
                    QTreeWidgetItem *newParent = nullptr;
                    newParent = signalToItem.value(sig->multiplexParent);
                    QTreeWidgetItem *prevParent = signalToItem.value(oldParent);
                    prevParent->removeChild(item);
                    newParent->addChild(item);
                    ui->treeDBC->setCurrentItem(item);
                    ui->treeDBC->sortItems(0, Qt::AscendingOrder); //resort because we just moved an item
                }
            }
        }
    }
    else qDebug() << "That signal doesn't exist. That's a bug dude.";
}

void DBCMainEditor::newNode(QString nodeName)
{
    DBC_NODE *nodePtr = new DBC_NODE;
    if(nodeName.isEmpty())
    {
        nodePtr->name = "Unnamed" + QString::number(randGen.bounded(50000));
    }
    else
    {
        nodePtr->name = nodeName;
    }
    dbcFile->dbc_nodes.append(nodePtr);
    QTreeWidgetItem *nodeItem = new QTreeWidgetItem();
    nodeItem->setText(0, nodePtr->name);
    nodeItem->setIcon(0, nodeIcon);
    nodeItem->setData(0, Qt::UserRole, DBCItemTypes::NODE);
    nodeToItem.insert(nodePtr, nodeItem);
    itemToNode.insert(nodeItem, nodePtr);
    ui->treeDBC->addTopLevelItem(nodeItem);
    ui->treeDBC->setCurrentItem(nodeItem);
    dbcFile->setDirtyFlag();
}

void DBCMainEditor::newNode()
{
    newNode(QString());
}

void DBCMainEditor::copyMessageToNode(DBC_NODE *parentNode, DBC_MESSAGE *source, uint newMsgId)
{
    DBC_NODE *node = parentNode;
    if (!node) node = dbcFile->findNodeByIdx(0);
    QTreeWidgetItem *nodeItem = nullptr;
    DBC_MESSAGE *msgPtr = new DBC_MESSAGE();

    nodeItem = ui->treeDBC->currentItem();

    msgPtr->name = source->name;
    msgPtr->ID = newMsgId;
    msgPtr->len = source->len;
    msgPtr->bgColor = source->bgColor;
    msgPtr->fgColor = source->fgColor;
    msgPtr->comment = source->comment;
    msgPtr->sender = node;

    DBC_SIGNAL *sigSource;
    int sigCount = source->sigHandler->getCount();
    QList<DBC_SIGNAL *> sigs = source->sigHandler->getSignalsAsList();

    for(int i=0; i<sigCount; i++)
    {
        sigSource = sigs[i];

        DBC_SIGNAL* sig = new DBC_SIGNAL;

        //Does not properly handle multiplexed signals, for now
        sig->name = sigSource->name;
        sig->bias = sigSource->bias;
        sig->isMultiplexed = false; //sigSource->isMultiplexed;
        sig->isMultiplexor = false; //sigSource->isMultiplexor;
        sig->max = sigSource->max;
        sig->min = sigSource->min;
        sig->copyMultiplexValuesFromSignal(*sigSource);
        sig->factor = sigSource->factor;
        sig->intelByteOrder = sigSource->intelByteOrder;
        sig->parentMessage = msgPtr;
        sig->multiplexParent = nullptr;  //need to learn about multiplexed signals and track them when copying
        sig->receiver = node;
        sig->signalSize = sigSource->signalSize;
        sig->startBit = sigSource->startBit;
        sig->valType = sigSource->valType;

        msgPtr->sigHandler->addSignal(sig);
    }

    //msgPtr->sigHandler->sort();

    dbcFile->messageHandler->addMessage(msgPtr);
    QTreeWidgetItem *newMsgItem = new QTreeWidgetItem();
    QString msgInfo = Utility::formatCANID(msgPtr->ID) + " " + msgPtr->name;
    if (msgPtr->comment.length() > 0) msgInfo.append(" - ").append(msgPtr->comment);
    newMsgItem->setText(0, msgInfo);
    newMsgItem->setIcon(0, messageIcon);
    newMsgItem->setData(0, Qt::UserRole, DBCItemTypes::MESG);
    messageToItem.insert(msgPtr, newMsgItem);
    itemToMessage.insert(newMsgItem, msgPtr);
    nodeItem->addChild(newMsgItem);
    //ui->treeDBC->setCurrentItem(newMsgItem);
    dbcFile->setDirtyFlag();
}

//create a new message with it's parent being the node we're currently within
void DBCMainEditor::newMessage()
{
    QTreeWidgetItem *nodeItem = nullptr;
    QTreeWidgetItem *msgItem = nullptr;
    nodeItem = ui->treeDBC->currentItem();
    int typ = nodeItem->data(0, Qt::UserRole).toInt();
    if (!nodeItem) return; //nothing selected!
    if (typ == DBCItemTypes::MESG)
    {
        msgItem = nodeItem;
        nodeItem = nodeItem->parent();
    }
    if (typ == DBCItemTypes::SIG)
    {
        msgItem = nodeItem->parent();
        nodeItem = msgItem->parent();
    }
    if (typ == DBCItemTypes::NODE){
        msgItem = nullptr; //we have no message, just a node selected
    }

    //if there was a comment this will find the location of the comment and snip it out.
    QString nodeName = nodeItem->data(0, Qt::DisplayRole).toString().split(" - ")[0];

    DBC_NODE *node = dbcFile->findNodeByName(nodeName);
    if (!node) node = dbcFile->findNodeByIdx(0);
    DBC_MESSAGE *msgPtr = new DBC_MESSAGE;
    if (msgItem)
    {
        QString idString = msgItem->text(0).split(" ")[0];
        int msgID = static_cast<uint32_t>(Utility::ParseStringToNum(idString));
        DBC_MESSAGE *oldMsg = dbcFile->messageHandler->findMsgByID(msgID);
        if (oldMsg)
        {
            msgPtr->name = "Msg" + QString::number(randGen.bounded(500));
            msgPtr->ID = oldMsg->ID + 1;

            DBC_MESSAGE *overlappedMsg = dbcFile->messageHandler->findMsgByID(msgPtr->ID);
            while (overlappedMsg)
            {
                msgPtr->ID++;
                overlappedMsg = dbcFile->messageHandler->findMsgByID(msgPtr->ID);
            }

            msgPtr->len = oldMsg->len;
            msgPtr->bgColor = oldMsg->bgColor;
            msgPtr->fgColor = oldMsg->fgColor;
            msgPtr->comment = oldMsg->comment;            
        }
        else
        {
            msgPtr->name = nodeName + "Msg" + QString::number(randGen.bounded(500));
            msgPtr->ID = 0x003;
            msgPtr->len = 8;
            msgPtr->bgColor = QApplication::palette().color(QPalette::Base);
        }
    }
    else
    {
        msgPtr->name = nodeName + "Msg" + QString::number(randGen.bounded(500));
        msgPtr->ID = 0x004;
        msgPtr->len = 8;
    }    
    msgPtr->sender = node;

    dbcFile->messageHandler->addMessage(msgPtr);
    QTreeWidgetItem *newMsgItem = new QTreeWidgetItem();
    QString msgInfo = Utility::formatCANID(msgPtr->ID) + " " + msgPtr->name;
    if (msgPtr->comment.length() > 0) msgInfo.append(" - ").append(msgPtr->comment);
    newMsgItem->setText(0, msgInfo);
    newMsgItem->setIcon(0, messageIcon);
    newMsgItem->setData(0, Qt::UserRole, DBCItemTypes::MESG);
    messageToItem.insert(msgPtr, newMsgItem);
    itemToMessage.insert(newMsgItem, msgPtr);
    nodeItem->addChild(newMsgItem);
    ui->treeDBC->setCurrentItem(newMsgItem);
    dbcFile->setDirtyFlag();
}

void DBCMainEditor::newSignal()
{
    QTreeWidgetItem *msgItem = nullptr;
    QTreeWidgetItem *sigItem = nullptr;
    QTreeWidgetItem *parentItem = nullptr;
    msgItem = ui->treeDBC->currentItem();
    parentItem = msgItem;
    if (!msgItem)
    {
        qDebug() << "Nothing selected!";
        return; //nothing selected!
    }
    int typ = msgItem->data(0, Qt::UserRole).toInt();
    if (typ == DBCItemTypes::NODE)
    {
        qDebug() << "Can't add signals to a node!";
        return; //can't add signals to a node!
    }
    if (typ == DBCItemTypes::SIG)
    {
        sigItem = msgItem;
        msgItem = msgItem->parent();
        parentItem = msgItem;
        //walk up the tree to find the parent msg
        while (msgItem && msgItem->data(0, Qt::UserRole).toInt() != DBCItemTypes::MESG) msgItem = msgItem->parent();
        if (!msgItem)
        {
            qDebug() << "Could not find parent message to attach signal to!";
            return; //something bad happened. abort.
        }
    }

    QString idString = msgItem->text(0).split(" ")[0];
    int msgID = static_cast<uint32_t>(Utility::ParseStringToNum(idString));
    DBCMessageHandler *msgHandler = dbcFile->messageHandler;

    DBC_MESSAGE *msg = msgHandler->findMsgByID(msgID);
    if (!msg)
    {
        qDebug() << "Could not find data structure for message 0x" << QString::number(msgID, 16) << ". Aborting!";
        return; //null pointers are a bummer. Do not follow them.
    }
    DBC_SIGNAL *sigPtr = new DBC_SIGNAL;
    if (sigItem)
    {
        QString txt = sigItem->text(0);
        if (txt.startsWith('(')) txt = txt.split(" ")[1]; //if it was a multiplexed signal we need to ignore that part and still grab sig name
        else txt = txt.split(" ")[0];
        DBC_SIGNAL *oldSig = msg->sigHandler->findSignalByName(txt);
        if (oldSig)
        {
            *sigPtr = *oldSig;
            sigPtr->name = sigPtr->name + QString::number(randGen.bounded(100));
        }
        else
        {
            sigPtr->name = msgItem->text(0).split(" ")[1] + "Sig" + QString::number(randGen.bounded(500));
        }
    }
    else
    {
        sigPtr->name = msgItem->text(0).split(" ")[1] + "Sig" + QString::number(randGen.bounded(500));
    }

    sigPtr->parentMessage = msg;
    if (!sigPtr->receiver) sigPtr->receiver = dbcFile->dbc_nodes[0]; //if receiver not set then set it to... something.
    msg->sigHandler->addSignal(sigPtr);
    QTreeWidgetItem *newSigItem = new QTreeWidgetItem();
    QString sigInfo = createSignalText(sigPtr);
    newSigItem->setText(0, sigInfo);
    if (sigPtr->isMultiplexed) newSigItem->setIcon(0, multiplexedSignalIcon);
    else if (sigPtr->isMultiplexor) newSigItem->setIcon(0, multiplexorSignalIcon);
    else newSigItem->setIcon(0, signalIcon);
    newSigItem->setData(0, Qt::UserRole, DBCItemTypes::SIG);
    signalToItem.insert(sigPtr, newSigItem);
    itemToSignal.insert(newSigItem, sigPtr);
    parentItem->addChild(newSigItem);
    ui->treeDBC->setCurrentItem(newSigItem);
    dbcFile->setDirtyFlag();
}

//gets confirmation before calling the real routines that delete things
void DBCMainEditor::deleteCurrentTreeItem()
{
    QTreeWidgetItem *currItem = ui->treeDBC->currentItem();
    int typ = currItem->data(0, Qt::UserRole).toInt();
    QString idString, columnText;
    int msgID;
    DBC_MESSAGE *msg;
    QList<DBC_MESSAGE *> msgs = dbcFile->messageHandler->getMsgsAsList();
    DBC_NODE *node;
    int numMsg = 0, numSig = 0;

    QMessageBox::StandardButton confirmDialog;

    switch (typ)
    {
    case DBCItemTypes::NODE: //deleting a node cascades deletion down to messages and signals
        columnText = currItem->text(0);
        node = dbcFile->findNodeByNameAndComment(columnText);
        if (!node) return;
        for (int x = 0; x < dbcFile->messageHandler->getCount(); x++)
        {
            if (msgs[x]->sender == node)
            {
                numMsg++;
                numSig += msgs[x]->sigHandler->getCount();
            }
        }
        confirmDialog = QMessageBox::question(this, "Really?", "Are you sure you want to delete this node, its\n"
                                  + QString::number(numMsg) + " messages, and their " + QString::number(numSig) + " combined signals?", QMessageBox::Yes|QMessageBox::No);
        if (confirmDialog == QMessageBox::Yes)
        {
            if (itemToNode.contains(currItem))
            {
                DBC_NODE *node = itemToNode.value(currItem);
                deleteNode(node);
            }
            else
            {
                qDebug() << "Could not find the node in the map. That should not happen.";
            }
        }
        break;

    case DBCItemTypes::MESG: //cascades to removing all signals too.
        idString = currItem->text(0).split(" ")[0];
        msgID = static_cast<uint32_t>(Utility::ParseStringToNum(idString));
        msg = dbcFile->messageHandler->findMsgByID(msgID);

        confirmDialog = QMessageBox::question(this, "Really?", "Are you sure you want to delete\nthis message and its "
                                  + QString::number(msg->sigHandler->getCount()) + " signals?", QMessageBox::Yes|QMessageBox::No);
        if (confirmDialog == QMessageBox::Yes)
        {
            QTreeWidgetItem *currItem = ui->treeDBC->currentItem();
            if (itemToMessage.contains(currItem))
            {
                DBC_MESSAGE *msg = itemToMessage.value(currItem);
                deleteMessage(msg);
            }
            else
            {
                qDebug() << "Could not find the message in the map. That should not happen.";
            }
        }
        break;

    case DBCItemTypes::SIG: //no cascade, just this one signal.
        confirmDialog = QMessageBox::question(this, "Really?", "Are you sure you want to delete this signal?",
                                  QMessageBox::Yes|QMessageBox::No);
        if (confirmDialog == QMessageBox::Yes)
        {
            QTreeWidgetItem *currItem = ui->treeDBC->currentItem();
            if (itemToSignal.contains(currItem))
            {
                DBC_SIGNAL *sig = itemToSignal.value(currItem);
                deleteSignal(sig);
            }
            else
            {
                qDebug() << "Could not find the signal in the map. That should not happen.";
            }
        }
        break;
    }
}

//these don't ask for permission. You call it, it disappears forever.
void DBCMainEditor::deleteNode(DBC_NODE *node)
{
    qDebug() << "Going through with it you mass deleter!";
    if (!nodeToItem.contains(node)) return;
    QTreeWidgetItem *currItem = nodeToItem[node];
    //don't actually store which messages are associated to which nodes so just iterate through the messages list and whack the ones
    //that claim to be associated to this node.
    int numItems = dbcFile->messageHandler->getCount();
    QList<DBC_MESSAGE *> msgs = dbcFile->messageHandler->getMsgsAsList();
    for (int i = numItems - 1; i > -1; i--)
    {
        if (msgs[i]->sender == node) deleteMessage(msgs[i]);

        //also, each signal has a receiver field that references the nodes. It was probably stupid to store
        //pointers to node structures in signals but that's how it is currently. Need to iterate over all
        //signals in all messages and set the receiver field to Vector__XXX if the old receiver node was this one.
        else //still check for signals with receiver set to this node
        {
            DBC_NODE *unset_node = dbcFile->findNodeByName("Vector__XXX");
            QList<DBC_SIGNAL *> sigs = msgs[i]->sigHandler->getSignalsAsList();
            int numSigs = msgs[i]->sigHandler->getCount();
            for (int j = numSigs - 1; j > -1; j--)
            {
                DBC_SIGNAL *sig = sigs[j];
                if (sig->receiver == node) sig->receiver = unset_node;
            }
        }
    }

    nodeToItem.remove(node);
    itemToNode.remove(currItem);
    ui->treeDBC->removeItemWidget(currItem, 0);
    delete currItem;

    for (int j = 0; j < dbcFile->dbc_nodes.count(); j++)
    {
        if (dbcFile->dbc_nodes.at(j)->name == node->name)
        {
            delete dbcFile->dbc_nodes[j];
            dbcFile->dbc_nodes.removeAt(j);
            break;
        }
    }

    dbcFile->setDirtyFlag();
}

void DBCMainEditor::deleteMessage(DBC_MESSAGE *msg)
{
    qDebug() << "Deleting the message and all signals. Bye bye!";
    if (!messageToItem.contains(msg)) return;
    QTreeWidgetItem *currItem = messageToItem[msg];

    int numItems = msg->sigHandler->getCount();
    QList<DBC_SIGNAL *> sigs = msg->sigHandler->getSignalsAsList();
    for (int i = numItems - 1; i > -1; i--)
    {
        deleteSignal(sigs[i]);
    }

    dbcFile->messageHandler->removeMessage(msg->ID);

    itemToMessage.remove(currItem);
    messageToItem.remove(msg);
    ui->treeDBC->removeItemWidget(currItem, 0);
    delete currItem;
    dbcFile->setDirtyFlag();
}

void DBCMainEditor::deleteSignal(DBC_SIGNAL *sig)
{
    qDebug() << "Signal about to vanish.";
    if (!signalToItem.contains(sig))
    {
        qDebug() << "Could not find signal in signalToItem collection! Aborting!";
        return;
    }
    QTreeWidgetItem *currItem = signalToItem[sig];
    if (!currItem)
    {
        qDebug() << "currItem was null in deleteSignal. Aborting!";
        return;
    }
    sig->parentMessage->sigHandler->removeSignal(sig->name);

    itemToSignal.remove(currItem);
    signalToItem.remove(sig);
    ui->treeDBC->removeItemWidget(currItem, 0);
    delete currItem; //should have been removed above but do we need to call this anyway?!
    dbcFile->setDirtyFlag();
}
