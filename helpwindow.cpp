#include <QtDebug>
#include "utility.h"
#include "helpwindow.h"
#include "ui_helpwindow.h"

HelpWindow* HelpWindow::self = nullptr;

HelpWindow::HelpWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HelpWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

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
        move(Utility::constrainedWindowPos(settings.value("HelpViewer/WindowPos", QPoint(50, 50)).toPoint()));
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
    QString helpfile = QCoreApplication::applicationDirPath() + "/help/" + help;
    QUrl url = QUrl::fromLocalFile(helpfile);
    qDebug() << "Searching for " << url;
    ui->textHelp->setSource(url);

    readSettings();
    self->show();
}


