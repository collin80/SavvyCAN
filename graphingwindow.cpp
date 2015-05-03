#include "graphingwindow.h"
#include "ui_graphingwindow.h"
#include "newgraphdialog.h"

GraphingWindow::GraphingWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GraphingWindow)
{
    ui->setupUi(this);

    ui->graphingView->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes |
                                    QCP::iSelectLegend | QCP::iSelectPlottables);

    connect(ui->graphingView, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(mousePress()));
    connect(ui->graphingView, SIGNAL(mouseWheel(QWheelEvent*)), this, SLOT(mouseWheel()));

    // make bottom and left axes transfer their ranges to top and right axes:
    connect(ui->graphingView->xAxis, SIGNAL(rangeChanged(QCPRange)), ui->graphingView->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->graphingView->yAxis, SIGNAL(rangeChanged(QCPRange)), ui->graphingView->yAxis2, SLOT(setRange(QCPRange)));

    connect(ui->graphingView, SIGNAL(titleDoubleClick(QMouseEvent*,QCPPlotTitle*)), this, SLOT(titleDoubleClick(QMouseEvent*,QCPPlotTitle*)));
    connect(ui->graphingView, SIGNAL(axisDoubleClick(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)), this, SLOT(axisLabelDoubleClick(QCPAxis*,QCPAxis::SelectablePart)));
    connect(ui->graphingView, SIGNAL(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)), this, SLOT(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*)));

    // setup policy and connect slot for context menu popup:
    ui->graphingView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->graphingView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenuRequest(QPoint)));



}

GraphingWindow::~GraphingWindow()
{
    delete ui;
}


void GraphingWindow::titleDoubleClick(QMouseEvent* event, QCPPlotTitle* title)
{
  Q_UNUSED(event)
  // Set the plot title by double clicking on it
  bool ok;
  QString newTitle = QInputDialog::getText(this, "SavvyCAN Graphing", "New plot title:", QLineEdit::Normal, title->text(), &ok);
  if (ok)
  {
    title->setText(newTitle);
    ui->graphingView->replot();
  }
}

void GraphingWindow::axisLabelDoubleClick(QCPAxis *axis, QCPAxis::SelectablePart part)
{
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

void GraphingWindow::legendDoubleClick(QCPLegend *legend, QCPAbstractLegendItem *item)
{
  // Rename a graph by double clicking on its legend item
  Q_UNUSED(legend)
  if (item) // only react if item was clicked (user could have clicked on border padding of legend where there is no item, then item is 0)
  {
    QCPPlottableLegendItem *plItem = qobject_cast<QCPPlottableLegendItem*>(item);
    bool ok;
    QString newName = QInputDialog::getText(this, "SavvyCAN Graphing", "New graph name:", QLineEdit::Normal, plItem->plottable()->name(), &ok);
    if (ok)
    {
      plItem->plottable()->setName(newName);
      ui->graphingView->replot();
    }
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
      graph->setSelected(true);
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

void GraphingWindow::removeSelectedGraph()
{
  if (ui->graphingView->selectedGraphs().size() > 0)
  {
    ui->graphingView->removeGraph(ui->graphingView->selectedGraphs().first());
    ui->graphingView->replot();
  }
}

void GraphingWindow::removeAllGraphs()
{
  ui->graphingView->clearGraphs();
  ui->graphingView->replot();
}

void GraphingWindow::contextMenuRequest(QPoint pos)
{
  QMenu *menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose);

  if (ui->graphingView->legend->selectTest(pos, false) >= 0) // context menu on legend requested
  {
    menu->addAction("Move to top left", this, SLOT(moveLegend()))->setData((int)(Qt::AlignTop|Qt::AlignLeft));
    menu->addAction("Move to top center", this, SLOT(moveLegend()))->setData((int)(Qt::AlignTop|Qt::AlignHCenter));
    menu->addAction("Move to top right", this, SLOT(moveLegend()))->setData((int)(Qt::AlignTop|Qt::AlignRight));
    menu->addAction("Move to bottom right", this, SLOT(moveLegend()))->setData((int)(Qt::AlignBottom|Qt::AlignRight));
    menu->addAction("Move to bottom left", this, SLOT(moveLegend()))->setData((int)(Qt::AlignBottom|Qt::AlignLeft));
  } else  // general context menu on graphs requested
  {
    menu->addAction("Add new graph", this, SLOT(addNewGraph()));
    if (ui->graphingView->selectedGraphs().size() > 0)
      menu->addAction("Remove selected graph", this, SLOT(removeSelectedGraph()));
    if (ui->graphingView->graphCount() > 0)
      menu->addAction("Remove all graphs", this, SLOT(removeAllGraphs()));
  }

  menu->popup(ui->graphingView->mapToGlobal(pos));
}

void GraphingWindow::addNewGraph()
{
    NewGraphDialog *thisDialog = new NewGraphDialog();
    thisDialog->show();
}

void GraphingWindow::moveLegend()
{
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
