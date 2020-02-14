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
    explicit SignalViewerWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~SignalViewerWindow();

private slots:
    void loadMessages();
    void loadSignals(int idx);
    void addSignal();
    void removeSelectedSignal();
    void updatedFrames(int);

private:
    Ui::SignalViewerWindow *ui;
    DBCHandler *dbcHandler;

    QList<DBC_SIGNAL *> signalList;
    const QVector<CANFrame> *modelFrames;

    void processFrame(CANFrame &frame);
};

#endif // SIGNALVIEWERWINDOW_H
