#include "dbcmaineditor.h"
#include "ui_dbcmaineditor.h"

#include <QMessageBox>

DBCMainEditor::DBCMainEditor(DBCHandler *handler, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCMainEditor)
{
    ui->setupUi(this);

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


    connect(ui->MessagesTable, SIGNAL(cellChanged(int,int)), this, SLOT(onCellChangedMessage(int,int)));
    connect(ui->MessagesTable, SIGNAL(cellClicked(int,int)), this, SLOT(onCellClickedMessage(int,int)));

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
                msg.setText("An existing node with that name already exists! Aborting!");
                msg.show();
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
            oldNode->name = nodeName;
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

}

void DBCMainEditor::onCellChangedMessage(int row,int col)
{
    if (inhibitCellChanged) return;
    if (row == ui->MessagesTable->rowCount() - 1)
    {
        ui->MessagesTable->insertRow(ui->MessagesTable->rowCount());
    }
}

void DBCMainEditor::onCellClickedNode(int row, int col)
{
    //reload messages list if the currently selected row has changed.
    qDebug() << "Entered at row " << row  << " col " << col;
    if (row != currRow)
    {
        currRow = row;
        QTableWidgetItem *item = ui->NodesTable->item(currRow, 0);
        QString nodeName;
        if (nodeName == NULL) return;

        nodeName = item->text();
        qDebug() << "Trying to find node with name " << nodeName;
        DBC_NODE *node = dbcHandler->findNodeByName(nodeName);
        //qDebug() << "Address of node: " << (int)node;
        refreshMessagesTable(node);
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
    }
}

void DBCMainEditor::refreshNodesTable()
{
    ui->NodesTable->clearContents(); //first step, clear out any previous crap
    ui->NodesTable->setRowCount(0);

    int rowIdx;

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
