#include "dbcmessageeditor.h"
#include "ui_dbcmessageeditor.h"

#include <QSettings>
#include <QKeyEvent>
#include <QColorDialog>
#include "helpwindow.h"
#include "utility.h"

DBCMessageEditor::DBCMessageEditor(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCMessageEditor)
{
    ui->setupUi(this);

    readSettings();

    dbcHandler = DBCHandler::getReference();
    dbcMessage = nullptr;

    connect(ui->lineComment, &QLineEdit::editingFinished,
        [=]()
        {
            if (dbcMessage == nullptr) return;
            dbcMessage->comment = ui->lineComment->text();
            emit updatedTreeInfo(dbcMessage);
        });

    connect(ui->lineFrameID, &QLineEdit::editingFinished,
        [=]()
        {
            if (dbcMessage == nullptr) return;
            dbcMessage->ID = Utility::ParseStringToNum(ui->lineFrameID->text());
            emit updatedTreeInfo(dbcMessage);
        });

    connect(ui->lineMsgName, &QLineEdit::editingFinished,
        [=]()
        {
            if (dbcMessage == nullptr) return;
            dbcMessage->name = ui->lineMsgName->text().simplified().replace(' ', '_');
            emit updatedTreeInfo(dbcMessage);
        });

    connect(ui->lineFrameLen, &QLineEdit::editingFinished,
        [=]()
        {
            if (dbcMessage == nullptr) return;
            dbcMessage->len = Utility::ParseStringToNum(ui->lineFrameLen->text());
        });

    connect(ui->btnTextColor, &QAbstractButton::clicked,
        [=]()
        {
            QColor newColor = QColorDialog::getColor(dbcMessage->fgColor);

            dbcMessage->fgColor = newColor;
            DBC_ATTRIBUTE_VALUE *val = dbcMessage->findAttrValByName("GenMsgForegroundColor");
            if (val)
            {
                val->value = newColor.name();
            }
            else
            {
                DBC_ATTRIBUTE_VALUE newVal;
                newVal.attrName = "GenMsgForegroundColor";
                newVal.value = newColor.name();
                dbcMessage->attributes.append(newVal);
            }
            generateSampleText();
        });

    connect(ui->btnBackgroundColor, &QAbstractButton::clicked,
        [=]()
        {
            QColor newColor = QColorDialog::getColor(dbcMessage->bgColor);

            dbcMessage->bgColor = newColor;
            DBC_ATTRIBUTE_VALUE *val = dbcMessage->findAttrValByName("GenMsgBackgroundColor");
            if (val)
            {
                val->value = newColor.name();
            }
            else
            {
                DBC_ATTRIBUTE_VALUE newVal;
                newVal.attrName = "GenMsgBackgroundColor";
                newVal.value = newColor.name();
                dbcMessage->attributes.append(newVal);
            }
            generateSampleText();
        });

    installEventFilter(this);
}

DBCMessageEditor::~DBCMessageEditor()
{
    removeEventFilter(this);
    delete ui;
}

void DBCMessageEditor::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
}

bool DBCMessageEditor::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("messageeditor.html");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void DBCMessageEditor::setFileIdx(int idx)
{
    if (idx < 0 || idx > dbcHandler->getFileCount() - 1) return;
    dbcFile = dbcHandler->getFileByIdx(idx);

    for (int x = 0; x < dbcFile->dbc_nodes.count(); x++)
    {
        ui->comboSender->addItem(dbcFile->dbc_nodes[x].name);
    }
}

void DBCMessageEditor::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("DBCMessageEditor/WindowSize", QSize(340, 400)).toSize());
        move(settings.value("DBCMessageEditor/WindowPos", QPoint(100, 100)).toPoint());
    }
}

void DBCMessageEditor::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("DBCMessageEditor/WindowSize", size());
        settings.setValue("DBCMessageEditor/WindowPos", pos());
    }
}


void DBCMessageEditor::setMessageRef(DBC_MESSAGE *msg)
{
    dbcMessage = msg;
}

void DBCMessageEditor::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    refreshView();
}

void DBCMessageEditor::refreshView()
{
    ui->lineComment->setText(dbcMessage->comment);
    ui->lineFrameID->setText(Utility::formatCANID(dbcMessage->ID));
    ui->lineMsgName->setText(dbcMessage->name);
    ui->lineFrameLen->setText(QString::number(dbcMessage->len));
    for (int i = 0; i < ui->comboSender->count(); i++)
    {
        if (ui->comboSender->itemText(i) == dbcMessage->sender->name)
        {
            ui->comboSender->setCurrentIndex(i);
            break;
        }
    }

    generateSampleText();
}

void DBCMessageEditor::generateSampleText()
{
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
}
