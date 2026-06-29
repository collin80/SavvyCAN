#ifndef FRAMEINFOWINDOW_H
#define FRAMEINFOWINDOW_H

#include <QDialog>
#include <QFile>
#include <QListWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <candatagrid.h>
#include "can_structs.h"
#include "utility.h"
#include "bus_protocols/j1939_handler.h"
#include "dbc/dbchandler.h"

#include "qcustomplot.h"

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
    void mousePress();
    void mouseWheel();
    void mouseDoubleClick();
    void resetByteGraph(int idx);
    void resetAllByteGraphs();
    void togglePlotType(int idx, bool scatter);
    void toggleAllPlotType(bool scatter);

private:
    Ui::FrameInfoWindow *ui;
    QCustomPlot *graphByte[8];
    QCustomPlot *graphHistogram;
    CANDataGrid *heatmap;

    QList<int> foundID;
    QList<CANFrame> frameCache;
    const QVector<CANFrame> *modelFrames;
    bool useOpenGL;
    bool useHexTicker;
    TimeStyle timeStyle;
    static const QColor byteGraphColors[8];
    static QPen bytePens[8];
    DBCHandler *dbcHandler;

    QCPGraph *graphRef[8];
    QPushButton *btnResetByteGraph[8];
    QPushButton *btnTogglePlotType[8];

    void refreshIDList();
    void captureXRange(double &xmin, double &xmax);
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    void setupByteGraph(QCustomPlot *plot, int num);
    void readSettings();
    void writeSettings();
    void dumpNode(QTreeWidgetItem* item, QFile *file, int indent);
};

#endif // FRAMEINFOWINDOW_H
