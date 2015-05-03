#ifndef GRAPHINGWINDOW_H
#define GRAPHINGWINDOW_H

#include "qcustomplot.h"

#include <QDialog>

namespace Ui {
class GraphingWindow;
}

class GraphingWindow : public QDialog
{
    Q_OBJECT

public:
    explicit GraphingWindow(QWidget *parent = 0);
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

private:
    Ui::GraphingWindow *ui;
};

#endif // GRAPHINGWINDOW_H
