#include "temporalgraphwindow.h"
#include "ui_temporalgraphwindow.h"
#include "helpwindow.h"
#include "mainwindow.h"

QString HexTicker::getTickLabel (double tick, const QLocale& locale, QChar formatChar, int precision)
{
    Q_UNUSED(formatChar);
    Q_UNUSED(precision);
    Q_UNUSED(locale);
    int valu = static_cast<int>(tick);
    //qDebug() << valu;
    return "0x" + QString::number(valu, 16).toUpper().rightJustified(3,'0');
}

TemporalGraphWindow::TemporalGraphWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TemporalGraphWindow)
{
    ui->setupUi(this);

    setWindowFlags(Qt::Window);

    readSettings();

    modelFrames = frames;

    ui->graphingView->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes);

    ui->graphingView->xAxis->setRange(0, 8);
    ui->graphingView->yAxis->setRange(0, 255);
    ui->graphingView->axisRect()->setupFullAxesBox();


    ui->graphingView->xAxis->setLabel("Elapsed Time");
    ui->graphingView->yAxis->setLabel("ID");
    ui->graphingView->xAxis->setNumberFormat("f");
    ui->graphingView->xAxis->setNumberPrecision(6);

    HexTicker *tick = new HexTicker;
    QSharedPointer<QCPAxisTicker> sharedTicker(tick);
    tick->setTickCount(10);
    ui->graphingView->yAxis->setTicker(sharedTicker);

    // connect slot that ties some axis selections together (especially opposite axes):
    connect(ui->graphingView, SIGNAL(selectionChangedByUser()), this, SLOT(selectionChanged()));
    connect(ui->graphingView, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(mousePress()));
    connect(ui->graphingView, SIGNAL(mouseWheel(QWheelEvent*)), this, SLOT(mouseWheel()));
    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    // make bottom and left axes transfer their ranges to top and right axes:
    connect(ui->graphingView->xAxis, SIGNAL(rangeChanged(QCPRange)), ui->graphingView->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->graphingView->yAxis, SIGNAL(rangeChanged(QCPRange)), ui->graphingView->yAxis2, SLOT(setRange(QCPRange)));

    if (useOpenGL)
    {
        ui->graphingView->setAntialiasedElements(QCP::aeAll);
        //ui->graphingView->setNoAntialiasingOnDrag(true);
        ui->graphingView->setOpenGl(true);
    }
    else
    {
        ui->graphingView->setOpenGl(false);
        ui->graphingView->setAntialiasedElements(QCP::aeNone);
    }
}

TemporalGraphWindow::~TemporalGraphWindow()
{
    delete ui;
}

void TemporalGraphWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    installEventFilter(this);
    readSettings();
    generateGraph();
    ui->graphingView->replot();
}

void TemporalGraphWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    removeEventFilter(this);
    writeSettings();
}

void TemporalGraphWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("Temporal/WindowSize", QSize(800, 600)).toSize());
        move(Utility::constrainedWindowPos(settings.value("Temporal/WindowPos", QPoint(50, 50)).toPoint()));
    }
    useOpenGL = settings.value("Main/UseOpenGL", false).toBool();
}

void TemporalGraphWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("Temporal/WindowSize", size());
        settings.setValue("Temporal/WindowPos", pos());
    }
}

void TemporalGraphWindow::updatedFrames(int numFrames)
{
    CANFrame thisFrame;
    QVector<double> x, y;
    bool appendedToGraph = false;
    bool needReplot = false;

    if (numFrames == -1) //all frames deleted. Kill the display
    {
        //removeAllGraphs();
        //now instead of removing the graphs regenerate them which will blank them out but leave them there in case
        //more traffic that matches comes in or someone otherwise loads more data
        ui->graphingView->clearGraphs(); //temporarily remove the graphs from the graph view
        ui->graphingView->clearPlottables();
        ui->graphingView->replot(); //now, redisplay them all

    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        //there shouldn't be any need to actually remove the graphs.
        //regenerate them instead
        ui->graphingView->clearGraphs(); //temporarily remove the graphs from the graph view
        //needScaleSetup = true;
        generateGraph();
        //ui->graphingView->replot(); //now, redisplay them all
    }
    else //just got some new frames. See if they are relevant.
    {
        if (numFrames > modelFrames->count()) return;

            appendedToGraph = false;
            x.clear();
            y.clear();
            for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
            {
                thisFrame = modelFrames->at(i);
                /*
                if (graphParams[j].ID == thisFrame.ID)
                {
                    appendToGraph(graphParams[j], thisFrame, x, y);
                    appendedToGraph = true;
                }*/
            }
            if (appendedToGraph)
            {
                //graphParams[j].ref->addData(x, y);
                needReplot = true;
            }

        if (needReplot)
        {
            if (followGraphEnd)
            {
                //find the current X span and maintain that span but move the end of it over to match the new end
                //of the actual graph. This causes the view to move with the data to always show the end
                QCPRange range = ui->graphingView->xAxis->range();
                double size = range.size();
                bool foundRange;
                QCPRange keyRange = ui->graphingView->graph()->getKeyRange(foundRange);
                if (foundRange)
                {
                    double end, start;
                    end = keyRange.upper;
                    start = end - size;
                    ui->graphingView->xAxis->setRange(start, end);
                }
            }
            ui->graphingView->replot();
        }
    }
}

void TemporalGraphWindow::generateGraph()
{
    if (modelFrames->count() == 0) return;

    ui->graphingView->clearGraphs();
    ui->graphingView->clearPlottables();

    qDebug() << "Regenerating the graph";
    ui->graphingView->addGraph();
    graph = ui->graphingView->graph();

    QVector<double> x, y;
    int frameCount = modelFrames->count();

    x.reserve(frameCount);
    y.reserve(frameCount);

    xminval = xmaxval = modelFrames->at(0).timeStamp().microSeconds() / 1000000.0;
    yminval = ymaxval = modelFrames->at(0).frameId();

    for (int i = 0; i < frameCount; i++)
    {
        x.append(modelFrames->at(i).timeStamp().microSeconds() / 1000000.0);
        y.append(modelFrames->at(i).frameId());
        if (x[i] > xmaxval) xmaxval = x[i];
        if (x[i] < xminval) xminval = x[i];
        if (y[i] > ymaxval) ymaxval = y[i];
        if (y[i] < yminval) yminval = y[i];
    }

    ui->graphingView->graph()->setData(x,y);
    ui->graphingView->graph()->setLineStyle(QCPGraph::lsNone); //no lines
    ui->graphingView->graph()->setScatterStyle(QCPScatterStyle::ssCircle);
    QPen graphPen;
    graphPen.setColor(Qt::blue);
    graphPen.setWidth(2);
    ui->graphingView->graph()->setPen(graphPen);

    qDebug() << "xmin: " << xminval;
    qDebug() << "xmax: " << xmaxval;
    qDebug() << "ymin: " << yminval;
    qDebug() << "ymax: " << ymaxval;

    ui->graphingView->xAxis->setRange(xminval, xmaxval);
    ui->graphingView->yAxis->setRange(yminval, ymaxval);
    ui->graphingView->axisRect()->setupFullAxesBox();

    ui->graphingView->replot();


    QCPColorMap *colorMap = new QCPColorMap(ui->graphingView->xAxis, ui->graphingView->yAxis);

    int ySize = static_cast<int>(ymaxval - yminval) / 30 + 1;
    int xSize = static_cast<int>((xmaxval - xminval) * 4.0) + 1;
    colorMap->data()->setSize(xSize, ySize);
    colorMap->data()->setRange(QCPRange(xminval, xmaxval), QCPRange(yminval, ymaxval));
    for (int x = 0; x < xSize; ++x)
    {
      for (int y = 0; y < ySize; ++y)
      {
        colorMap->data()->setAlpha(x, y, 180);
        colorMap->data()->setCell(x, y, 0.0);
      }
    }

    for (int i = 0; i < frameCount; i++)
    {
        int x = static_cast<int>(((modelFrames->at(i).timeStamp().microSeconds() / 1000000.0) - xminval) * 4.0);
        int y = static_cast<int>(modelFrames->at(i).frameId() - yminval) / 30;
        double val = colorMap->data()->cell(x, y);
        double inc;
        inc = 1 / (val + 1); //logarithmic decay
        inc = inc * inc; //square the increment to make it even more stark
        val = val + inc;
        qDebug() << "X: " << x << " Y: " << y << "Val: " << val;
        colorMap->data()->setCell(x, y, val);
    }

    colorMap->setGradient(QCPColorGradient::gpJet);
    colorMap->rescaleDataRange(true);
    ui->graphingView->rescaleAxes();
    ui->graphingView->replot();
}

void TemporalGraphWindow::selectionChanged()
{
  /*
   normally, axis base line, axis tick labels and axis labels are selectable separately, but we want
   the user only to be able to select the axis as a whole, so we tie the selected states of the tick labels
   and the axis base line together. However, the axis label shall be selectable individually.

   The selection state of the left and right axes shall be synchronized as well as the state of the
   bottom and top axes.

   Further, we want to synchronize the selection of the graphs with the selection state of the respective
   legend item belonging to that graph. So the user can select a graph by either clicking on the graph itself
   or on its legend item.
  */

    qDebug() << "SelectionChanged";

  // make top and bottom axes be selected synchronously, and handle axis and tick labels as one selectable object:
  if (ui->graphingView->xAxis->selectedParts().testFlag(QCPAxis::spAxis) || ui->graphingView->xAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
      ui->graphingView->xAxis2->selectedParts().testFlag(QCPAxis::spAxis) || ui->graphingView->xAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
  {
    ui->graphingView->xAxis2->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
    ui->graphingView->xAxis->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
  }
  // make left and right axes be selected synchronously, and handle axis and tick labels as one selectable object:
  if (ui->graphingView->yAxis->selectedParts().testFlag(QCPAxis::spAxis) || ui->graphingView->yAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
      ui->graphingView->yAxis2->selectedParts().testFlag(QCPAxis::spAxis) || ui->graphingView->yAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
  {
    ui->graphingView->yAxis2->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
    ui->graphingView->yAxis->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
  }
}

void TemporalGraphWindow::mousePress()
{
  // if an axis is selected, only allow the direction of that axis to be dragged
  // if no axis is selected, both directions may be dragged

  if (ui->graphingView->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
  {
    ui->graphingView->axisRect()->setRangeDrag(ui->graphingView->xAxis->orientation());
  }
  else if (ui->graphingView->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
  {
    ui->graphingView->axisRect()->setRangeDrag(ui->graphingView->yAxis->orientation());
  }
  else
  {
    ui->graphingView->axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
  }
}

void TemporalGraphWindow::mouseWheel()
{
   qDebug() << "Mouse WHeel";
  // if an axis is selected, only allow the direction of that axis to be zoomed
  // if no axis is selected, both directions may be zoomed

  if (ui->graphingView->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
    ui->graphingView->axisRect()->setRangeZoom(ui->graphingView->xAxis->orientation());
  else if (ui->graphingView->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
    ui->graphingView->axisRect()->setRangeZoom(ui->graphingView->yAxis->orientation());
  else
    ui->graphingView->axisRect()->setRangeZoom(Qt::Horizontal|Qt::Vertical);
}

bool TemporalGraphWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_Plus:
            zoomIn();
            break;
        case Qt::Key_Minus:
            zoomOut();
            break;
        case Qt::Key_R:
            resetView();
            break;
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("temporalwindow.md");
            break;
        }
        return true;
    }  else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }

    return false;
}

void TemporalGraphWindow::resetView()
{
    ui->graphingView->xAxis->setRange(xminval, xmaxval);
    ui->graphingView->yAxis->setRange(yminval, ymaxval);
    ui->graphingView->axisRect()->setupFullAxesBox();

    ui->graphingView->replot();
}

void TemporalGraphWindow::zoomIn()
{
    QCPRange xrange = ui->graphingView->xAxis->range();
    QCPRange yrange = ui->graphingView->yAxis->range();
    if (ui->graphingView->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        ui->graphingView->xAxis->scaleRange(0.666, xrange.center());
    }

    else if (ui->graphingView->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        ui->graphingView->yAxis->scaleRange(0.666, yrange.center());
    }
    else
    {
        ui->graphingView->xAxis->scaleRange(0.666, xrange.center());
        ui->graphingView->yAxis->scaleRange(0.666, yrange.center());
    }
    ui->graphingView->replot();
}

void TemporalGraphWindow::zoomOut()
{
    QCPRange xrange = ui->graphingView->xAxis->range();
    QCPRange yrange = ui->graphingView->yAxis->range();
    if (ui->graphingView->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        ui->graphingView->xAxis->scaleRange(1.5, xrange.center());
    }

    else if (ui->graphingView->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        ui->graphingView->yAxis->scaleRange(1.5, yrange.center());
    }
    else
    {
        ui->graphingView->xAxis->scaleRange(1.5, xrange.center());
        ui->graphingView->yAxis->scaleRange(1.5, yrange.center());
    }
    ui->graphingView->replot();
}

