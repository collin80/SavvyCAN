#include "scriptingwindow.h"
#include "ui_scriptingwindow.h"

#include <QFile>
#include <QFileDialog>

#include "mainwindow.h"

ScriptingWindow::ScriptingWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ScriptingWindow)
{
    ui->setupUi(this);

    modelFrames = frames;

    connect(ui->btnLoadScript, &QAbstractButton::pressed, this, &ScriptingWindow::loadNewScript);
    connect(ui->btnNewScript, &QAbstractButton::pressed, this, &ScriptingWindow::createNewScript);
    connect(ui->btnRecompile, &QAbstractButton::pressed, this, &ScriptingWindow::recompileScript);
    connect(ui->btnRemoveScript, &QAbstractButton::pressed, this, &ScriptingWindow::deleteCurrentScript);
    connect(ui->btnRevertScript, &QAbstractButton::pressed, this, &ScriptingWindow::revertScript);
    connect(ui->btnSaveScript, &QAbstractButton::pressed, this, &ScriptingWindow::saveScript);
    connect(MainWindow::getReference(), &MainWindow::framesUpdated, this, &ScriptingWindow::updatedFrames);
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
            outFile->write(ui->txtScriptSource->toPlainText().toUtf8());
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
    currentScript->scriptText = ui->txtScriptSource->toPlainText();
    currentScript->compileScript();
}
