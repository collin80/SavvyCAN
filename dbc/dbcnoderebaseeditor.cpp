#include "dbcnoderebaseeditor.h"
#include "ui_dbcnoderebaseeditor.h"

#include <QSettings>
#include <QKeyEvent>
#include <QColorDialog>
#include <QMessageBox>
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
            if (dbcNode == nullptr)
            {
                QMessageBox::question(this, "Node Invalid", "There was an problem identifying the selected node.",
                                                  QMessageBox::Ok);
                return;
            }

            if (lowestMsgId > 0x1FFFFFFFul)
            {
                QMessageBox::question(this, "No Valid Messages", "The node has no valid messages to change.",
                                                  QMessageBox::Ok);
                return;
            }

            uint newBase = Utility::ParseStringToNum(ui->lineNewBaseId->text());

            if(newBase <= 0 || newBase > 0x1FFFFFFFul)
            {
                QMessageBox::question(this, "Invalid Address", "The new address is outside of the valid range.",
                                                  QMessageBox::Ok);
                return;
            }

            if(newBase == lowestMsgId)
            {
                QMessageBox::question(this, "Invalid Address", "The new address is the same as the original.",
                                                  QMessageBox::Ok);
                return;
            }

            uint rebaseDiff = newBase - lowestMsgId;

            QList<DBC_MESSAGE*> messagesForNode = dbcFile->messageHandler->findMsgsByNode(dbcNode);
            if(messagesForNode.count() == 0)
            {
                QMessageBox::question(this, "No Messages", "The node has no messages to change.",
                                                  QMessageBox::Ok);
                return;
            }

            for (int i=0; i<messagesForNode.count(); i++)
            {
                uint newMsgId = messagesForNode[i]->ID + rebaseDiff;

                if(newMsgId < 0 || newMsgId > 0x1FFFFFFFul)
                {
                    QMessageBox::question(this, "Invalid Address Range", "The new starting address would cause a message to be outside of the valid address range.",
                                                      QMessageBox::Ok);
                    return;
                }
            }

            for (int i=0; i<messagesForNode.count(); i++)
            {
                messagesForNode[i]->ID += rebaseDiff;
                emit updatedTreeInfo(messagesForNode[i]);
            }

            dbcFile->setDirtyFlag();

            this->close();

        });

    connect(ui->btnCancel, &QPushButton::pressed,
        [=]()
        {
            this->close();
        });

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

bool DBCNodeRebaseEditor::refreshView()
{
    ui->lineNewBaseId->setText("");

    if(dbcNode)
    {
        QList<DBC_MESSAGE*> messagesForNode = dbcFile->messageHandler->findMsgsByNode(dbcNode);
        lowestMsgId = 0xFFFFFFFF;

        if(messagesForNode.count() == 0)
        {
            return false;
        }

        for (int i=0; i<messagesForNode.count(); i++)
        {
            if(messagesForNode[i]->ID < lowestMsgId)
                lowestMsgId = messagesForNode[i]->ID;
        }

        ui->lineOriginalBaseId->setText(Utility::formatCANID(lowestMsgId & 0x1FFFFFFFul));
        ui->lineNodeName->setText(dbcNode->name);

        return true;
    }

    return false;
}
