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
    connect(ui->btnSearchNext, &QAbstractButton::clicked, this, &DBCMainEditor::handleSearchForward);
    connect(ui->btnSearchPrev, &QAbstractButton::clicked, this, &DBCMainEditor::handleSearchBackward);
    connect(ui->treeDBC, &QTreeWidget::doubleClicked, this, &DBCMainEditor::onTreeDoubleClicked);

    sigEditor = new DBCSignalEditor(this);
    msgEditor = new DBCMessageEditor(this);
    nodeEditor = new DBCNodeEditor(this);

    //all three might potentially change the data stored and force the tree to be updated
    connect(sigEditor, &DBCSignalEditor::updatedTreeInfo, this, &DBCMainEditor::updatedTreeInfo);
    connect(msgEditor, &DBCMessageEditor::updatedTreeInfo, this, &DBCMainEditor::updatedTreeInfo);
    connect(nodeEditor, &DBCNodeEditor::updatedTreeInfo, this, &DBCMainEditor::updatedTreeInfo);

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
            HelpWindow::getRef()->showHelp("dbc_editor.html");
            break;
        case Qt::Key_F3:
            handleSearchForward();
            break;
        case Qt::Key_F4:
            handleSearchBackward();
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

//Double clicking is interpreted as a desire to edit the given item.
void DBCMainEditor::onTreeDoubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index)

    QTreeWidgetItem* firstCol = ui->treeDBC->currentItem();
    bool ret = false;
    DBC_MESSAGE *msg;
    DBC_SIGNAL *sig;
    DBC_NODE *node;
    uint32_t msgID;
    QString idString;

    qDebug() << firstCol->data(0, Qt::UserRole) << " - " << firstCol->text(0);

    switch (firstCol->data(0, Qt::UserRole).toInt())
    {
    case 1: //a node
        idString = firstCol->text(0).split(" ")[0];
        node = dbcFile->findNodeByName(idString);
        nodeEditor->setFileIdx(fileIdx);
        nodeEditor->setNodeRef(node);
        nodeEditor->refreshView();
        nodeEditor->show();
        break;
    case 2: //a message
        idString = firstCol->text(0).split(" ")[0];
        msgID = static_cast<uint32_t>(Utility::ParseStringToNum(idString));
        msg = dbcFile->messageHandler->findMsgByID(msgID);
        msgEditor->setMessageRef(msg);
        msgEditor->setFileIdx(fileIdx);
        //msgEditor->setWindowModality(Qt::WindowModal);
        msgEditor->refreshView();
        msgEditor->show(); //show allows the rest of the forms to keep going
        break;
    case 3: //a signal
        idString = firstCol->parent()->text(0).split(" ")[0];
        msgID = static_cast<uint32_t>(Utility::ParseStringToNum(idString));
        msg = dbcFile->messageHandler->findMsgByID(msgID);
        sig = msg->sigHandler->findSignalByName(firstCol->text(0).split(" ")[0]);
        sigEditor->setSignalRef(sig);
        sigEditor->setMessageRef(msg);
        sigEditor->setFileIdx(fileIdx);
        //sigEditor->setWindowModality(Qt::WindowModal);
        sigEditor->refreshView();
        sigEditor->show();
        break;
    }
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

void DBCMainEditor::updatedTreeInfo(QString oldData, QString newData, int type)
{

}
