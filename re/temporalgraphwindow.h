#ifndef TEMPORALGRAPHWINDOW_H
#define TEMPORALGRAPHWINDOW_H

#include <QDialog>
#include "can_structs.h"

namespace Ui {
class TemporalGraphWindow;
}

class QCPGraph;

class TemporalGraphWindow : public QDialog
{
public:
    explicit TemporalGraphWindow(const QVector<CANFrame> *, QWidget *parent = nullptr);
    ~TemporalGraphWindow();
    void showEvent(QShowEvent*);

private:
    void updatedFrames(int);
    void mousePress();
    void mouseWheel();
    void resetView();
    void zoomIn();
    void zoomOut();
    void selectionChanged();

    Ui::TemporalGraphWindow *ui;    
    const QVector<CANFrame> *modelFrames;
    bool useOpenGL;
    bool followGraphEnd;
    QCPGraph *graph;
    double xminval, xmaxval, yminval, ymaxval;
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    void readSettings();
    void writeSettings();
    void generateGraph();

};

#endif // TEMPORALGRAPHWINDOW_H
