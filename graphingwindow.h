#ifndef GRAPHINGWINDOW_H
#define GRAPHINGWINDOW_H

#include "qcustomplot.h"
#include "can_structs.h"

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
};

class GraphingWindow : public QDialog
{
    Q_OBJECT

public:
    explicit GraphingWindow(const QVector<CANFrame> *, QWidget *parent = 0);
    ~GraphingWindow();
    void showEvent(QShowEvent*);

private slots:
    void titleDoubleClick(QMouseEvent *event, QCPPlotTitle *title);
    void axisLabelDoubleClick(QCPAxis* axis, QCPAxis::SelectablePart part);
    void legendDoubleClick(QCPLegend* legend, QCPAbstractLegendItem* item);
    void selectionChanged();
    void mousePress();
    void mouseWheel();
    void removeSelectedGraph();
    void removeAllGraphs();
    void contextMenuRequest(QPoint pos);
    void moveLegend();
    void saveGraphs();
    void addNewGraph();
    void createGraph(GraphParams &params, bool createGraphParam = true);
    void editSelectedGraph();
    void updatedFrames(int);

private:
    Ui::GraphingWindow *ui;
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
};

#endif // GRAPHINGWINDOW_H
