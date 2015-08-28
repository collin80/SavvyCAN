#include "discretestatewindow.h"
#include "ui_discretestatewindow.h"
#include "mainwindow.h"

DiscreteStateWindow::DiscreteStateWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DiscreteStateWindow)
{
    ui->setupUi(this);

    modelFrames = frames;
    operatingState = DWStates::IDLE;

    timer = new QTimer();
    timer->setInterval(100);

    connect(ui->btnStart, SIGNAL(clicked(bool)), this, SLOT(handleStartButton()));
    connect(timer, SIGNAL(timeout()), this, SLOT(handleTick()));
    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
}

DiscreteStateWindow::~DiscreteStateWindow()
{
    timer->stop();

    for (int i = 0; i < stateFrames.count(); i++)
    {
        stateFrames[i]->clear();
        delete(stateFrames[i]);
    }

    delete timer;
    delete ui;
}

void DiscreteStateWindow::updatedFrames(int numFrames)
{
    CANFrame thisFrame;
    if (numFrames == -1) //all frames deleted. Kill the display
    {
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
    }
    else //just got some new frames. See if they are relevant.
    {
        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);
            if (operatingState == DWStates::IDLE) stateFrames[0]->append(thisFrame);
            else stateFrames[currToggleState + 1]->append(thisFrame);
        }
    }
}

void DiscreteStateWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    readSettings();

    qDebug() << "DiscreteState show event was processed";
}

void DiscreteStateWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
}

void DiscreteStateWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("DiscreteState/WindowSize", QSize(400, 300)).toSize());
        move(settings.value("DiscreteState/WindowPos", QPoint(50, 50)).toPoint());
    }

}

void DiscreteStateWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("DiscreteState/WindowSize", size());
        settings.setValue("DiscreteState/WindowPos", pos());
    }
}

void DiscreteStateWindow::updateStateLabel()
{
    QPalette pal;
    switch(operatingState)
    {
    case DWStates::IDLE:
        ui->lblStatus->setText("WAIT");
        pal = ui->lblStatus->palette();
        pal.setColor(QPalette::WindowText, Qt::red);
        ui->lblStatus->setPalette(pal);
        break;
    case DWStates::COUNTDOWN_SIGNAL:
        ui->lblStatus->setText(QString::number(ticksUntilStateChange * 0.1, 'f', 1) + "...");
        pal = ui->lblStatus->palette();
        pal.setColor(QPalette::WindowText, Qt::blue);
        ui->lblStatus->setPalette(pal);
        break;
    case DWStates::COUNTDOWN_WAITING:
        ui->lblStatus->setText("Return to resting state");
        pal = ui->lblStatus->palette();
        pal.setColor(QPalette::WindowText, Qt::red);
        ui->lblStatus->setPalette(pal);
        break;
    case DWStates::GETTING_SIGNAL:
        ui->lblStatus->setText("Go to state " + QString::number(currToggleState + 1));
        pal = ui->lblStatus->palette();
        pal.setColor(QPalette::WindowText, Qt::green);
        ui->lblStatus->setPalette(pal);
        break;
    }
}

void DiscreteStateWindow::handleStartButton()
{
    operatingState = DWStates::COUNTDOWN_SIGNAL;

    ticksPerStateChange = ticksUntilStateChange = ui->spinFreq->value() * 10;
    numToggleStates = ui->spinStates->value();
    numIterations = ui->spinIterations->value();

    currToggleState = 0;
    currIteration = 0;

    for (int i = 0; i < stateFrames.count(); i++)
    {
        stateFrames[i]->clear();
        delete(stateFrames[i]);
    }

    for (int j = 0; j <= numToggleStates; j++)
    {
        stateFrames.append(new QVector<CANFrame>());
    }

    timer->start();
}

void DiscreteStateWindow::handleTick()
{
    switch (operatingState)
    {
    case DWStates::IDLE:
        break;
    case DWStates::COUNTDOWN_SIGNAL:
        ticksUntilStateChange--;
        if (ticksUntilStateChange == 0)
        {
            ticksUntilStateChange = ticksPerStateChange;
            operatingState = DWStates::GETTING_SIGNAL;
        }
        break;
    case DWStates::COUNTDOWN_WAITING:
        ticksUntilStateChange--;
        if (ticksUntilStateChange == 0)
        {
            ticksUntilStateChange = ticksPerStateChange;
            currIteration++;
            if (currIteration == numIterations)
            {
                operatingState = DWStates::IDLE;
                timer->stop();
                //call to calculate our findings here.
            }
            else operatingState = DWStates::COUNTDOWN_SIGNAL;
        }
        break;
    case DWStates::GETTING_SIGNAL:
        ticksUntilStateChange--;
        if (ticksUntilStateChange == 0)
        {
            ticksUntilStateChange = ticksPerStateChange;
            operatingState = DWStates::COUNTDOWN_WAITING;
            currToggleState++;
            if (currToggleState == numToggleStates) currToggleState = 0;
        }
        break;
    }
    updateStateLabel();
}

void DiscreteStateWindow::calculateResults()
{

}
