#include "dbcmaineditor.h"
#include "ui_dbcmaineditor.h"

#include <QMenu>
#include <QMessageBox>
#include <QSettings>

DBCMainEditor::DBCMainEditor(DBCHandler *handler, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCMainEditor)
{
    ui->setupUi(this);

    readSettings();

    dbcHandler = handler;

    QStringList headers;
    headers << "Node Name" << "Comment";
    ui->NodesTable->setColumnCount(2);
    ui->NodesTable->setColumnWidth(0, 240);
    ui->NodesTable->setColumnWidth(1, 700);
    ui->NodesTable->setHorizontalHeaderLabels(headers);
    ui->NodesTable->horizontalHeader()->setStretchLastSection(true);

    QStringList headers2;
    headers2 << "Msg ID" << "Msg Name" << "Data Len" << "Signals" << "Comment";
    ui->MessagesTable->setColumnCount(5);
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
    if (dbcHandler->dbc_nodes.count() > 0)
        refreshMessagesTable(&dbcHandler->dbc_nodes.at(0));

    currRow = 0;
    inhibitCellChanged = false;
}

DBCMainEditor::~DBCMainEditor()
{
    delete ui;
    delete sigEditor;
}

void DBCMainEditor::closeEvent(QCloseEvent *event)
{
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
        dbcHandler->dbc_nodes.removeAt(thisRow);
        inhibitCellChanged = true;
        refreshMessagesTable(&dbcHandler->dbc_nodes[0]);
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
        dbcHandler->dbc_messages.removeAt(thisRow);
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
            if (dbcHandler->findNodeByName(newName) != NULL) //duplicates an existing node!
            {
                QMessageBox msg;
                msg.setParent(0);
                msg.setText("An existing node with that name already exists! Aborting!");
                msg.exec();
                return;
            }
            newNode.name = newName;
            dbcHandler->dbc_nodes.append(newNode);
            qDebug() <<  "# of nodes now " << dbcHandler->dbc_nodes.count();
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
            DBC_NODE *oldNode = dbcHandler->findNodeByIdx(row);
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
            DBC_NODE *thisNode = dbcHandler->findNodeByName(nodeName);
            if (thisNode == NULL) return;
            thisNode->comment = ui->NodesTable->item(row, col)->text().simplified();
            qDebug() << "New comment: " << thisNode->comment;
        }
    }
    ui->NodesTable->setCurrentCell(row, col);
}

void DBCMainEditor::onCellChangedMessage(int row,int col)
{
    if (inhibitCellChanged) return;

    QTableWidgetItem *replacement = NULL;
    int msgID;
    QString msgName;
    int msgLen;
    QString msgComment;
    DBC_NODE *node = dbcHandler->findNodeByIdx(ui->NodesTable->currentRow());
    if (node == NULL) dbcHandler->findNodeByIdx(0);

    switch(col)
    {
    case 0: //msg id
        msgID = ui->MessagesTable->item(row, col)->text().toInt(NULL, 16);
        if (row == ui->MessagesTable->rowCount() - 1) //new record
        {
            DBC_MESSAGE newMsg;
            if (dbcHandler->findMsgByID(msgID) != NULL)
            {
                QMessageBox msg;
                msg.setParent(0);
                msg.setText("An existing msg with that ID already exists! Aborting!");
                msg.exec();
                return;
            }
            newMsg.ID = msgID;
            newMsg.name = "";
            newMsg.sender = node;
            dbcHandler->dbc_messages.append(newMsg);
        }
        else //editing an existing record
        {
            DBC_MESSAGE *msg = dbcHandler->findMsgByIdx(row);
            msg->ID = msgID;
        }
        inhibitCellChanged = true;
        replacement = new QTableWidgetItem(QString::number(msgID, 16));
        ui->MessagesTable->setItem(row, col, replacement);
        inhibitCellChanged = false;
        break;

    case 1: //msg name
        msgName = ui->MessagesTable->item(row, col)->text().simplified().replace(' ', '_');
        if (msgName.length() == 0) return;
        if (row == ui->MessagesTable->rowCount() - 1) //new record
        {
            DBC_MESSAGE newMsg;
            if (dbcHandler->findMsgByName(msgName) != NULL)
            {
                QMessageBox msg;
                msg.setParent(0);
                msg.setText("An existing msg with that name already exists! Aborting!");
                msg.exec();
                return;
            }
            newMsg.ID = -1;
            newMsg.name = msgName;
            newMsg.sender = node;
            dbcHandler->dbc_messages.append(newMsg);
        }
        else
        {
            DBC_MESSAGE *msg = dbcHandler->findMsgByIdx(row);
            msg->name = msgName;
        }
        inhibitCellChanged = true;
        replacement = new QTableWidgetItem(msgName);
        ui->MessagesTable->setItem(row, col, replacement);
        inhibitCellChanged = false;
        break;

    case 2: //data length
        msgLen = ui->MessagesTable->item(row, col)->text().toInt();
        if (msgLen < 0) msgLen = 0;
        if (msgLen > 8) msgLen = 8;
        if (row == ui->MessagesTable->rowCount() - 1) //new record
        {
            DBC_MESSAGE newMsg;
            newMsg.ID = -1;
            newMsg.name = "";
            newMsg.len = msgLen;
            newMsg.sender = node;
            dbcHandler->dbc_messages.append(newMsg);
        }
        else //editing an existing record
        {
            DBC_MESSAGE *msg = dbcHandler->findMsgByIdx(row);
            msg->len = msgLen;
        }
        inhibitCellChanged = true;
        replacement = new QTableWidgetItem(QString::number(msgLen, 16));
        ui->MessagesTable->setItem(row, col, replacement);
        inhibitCellChanged = false;

        break;
    case 3: //signals (number) - we don't handle anything here. User cannot directly change this value
        break;
    case 4: //comment
        msgComment = ui->MessagesTable->item(row, col)->text().simplified();
        if (row == ui->MessagesTable->rowCount() - 1) //new record
        {
            DBC_MESSAGE newMsg;
            newMsg.ID = -1;
            newMsg.name = "";
            newMsg.len = 0;
            newMsg.comment = msgComment;
            newMsg.sender = node;
            dbcHandler->dbc_messages.append(newMsg);
        }
        else //editing an existing record
        {
            DBC_MESSAGE *msg = dbcHandler->findMsgByIdx(row);
            msg->comment = msgComment;
        }
        inhibitCellChanged = true;
        replacement = new QTableWidgetItem(msgComment);
        ui->MessagesTable->setItem(row, col, replacement);
        inhibitCellChanged = false;
        break;

    }

    if (row == ui->MessagesTable->rowCount() - 1)
    {
        ui->MessagesTable->insertRow(ui->MessagesTable->rowCount());
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
        DBC_NODE *node = dbcHandler->findNodeByName(nodeName);
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
        QString idString = ui->MessagesTable->item(row, 0)->text();
        DBC_MESSAGE *message = dbcHandler->findMsgByID(idString.toInt(NULL, 16));
        sigEditor->setMessageRef(message);
        sigEditor->exec(); //blocks this window from being active until we're done
        //now update the displayed # of signals
        inhibitCellChanged = true;
        QTableWidgetItem *replacement = new QTableWidgetItem(QString::number(message->msgSignals.count()));
        ui->MessagesTable->setItem(row, col, replacement);
        inhibitCellChanged = false;
    }
}

void DBCMainEditor::refreshNodesTable()
{
    ui->NodesTable->clearContents(); //first step, clear out any previous crap
    ui->NodesTable->setRowCount(0);

    int rowIdx;

    if (dbcHandler->findNodeByName("Vector__XXX") == NULL)
    {
        DBC_NODE newNode;
        newNode.name = "Vector__XXX";
        newNode.comment = "Default node if no other node is specified";
        dbcHandler->dbc_nodes.append(newNode);
    }

    for (int x = 0; x < dbcHandler->dbc_nodes.count(); x++)
    {
        DBC_NODE node = dbcHandler->dbc_nodes.at(x);
        QTableWidgetItem *nodeName = new QTableWidgetItem(node.name);
        QTableWidgetItem *nodeComment = new QTableWidgetItem(node.comment);
        rowIdx = ui->NodesTable->rowCount();
        ui->NodesTable->insertRow(rowIdx);
        ui->NodesTable->setItem(rowIdx, 0, nodeName);
        ui->NodesTable->setItem(rowIdx, 1, nodeComment);
    }

    //insert a fresh entry at the bottom that contains nothing
    ui->NodesTable->insertRow(ui->NodesTable->rowCount());
}

void DBCMainEditor::refreshMessagesTable(const DBC_NODE *node)
{
    ui->MessagesTable->clearContents();
    ui->MessagesTable->setRowCount(0);

    qDebug() << "In refreshMessagesTable";

    int rowIdx;

    if (node != NULL)
    {
        for (int x = 0; x < dbcHandler->dbc_messages.count(); x++)
        {
            DBC_MESSAGE msg = dbcHandler->dbc_messages.at(x);
            if (msg.sender == node)
            {
                //many of these are simplistic first versions just to test functionality.
                QTableWidgetItem *msgID = new QTableWidgetItem(QString::number(msg.ID, 16));
                QTableWidgetItem *msgName = new QTableWidgetItem(msg.name);
                QTableWidgetItem *msgLen = new QTableWidgetItem(QString::number(msg.len));
                QTableWidgetItem *msgSignals = new QTableWidgetItem(QString::number(msg.msgSignals.count()));
                QTableWidgetItem *msgComment = new QTableWidgetItem(msg.comment);

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
    ui->MessagesTable->insertRow(ui->MessagesTable->rowCount());
}
