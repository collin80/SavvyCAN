#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QFontDatabase>
#include <QDebug>

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
    //uncomment for verbose debug data in application output
    //qputenv("QT_FATAL_WARNINGS", "1");
    //qSetMessagePattern("Type: %{type}\nProduct Name: %{appname}\nFile: %{file}\nLine: %{line}\nMethod: %{function}\nThreadID: %{threadid}\nThreadPtr: %{qthreadptr}\nMessage: %{message}");
#endif

    SavvyCANApplication a(argc, argv);

    //Add a local path for Qt extensions, to allow for per-application extensions.
    a.addLibraryPath("plugins");

    //These things are used by QSettings to set up setting storage
    a.setOrganizationName("EVTV");
    a.setApplicationName("SavvyCAN");
    a.setOrganizationDomain("evtv.me");

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings settings;

    QString localeString = settings.value("Main/Language").toString();
    if (localeString.isEmpty()) {
        QLocale sysLocale = QLocale::system();
        localeString = sysLocale.name();
        settings.setValue("Main/Language", localeString);
    }

    QTranslator translator;
    QLocale locale(localeString);
    QString lang = locale.name();
    QString shortLang = locale.name().left(2);

    if (QString translationDir = QCoreApplication::applicationDirPath() + "/translations"; !translator.load("SavvyCAN_" + lang, translationDir)) {
        translator.load("SavvyCAN_" + shortLang, translationDir);
    }
    a.installTranslator(&translator);

    qInfo() << "Locale Value is:" << locale.name();

    a.mainWindow = new MainWindow();

    // Load the bundled Smiley Sans (得意黑) font and use it as the application font.
    QString appFontFamily = "Smiley Sans Oblique";
    int fontId = QFontDatabase::addApplicationFont(":/fonts/SmileySans-Oblique.ttf");
    if (fontId != -1) {
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty())
            appFontFamily = families.at(0);
    }

    int fontSize = settings.value("Main/FontSize", 9).toUInt();
    QFont sysFont(appFontFamily); //use the bundled Smiley Sans font
    sysFont.setPointSize(fontSize);
    a.setFont(sysFont);

    a.mainWindow->show();

    int retCode = a.exec();

    delete a.mainWindow; a.mainWindow = NULL;

    return retCode;
}
