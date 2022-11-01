#include "mainwindow.h"
#include <QApplication>

class SavvyCANApplication : public QApplication
{
public:
    MainWindow *mainWindow;
    
    SavvyCANApplication(int &argc, char **argv) : QApplication(argc, argv)
    {
    }

    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::FileOpen)
        {
            QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
            mainWindow->handleDroppedFile(openEvent->file());
        }

        return QApplication::event(event);
    }
};

int main(int argc, char *argv[])
{
#ifdef QT_DEBUG
    qputenv("QT_FATAL_WARNINGS", "1");
    //qputenv("QT_MESSAGE_PATTERN", "Type: %{type}\nProduct Name: %{appname}\nFile: %{file}\nLine: %{line}\nMethod: %{function}\nThreadID: %{threadid}\nThreadPtr: %{qthreadptr}\nMessage: %{message}");
    qSetMessagePattern("Type: %{type}\nProduct Name: %{appname}\nFile: %{file}\nLine: %{line}\nMethod: %{function}\nThreadID: %{threadid}\nThreadPtr: %{qthreadptr}\nMessage: %{message}");
#endif

    SavvyCANApplication a(argc, argv);

    //Add a local path for Qt extensions, to allow for per-application extensions.
    a.addLibraryPath("plugins");

    //These things are used by QSettings to set up setting storage
    a.setOrganizationName("EVTV");
    a.setApplicationName("SavvyCAN");
    a.setOrganizationDomain("evtv.me");
    QSettings::setDefaultFormat(QSettings::IniFormat);

    a.mainWindow = new MainWindow();

    QSettings settings;
    int fontSize = settings.value("Main/FontSize", 9).toUInt();
    QFont sysFont = QFont(); //get default font
    sysFont.setPointSize(fontSize);
    a.setFont(sysFont);

    a.mainWindow->show();

    int retCode = a.exec();
    
    delete a.mainWindow; a.mainWindow = NULL;
    
    return retCode;
}
