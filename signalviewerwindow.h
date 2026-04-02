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
    explicit SignalViewerWindow(const QVector<CommFrame> *frames, QWidget *parent = 0);
    ~SignalViewerWindow();

private slots:
    void loadNodes();
    void loadMessages(int idx);
    void loadSignals(int idx);
    void addSignal();
    void addSignal(DBC_SIGNAL *sig);
    void removeSelectedSignal();
    void updatedFrames(int);
    void saveSignalsFile();
    void loadSignalsFile();
    void appendSignalsFile();
    void clearSignalsTable();
    void clearSignalsTable(bool);
    void saveDefinitions();
    void loadDefinitions(bool);

private:
    Ui::SignalViewerWindow *ui;
    DBCHandler *dbcHandler;

    DBC_MESSAGE *currentlySelectedMsg;

    QList<DBC_SIGNAL *> signalList;
    const QVector<CommFrame> *modelFrames;

    void processFrame(CommFrame &frame);
};

#endif // SIGNALVIEWERWINDOW_H
