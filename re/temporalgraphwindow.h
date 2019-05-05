#ifndef TEMPORALGRAPHWINDOW_H
#define TEMPORALGRAPHWINDOW_H

#include <QDialog>
#include "qcustomplot.h"
#include "can_structs.h"

namespace Ui {
class TemporalGraphWindow;
}

class HexTicker : public QCPAxisTicker
{
    QString getTickLabel (double tick, const QLocale& locale, QChar formatChar, int precision);
};

class TemporalGraphWindow : public QDialog
{
    Q_OBJECT

public:
    explicit TemporalGraphWindow(const QVector<CANFrame> *, QWidget *parent = nullptr);
    ~TemporalGraphWindow();
    void showEvent(QShowEvent*);

private slots:
    void updatedFrames(int);
    void mousePress();
    void mouseWheel();
    void resetView();
    void zoomIn();
    void zoomOut();
    void selectionChanged();

private:
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
