#include "dbcnoderebaseeditor.h"
#include "ui_dbcnoderebaseeditor.h"

#include <QSettings>
#include <QKeyEvent>
#include <QColorDialog>
#include "helpwindow.h"
#include "utility.h"

DBCNodeRebaseEditor::DBCNodeRebaseEditor(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCNodeRebaseEditor)
{
    ui->setupUi(this);

    readSettings();

    dbcHandler = DBCHandler::getReference();
    dbcNode = nullptr;

        connect(ui->btnDoRebase, &QPushButton::pressed,
            [=]()
            {
                if (dbcNode == nullptr) return;
                if (lowestMsgId > 0x1FFFFFFFul) return;

                uint newBase = Utility::ParseStringToNum(ui->lineEdit->text());

                if (newBase <= 0x1FFFFFFFul && newBase != lowestMsgId)
                {
                    uint rebaseDiff = newBase - lowestMsgId;

                    QList<DBC_MESSAGE*> messagesForNode = dbcFile->messageHandler->findMsgsByNode(dbcNode);
                    if(messagesForNode.count() == 0)
                    {
                        return;
                    }

                    for (int i=0; i<messagesForNode.count(); i++)
                    {
                        messagesForNode[i]->ID += rebaseDiff;
                        emit updatedTreeInfo(messagesForNode[i]);
                    }

                    dbcFile->setDirtyFlag();
                }


            });

//    connect(ui->lineOriginalBaseId, &QLineEdit::editingFinished,
//        [=]()
//        {
//            if (dbcNode == nullptr) return;
//            if (dbcNode->comment != ui->lineComment->text()) dbcFile->setDirtyFlag();
//            dbcNode->comment = ui->lineComment->text();
//            emit updatedTreeInfo(dbcNode);
//        });

//    connect(ui->lineMsgName, &QLineEdit::editingFinished,
//        [=]()
//        {
//            if (dbcNode == nullptr) return;
//            if (dbcNode->name != ui->lineMsgName->text()) dbcFile->setDirtyFlag();
//            dbcNode->name = ui->lineMsgName->text();
//            emit updatedTreeInfo(dbcNode);
//        });

    installEventFilter(this);
}

DBCNodeRebaseEditor::~DBCNodeRebaseEditor()
{
    removeEventFilter(this);
    delete ui;
}

void DBCNodeRebaseEditor::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
}

bool DBCNodeRebaseEditor::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("nodeeditor.md");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void DBCNodeRebaseEditor::setFileIdx(int idx)
{
    if (idx < 0 || idx > dbcHandler->getFileCount() - 1) return;
    dbcFile = dbcHandler->getFileByIdx(idx);
}

void DBCNodeRebaseEditor::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("DBCNodeRebaseEditor/WindowSize", QSize(312, 128)).toSize());
        move(Utility::constrainedWindowPos(settings.value("DBCNodeRebaseEditor/WindowPos", QPoint(100, 100)).toPoint()));
    }
}

void DBCNodeRebaseEditor::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("DBCNodeRebaseEditor/WindowSize", size());
        settings.setValue("DBCNodeRebaseEditor/WindowPos", pos());
    }
}


void DBCNodeRebaseEditor::setNodeRef(DBC_NODE *node)
{
    dbcNode = node;
}

void DBCNodeRebaseEditor::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    refreshView();
}

void DBCNodeRebaseEditor::refreshView()
{
    if(dbcNode)
    {
        QList<DBC_MESSAGE*> messagesForNode = dbcFile->messageHandler->findMsgsByNode(dbcNode);
        if(messagesForNode.count() == 0)
        {
            //??

        }

        lowestMsgId = 0xFFFFFFFF;

        for (int i=0; i<messagesForNode.count(); i++)
        {
            if(messagesForNode[i]->ID < lowestMsgId)
                lowestMsgId = messagesForNode[i]->ID;
        }

        ui->lineOriginalBaseId->setText(Utility::formatCANID(lowestMsgId & 0x1FFFFFFFul));
        ui->lineMsgName->setText(dbcNode->name);
    }

    //generateSampleText();
}
