#ifndef GRAPHINGWINDOW_H
#define GRAPHINGWINDOW_H

#include "qcustomplot.h"
#include "can_structs.h"
#include "dbc/dbchandler.h"

#include <QDialog>

namespace Ui {
class GraphingWindow;
}

class GraphParams
{
public:
    GraphParams();

    uint32_t ID;
    int startBit, numBits;
    bool intelFormat;
    bool isSigned;
    uint64_t mask;
    double bias;
    double scale;
    int stride;
    int strideSoFar;
    int bus;
    QColor lineColor;
    QColor fillColor;
    int lineWidth;
    bool drawOnlyPoints;
    int pointType;
    QCPGraph *ref;
    QString graphName;
    DBC_SIGNAL *associatedSignal;

    //the below stuff is used for internal purposes only - code should be refactored so these can be private
    QVector<double> x, y;
    double xbias;
    int64_t prevValTable;
    QPointF prevValLocation;
    QString prevValStr;
    QCPItemBracket *lastBracket;
    QList<QCPItemBracket *> brackets;
    QList<QCPItemText *> bracketTexts;
};

class GraphingWindow : public QDialog
{
    Q_OBJECT

public:
    explicit GraphingWindow(const QVector<CANFrame> *, QWidget *parent = 0);
    ~GraphingWindow();
    void showEvent(QShowEvent*);

public slots:
    void createGraph(GraphParams &params, bool createGraphParam = true);

private slots:
    void titleDoubleClick(QMouseEvent *event, QCPTextElement *title);
    void axisDoubleClick(QCPAxis* axis, QCPAxis::SelectablePart part);
    void legendDoubleClick(QCPLegend* legend, QCPAbstractLegendItem* item);
    void legendSingleClick(QCPLegend* legend, QCPAbstractLegendItem* item);
    void plottableDoubleClick(QCPAbstractPlottable* plottable,int dataIdx, QMouseEvent* event);
    void plottableClick(QCPAbstractPlottable* plottable, int dataIdx, QMouseEvent* event);
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
    void rescaleAxis(QCPAxis* axis);
    void rescaleToData();
    void toggleFollowMode();
    void addNewGraph();    
    void appendToGraph(GraphParams &params, CANFrame &frame, QVector<double> &x, QVector<double> &y);
    void editSelectedGraph();
    void updatedFrames(int);
    void gotCenterTimeID(uint32_t ID, double timestamp);
    void resetView();
    void zoomIn();
    void zoomOut();

signals:
    void sendCenterTimeID(uint32_t ID, double timestamp);

private:
    Ui::GraphingWindow *ui;
    DBCHandler *dbcHandler;
    QList<CANFrame> frameCache;
    const QVector<CANFrame> *modelFrames;
    QList<GraphParams> graphParams;
    QPen selectedPen;
    QCPSelectionDecorator *selDecorator;
    QCPItemText *locationText;
    QCPItemTracer *itemTracer;
    bool needScaleSetup; //do we need to set x,y graphing extents?
    bool useOpenGL;
    bool followGraphEnd;

    void showParamsDialog(int idx);
    void closeEvent(QCloseEvent *event);
    void readSettings();
    void writeSettings();
    bool eventFilter(QObject *obj, QEvent *event);
    void changeEvent(QEvent *event);
};

#endif // GRAPHINGWINDOW_H
