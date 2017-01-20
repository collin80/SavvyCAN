#ifndef SIGNALVIEWERWINDOW_H
#define SIGNALVIEWERWINDOW_H

#include <QDialog>
#include "dbc/dbchandler.h"

namespace Ui {
class SignalViewerWindow;
}

class SignalViewerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SignalViewerWindow(QWidget *parent = 0);
    ~SignalViewerWindow();

private slots:
    void loadMessages();
    void loadSignals(int idx);
    void addSignal();

private:
    Ui::SignalViewerWindow *ui;
    DBCHandler *dbcHandler;

    QList<DBC_SIGNAL *> signalList;
};

#endif // SIGNALVIEWERWINDOW_H
