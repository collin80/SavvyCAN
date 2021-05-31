#include "dbcnodeeditor.h"
#include "ui_dbcnodeeditor.h"

#include <QSettings>
#include <QKeyEvent>
#include <QColorDialog>
#include "helpwindow.h"
#include "utility.h"

DBCNodeEditor::DBCNodeEditor(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCNodeEditor)
{
    ui->setupUi(this);

    readSettings();

    dbcHandler = DBCHandler::getReference();
    dbcNode = nullptr;

    connect(ui->lineComment, &QLineEdit::editingFinished,
        [=]()
        {
            if (dbcNode == nullptr) return;
            if (dbcNode->comment != ui->lineComment->text()) dbcFile->setDirtyFlag();
            dbcNode->comment = ui->lineComment->text();
            emit updatedTreeInfo(dbcNode);
        });

    connect(ui->lineMsgName, &QLineEdit::editingFinished,
        [=]()
        {
            if (dbcNode == nullptr) return;
            if (dbcNode->name != ui->lineMsgName->text()) dbcFile->setDirtyFlag();
            dbcNode->name = ui->lineMsgName->text();
            emit updatedTreeInfo(dbcNode);
        });

    installEventFilter(this);
}

DBCNodeEditor::~DBCNodeEditor()
{
    removeEventFilter(this);
    delete ui;
}

void DBCNodeEditor::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
}

bool DBCNodeEditor::eventFilter(QObject *obj, QEvent *event)
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

void DBCNodeEditor::setFileIdx(int idx)
{
    if (idx < 0 || idx > dbcHandler->getFileCount() - 1) return;
    dbcFile = dbcHandler->getFileByIdx(idx);
}

void DBCNodeEditor::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("DBCNodeEditor/WindowSize", QSize(312, 128)).toSize());
        move(Utility::constrainedWindowPos(settings.value("DBCNodeEditor/WindowPos", QPoint(100, 100)).toPoint()));
    }
}

void DBCNodeEditor::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("DBCNodeEditor/WindowSize", size());
        settings.setValue("DBCNodeEditor/WindowPos", pos());
    }
}


void DBCNodeEditor::setNodeRef(DBC_NODE *node)
{
    dbcNode = node;
}

void DBCNodeEditor::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    refreshView();
}

void DBCNodeEditor::refreshView()
{
    ui->lineComment->setText(dbcNode->comment);
    ui->lineMsgName->setText(dbcNode->name);

    //generateSampleText();
}

void DBCNodeEditor::generateSampleText()
{
    /*
    QBrush fg, bg;

    if (dbcMessage->fgColor.isValid()) fg = QBrush(dbcMessage->fgColor);
    else fg = QBrush(QColor(dbcFile->findAttributeByName("GenMsgForegroundColor")->defaultValue.toString()));
    if (dbcMessage->bgColor.isValid()) bg = QBrush(dbcMessage->bgColor);
    else bg = QBrush(QColor(dbcFile->findAttributeByName("GenMsgBackgroundColor")->defaultValue.toString()));

    ui->listSample->clear();
    QListWidgetItem *item = new QListWidgetItem("Test String");
    item->setForeground(fg);
    item->setBackground(bg);
    ui->listSample->addItem(item);
    item = new QListWidgetItem("0x20F TestMsg");
    item->setForeground(fg);
    item->setBackground(bg);
    ui->listSample->addItem(item);
    item = new QListWidgetItem("20 FF 10 A1 BB CC 4D");
    item->setForeground(fg);
    item->setBackground(bg);
    ui->listSample->addItem(item);
    item = new QListWidgetItem("1024.3434");
    item->setForeground(fg);
    item->setBackground(bg);
    ui->listSample->addItem(item);
    */
}
