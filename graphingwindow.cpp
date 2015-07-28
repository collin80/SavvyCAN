#include "graphingwindow.h"
#include "ui_graphingwindow.h"
#include "newgraphdialog.h"
#include "mainwindow.h"
#include <QDebug>

GraphingWindow::GraphingWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GraphingWindow)
{
    ui->setupUi(this);

    readSettings();

    modelFrames = frames;

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

    // connect slot that ties some axis selections together (especially opposite axes):
    connect(ui->graphingView, SIGNAL(selectionChangedByUser()), this, SLOT(selectionChanged()));
    //connect up the mouse controls
    connect(ui->graphingView, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(mousePress()));
    connect(ui->graphingView, SIGNAL(mouseWheel(QWheelEvent*)), this, SLOT(mouseWheel()));

    // make bottom and left axes transfer their ranges to top and right axes:
    connect(ui->graphingView->xAxis, SIGNAL(rangeChanged(QCPRange)), ui->graphingView->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->graphingView->yAxis, SIGNAL(rangeChanged(QCPRange)), ui->graphingView->yAxis2, SLOT(setRange(QCPRange)));

    connect(ui->graphingView, SIGNAL(titleDoubleClick(QMouseEvent*,QCPPlotTitle*)), this, SLOT(titleDoubleClick(QMouseEvent*,QCPPlotTitle*)));
    connect(ui->graphingView, SIGNAL(axisDoubleClick(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)), this, SLOT(axisLabelDoubleClick(QCPAxis*,QCPAxis::SelectablePart)));
    connect(ui->graphingView, SIGNAL(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)), this, SLOT(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*)));

    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));

    // setup policy and connect slot for context menu popup:
    ui->graphingView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->graphingView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenuRequest(QPoint)));

    selectedPen.setWidth(1);
    selectedPen.setColor(Qt::blue);

    needScaleSetup = true;
}

GraphingWindow::~GraphingWindow()
{
    delete ui;
}

void GraphingWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    readSettings();
    ui->graphingView->replot();
}

void GraphingWindow::closeEvent(QCloseEvent *event)
{
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
    if (numFrames == -1) //all frames deleted. Kill the display
    {
        removeAllGraphs();
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
    }
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
    menu->addAction(tr("Add new graph"), this, SLOT(addNewGraph()));
    if (ui->graphingView->selectedGraphs().size() > 0)
    {
        menu->addAction(tr("Edit selected graph"), this, SLOT(editSelectedGraph()));
        menu->addAction(tr("Remove selected graph"), this, SLOT(removeSelectedGraph()));
    }
    if (ui->graphingView->graphCount() > 0)
        menu->addAction(tr("Remove all graphs"), this, SLOT(removeAllGraphs()));
  }

  menu->popup(ui->graphingView->mapToGlobal(pos));
}

void GraphingWindow::saveGraphs()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("PDF Files (*.pdf)")));
    filters.append(QString(tr("PNG Files (*.png)")));
    filters.append(QString(tr("JPEG Files (*.jpg)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];

        if (dialog.selectedNameFilter() == filters[0]) ui->graphingView->savePdf(filename, true, 0, 0);
        if (dialog.selectedNameFilter() == filters[1]) ui->graphingView->savePng(filename, 0, 0);
        if (dialog.selectedNameFilter() == filters[2]) ui->graphingView->saveJpg(filename, 0, 0);
    }
}

void GraphingWindow::saveDefinitions()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("Graph definition (*.gdf)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];

        QFile *outFile = new QFile(filename);

        if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
            return;

        QList<GraphParams>::iterator iter;
        for (iter = graphParams.begin(); iter != graphParams.end(); ++iter)
        {
            outFile->write(QString::number(iter->ID, 16).toUtf8());
            outFile->putChar(',');
            outFile->write(QString::number(iter->mask, 16).toUtf8());
            outFile->putChar(',');
            outFile->write(QString::number(iter->startByte).toUtf8());
            outFile->putChar(',');
            outFile->write(QString::number(iter->endByte).toUtf8());
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
            outFile->write("\n");
        }
        outFile->close();
    }
}

void GraphingWindow::loadDefinitions()
{
    QString filename;
    QFileDialog dialog;

    QStringList filters;
    filters.append(QString(tr("Graph definition (*.gdf)")));

    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
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

                gp.ID = tokens[0].toInt(NULL, 16);
                gp.mask = tokens[1].toULongLong(NULL, 16);
                qDebug() << gp.mask;
                gp.startByte = tokens[2].toInt();
                gp.endByte = tokens[3].toInt();
                if (tokens[4] == "Y") gp.isSigned = true;
                    else gp.isSigned = false;
                gp.bias = tokens[5].toFloat();
                gp.scale = tokens[6].toFloat();
                gp.stride = tokens[7].toInt();
                gp.color.setRed(tokens[8].toInt());
                gp.color.setGreen(tokens[9].toInt());
                gp.color.setBlue(tokens[10].toInt());
                createGraph(gp, true);
            }
        }
        inFile->close();
    }
}

void GraphingWindow::showParamsDialog(int idx = -1)
{
    NewGraphDialog *thisDialog = new NewGraphDialog();
    if (idx > -1) thisDialog->setParams(graphParams[idx]);
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

void GraphingWindow::createGraph(GraphParams &params, bool createGraphParam)
{
    long long tempVal; //64 bit temp value.
    float yminval=10000000.0, ymaxval = -1000000.0;
    float xminval=10000000000.0, xmaxval = -10000000000.0;

    qDebug() << "New Graph ID: " << params.ID;
    qDebug() << "Start byte: " << params.startByte;
    qDebug() << "End Byte: " << params.endByte;
    qDebug() << "Mask: " << params.mask;

    frameCache.clear();
    for (int i = 0; i < modelFrames->count(); i++)
    {
        CANFrame thisFrame = modelFrames->at(i);
        if (thisFrame.ID == params.ID) frameCache.append(thisFrame);
    }

    int numEntries = frameCache.count() / params.stride;

    QVector<double> x(numEntries), y(numEntries);

    if (params.endByte == -1  || params.startByte == params.endByte)
    {
        for (int j = 0; j < numEntries; j++)
        {
            tempVal = (frameCache[j * params.stride].data[params.startByte] & params.mask);
            if (params.isSigned && tempVal > 127)
            {
                tempVal = tempVal - 256;
            }
            if (secondsMode)
            {
                x[j] = (double)(frameCache[j].timestamp) / 1000000.0;
            }
            else
            {
                x[j] = frameCache[j].timestamp;
            }
            y[j] = (tempVal + params.bias) * params.scale;
            if (y[j] < yminval) yminval = y[j];
            if (y[j] > ymaxval) ymaxval = y[j];
            if (x[j] < xminval) xminval = x[j];
            if (x[j] > xmaxval) xmaxval = x[j];

        }
    }
    else if (params.endByte > params.startByte) //big endian
    {
        float tempValue;
        long long tempValInt;
        int numBytes = (params.endByte - params.startByte) + 1;
        long long shiftRef = 1 << (numBytes * 8);
        uint64_t maskShifter;
        uint8_t tempByte;
        for (int j = 0; j < numEntries; j++)
        {
            tempValInt = 0;
            long long expon = 1;
            maskShifter = params.mask;
            for (int c = 0; c < numBytes; c++)
            {
                tempByte = frameCache[j * params.stride].data[params.endByte - c];
                tempByte &= maskShifter;
                tempValInt += (tempByte * expon);
                expon *= 256;
                maskShifter = maskShifter >> 8;
            }

            tempValInt &= params.mask;

            long long twocompPoint = params.mask;
            if (shiftRef < twocompPoint) twocompPoint = shiftRef;
            if (params.isSigned && tempValInt > ((twocompPoint / 2) - 1))
            {
                tempValInt = tempValInt - twocompPoint;
            }

            tempValue = (float)tempValInt;

            if (secondsMode)
            {
                x[j] = (double)(frameCache[j].timestamp) / 1000000.0;
            }
            else
            {
                x[j] = frameCache[j].timestamp;
            }

            y[j] = (tempValue + params.bias) * params.scale;
            if (y[j] < yminval) yminval = y[j];
            if (y[j] > ymaxval) ymaxval = y[j];
            if (x[j] < xminval) xminval = x[j];
            if (x[j] > xmaxval) xmaxval = x[j];
        }
    }
    else //little endian
    {
        float tempValue;
        long long tempValInt;
        int numBytes = (params.startByte - params.endByte) + 1;
        long long shiftRef = 1 << (numBytes * 8);
        uint64_t maskShifter;
        uint8_t tempByte;
        for (int j = 0; j < numEntries; j++)
        {
            tempValInt = 0;
            long long expon = 1;
            maskShifter = params.mask;
            for (int c = 0; c < numBytes; c++)
            {
                tempByte = frameCache[j * params.stride].data[params.endByte + c];
                tempByte &= maskShifter;
                tempValInt += tempByte * expon;
                expon *= 256;
                maskShifter = maskShifter >> 8;
            }
            tempValInt &= params.mask;

            long long twocompPoint = params.mask;
            if (shiftRef < twocompPoint) twocompPoint = shiftRef;
            if (params.isSigned && tempValInt > ((twocompPoint / 2) - 1))
            {
                tempValInt = tempValInt - twocompPoint;
            }

            tempValue = (float)tempValInt;

            if (secondsMode)
            {
                x[j] = (double)(frameCache[j].timestamp) / 1000000.0;
            }
            else
            {
                x[j] = frameCache[j].timestamp;
            }

            y[j] = (tempValue + params.bias) * params.scale;
            if (y[j] < yminval) yminval = y[j];
            if (y[j] > ymaxval) ymaxval = y[j];
            if (x[j] < xminval) xminval = x[j];
            if (x[j] > xmaxval) xmaxval = x[j];
        }
    }

    for (int ct = 0; ct < x.count(); ct++)
    {
        x[ct] -= xminval;
    }
    xmaxval -= xminval;
    xminval = 0;

    ui->graphingView->addGraph();
    params.ref = ui->graphingView->graph();
    if (createGraphParam) graphParams.append(params);
    ui->graphingView->graph()->setSelectedBrush(Qt::NoBrush);
    ui->graphingView->graph()->setSelectedPen(selectedPen);

    QString gName = "0x"+ QString::number(params.ID, 16) + ":" + QString::number(params.startByte);
    if ((params.endByte != -1) && (params.endByte != params.startByte)) gName += "-" + QString::number(params.endByte);
    ui->graphingView->graph()->setName(gName);

    ui->graphingView->graph()->setData(x,y);
    ui->graphingView->graph()->setLineStyle(QCPGraph::lsLine); //connect points with lines
    QPen graphPen;
    graphPen.setColor(params.color);
    graphPen.setWidth(1);
    ui->graphingView->graph()->setPen(graphPen);

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
