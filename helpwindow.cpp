#include <QtDebug>
#include "helpwindow.h"
#include "ui_helpwindow.h"

HelpWindow* HelpWindow::self = nullptr;

HelpWindow::HelpWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HelpWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    m_helpEngine = new QHelpEngineCore(QApplication::applicationDirPath() +"/SavvyCAN.qhc", this);
    if (!m_helpEngine->setupData()) {
        delete m_helpEngine;
        m_helpEngine = nullptr;
        qDebug() << "Could not load help file!";
    }

    readSettings();
}

HelpWindow::~HelpWindow()
{
    writeSettings();
    delete ui;
}

void HelpWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
}

void HelpWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("HelpViewer/WindowSize", QSize(600, 700)).toSize());
        move(settings.value("HelpViewer/WindowPos", QPoint(50, 50)).toPoint());
    }
}

void HelpWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("HelpViewer/WindowSize", size());
        settings.setValue("HelpViewer/WindowPos", pos());
    }
}

HelpWindow* HelpWindow::getRef()
{
    if (!self)
    {
        self = new HelpWindow();
    }

    return self;
}

void HelpWindow::showHelp(QString help)
{
    if (m_helpEngine) {
        QString url = "qthelp://org.sphinx.savvycan.189/doc/" + help;
        qDebug() << "Searching for " << url;
        QByteArray helpData = m_helpEngine->fileData(QUrl(url));
        qDebug() << "Help file size: " << helpData.length();
        ui->textHelp->setText(helpData);
    }
    else
    {
        qDebug() << "Help engine not loaded!";
    }

    readSettings();
    self->show();
}


