#include "scriptingwindow.h"
#include "ui_scriptingwindow.h"

#include <QFile>
#include <QFileDialog>
#include <QSettings>

#include "connections/canconmanager.h"

ScriptingWindow::ScriptingWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ScriptingWindow)
{
    ui->setupUi(this);

    editor = new JSEdit();
    editor->setFrameShape(JSEdit::NoFrame);
    editor->setWordWrapMode(QTextOption::NoWrap);
    editor->setTabStopWidth(4);
    editor->setEnabled(false);
    editor->show();
    ui->verticalLayout->insertWidget(2,editor, 10);

    readSettings();

    modelFrames = frames;

    connect(ui->btnLoadScript, &QAbstractButton::pressed, this, &ScriptingWindow::loadNewScript);
    connect(ui->btnNewScript, &QAbstractButton::pressed, this, &ScriptingWindow::createNewScript);
    connect(ui->btnRecompile, &QAbstractButton::pressed, this, &ScriptingWindow::recompileScript);
    connect(ui->btnRemoveScript, &QAbstractButton::pressed, this, &ScriptingWindow::deleteCurrentScript);
    connect(ui->btnRevertScript, &QAbstractButton::pressed, this, &ScriptingWindow::revertScript);
    connect(ui->btnSaveScript, &QAbstractButton::pressed, this, &ScriptingWindow::saveScript);
    connect(ui->btnClearLog, &QAbstractButton::pressed, this, &ScriptingWindow::clickedLogClear);
    connect(ui->listLoadedScripts, &QListWidget::currentRowChanged, this, &ScriptingWindow::changeCurrentScript);

    connect(CANConManager::getInstance(), &CANConManager::framesReceived, this, &ScriptingWindow::newFrames);

    elapsedTime.start();
}

ScriptingWindow::~ScriptingWindow()
{
    delete editor;
    delete ui;
}


void ScriptingWindow::newFrames(const CANConnection* pConn, const QVector<CANFrame>& pFrames)
{
    /*FIXME: name of the probe and bus should be checked */
    Q_UNUSED(pConn);

    for (int j = 0; j < scripts.length(); j++)
    {
        foreach(const CANFrame& frame, pFrames)
        {
            //scripts[j]->gotFrame(frame);
        }
    }
}


void ScriptingWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
}

void ScriptingWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("ScriptingWindow/WindowSize", QSize(860, 650)).toSize());
        move(settings.value("ScriptingWindow/WindowPos", QPoint(100, 100)).toPoint());
    }
}

void ScriptingWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("ScriptingWindow/WindowSize", size());
        settings.setValue("ScriptingWindow/WindowPos", pos());
    }
}

void ScriptingWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
}

void ScriptingWindow::changeCurrentScript()
{
    int sel = ui->listLoadedScripts->currentRow();
    if (sel < 0) return;

    if (currentScript) currentScript->scriptText = editor->toPlainText();

    ScriptContainer *container = scripts.at(sel);
    currentScript = container;
    editor->setPlainText(container->scriptText);
    editor->setEnabled(true);
}

void ScriptingWindow::loadNewScript()
{
    QString filename;
    QFileDialog dialog;
    ScriptContainer *container;

    QStringList filters;
    filters.append(QString(tr("Javascript File (*.js)")));

    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];

        if (dialog.selectedNameFilter() == filters[0])
        {
            QFile scriptFile(filename);

            if (scriptFile.open(QIODevice::ReadOnly))
            {
                QString contents = scriptFile.readAll();
                scriptFile.close();
                QStringList fileList = filename.split('/');
                QString justFileName = fileList[fileList.length() - 1];
                ui->listLoadedScripts->addItem(justFileName);

                container = new ScriptContainer;
                container->fileName = justFileName;
                container->filePath = filename;
                container->scriptText = contents;
                container->setScriptWindow(this);
                container->compileScript();
                scripts.append(container);

                ui->listLoadedScripts->setCurrentRow(ui->listLoadedScripts->count() - 1);
                changeCurrentScript();
            }
        }
    }
}

void ScriptingWindow::createNewScript()
{
    ScriptContainer *container;

    container = new ScriptContainer;

    container->fileName = "UNNAMED_" + QString::number((qrand() % 10000)) + ".js";
    container->filePath = QString();
    container->scriptText = QString();
    container->setScriptWindow(this);
    scripts.append(container);
    ui->listLoadedScripts->addItem(container->fileName);
    currentScript = container;
    editor->setPlainText(container->scriptText);
    editor->setEnabled(true);
}

void ScriptingWindow::deleteCurrentScript()
{

}

void ScriptingWindow::refreshSourceWindow()
{
    editor->setPlainText(currentScript->scriptText);
}

void ScriptingWindow::saveScript()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("Javascript File (*.js)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        if (!filename.contains('.')) filename += ".js";
        if (dialog.selectedNameFilter() == filters[0])
        {
            QFile *outFile = new QFile(filename);

            if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
            {
                delete outFile;
                return;
            }
            outFile->write(editor->toPlainText().toUtf8());
            outFile->close();
            delete outFile;
        }
    }
}

void ScriptingWindow::revertScript()
{

}

void ScriptingWindow::recompileScript()
{
    currentScript->scriptText = editor->toPlainText();
    currentScript->compileScript();
}

void ScriptingWindow::clickedLogClear()
{
    ui->listLog->clear();
    elapsedTime.start();
}

void ScriptingWindow::log(QString text)
{
    ScriptContainer *cont = qobject_cast<ScriptContainer*>(sender());
    if (cont != NULL)
       ui->listLog->addItem(QString::number(elapsedTime.elapsed()) + "(" + cont->fileName + "): " + text);
    else
       ui->listLog->addItem(QString::number(elapsedTime.elapsed()) + ": " + text);

    if (ui->cbAutoScroll->isChecked())
    {
        ui->listLog->scrollToBottom();
    }
}
