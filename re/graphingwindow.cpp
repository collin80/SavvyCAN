#include "graphingwindow.h"
#include "ui_graphingwindow.h"
#include "newgraphdialog.h"
#include "mainwindow.h"
#include "helpwindow.h"
#include <QDebug>

GraphingWindow::GraphingWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GraphingWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    readSettings();

    modelFrames = frames;
    dbcHandler = DBCHandler::getReference();

    ui->graphingView->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes |
                                    QCP::iSelectLegend | QCP::iSelectPlottables);

    ui->graphingView->xAxis->setRange(0, 8);
    ui->graphingView->yAxis->setRange(0, 255);
    ui->graphingView->axisRect()->setupFullAxesBox();

    //ui->graphingView->plotLayout()->insertRow(0);
    //ui->graphingView->plotLayout()->addElement(0, 0, new QCPPlotTitle(ui->graphingView, "Data Graphing"));

    ui->graphingView->xAxis->setLabel("Time Axis");
    ui->graphingView->yAxis->setLabel("Value Axis");
    ui->graphingView->xAxis->setNumberFormat("f");
    if (secondsMode) ui->graphingView->xAxis->setNumberPrecision(6);
        else ui->graphingView->xAxis->setNumberPrecision(0);

    ui->graphingView->legend->setVisible(true);
    QFont legendFont = font();
    legendFont.setPointSize(10);
    QFont legendSelectedFont = font();
    legendSelectedFont.setPointSize(12);
    legendSelectedFont.setBold(true);
    ui->graphingView->legend->setFont(legendFont);
    ui->graphingView->legend->setSelectedFont(legendSelectedFont);
    ui->graphingView->legend->setSelectableParts(QCPLegend::spItems); // legend box shall not be selectable, only legend items

    locationText = new QCPItemText(ui->graphingView);
    locationText->position->setType(QCPItemPosition::ptAxisRectRatio);
    locationText->position->setCoords(QPointF(0.16, 0.03));
    locationText->setText("X: 0  Y: 0");
    locationText->setFont(legendSelectedFont);

    itemTracer = new QCPItemTracer(ui->graphingView);
    itemTracer->setInterpolating(true);
    itemTracer->setVisible(false); //no graph selected yet
    itemTracer->setStyle(QCPItemTracer::tsCircle);
    itemTracer->setSize(20);

    // connect slot that ties some axis selections together (especially opposite axes):
    connect(ui->graphingView, SIGNAL(selectionChangedByUser()), this, SLOT(selectionChanged()));
    //connect up the mouse controls
    connect(ui->graphingView, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(mousePress()));
    connect(ui->graphingView, SIGNAL(plottableDoubleClick(QCPAbstractPlottable*,int,QMouseEvent*)), this, SLOT(plottableDoubleClick(QCPAbstractPlottable*,int,QMouseEvent*)));
    connect(ui->graphingView, SIGNAL(plottableClick(QCPAbstractPlottable*,int,QMouseEvent*)), this, SLOT(plottableClick(QCPAbstractPlottable*,int,QMouseEvent*)));
    connect(ui->graphingView, SIGNAL(mouseWheel(QWheelEvent*)), this, SLOT(mouseWheel()));

    // make bottom and left axes transfer their ranges to top and right axes:
    connect(ui->graphingView->xAxis, SIGNAL(rangeChanged(QCPRange)), ui->graphingView->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->graphingView->yAxis, SIGNAL(rangeChanged(QCPRange)), ui->graphingView->yAxis2, SLOT(setRange(QCPRange)));

    //connect(ui->graphingView, SIGNAL(titleDoubleClick(QMouseEvent*,QCPTextElement*)), this, SLOT(titleDoubleClick(QMouseEvent*,QCPTextElement*)));
    connect(ui->graphingView, SIGNAL(axisDoubleClick(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)), this, SLOT(axisLabelDoubleClick(QCPAxis*,QCPAxis::SelectablePart)));
    connect(ui->graphingView, SIGNAL(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)), this, SLOT(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*)));
    connect(ui->graphingView, SIGNAL(legendClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)), this, SLOT(legendSingleClick(QCPLegend*,QCPAbstractLegendItem*)));

    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));

    // setup policy and connect slot for context menu popup:
    ui->graphingView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->graphingView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenuRequest(QPoint)));

    selectedPen.setWidth(1);
    selectedPen.setColor(Qt::blue);

    //ui->graphingView->setAttribute(Qt::WA_AcceptTouchEvents);

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

    needScaleSetup = true;
    followGraphEnd = false;
}

GraphingWindow::~GraphingWindow()
{
    delete ui;
}

void GraphingWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    installEventFilter(this);
    readSettings();
    ui->graphingView->replot();
}

void GraphingWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    removeEventFilter(this);
    writeSettings();
}

void GraphingWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("Graphing/WindowSize", QSize(800, 600)).toSize());
        move(settings.value("Graphing/WindowPos", QPoint(50, 50)).toPoint());
    }
    secondsMode = settings.value("Main/TimeSeconds", false).toBool();
    useOpenGL = settings.value("Main/UseOpenGL", false).toBool();
}

void GraphingWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("Graphing/WindowSize", size());
        settings.setValue("Graphing/WindowPos", pos());
    }
}

void GraphingWindow::updatedFrames(int numFrames)
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
        for (int i = 0; i < graphParams.count(); i++)
        {
            createGraph(graphParams[i], false); //regenerate each one
        }
        ui->graphingView->replot(); //now, redisplay them all

    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        //there shouldn't be any need to actually remove the graphs.
        //regenerate them instead
        ui->graphingView->clearGraphs(); //temporarily remove the graphs from the graph view
        //needScaleSetup = true;
        for (int i = 0; i < graphParams.count(); i++)
        {
            createGraph(graphParams[i], false); //regenerate each one
        }
        ui->graphingView->replot(); //now, redisplay them all
    }
    else //just got some new frames. See if they are relevant.
    {
        if (numFrames > modelFrames->count()) return;

        for (int j = 0; j < graphParams.count(); j++)
        {
            appendedToGraph = false;
            x.clear();
            y.clear();
            for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
            {
                thisFrame = modelFrames->at(i);
                if (graphParams[j].ID == thisFrame.ID)
                {
                    appendToGraph(graphParams[j], thisFrame, x, y);
                    appendedToGraph = true;
                }
            }
            if (appendedToGraph)
            {
                graphParams[j].ref->addData(x, y);
                needReplot = true;
            }
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

void GraphingWindow::plottableClick(QCPAbstractPlottable* plottable, int dataIdx, QMouseEvent* event)
{
    Q_UNUSED(dataIdx);
    qDebug() << "plottableClick";
    double x, y;
    QCPGraph *graph = reinterpret_cast<QCPGraph *>(plottable);
    graph->pixelsToCoords(event->localPos(), x, y);
    locationText->setText("X: " + QString::number(x, 'f', 3) + " Y: " + QString::number(y, 'f', 3));
}

void GraphingWindow::plottableDoubleClick(QCPAbstractPlottable* plottable, int dataIdx, QMouseEvent* event)
{
    Q_UNUSED(dataIdx);
    qDebug() << "plottableDoubleClick";
    int id = 0;
    //apply transforms to get the X axis value where we double clicked
    double coord = plottable->keyAxis()->pixelToCoord(event->localPos().x());
    id = plottable->property("id").toInt();
    if (secondsMode) emit sendCenterTimeID(id, coord);
    else emit sendCenterTimeID(id, coord / 1000000.0);

    double x, y;
    QCPGraph *graph = reinterpret_cast<QCPGraph *>(plottable);
    graph->pixelsToCoords(event->localPos(), x, y);
    x = ui->graphingView->xAxis->pixelToCoord(event->localPos().x());

    itemTracer->setGraph(graph);
    itemTracer->setVisible(true);
    itemTracer->setInterpolating(true);
    itemTracer->setGraphKey(x);
    itemTracer->updatePosition();
    qDebug() << "val " << itemTracer->position->value();
    locationText->setText("X: " + QString::number(x) + " Y: " + QString::number(itemTracer->position->value()));
}

void GraphingWindow::gotCenterTimeID(int32_t ID, double timestamp)
{
    Q_UNUSED(ID)
    //its problematic to try to highlight a graph since we get the ID
    //and timestamp not the signal in question so there is no real way
    //to know which graph. But, if that changes here is a stub
    //for (int i = 0; i < graphParams.count(); i++)
    //{
    //}

    QCPRange range = ui->graphingView->xAxis->range();
    double offset = range.size() / 2.0;
    if (!secondsMode) timestamp *= 1000000.0; //timestamp is always in seconds when being passed so convert if necessary
    ui->graphingView->xAxis->setRange(timestamp - offset, timestamp + offset);
    ui->graphingView->replot();
}

void GraphingWindow::titleDoubleClick(QMouseEvent* event, QCPTextElement* title)
{
  Q_UNUSED(event)
  Q_UNUSED(title)
    qDebug() << "title Double Click";
  // Set the plot title by double clicking on it

    /*
  bool ok;
  QString newTitle = QInputDialog::getText(this, "SavvyCAN Graphing", "New plot title:", QLineEdit::Normal, title->text(), &ok);
  if (ok)
  {
    title->setText(newTitle);
    ui->graphingView->replot();
  } */

  editSelectedGraph();
}

void GraphingWindow::axisLabelDoubleClick(QCPAxis *axis, QCPAxis::SelectablePart part)
{
  qDebug() << "axisLabelDoubleClick";
  // Set an axis label by double clicking on it
  if (part == QCPAxis::spAxisLabel) // only react when the actual axis label is clicked, not tick label or axis backbone
  {
    bool ok;
    QString newLabel = QInputDialog::getText(this, "SavvyCAN Graphing", "New axis label:", QLineEdit::Normal, axis->label(), &ok);
    if (ok)
    {
      axis->setLabel(newLabel);
      ui->graphingView->replot();
    }
  }
}

void GraphingWindow::legendSingleClick(QCPLegend *legend, QCPAbstractLegendItem *item)
{
   // select a graph by clicking on the legend
   qDebug() << "Legend Single Click " << item;
   Q_UNUSED(legend)
   if (item) // only react if item was clicked (user could have clicked on border padding of legend where there is no item, then item is 0)
   {
      QCPPlottableLegendItem *plItem = qobject_cast<QCPPlottableLegendItem*>(item);
      QCPGraph *pGraph = qobject_cast<QCPGraph *>(plItem->plottable());
      QCPDataSelection sel;
      QCPDataRange rang;
      rang.setBegin(0);
      rang.setEnd(pGraph->dataCount());
      sel.addDataRange(rang);
      pGraph->setSelection(sel);
   }
}

void GraphingWindow::legendDoubleClick(QCPLegend *legend, QCPAbstractLegendItem *item)
{
   // edit a graph by double clicking on its legend item
   qDebug() << "Legend Double Click " << item;
   Q_UNUSED(legend)
   if (item) // only react if item was clicked (user could have clicked on border padding of legend where there is no item, then item is 0)
   {
      QCPPlottableLegendItem *plItem = qobject_cast<QCPPlottableLegendItem*>(item);
      QCPGraph *pGraph = qobject_cast<QCPGraph *>(plItem->plottable());
      QCPDataSelection sel;
      QCPDataRange rang;
      rang.setBegin(0);
      rang.setEnd(pGraph->dataCount());
      sel.addDataRange(rang);
      pGraph->setSelection(sel);
      editSelectedGraph();
   }
}

void GraphingWindow::selectionChanged()
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

  // synchronize selection of graphs with selection of corresponding legend items:
  for (int i=0; i<ui->graphingView->graphCount(); ++i)
  {
    QCPGraph *graph = ui->graphingView->graph(i);
    QCPPlottableLegendItem *item = ui->graphingView->legend->itemWithPlottable(graph);
    if (item->selected() || graph->selected())
    {
      item->setSelected(true);
      //select graph too.
      QCPDataSelection sel;
      QCPDataRange rang;
      rang.setBegin(0);
      rang.setEnd(graph->dataCount());
      sel.addDataRange(rang);
      graph->setSelection(sel);
    }
  }
}

void GraphingWindow::mousePress()
{
  // if an axis is selected, only allow the direction of that axis to be dragged
  // if no axis is selected, both directions may be dragged

  if (ui->graphingView->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
    ui->graphingView->axisRect()->setRangeDrag(ui->graphingView->xAxis->orientation());
  else if (ui->graphingView->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
    ui->graphingView->axisRect()->setRangeDrag(ui->graphingView->yAxis->orientation());
  else
    ui->graphingView->axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
}

void GraphingWindow::mouseWheel()
{
  // if an axis is selected, only allow the direction of that axis to be zoomed
  // if no axis is selected, both directions may be zoomed

  if (ui->graphingView->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
    ui->graphingView->axisRect()->setRangeZoom(ui->graphingView->xAxis->orientation());
  else if (ui->graphingView->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
    ui->graphingView->axisRect()->setRangeZoom(ui->graphingView->yAxis->orientation());
  else
    ui->graphingView->axisRect()->setRangeZoom(Qt::Horizontal|Qt::Vertical);
}

bool GraphingWindow::eventFilter(QObject *obj, QEvent *event)
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
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("graphwindow.html");
            break;
        }
        return true;
    } else if (event->type() == QEvent::TouchBegin)
    {
        qDebug() << "Touch begin";
    } else if (event->type() == QEvent::TouchCancel)
    {
        qDebug() << "Touch cancel";
    } else if (event->type() == QEvent::TouchEnd)
    {
        qDebug() << "Touch End";
    } else if (event->type() == QEvent::TouchUpdate)
    {
        qDebug() << "Touch Update";
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }

    return false;
}

void GraphingWindow::resetView()
{
    double yminval=10000000.0, ymaxval = -1000000.0;
    double xminval=100000000000, xmaxval = -10000000000.0;
    for (int i = 0; i < graphParams.count(); i++)
    {
        for (int j = 0; j < graphParams[i].x.count(); j++)
        {
            if (graphParams[i].x[j] < xminval) xminval = graphParams[i].x[j];
            if (graphParams[i].x[j] > xmaxval) xmaxval = graphParams[i].x[j];
            if (graphParams[i].y[j] < yminval) yminval = graphParams[i].y[j];
            if (graphParams[i].y[j] > ymaxval) ymaxval = graphParams[i].y[j];
        }
    }

    ui->graphingView->xAxis->setRange(xminval, xmaxval);
    ui->graphingView->yAxis->setRange(yminval, ymaxval);
    ui->graphingView->axisRect()->setupFullAxesBox();

    ui->graphingView->replot();
}

void GraphingWindow::zoomIn()
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

void GraphingWindow::zoomOut()
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

void GraphingWindow::removeSelectedGraph()
{
  if (ui->graphingView->selectedGraphs().size() > 0)
  {
    int idx = -1;
    for (int i = 0; i < graphParams.count(); i++)
    {
        if (graphParams[i].ref == ui->graphingView->selectedGraphs().first())
        {
            idx = i;
            break;
        }
    }
    graphParams.removeAt(idx);

    ui->graphingView->removeGraph(ui->graphingView->selectedGraphs().first());

    if (graphParams.count() == 0) needScaleSetup = true;

    ui->graphingView->replot();
  }
}

void GraphingWindow::editSelectedGraph()
{
    if (ui->graphingView->selectedGraphs().size() > 0)
    {
        int idx = -1;
        for (int i = 0; i < graphParams.count(); i++)
        {
            if (graphParams[i].ref == ui->graphingView->selectedGraphs().first())
            {
                idx = i;
                break;
            }
        }

        qDebug() << "Selected index for editing: " << idx;

        showParamsDialog(idx);

        //ui->graphingView->replot();
    }
}

void GraphingWindow::removeAllGraphs()
{
    QMessageBox::StandardButton confirmDialog;
    confirmDialog = QMessageBox::question(this, "Really?", "Remove all graphs?",
                                  QMessageBox::Yes|QMessageBox::No);
    if (confirmDialog == QMessageBox::Yes) {
        ui->graphingView->clearGraphs();
        graphParams.clear();
        needScaleSetup = true;
        ui->graphingView->replot();
    }
}

void GraphingWindow::toggleFollowMode()
{
    followGraphEnd = !followGraphEnd;
}

void GraphingWindow::contextMenuRequest(QPoint pos)
{
  QMenu *menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose);

  if (ui->graphingView->legend->selectTest(pos, false) >= 0) // context menu on legend requested
  {
    menu->addAction(tr("Move to top left"), this, SLOT(moveLegend()))->setData((int)(Qt::AlignTop|Qt::AlignLeft));
    menu->addAction(tr("Move to top center"), this, SLOT(moveLegend()))->setData((int)(Qt::AlignTop|Qt::AlignHCenter));
    menu->addAction(tr("Move to top right"), this, SLOT(moveLegend()))->setData((int)(Qt::AlignTop|Qt::AlignRight));
    menu->addAction(tr("Move to bottom right"), this, SLOT(moveLegend()))->setData((int)(Qt::AlignBottom|Qt::AlignRight));
    menu->addAction(tr("Move to bottom left"), this, SLOT(moveLegend()))->setData((int)(Qt::AlignBottom|Qt::AlignLeft));
  }
  else  // general context menu on graphs requested
  {
    menu->addAction(tr("Save graph image to file"), this, SLOT(saveGraphs()));
    menu->addAction(tr("Save graph definitions to file"), this, SLOT(saveDefinitions()));
    menu->addAction(tr("Load graph definitions from file"), this, SLOT(loadDefinitions()));
    menu->addAction(tr("Save spreadsheet of data"), this, SLOT(saveSpreadsheet()));
    QAction *act = menu->addAction(tr("Follow end of graph"), this, SLOT(toggleFollowMode()));
    act->setCheckable(true);
    act->setChecked(followGraphEnd);
    menu->addAction(tr("Add new graph"), this, SLOT(addNewGraph()));
    if (ui->graphingView->selectedGraphs().size() > 0)
    {
        menu->addSeparator();
        menu->addAction(tr("Edit selected graph"), this, SLOT(editSelectedGraph()));
        menu->addAction(tr("Remove selected graph"), this, SLOT(removeSelectedGraph()));
    }
    if (ui->graphingView->graphCount() > 0)
    {
        menu->addSeparator();
        menu->addAction(tr("Remove all graphs"), this, SLOT(removeAllGraphs()));
    }
    menu->addSeparator();
    menu->addAction(tr("Reset View"), this, SLOT(resetView()));
    menu->addAction(tr("Zoom In"), this, SLOT(zoomIn()));
    menu->addAction(tr("Zoom Out"), this, SLOT(zoomOut()));
  }

  menu->popup(ui->graphingView->mapToGlobal(pos));
}

void GraphingWindow::saveGraphs()
{
    QString filename;
    QFileDialog dialog(this);
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("PDF Files (*.pdf)")));
    filters.append(QString(tr("PNG Files (*.png)")));
    filters.append(QString(tr("JPEG Files (*.jpg)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDirectory(settings.value("Graphing/LoadSaveDirectory", dialog.directory().path()).toString());

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        settings.setValue("Graphing/LoadSaveDirectory", dialog.directory().path());

        if (dialog.selectedNameFilter() == filters[0])
        {
            if (!filename.contains('.')) filename += ".pdf";
            ui->graphingView->savePdf(filename, 0, 0);
        }
        if (dialog.selectedNameFilter() == filters[1])
        {
            if (!filename.contains('.')) filename += ".png";
            ui->graphingView->savePng(filename, 0, 0);
        }
        if (dialog.selectedNameFilter() == filters[2])
        {
            if (!filename.contains('.')) filename += ".jpg";
            ui->graphingView->saveJpg(filename, 0, 0);
        }
    }
}

void GraphingWindow::saveSpreadsheet()
{
    QString filename;
    QFileDialog dialog(this);
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("Spreadsheet (*.csv)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDirectory(settings.value("Graphing/LoadSaveDirectory", dialog.directory().path()).toString());

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        settings.setValue("Graphing/LoadSaveDirectory", dialog.directory().path());

        if (!filename.contains('.')) filename += ".csv";

        QFile *outFile = new QFile(filename);

        if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
            return;

        /*
         * save some data
         * The problem here is that we've got X number of graphs that all have different
         * timestamps but a spreadsheet would be best if each graph were taken at the same slice
         * such that you have a list of slices with the value of each graph at that slice.
         *
         * But, for now export each graph in turn with the proper timestamp for each piece of data
         * and a reference for which graph it came from. This is better than nothing.
        */

        QList<GraphParams>::iterator iter;
        double xMin = 10000000000000, xMax=-10000000000000;
        int maxCount = 0;
        int numGraphs = 0;
        for (iter = graphParams.begin(); iter != graphParams.end(); ++iter)
        {
            if (iter->x[0] < xMin) xMin = iter->x[0];
            if (iter->x[iter->x.count() - 1] > xMax) xMax = iter->x[iter->x.count() - 1];
            if (maxCount < iter->x.count()) maxCount = iter->x.count();
            numGraphs++;
        }
        qDebug() << "xMin: " << xMin;
        qDebug() << "xMax: " << xMax;
        qDebug() << "MaxCount: " << maxCount;
        //The idea now is to iterate from xMin to xMax slicing all graphs up into MaxCount slices.
        //But, actually, don't visit actual xMin or xMax, inset from there by one slice. Then, if
        //a given graph doesn't exist there use the value from the nearest place that does exist.
        double xSize = xMax - xMin;
        double sliceSize = xSize / ((double)maxCount);
        double equivValue = sliceSize / 100.0;
        double currentX;
        double value;
        QList<int> indices;
        indices.reserve(numGraphs);

        outFile->write("TimeStamp");
        for (int zero = 0; zero < numGraphs; zero++)
        {
            indices.append(0);
            outFile->putChar(',');
            outFile->write(graphParams[zero].graphName.toUtf8());
        }
        outFile->write("\n");

        for (int j = 1; j < (maxCount - 1); j++)
        {
            currentX = xMin + (j * sliceSize);
            qDebug() << "X: " << currentX;
            outFile->write(QString::number(currentX).toUtf8());
            for (int k = 0; k < graphParams.count(); k++)
            {
                value = 0.0;
                //five possibilities.
                //1: we're at the beginning for this graph but the slice is before this graph even starts
                if (indices[k] == 0 && graphParams[k].x[indices[k]] > currentX)
                {
                    value = graphParams[k].y[indices[k]];
                }
                //2: The opposite, we're at the end of this graph but the slices keep going
                else if (indices[k] == (graphParams[k].x.count() - 1) && graphParams[k].x[indices[k]] < currentX)
                {
                    value = graphParams[k].y[indices[k]];
                }
                //3: the slice is right near the current value we're at for this graph
                else if (fabs(graphParams[k].x[indices[k]] - currentX) < equivValue)
                {
                    value = graphParams[k].y[indices[k]];
                }
                //4: the slice is right next to the next value for this graph
                else if (fabs(graphParams[k].x[indices[k] + 1] - currentX) < equivValue)
                {
                    value = graphParams[k].y[indices[k] + 1];
                }
                //5: it's somewhere in between two values for this graph
                //the two values will be indices[k] and indices[k] + 1
                else
                {
                    double span = graphParams[k].x[indices[k] + 1] - graphParams[k].x[indices[k]];
                    double progress = (currentX - graphParams[k].x[indices[k]]) / span;
                    value = Utility::Lerp(graphParams[k].y[indices[k]], graphParams[k].y[indices[k] + 1], progress);
                    qDebug() << "Span: " << span << " Prog: " << progress << " Value: " << value;
                }

                if (currentX >= graphParams[k].x[indices[k]]) indices[k]++;

                outFile->putChar(',');
                outFile->write(QString::number(value).toUtf8());
            }
            outFile->write("\n");
        }

        outFile->close();
    }

}

void GraphingWindow::saveDefinitions()
{
    QString filename;
    QFileDialog dialog(this);
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("Graph definition (*.gdf)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDirectory(settings.value("Graphing/LoadSaveDirectory", dialog.directory().path()).toString());

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        settings.setValue("Graphing/LoadSaveDirectory", dialog.directory().path());

        if (!filename.contains('.')) filename += ".gdf";

        QFile *outFile = new QFile(filename);

        if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
            return;

        QList<GraphParams>::iterator iter;
        for (iter = graphParams.begin(); iter != graphParams.end(); ++iter)
        {
            outFile->write("X,");
            outFile->write(QString::number(iter->ID, 16).toUtf8());
            outFile->putChar(',');
            outFile->write(QString::number(iter->mask, 16).toUtf8());
            outFile->putChar(',');
            if (iter->intelFormat) outFile->write(QString::number(iter->startBit).toUtf8());
                else outFile->write(QString::number(iter->startBit * -1).toUtf8());
            outFile->putChar(',');
            outFile->write(QString::number(iter->numBits).toUtf8());
            outFile->putChar(',');
            if (iter->isSigned) outFile->putChar('Y');
                else outFile->putChar('N');
            outFile->putChar(',');
            outFile->write(QString::number(iter->bias).toUtf8());
            outFile->putChar(',');
            outFile->write(QString::number(iter->scale).toUtf8());
            outFile->putChar(',');
            outFile->write(QString::number(iter->stride).toUtf8());
            outFile->putChar(',');
            outFile->write(QString::number(iter->color.red()).toUtf8());
            outFile->putChar(',');
            outFile->write(QString::number(iter->color.green()).toUtf8());
            outFile->putChar(',');
            outFile->write(QString::number(iter->color.blue()).toUtf8());
            outFile->putChar(',');
            outFile->write(iter->graphName.toUtf8());
            outFile->write("\n");
        }
        outFile->close();
    }
}

void GraphingWindow::loadDefinitions()
{
    QString filename;
    QFileDialog dialog;
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("Graph definition (*.gdf)")));

    if (dbcHandler == nullptr) return;
    if (dbcHandler->getFileCount() == 0) dbcHandler->createBlankFile();

    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setDirectory(settings.value("Graphing/LoadSaveDirectory", dialog.directory().path()).toString());

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        settings.setValue("Graphing/LoadSaveDirectory", dialog.directory().path());

        QFile *inFile = new QFile(filename);
        QByteArray line;

        if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
            return;

        while (!inFile->atEnd()) {
            line = inFile->readLine().simplified();
            if (line.length() > 2)
            {
                GraphParams gp;

                QList<QByteArray> tokens = line.split(',');

                if (tokens[0] == "X") //newest format based around signals
                {
                    gp.ID = tokens[1].toUInt(nullptr, 16);
                    gp.mask = tokens[2].toULongLong(nullptr, 16);
                    gp.startBit = tokens[3].toInt();
                    if (gp.startBit < 0) {
                        gp.intelFormat = false;
                        gp.startBit *= -1;
                    }
                    else gp.intelFormat = true;
                    gp.numBits = tokens[4].toInt();
                    if (tokens[5] == "Y") gp.isSigned = true;
                        else gp.isSigned = false;
                    gp.bias = tokens[6].toFloat();
                    gp.scale = tokens[7].toFloat();
                    gp.stride = tokens[8].toInt();

                    gp.color.setRed(tokens[9].toInt());
                    gp.color.setGreen(tokens[10].toInt());
                    gp.color.setBlue(tokens[11].toInt());
                    if (tokens.length() > 12)
                        gp.graphName = tokens[12];
                    else
                        gp.graphName = QString();
                    createGraph(gp, true);
                }
                else //one of the two older formats then
                {
                    gp.ID = tokens[0].toUInt(nullptr, 16);
                    if (tokens[1] == "S") //old signal based graph definition
                    {
                        //tokens[2] is the signal name. Need to use the message ID and this name to look it up
                        DBC_MESSAGE *msg = dbcHandler->getFileByIdx(0)->messageHandler->findMsgByID(gp.ID);
                        if (msg != nullptr)
                        {
                            DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(tokens[2]);
                            if (sig)
                            {
                                gp.mask = 0xFFFFFFFF;
                                gp.bias = (float)sig->bias;
                                gp.color.setRed(tokens[3].toInt());
                                gp.color.setGreen(tokens[4].toInt());
                                gp.color.setBlue(tokens[5].toInt());
                                gp.graphName = sig->name;
                                gp.intelFormat = sig->intelByteOrder;
                                if (sig->valType == SIGNED_INT) gp.isSigned = true;
                                    else gp.isSigned = false;
                                gp.numBits = sig->signalSize;
                                gp.scale = (float)sig->factor;
                                gp.startBit = sig->startBit;
                                gp.stride = 1;
                                createGraph(gp, true);
                            }
                        }
                    }
                    else //old standard graph definition
                    {
                        //hard part - this all changed drastically
                        //the difference between intel and motorola format is whether
                        //start is larger than end byte or not.
                        uint64_t oldMask = tokens[1].toULongLong(nullptr, 16);
                        int oldStart = tokens[2].toInt();
                        int oldEnd = tokens[3].toInt();

                        if (oldEnd > oldStart) //motorola / big endian - hell...
                        {
                            gp.intelFormat = false;
                            //for now just naively use the entire bytes called for.
                            gp.startBit = 8 * oldStart + 7;
                            gp.numBits = (oldEnd - oldStart + 1) * 8;
                        }
                        else if (oldStart > oldEnd) //intel / little endian - easiest of multi-byte types
                        {
                            //have to find both ends. start bit is somewhere in oldEnd and last bit is somewhere in
                            //oldStart.

                            gp.intelFormat = true;

                            //start by setting a safe default if nothing else pans out.
                            gp.startBit = 8 * oldEnd;

                            int numBytes = oldStart - oldEnd + 1;
                            gp.numBits = numBytes * 8;

                            for (int b = 0; b < 8; b++)
                            {
                                if (oldMask & (1ull << b))
                                {
                                    gp.startBit = (8 * oldEnd) + b;
                                    break;
                                }
                            }

                            for (int c = 7; c >= 0; c--)
                            {
                                if ( oldMask & (1ull << (((numBytes - 1) * 8) + c)) )
                                {
                                    gp.numBits -= (7-c);
                                    break;
                                }
                            }
                        }
                        else //within a single byte - easier than the above two by a bit - always use intel format for this
                        {
                            gp.intelFormat = true;
                            oldMask = oldMask & 0xFF; //only this part matters
                            //for intel format we give startbit as the lowest bit number in the signal
                            //we can find that by going backward from bit 0 to 7 and picking the first bit that is 1.
                            //that's our start bit (+ 8*oldStart)
                            //set default first in case the rest falls through
                            gp.startBit = 8 * oldStart;
                            gp.numBits = 8;
                            for (int b = 0; b < 8; b++)
                            {
                                if (oldMask & (1ull << b))
                                {
                                    gp.startBit = 8 * oldStart + b;
                                    gp.numBits = 8 - b;
                                    break;
                                }
                            }
                        }

                        //the rest is easy stuff
                        if (tokens[4] == "Y") gp.isSigned = true;
                            else gp.isSigned = false;
                        gp.bias = tokens[5].toFloat();
                        gp.scale = tokens[6].toFloat();
                        gp.stride = tokens[7].toInt();
                        gp.color.setRed(tokens[8].toInt());
                        gp.color.setGreen(tokens[9].toInt());
                        gp.color.setBlue(tokens[10].toInt());
                        if (tokens.length() > 11)
                            gp.graphName = tokens[11];
                        else
                            gp.graphName = QString();
                        createGraph(gp, true);
                    }
                }
            }
        }
        inFile->close();
    }
}

void GraphingWindow::showParamsDialog(int idx = -1)
{
    dbcHandler = DBCHandler::getReference();

    NewGraphDialog *thisDialog = new NewGraphDialog(dbcHandler, this);

    if (idx > -1)
    {
        thisDialog->setParams(graphParams[idx]);
    }
    else thisDialog->clearParams();

    if (thisDialog->exec() == QDialog::Accepted)
    {
        if (idx > -1) //if there was an existing graph then delete it
        {
            graphParams.removeAt(idx);
            ui->graphingView->removeGraph(idx);
        }
        //create a new graph with the returned parameters.
        GraphParams params;
        thisDialog->getParams(params);
        createGraph(params);
    }
    delete thisDialog;
}

void GraphingWindow::addNewGraph()
{
    showParamsDialog(-1);
}

void GraphingWindow::appendToGraph(GraphParams &params, CANFrame &frame, QVector<double> &x, QVector<double> &y)
{
    params.strideSoFar++;
    if (params.strideSoFar >= params.stride)
    {
        params.strideSoFar = 0;
        int64_t tempVal; //64 bit temp value.
        tempVal = Utility::processIntegerSignal(frame.data, params.startBit, params.numBits, params.intelFormat, params.isSigned); //& params.mask;
        double xVal, yVal;
        if (secondsMode)
        {
            xVal = ((double)(frame.timestamp) / 1000000.0 - params.xbias);
        }
        else
        {
            xVal = (frame.timestamp - params.xbias);
        }
        yVal = (tempVal * params.scale) + params.bias;
        params.x.append(xVal);
        params.y.append(yVal);
        x.append(xVal);
        y.append(yVal);
    }
}

void GraphingWindow::createGraph(GraphParams &params, bool createGraphParam)
{
    int64_t tempVal; //64 bit temp value.
    double yminval=10000000.0, ymaxval = -1000000.0;
    double xminval=10000000000.0, xmaxval = -10000000000.0;
    GraphParams *refParam = &params;
    int sBit, bits;
    bool intelFormat, isSigned;

    qDebug() << "New Graph ID: " << params.ID;
    qDebug() << "Start bit: " << params.startBit;
    qDebug() << "Data length: " << params.numBits;
    qDebug() << "Intel Mode: " << params.intelFormat;
    qDebug() << "Signed: " << params.isSigned;
    qDebug() << "Mask: " << params.mask;

    frameCache.clear();
    for (int i = 0; i < modelFrames->count(); i++)
    {
        CANFrame thisFrame = modelFrames->at(i);
        if (thisFrame.ID == params.ID && thisFrame.remote == false) frameCache.append(thisFrame);
    }

    //to fix weirdness where a graph that has no data won't be able to be edited, selected, or deleted properly
    //we'll check for the condition that there is nothing to graph and add a single dummy frame to the cache
    //that has all data bytes = 0. This allows the graph to be edited and deleted. No idea why you can't otherwise.
    if (frameCache.count() == 0)
    {
        CANFrame dummy;
        dummy.ID = params.ID;
        dummy.bus = 0;
        dummy.len = 8;
        dummy.remote = false;
        dummy.timestamp = 0;
        for (int i = 0; i < 8; i++) dummy.data[i] = 0;
        frameCache.append(dummy);
    }

    int numEntries = frameCache.count() / params.stride;
    if (numEntries < 1) numEntries = 1; //could happen if stride is larger than frame count

    params.x.reserve(numEntries);
    params.y.reserve(numEntries);
    params.x.fill(0, numEntries);
    params.y.fill(0, numEntries);

    sBit = params.startBit;
    bits = params.numBits;
    intelFormat = params.intelFormat;
    isSigned = params.isSigned;

    for (int j = 0; j < numEntries; j++)
    {
        int k = j * params.stride;
        tempVal = Utility::processIntegerSignal(frameCache[k].data, sBit, bits, intelFormat, isSigned); //& params.mask;
        //qDebug() << tempVal;
        if (secondsMode)
        {
            params.x[j] = (frameCache[k].timestamp) / 1000000.0;
        }
        else
        {
            params.x[j] = frameCache[k].timestamp;
        }
        params.y[j] = (tempVal * params.scale) + params.bias;
        if (params.y[j] < yminval) yminval = params.y[j];
        if (params.y[j] > ymaxval) ymaxval = params.y[j];
        if (params.x[j] < xminval) xminval = params.x[j];
        if (params.x[j] > xmaxval) xmaxval = params.x[j];
    }

    if (numEntries == 0)
    {
        yminval = -128.0;
        ymaxval = 128.0;
        xminval = 0;
        xmaxval = 100;
    }

    params.xbias = 0;

    ui->graphingView->addGraph();
    params.ref = ui->graphingView->graph();
    if (createGraphParam)
    {
        graphParams.append(params);
        refParam = &graphParams.last();
    }

    selDecorator = new QCPSelectionDecorator(); //this has to be a pointer as it is freed internally to qcustomplot classes
    selDecorator->setBrush(Qt::NoBrush);
    selDecorator->setPen(selectedPen);
    ui->graphingView->graph()->setSelectionDecorator(selDecorator);

    if (params.graphName == nullptr || params.graphName.length() == 0)
    {
        params.graphName = QString("0x") + QString::number(params.ID, 16) + ":" + QString::number(params.startBit);
        params.graphName += "-" + QString::number(params.numBits);
    }
    ui->graphingView->graph()->setName(params.graphName);
    ui->graphingView->graph()->setProperty("id", params.ID);

    ui->graphingView->graph()->setData(refParam->x,refParam->y);
    ui->graphingView->graph()->setLineStyle(QCPGraph::lsLine); //connect points with lines
    QPen graphPen;
    graphPen.setColor(params.color);
    graphPen.setWidth(1);
    ui->graphingView->graph()->setPen(graphPen);

    qDebug() << "xmin: " << xminval;
    qDebug() << "xmax: " << xmaxval;
    qDebug() << "ymin: " << yminval;
    qDebug() << "ymax: " << ymaxval;

    if (needScaleSetup)
    {
        needScaleSetup = false;
        ui->graphingView->xAxis->setRange(xminval, xmaxval);
        ui->graphingView->yAxis->setRange(yminval, ymaxval);
        ui->graphingView->axisRect()->setupFullAxesBox();
    }

    ui->graphingView->replot();
}

void GraphingWindow::moveLegend()
{
    qDebug() << "moveLegend";
  if (QAction* contextAction = qobject_cast<QAction*>(sender())) // make sure this slot is really called by a context menu action, so it carries the data we need
  {
    bool ok;
    int dataInt = contextAction->data().toInt(&ok);
    if (ok)
    {
      ui->graphingView->axisRect()->insetLayout()->setInsetAlignment(0, (Qt::Alignment)dataInt);
      ui->graphingView->replot();
    }
  }
}
