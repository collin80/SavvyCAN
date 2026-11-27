#include "dbcmessageeditor.h"
#include "ui_dbcmessageeditor.h"

#include <QApplication>
#include <QSettings>
#include <QKeyEvent>
#include <QColorDialog>
#include <QPalette>
#include "helpwindow.h"
#include "utility.h"

#include <QCheckBox>

DBCMessageEditor::DBCMessageEditor(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBCMessageEditor)
{
    ui->setupUi(this);

    readSettings();

    dbcHandler = DBCHandler::getReference();
    dbcMessage = nullptr;
    dbcFile = nullptr;
    suppressEditCallbacks = false;
    origID = 0;

    connect(ui->lineComment, &QLineEdit::editingFinished,
        [=]()
        {
            if (dbcMessage == nullptr) return;
            if (suppressEditCallbacks) return;
            if (dbcMessage->comment != ui->lineComment->text() && dbcFile) dbcFile->setDirtyFlag();
            dbcMessage->comment = ui->lineComment->text();
            emit updatedTreeInfo(dbcMessage, origID);
        });

    connect(ui->lineFrameID, &QLineEdit::editingFinished,
        [=]()
        {
            if (dbcMessage == nullptr) return;
            if (suppressEditCallbacks) return;
            const uint32_t newID = static_cast<uint32_t>(Utility::ParseStringToNum(ui->lineFrameID->text())) & 0x1FFFFFFFul;
            const bool newExtended = (newID > 0x7FF) ? true : dbcMessage->extendedID;
            if (((dbcMessage->ID & 0x1FFFFFFFul) != newID || dbcMessage->extendedID != newExtended) && dbcFile)
            {
                dbcFile->setDirtyFlag();
            }
            dbcMessage->ID = newID;
            dbcMessage->extendedID = newExtended;

            refreshView();
            emit updatedTreeInfo(dbcMessage, origID);
            origID = dbcMessage->ID;
        });

    connect(ui->lineMsgName, &QLineEdit::editingFinished,
        [=]()
        {
            if (dbcMessage == nullptr) return;
            if (suppressEditCallbacks) return;
            if (dbcMessage->name != ui->lineMsgName->text().simplified().replace(' ', '_') && dbcFile) dbcFile->setDirtyFlag();
            dbcMessage->name = ui->lineMsgName->text().simplified().replace(' ', '_');
            emit updatedTreeInfo(dbcMessage, origID);
        });

    connect(ui->lineFrameLen, &QLineEdit::editingFinished,
        [=]()
        {
            if (dbcMessage == nullptr) return;
            if (suppressEditCallbacks) return;
            unsigned int newLen = Utility::ParseStringToNum(ui->lineFrameLen->text());
            if (newLen > 64) newLen = 64; //CAN FD maximum
            if (dbcMessage->len != newLen && dbcFile) dbcFile->setDirtyFlag();
            dbcMessage->len = newLen;
            if (newLen > 8) dbcMessage->canFD = true; //only CAN FD carries more than 8 bytes
            refreshView();
        });

    connect(ui->checkCanFD, &QCheckBox::toggled,
        [=](bool checked)
        {
            if (dbcMessage == nullptr) return;
            if (suppressEditCallbacks) return;
            if (dbcMessage->canFD != checked && dbcFile) dbcFile->setDirtyFlag();
            dbcMessage->canFD = checked;
            if (!checked && dbcMessage->len > 8) dbcMessage->len = 8; //classic CAN carries at most 8 bytes
            refreshView();
        });

    connect(ui->checkBRS, &QCheckBox::toggled,
        [=](bool checked)
        {
            if (dbcMessage == nullptr) return;
            if (suppressEditCallbacks) return;
            if (dbcMessage->bitRateSwitch != checked && dbcFile) dbcFile->setDirtyFlag();
            dbcMessage->bitRateSwitch = checked;
        });

    connect(ui->checkExtended, &QCheckBox::toggled,
        [=](bool checked)
        {
            if (dbcMessage == nullptr) return;
            if (suppressEditCallbacks) return;
            const uint32_t maskedId = dbcMessage->ID & 0x1FFFFFFFul;
            const bool effective = (maskedId > 0x7FF) ? true : checked;
            if (dbcMessage->extendedID != effective)
            {
                dbcMessage->extendedID = effective;
                if (dbcFile) dbcFile->setDirtyFlag();
                emit updatedTreeInfo(dbcMessage, origID);
                refreshView();
            }
            else if (maskedId > 0x7FF && !checked)
            {
                refreshView();
            }
        });

    connect(ui->comboSender, &QComboBox::currentTextChanged,
            [=](const QString newText)
            {
                if (dbcMessage == nullptr || dbcFile == nullptr) return;
                if (suppressEditCallbacks) return;
                DBC_NODE *node = dbcFile->findNodeByName(newText);
                if (!node) return;
                if (node != dbcMessage->sender) dbcFile->setDirtyFlag();
                dbcMessage->sender = node;
                emit updatedTreeInfo(dbcMessage, origID);
            });

    connect(ui->comboSender->lineEdit(), &QLineEdit::editingFinished,
            [=]()
            {
                if (dbcMessage == nullptr || dbcFile == nullptr) return;
                if (suppressEditCallbacks) return;
                QString newText = ui->comboSender->currentText();
                DBC_NODE *node = dbcFile->findNodeByName(newText);
                if (!node)
                {
                    DBC_NODE *newNode = new DBC_NODE;
                    newNode->name = newText;
                    dbcFile->dbc_nodes.append(newNode);
                    node = dbcFile->findNodeByName(newText);
                    ui->comboSender->addItem(newText);
                }
                if (node != dbcMessage->sender) dbcFile->setDirtyFlag();
                dbcMessage->sender = node;
                emit updatedTreeInfo(dbcMessage, origID);
            });

    ui->btnTextColor->setAutoDefault(false);
    ui->btnBackgroundColor->setAutoDefault(false);

    connect(ui->btnTextColor, &QAbstractButton::clicked,
        [=]()
        {
            if (dbcMessage == nullptr || suppressEditCallbacks) return;
            QColor newColor = QColorDialog::getColor(dbcMessage->fgColor);
            if (dbcMessage->fgColor != newColor && dbcFile) dbcFile->setDirtyFlag();
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
            if (dbcMessage == nullptr || suppressEditCallbacks) return;
            QColor newColor = QColorDialog::getColor(dbcMessage->bgColor);
            if (dbcMessage->bgColor != newColor && dbcFile) dbcFile->setDirtyFlag();
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
            HelpWindow::getRef()->showHelp("messageeditor.md");
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

    suppressEditCallbacks = true;
    ui->comboSender->clear();
    for (int x = 0; x < dbcFile->dbc_nodes.count(); x++)
    {
        ui->comboSender->addItem(dbcFile->dbc_nodes[x]->name);
    }
    suppressEditCallbacks = false;
}

void DBCMessageEditor::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("DBCMessageEditor/WindowSize", QSize(340, 400)).toSize());
        move(Utility::constrainedWindowPos(settings.value("DBCMessageEditor/WindowPos", QPoint(100, 100)).toPoint()));
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
    if (!msg)
    {
        dbcMessage = nullptr;
        origID = 0;
        return;
    }
    dbcMessage = msg;
    origID = msg->ID;
}

void DBCMessageEditor::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    refreshView();
}

void DBCMessageEditor::refreshView()
{
    if (!dbcMessage) return;

    suppressEditCallbacks = true;

    const uint32_t maskedId = dbcMessage->ID & 0x1FFFFFFFul;
    const bool forceExtended = maskedId > 0x7FF;
    const bool isExtended = forceExtended || dbcMessage->extendedID;

    ui->lineComment->setText(dbcMessage->comment);
    ui->lineFrameID->setText(Utility::formatCANID(maskedId, isExtended));
    ui->lineMsgName->setText(dbcMessage->name);
    ui->lineFrameLen->setText(QString::number(dbcMessage->len));
    ui->checkCanFD->setChecked(dbcMessage->canFD);
    ui->checkBRS->setChecked(dbcMessage->canFD && dbcMessage->bitRateSwitch);
    ui->checkBRS->setEnabled(dbcMessage->canFD);
    if (ui->checkExtended)
    {
        ui->checkExtended->setChecked(isExtended);
        ui->checkExtended->setEnabled(!forceExtended);
    }

    if (dbcMessage->sender)
    {
        for (int i = 0; i < ui->comboSender->count(); i++)
        {
            if (ui->comboSender->itemText(i) == dbcMessage->sender->name)
            {
                ui->comboSender->setCurrentIndex(i);
                break;
            }
        }
    }

    suppressEditCallbacks = false;

    generateSampleText();
}

void DBCMessageEditor::generateSampleText()
{
    if (!dbcMessage) return;

    QBrush fg, bg;

    DBC_ATTRIBUTE *fgAttr = dbcFile ? dbcFile->findAttributeByName("GenMsgForegroundColor") : nullptr;
    DBC_ATTRIBUTE *bgAttr = dbcFile ? dbcFile->findAttributeByName("GenMsgBackgroundColor") : nullptr;

    if (dbcMessage->fgColor.isValid()) fg = QBrush(dbcMessage->fgColor);
    else if (fgAttr) fg = QBrush(QColor(fgAttr->defaultValue.toString()));
    else fg = QBrush(QApplication::palette().color(QPalette::WindowText));

    if (dbcMessage->bgColor.isValid()) bg = QBrush(dbcMessage->bgColor);
    else if (bgAttr) bg = QBrush(QColor(bgAttr->defaultValue.toString()));
    else bg = QBrush(QApplication::palette().color(QPalette::Base));

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
