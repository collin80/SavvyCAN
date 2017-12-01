#ifndef ISOTP_INTERPRETERWINDOW_H
#define ISOTP_INTERPRETERWINDOW_H

#include <QDialog>
#include "bus_protocols/isotp_handler.h"

class ISOTP_MESSAGE;
class ISOTP_HANDLER;

namespace Ui {
class ISOTP_InterpreterWindow;
}

class ISOTP_InterpreterWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ISOTP_InterpreterWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~ISOTP_InterpreterWindow();
    void showEvent(QShowEvent*);

private slots:
    void newISOMessage(ISOTP_MESSAGE msg);
    void newUDSMessage(UDS_MESSAGE msg);
    void showDetailView();
    void updatedFrames(int);
    void clearList();
    void listFilterItemChanged(QListWidgetItem *item);
    void filterAll();
    void filterNone();
    void interpretCapturedFrames();
    void useExtendedAddressing(bool checked);
    void headerClicked(int logicalIndex);

private:
    Ui::ISOTP_InterpreterWindow *ui;
    ISOTP_HANDLER *decoder;
    UDS_HANDLER *udsDecoder;

    const QVector<CANFrame> *modelFrames;
    QVector<ISOTP_MESSAGE> messages;
    QHash<int, bool> idFilters;

    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    void readSettings();
    void writeSettings();

};

#endif // ISOTP_INTERPRETERWINDOW_H
