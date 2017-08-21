#ifndef UDSSCANWINDOW_H
#define UDSSCANWINDOW_H

#include "can_structs.h"
#include "connections/canconnection.h"
#include "bus_protocols/uds_handler.h"

#include <QDialog>
#include <QFile>
#include <QTreeWidget>

namespace Ui {
class UDSScanWindow;
}

class UDSScanWindow : public QDialog
{
    Q_OBJECT

public:
    explicit UDSScanWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~UDSScanWindow();

private slots:
    void updatedFrames(int numFrames);
    void gotUDSReply(UDS_MESSAGE msg);
    void scanUDS();
    void saveResults();
    void timeOut();
    void adaptiveToggled();
    void wildcardToggled();
    void readByToggled();
    void numBytesChanged();
    void checkIDRange();
    void checkServiceRange();
    void checkSubFuncRange();

private:
    Ui::UDSScanWindow *ui;
    const QVector<CANFrame> *modelFrames;
    UDS_HANDLER *udsHandler;
    QTimer *waitTimer;
    QList<UDS_MESSAGE> sendingFrames;
    QTreeWidgetItem *nodeID;
    QTreeWidgetItem *nodeService;
    QTreeWidgetItem *nodeSubFunc;
    int currIdx = 0;
    bool currentlyRunning;

    void sendNextMsg();
    void sendOnBuses(UDS_MESSAGE frame, int buses);
    void setupNodes();
    void dumpNode(QTreeWidgetItem* item, QFile *file, int indent);
};
#endif // UDSSCANWINDOW_H
