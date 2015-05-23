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
    explicit GraphingWindow(QVector<CANFrame> *, QWidget *parent = 0);
    ~GraphingWindow();

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
    void addNewGraph();
    void createGraph(GraphParams &params);
    void editSelectedGraph();

private:
    Ui::GraphingWindow *ui;
    QList<CANFrame> frameCache;
    QVector<CANFrame> *modelFrames;
    QList<GraphParams> graphParams;
    QPen selectedPen;

    void showParamsDialog(int idx);
};

#endif // GRAPHINGWINDOW_H
