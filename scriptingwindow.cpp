#include "scriptingwindow.h"
#include "ui_scriptingwindow.h"

#include <QFile>
#include <QFileDialog>
#include <Qsci/qscilexerjavascript.h>

#include "mainwindow.h"

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
    connect(MainWindow::getReference(), &MainWindow::framesUpdated, this, &ScriptingWindow::updatedFrames);

    ui->txtScriptSource->setLexer(new QsciLexerJavaScript(ui->txtScriptSource));
}

ScriptingWindow::~ScriptingWindow()
{
    delete ui;
}

void ScriptingWindow::updatedFrames(int numFrames)
{
    CANFrame thisFrame;
    //-1 means all frames deleted and -2 means a full refresh, neither of which we care about here.
    if (numFrames > 0)
    {
        qDebug() << "Got frames into script window: " << numFrames;
        //for every new frame pass it on to each script container. The container will determine if it needs to actually
        //notify the script and do that if applicable.
        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);
            for (int j = 0; j < scripts.length(); j++)
            {
                scripts[j]->gotFrame(thisFrame);
            }
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
    bool result = false;
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
                container = new ScriptContainer();
                container->fileName = justFileName;
                container->filePath = filename;
                container->scriptText = contents;
                container->setErrorWidget(ui->listErrors);
                container->compileScript();
                connect(container, &ScriptContainer::sendCANFrame, this, &ScriptingWindow::sendCANFrame);
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

    container = new ScriptContainer();

    container->fileName = "UNNAMED_" + QString::number((qrand() % 10000)) + ".js";
    container->filePath = QString();
    container->scriptText = QString();
    container->setErrorWidget(ui->listErrors);
    connect(container, &ScriptContainer::sendCANFrame, this, &ScriptingWindow::sendCANFrame);
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
