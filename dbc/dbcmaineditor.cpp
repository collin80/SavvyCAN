#include "dbcmaineditor.h"
#include "ui_dbcmaineditor.h"

#include <QMenu>
#include <QMessageBox>
#include <QSettings>

#define MT_COLUMN_COUNT   5

DBCMainEditor::DBCMainEditor(DBCHandler *handler, const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCMainEditor)
{
    ui->setupUi(this);

    readSettings();

    dbcHandler = handler;
    referenceFrames = frames;

    QStringList headers;
    headers << "Node Name" << "Comment";
    ui->NodesTable->setColumnCount(2);
    ui->NodesTable->setColumnWidth(0, 240);
    ui->NodesTable->setColumnWidth(1, 700);
    ui->NodesTable->setHorizontalHeaderLabels(headers);
    ui->NodesTable->horizontalHeader()->setStretchLastSection(true);

    QStringList headers2;
    headers2 << "Msg ID" << "Msg Name" << "Data Len" << "Signals" << "Comment";
    ui->MessagesTable->setColumnCount(MT_COLUMN_COUNT);
    ui->MessagesTable->setColumnWidth(0, 80);
    ui->MessagesTable->setColumnWidth(1, 240);
    ui->MessagesTable->setColumnWidth(2, 80);
    ui->MessagesTable->setColumnWidth(3, 80);
    ui->MessagesTable->setColumnWidth(4, 340);
    ui->MessagesTable->setHorizontalHeaderLabels(headers2);
    ui->MessagesTable->horizontalHeader()->setStretchLastSection(true);

    connect(ui->NodesTable, SIGNAL(cellChanged(int,int)), this, SLOT(onCellChangedNode(int,int)));
    connect(ui->NodesTable, SIGNAL(cellClicked(int,int)), this, SLOT(onCellClickedNode(int,int)));
    connect(ui->NodesTable, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onCustomMenuNode(QPoint)));
    ui->NodesTable->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->MessagesTable, SIGNAL(cellChanged(int,int)), this, SLOT(onCellChangedMessage(int,int)));
    connect(ui->MessagesTable, SIGNAL(cellClicked(int,int)), this, SLOT(onCellClickedMessage(int,int)));
    connect(ui->MessagesTable, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onCustomMenuMessage(QPoint)));
    ui->MessagesTable->setContextMenuPolicy(Qt::CustomContextMenu);

    sigEditor = new DBCSignalEditor(handler);
}

void DBCMainEditor::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    inhibitCellChanged = true;
    refreshNodesTable();
    if (dbcFile->dbc_nodes.count() > 0)
        refreshMessagesTable(&dbcFile->dbc_nodes.at(0));

    currRow = 0;
    inhibitCellChanged = false;
}

DBCMainEditor::~DBCMainEditor()
{
    delete ui;
    delete sigEditor;
}

void DBCMainEditor::setFileIdx(int idx)
{
    if (idx < 0 || idx > dbcHandler->getFileCount() - 1) return;
    dbcFile = dbcHandler->getFileByIdx(idx);
    fileIdx = idx;
}

void DBCMainEditor::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
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

void DBCMainEditor::onCustomMenuNode(QPoint point)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(tr("Delete currently selected node"), this, SLOT(deleteCurrentNode()));

    menu->popup(ui->NodesTable->mapToGlobal(point));

}

void DBCMainEditor::onCustomMenuMessage(QPoint point)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(tr("Delete currently selected message"), this, SLOT(deleteCurrentMessage()));

    menu->popup(ui->MessagesTable->mapToGlobal(point));

}

void DBCMainEditor::deleteCurrentNode()
{
    int thisRow = ui->NodesTable->currentRow();
    QString nodeName = ui->NodesTable->item(thisRow, 0)->text();
    if (nodeName.length() > 0 && nodeName.compare("Vector__XXX", Qt::CaseInsensitive) != 0)
    {
        ui->NodesTable->removeRow(thisRow);
        dbcFile->dbc_nodes.removeAt(thisRow);
        inhibitCellChanged = true;
        refreshMessagesTable(&dbcFile->dbc_nodes[0]);
        ui->NodesTable->selectRow(0);
        inhibitCellChanged = false;
    }
}

void DBCMainEditor::deleteCurrentMessage()
{
    int thisRow = ui->MessagesTable->currentRow();
    if (ui->MessagesTable->item(thisRow, 0)->text().length() > 0)
    {
        ui->MessagesTable->removeRow(thisRow);
        dbcFile->messageHandler->removeMessageByIndex(thisRow);
    }
}

void DBCMainEditor::onCellChangedNode(int row,int col)
{
    if (inhibitCellChanged) return;
    if (row == ui->NodesTable->rowCount() - 1)
    {
        //if this was the last row then it's new and we need to both
        //create another potentially new record underneath and also
        //add a node entry in the nodes list and store the record
        //That is, so long as the node name was filled out
        if (col == 0)
        {
            DBC_NODE newNode;
            QString newName =  ui->NodesTable->item(row, col)->text().simplified().replace(' ', '_');
            qDebug() << "new name: " << newName;
            if (newName.length() == 0) return;
            if (dbcFile->findNodeByName(newName) != NULL) //duplicates an existing node!
            {
                QMessageBox msg;
                msg.setParent(0);
                msg.setText("An existing node with that name already exists! Aborting!");
                msg.exec();
                return;
            }
            newNode.name = newName;
            dbcFile->dbc_nodes.append(newNode);
            qDebug() <<  "# of nodes now " << dbcFile->dbc_nodes.count();
            QTableWidgetItem *widgetName = new QTableWidgetItem(newName);
            inhibitCellChanged = true;
            ui->NodesTable->setItem(row, col, widgetName);
            ui->NodesTable->insertRow(ui->NodesTable->rowCount());
            inhibitCellChanged = false;
        }
    }
    else
    {
        if (col == 0)
        {
            DBC_NODE *oldNode = dbcFile->findNodeByIdx(row);
            QString nodeName = ui->NodesTable->item(row, col)->text().simplified().replace(' ', '_');
            if (oldNode == NULL) return;
            if (row != 0) oldNode->name = nodeName;
            else nodeName = oldNode->name;
            inhibitCellChanged = true;
            QTableWidgetItem *widgetName = new QTableWidgetItem(nodeName);
            ui->NodesTable->setItem(row, col, widgetName);
            inhibitCellChanged = false;
        }
        if (col == 1) //must have been col 1 then
        {
            QString nodeName = ui->NodesTable->item(row, 0)->text().simplified().replace(' ', '_');
            qDebug() << "searching for node " << nodeName;
            DBC_NODE *thisNode = dbcFile->findNodeByName(nodeName);
            if (thisNode == NULL) return;
            thisNode->comment = ui->NodesTable->item(row, col)->text().simplified();
            qDebug() << "New comment: " << thisNode->comment;
        }
    }
    ui->NodesTable->setCurrentCell(row, col);
}

void DBCMainEditor::onCellChangedMessage(int row,int col)
{
    QTableWidgetItem* item  = NULL;
    bool ret                = false;
    DBC_MESSAGE *msg        = NULL;
    uint msgID;

    if (inhibitCellChanged) return;

    DBC_NODE *node = dbcFile->findNodeByIdx(ui->NodesTable->currentRow());
    if (node == NULL)
    {
        qDebug() << "No node set?!? This is bad!";
        return;
    }

    item = ui->MessagesTable->item(row, 0);
    if(!item) return;

    msgID = Utility::ParseStringToNum2(item->text(), &ret);
    msg = dbcFile->messageHandler->findMsgByID(msgID);

    switch(col)
    {
        case 0: //msg id
        {
            /* sanity checks */
            if(!ret)
            {
                /* bad message id */
                ui->MessagesTable->item(row, 0)->setText("");
                return;
            }
            if (msg != NULL)
            {
                QMessageBox msg;
                msg.setParent(0);
                msg.setText("An existing msg with that ID already exists! Aborting!");
                msg.exec();

                ui->MessagesTable->item(row, 0)->setText("");
                return;
            }

            /* insert row */
            DBC_MESSAGE newMsg;
            newMsg.ID = msgID;
            newMsg.name = "";
            newMsg.sender = node;
            newMsg.len = 0;
            for (int i = 0; i < referenceFrames->length(); i++)
            {
                if ((uint) referenceFrames->at(i).ID == msgID)
                {
                    newMsg.len = referenceFrames->at(i).len;
                    break;
                }
            }
            dbcFile->messageHandler->addMessage(newMsg);

            /* insert message in table */
            inhibitCellChanged = true;

            item =  ui->MessagesTable->item(row, 0);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setText(Utility::formatNumber(msgID));

            for(int i=1 ; i<MT_COLUMN_COUNT ; i++)
            {
                item = ui->MessagesTable->item(row, i);
                item->setFlags(item->flags() | Qt::ItemIsEditable);
            }

            /* set length */
            item = ui->MessagesTable->item(row, 2);
            item->setText(QString::number(newMsg.len));

            inhibitCellChanged = false;

            /* insert a new row */
            insertBlankRow();
            break;
        }
        case 1: //msg name
        {
            QString msgName = ui->MessagesTable->item(row, 1)->text().simplified().replace(' ', '_');
            if (msgName.length() == 0) return;
            if( ret && (msg!=NULL) )
                msg->name = msgName;
            break;
        }
        case 2: //data length
        {
            bool parseOk = false;
            uint msgLen = ui->MessagesTable->item(row, col)->text().toUInt(&parseOk);

            /* sanity checks */
            if(!parseOk)
            {
                ui->MessagesTable->item(row, col)->setText("");
                return;
            }
            if (msgLen > 8)
            {
                msgLen = 8;
                ui->MessagesTable->item(row, col)->setText(QString::number(msgLen));
            }

            if( ret && (msg!=NULL) )
                msg->len = msgLen;
            break;
        }
        case 3: //signals (number) - we don't handle anything here. User cannot directly change this value
            break;
        case 4: //comment
        {
            QString msgComment = ui->MessagesTable->item(row, col)->text().simplified();
            if( ret && (msgComment!=NULL) )
                msg->comment = msgComment;
            break;
        }
    }

    ui->MessagesTable->setCurrentCell(row, col);
}

void DBCMainEditor::onCellClickedNode(int row, int col)
{
    //reload messages list if the currently selected row has changed.
    qDebug() << "Clicked at row " << row  << " col " << col;
    if (row != currRow)
    {
        currRow = row;
        QTableWidgetItem *item = ui->NodesTable->item(currRow, 0);
        QString nodeName;
        if (item == NULL) return;

        nodeName = item->text();
        qDebug() << "Trying to find node with name " << nodeName;
        DBC_NODE *node = dbcFile->findNodeByName(nodeName);
        //qDebug() << "Address of node: " << (int)node;
        inhibitCellChanged = true;
        refreshMessagesTable(node);
        inhibitCellChanged = false;
    }
}

void DBCMainEditor::onCellClickedMessage(int row, int col)
{
    if (col == 3) //3 is the signals field. If clicked we go to the signals dialog
    {
        QTableWidgetItem* msg = ui->MessagesTable->item(row, 0);
        if(msg) {
            QString idString = msg->text();
            DBC_MESSAGE *message = dbcFile->messageHandler->findMsgByID(Utility::ParseStringToNum(idString));
            sigEditor->setMessageRef(message);
            sigEditor->setFileIdx(fileIdx);
            sigEditor->exec(); //blocks this window from being active until we're done
            //now update the displayed # of signals
            inhibitCellChanged = true;
            QTableWidgetItem *replacement = new QTableWidgetItem(QString::number(message->sigHandler->getCount()));
            ui->MessagesTable->setItem(row, col, replacement);
            inhibitCellChanged = false;
        }
    }
}

void DBCMainEditor::refreshNodesTable()
{
    ui->NodesTable->clearContents(); //first step, clear out any previous crap
    ui->NodesTable->setRowCount(0);

    int rowIdx;

    if (dbcFile->findNodeByName("Vector__XXX") == NULL)
    {
        DBC_NODE newNode;
        newNode.name = "Vector__XXX";
        newNode.comment = "Default node if no other node is specified";
        dbcFile->dbc_nodes.append(newNode);
    }

    for (int x = 0; x < dbcFile->dbc_nodes.count(); x++)
    {
        DBC_NODE node = dbcFile->dbc_nodes.at(x);
        QTableWidgetItem *nodeName = new QTableWidgetItem(node.name);
        QTableWidgetItem *nodeComment = new QTableWidgetItem(node.comment);
        rowIdx = ui->NodesTable->rowCount();
        ui->NodesTable->insertRow(rowIdx);
        ui->NodesTable->setItem(rowIdx, 0, nodeName);
        ui->NodesTable->setItem(rowIdx, 1, nodeComment);
    }

    //insert a fresh entry at the bottom that contains nothing
    ui->NodesTable->insertRow(ui->NodesTable->rowCount());
    ui->NodesTable->selectRow(0);
}

void DBCMainEditor::refreshMessagesTable(const DBC_NODE *node)
{
    ui->MessagesTable->clearContents();
    ui->MessagesTable->setRowCount(0);

    qDebug() << "In refreshMessagesTable";

    int rowIdx;

    if (node != NULL)
    {
        for (int x = 0; x < dbcFile->messageHandler->getCount(); x++)
        {
            DBC_MESSAGE *msg = dbcFile->messageHandler->findMsgByIdx(x);
            if (msg->sender == node)
            {
                //many of these are simplistic first versions just to test functionality.
                QTableWidgetItem *msgID = new QTableWidgetItem(Utility::formatNumber(msg->ID));
                QTableWidgetItem *msgName = new QTableWidgetItem(msg->name);
                QTableWidgetItem *msgLen = new QTableWidgetItem(QString::number(msg->len));
                QTableWidgetItem *msgSignals = new QTableWidgetItem(QString::number(msg->sigHandler->getCount()));
                QTableWidgetItem *msgComment = new QTableWidgetItem(msg->comment);

                rowIdx = ui->MessagesTable->rowCount();
                ui->MessagesTable->insertRow(rowIdx);
                ui->MessagesTable->setItem(rowIdx, 0, msgID);
                ui->MessagesTable->setItem(rowIdx, 1, msgName);
                ui->MessagesTable->setItem(rowIdx, 2, msgLen);
                ui->MessagesTable->setItem(rowIdx, 3, msgSignals);
                ui->MessagesTable->setItem(rowIdx, 4, msgComment);
                //note that there is a sending node field in the structure but we're
                //not displaying it. It has to be the node we selected from the node list
                //so no need to show that here.
            }
        }
    }

    //insert blank record that can be used to add new messages
    insertBlankRow();
}

void DBCMainEditor::insertBlankRow()
{
    int rowIdx = ui->MessagesTable->rowCount();
    ui->MessagesTable->insertRow(rowIdx);
    for(int i=1 ; i<MT_COLUMN_COUNT ; i++)
    {
        QTableWidgetItem *item = new QTableWidgetItem("");
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        ui->MessagesTable->setItem(rowIdx, i, item);
    }
}
