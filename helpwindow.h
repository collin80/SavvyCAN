#ifndef HELPWINDOW_H
#define HELPWINDOW_H

#include <Qt>
#include <QDialog>
#include <QSettings>

namespace Ui {
class HelpWindow;
}

class HelpWindow : public QDialog
{
    Q_OBJECT

public:
    explicit HelpWindow(QWidget *parent = 0);
    ~HelpWindow();
    void showHelp(QString help);
    static HelpWindow *getRef();    

private:
    void readSettings();
    void writeSettings();
    void closeEvent(QCloseEvent *event);

    Ui::HelpWindow *ui;
    static HelpWindow *self;
};

#endif // HELPWINDOW_H
