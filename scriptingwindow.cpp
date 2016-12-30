#include "scriptingwindow.h"
#include "ui_scriptingwindow.h"

#include <QFile>
#include <QFileDialog>
#include <QSettings>
#include <Qsci/qscilexerjavascript.h>

#include "connections/canconmanager.h"

ScriptingWindow::ScriptingWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ScriptingWindow)
{
    ui->setupUi(this);

    readSettings();

    modelFrames = frames;

    connect(ui->btnLoadScript, &QAbstractButton::pressed, this, &ScriptingWindow::loadNewScript);
    connect(ui->btnNewScript, &QAbstractButton::pressed, this, &ScriptingWindow::createNewScript);
    connect(ui->btnRecompile, &QAbstractButton::pressed, this, &ScriptingWindow::recompileScript);
    connect(ui->btnRemoveScript, &QAbstractButton::pressed, this, &ScriptingWindow::deleteCurrentScript);
    connect(ui->btnRevertScript, &QAbstractButton::pressed, this, &ScriptingWindow::revertScript);
    connect(ui->btnSaveScript, &QAbstractButton::pressed, this, &ScriptingWindow::saveScript);

    connect(CANConManager::getInstance(), &CANConManager::framesReceived, this, &ScriptingWindow::newFrames);

    ui->txtScriptSource->setLexer(new QsciLexerJavaScript(ui->txtScriptSource));
}

ScriptingWindow::~ScriptingWindow()
{
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
            scripts[j]->gotFrame(frame);
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
                /* get the first connection in the list for now */
                qDebug() << "FIXME: connection is always the first in list";
                QString portName;
                QList<CANConnection*> list = CANConManager::getInstance()->getConnections();
                if(!list.isEmpty())
                    portName = list.first()->getPort();

                container = new ScriptContainer(portName);
                container->fileName = justFileName;
                container->filePath = filename;
                container->scriptText = contents;
                container->setErrorWidget(ui->listErrors);
                container->compileScript();
                scripts.append(container);
                currentScript = container;
                ui->txtScriptSource->setText(container->scriptText);
                ui->txtScriptSource->setEnabled(true);
            }
        }
    }
}

void ScriptingWindow::createNewScript()
{
    ScriptContainer *container;

    QString portName;
    QList<CANConnection*> list = CANConManager::getInstance()->getConnections();
    if(!list.isEmpty())
        portName = list.first()->getPort();

    container = new ScriptContainer(portName);

    container->fileName = "UNNAMED_" + QString::number((qrand() % 10000)) + ".js";
    container->filePath = QString();
    container->scriptText = QString();
    container->setErrorWidget(ui->listErrors);
    scripts.append(container);
    ui->listLoadedScripts->addItem(container->fileName);
    currentScript = container;
    ui->txtScriptSource->setText(container->scriptText);
    ui->txtScriptSource->setEnabled(true);
}

void ScriptingWindow::deleteCurrentScript()
{

}

void ScriptingWindow::refreshSourceWindow()
{
    ui->txtScriptSource->setText(currentScript->scriptText);
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
            outFile->write(ui->txtScriptSource->text().toUtf8());
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
    currentScript->scriptText = ui->txtScriptSource->text();
    currentScript->compileScript();
}
