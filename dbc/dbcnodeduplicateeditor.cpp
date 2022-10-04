#include "dbcnodeduplicateeditor.h"
#include "ui_dbcnodeduplicateeditor.h"

#include <QSettings>
#include <QKeyEvent>
#include <QColorDialog>
#include "helpwindow.h"
#include "utility.h"

DBCNodeDuplicateEditor::DBCNodeDuplicateEditor(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCNodeDuplicateEditor)
{
    ui->setupUi(this);

    readSettings();

    dbcHandler = DBCHandler::getReference();
    dbcNode = nullptr;

        connect(ui->btnDuplicate, &QPushButton::pressed,
            [=]()
            {
                if (dbcNode == nullptr) return;
                if (lowestMsgId > 0x1FFFFFFFul) return;

                uint newBase = Utility::ParseStringToNum(ui->lineNewBaseId->text());

                if (newBase <= 0x1FFFFFFFul && newBase != lowestMsgId)
                {
                    uint rebaseDiff = newBase - lowestMsgId;

                    QList<DBC_MESSAGE*> messagesForNode = dbcFile->messageHandler->findMsgsByNode(dbcNode);
                    if(messagesForNode.count() == 0)
                    {
                        return;
                    }

                    if(ui->lineNodeName->text().isEmpty())
                    {
                        //tell!
                        return;
                    }

                    QString newNodeName = ui->lineNodeName->text();
                    emit createNode(newNodeName);

                    DBC_NODE *nodePtr = dbcFile->findNodeByName(newNodeName);

                    if(nodePtr == nullptr)
                    {
                        //uhoh
                        return;
                    }

                    for (int i=0; i<messagesForNode.count(); i++)
                    {
                        uint newMsgId = messagesForNode[i]->ID + rebaseDiff;
                        emit cloneMessageToNode(nodePtr, messagesForNode[i], newMsgId);
                    }

                    dbcFile->setDirtyFlag();
                    emit nodeAdded();
                }
            });

        connect(ui->btnCancel, &QPushButton::pressed,
            [=]()
            {


            });

    installEventFilter(this);
}

DBCNodeDuplicateEditor::~DBCNodeDuplicateEditor()
{
    removeEventFilter(this);
    delete ui;
}

void DBCNodeDuplicateEditor::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
}

bool DBCNodeDuplicateEditor::eventFilter(QObject *obj, QEvent *event)
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

void DBCNodeDuplicateEditor::setFileIdx(int idx)
{
    if (idx < 0 || idx > dbcHandler->getFileCount() - 1) return;
    dbcFile = dbcHandler->getFileByIdx(idx);
}

void DBCNodeDuplicateEditor::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("DBCNodeDuplicateEditor/WindowSize", QSize(312, 128)).toSize());
        move(Utility::constrainedWindowPos(settings.value("DBCNodeDuplicateEditor/WindowPos", QPoint(100, 100)).toPoint()));
    }
}

void DBCNodeDuplicateEditor::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("DBCNodeDuplicateEditor/WindowSize", size());
        settings.setValue("DBCNodeDuplicateEditor/WindowPos", pos());
    }
}


void DBCNodeDuplicateEditor::setNodeRef(DBC_NODE *node)
{
    dbcNode = node;
}

void DBCNodeDuplicateEditor::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    refreshView();
}

void DBCNodeDuplicateEditor::refreshView()
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
        ui->lineNodeName->setText(dbcNode->name + QString("_Copy"));
    }

    //generateSampleText();
}
