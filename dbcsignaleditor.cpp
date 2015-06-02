#include "dbcsignaleditor.h"
#include "ui_dbcsignaleditor.h"

DBCSignalEditor::DBCSignalEditor(DBCHandler *handler, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCSignalEditor)
{
    ui->setupUi(this);
    dbcHandler = handler;
    dbcMessage = NULL;
/*
    QStringList headers;
    headers << "Node Name" << "Comment";
    ui->signalsTable->setColumnCount(2);
    ui->signalsTable->setColumnWidth(0, 240);
    ui->signalsTable->setColumnWidth(1, 700);
    ui->signalsTable->setHorizontalHeaderLabels(headers);
    ui->signalsTable->horizontalHeader()->setStretchLastSection(true);
    name,
    a start bit,
    a length,
    byte order (intel or motorola),
    a type (signed, unsigned, float, string),
    a scaling factor,
    a bias,
    a minimum and
    maximum,
    a receiving node,
    a unit name,
    an optional comment,
    and an optional value list




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

*/

}

DBCSignalEditor::~DBCSignalEditor()
{
    delete ui;
}

void DBCSignalEditor::setMessageRef(DBC_MESSAGE *msg)
{
    dbcMessage = msg;
}

void DBCSignalEditor::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

}
