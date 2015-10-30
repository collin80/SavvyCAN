#ifndef GRAPHINGWINDOW_H
#define GRAPHINGWINDOW_H

#include "qcustomplot.h"
#include "can_structs.h"
#include "dbchandler.h"

#include <QDialog>

namespace Ui {
class GraphingWindow;
}

class GraphParams
{
public:
    int32_t ID;
    int startByte, endByte;
    bool isSigned;
    int32_t mask;
    float bias;
    float scale;
    int stride;
    QColor color;
    QCPGraph *ref;
    QString graphName;
    //the below stuff is used for internal purposes only
    QVector<double> x, y;
    double xbias;
};

class GraphingWindow : public QDialog
{
    Q_OBJECT

public:
    explicit GraphingWindow(DBCHandler *handler, const QVector<CANFrame> *, QWidget *parent = 0);
    ~GraphingWindow();
    void showEvent(QShowEvent*);

private slots:
    void titleDoubleClick(QMouseEvent *event, QCPPlotTitle *title);
    void axisLabelDoubleClick(QCPAxis* axis, QCPAxis::SelectablePart part);
    void legendDoubleClick(QCPLegend* legend, QCPAbstractLegendItem* item);
    void plottableDoubleClick(QCPAbstractPlottable* plottable,QMouseEvent* event);
    void selectionChanged();
    void mousePress();
    void mouseWheel();
    void removeSelectedGraph();
    void removeAllGraphs();
    void contextMenuRequest(QPoint pos);
    void moveLegend();
    void saveGraphs();
    void saveSpreadsheet();
    void saveDefinitions();
    void loadDefinitions();
    void addNewGraph();
    void createGraph(GraphParams &params, bool createGraphParam = true);
    void appendToGraph(GraphParams &params, CANFrame &frame);
    void editSelectedGraph();
    void updatedFrames(int);
    void gotCenterTimeID(int32_t ID, double timestamp);
    void resetView();
    void zoomIn();
    void zoomOut();

signals:
    void sendCenterTimeID(int32_t ID, double timestamp);

private:
    Ui::GraphingWindow *ui;
    DBCHandler *dbcHandler;
    QList<CANFrame> frameCache;
    const QVector<CANFrame> *modelFrames;
    QList<GraphParams> graphParams;
    QPen selectedPen;
    bool needScaleSetup; //do we need to set x,y graphing extents?
    bool secondsMode;

    void showParamsDialog(int idx);
    void closeEvent(QCloseEvent *event);
    void readSettings();
    void writeSettings();
    bool eventFilter(QObject *obj, QEvent *event);
};

#endif // GRAPHINGWINDOW_H
