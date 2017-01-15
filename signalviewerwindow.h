#ifndef SIGNALVIEWERWINDOW_H
#define SIGNALVIEWERWINDOW_H

#include <QDialog>

namespace Ui {
class SignalViewerWindow;
}

class SignalViewerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SignalViewerWindow(QWidget *parent = 0);
    ~SignalViewerWindow();

private:
    Ui::SignalViewerWindow *ui;
};

#endif // SIGNALVIEWERWINDOW_H
