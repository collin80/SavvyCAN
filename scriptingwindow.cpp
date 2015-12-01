#include "scriptingwindow.h"
#include "ui_scriptingwindow.h"

#include <QFile>
#include <QFileDialog>

ScriptingWindow::ScriptingWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ScriptingWindow)
{
    ui->setupUi(this);

    modelFrames = frames;

    connect(ui->btnLoadScript, SIGNAL(pressed()), this, SLOT(loadNewScript()));
    connect(ui->btnNewScript, SIGNAL(pressed()), this, SLOT(createNewScript()));
    connect(ui->btnRecompile, SIGNAL(pressed()), this, SLOT(recompileScript()));
    connect(ui->btnRemoveScript, SIGNAL(pressed()), this, SLOT(deleteCurrentScript()));
    connect(ui->btnRevertScript, SIGNAL(pressed()), this, SLOT(revertScript()));
    connect(ui->btnSaveScript, SIGNAL(pressed()), this, SLOT(saveScript()));

    currentScript = new ScriptContainer();
    currentScript->setErrorWidget(ui->listErrors);
}

ScriptingWindow::~ScriptingWindow()
{
    delete ui;
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
