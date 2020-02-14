#ifndef FRAMEINFOWINDOW_H
#define FRAMEINFOWINDOW_H

#include <QDialog>
#include <QFile>
#include <QListWidget>
#include <QTreeWidget>
#include "can_structs.h"
#include "bus_protocols/j1939_handler.h"

namespace Ui {
class FrameInfoWindow;
}

class FrameInfoWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FrameInfoWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~FrameInfoWindow();
    void showEvent(QShowEvent*);

private slots:
    void updateDetailsWindow(QString);
    void updatedFrames(int);
    void saveDetails();

private:
    Ui::FrameInfoWindow *ui;

    QList<int> foundID;
    QList<CANFrame> frameCache;
    const QVector<CANFrame> *modelFrames;
    bool useOpenGL;
    static const QColor byteGraphColors[8];
    static QPen bytePens[8];

    void refreshIDList();
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    void readSettings();
    void writeSettings();
    void dumpNode(QTreeWidgetItem* item, QFile *file, int indent);
};

#endif // FRAMEINFOWINDOW_H
