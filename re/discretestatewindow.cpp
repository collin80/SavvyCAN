#include "discretestatewindow.h"
#include "ui_discretestatewindow.h"
#include "mainwindow.h"
#include "helpwindow.h"

DiscreteStateWindow::DiscreteStateWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DiscreteStateWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    modelFrames = frames;
    operatingState = DWStates::IDLE;

    timer = new QTimer();
    timer->setInterval(100);

    isRealtime = ui->rbRealtime->isChecked();
    typeChanged();

    connect(ui->btnStart, SIGNAL(clicked(bool)), this, SLOT(handleStartButton()));
    connect(timer, SIGNAL(timeout()), this, SLOT(handleTick()));
    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    connect(ui->rbLogged, SIGNAL(clicked(bool)), this, SLOT(typeChanged()));
    connect(ui->rbRealtime, SIGNAL(clicked(bool)), this, SLOT(typeChanged()));

    connect(ui->btnAll, &QAbstractButton::clicked,
            [=]()
            {
                for (int i = 0; i < ui->listID->count(); i++)
                {
                    QListWidgetItem *item = ui->listID->item(i);
                    item->setCheckState(Qt::Checked);
                    idFilters[Utility::ParseStringToNum(item->text())] = true;
                }
            });

    connect(ui->btnNone, &QAbstractButton::clicked,
            [=]()
            {
                for (int i = 0; i < ui->listID->count(); i++)
                {
                    QListWidgetItem *item = ui->listID->item(i);
                    item->setCheckState(Qt::Unchecked);
                    idFilters[Utility::ParseStringToNum(item->text())] = false;
                }
            });

    connect(ui->listID, &QListWidget::itemChanged,
            [=](QListWidgetItem *item)
            {
                bool isChecked = false;
                int id = Utility::ParseStringToNum(item->text());
                if (item->checkState() == Qt::Checked) isChecked = true;
                idFilters[id] = isChecked;
            });

    refreshFilterList();
    installEventFilter(this);
}

DiscreteStateWindow::~DiscreteStateWindow()
{
    removeEventFilter(this);
    timer->stop();

    for (int i = 0; i < stateFrames.count(); i++)
    {
        stateFrames[i]->clear();
        delete(stateFrames[i]);
    }

    delete timer;
    delete ui;
}

bool DiscreteStateWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("discretestate.md");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void DiscreteStateWindow::typeChanged()
{
    if (ui->rbLogged->isChecked())
    {
        ui->spinFreq->setEnabled(false);
        ui->spinIterations->setEnabled(false);
        ui->lblStatus->setEnabled(false);
        ui->spinMaxBits->setEnabled(true);
        ui->spinMinBits->setEnabled(true);
        ui->listID->setEnabled(true);
        ui->btnAll->setEnabled(true);
        ui->btnNone->setEnabled(true);
        isRealtime = false;
    }
    else
    {
        ui->spinFreq->setEnabled(true);
        ui->spinIterations->setEnabled(true);
        ui->lblStatus->setEnabled(true);
        ui->spinMaxBits->setEnabled(false);
        ui->spinMinBits->setEnabled(false);
        ui->listID->setEnabled(false);
        ui->btnAll->setEnabled(false);
        ui->btnNone->setEnabled(false);
        isRealtime = true;
    }
}

void DiscreteStateWindow::updatedFrames(int numFrames)
{
    CANFrame thisFrame;
    if (numFrames == -1) //all frames deleted. Kill the display
    {
        ui->listID->clear();
        idFilters.clear();
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        refreshFilterList();
    }
    else //just got some new frames. See if they are relevant.
    {
        if (numFrames > modelFrames->count()) return;
        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);

            if (!idFilters.contains(thisFrame.frameId()))
            {
                idFilters.insert(thisFrame.frameId(), true);
                QListWidgetItem* listItem = new QListWidgetItem(Utility::formatCANID(thisFrame.frameId(), thisFrame.hasExtendedFrameFormat()), ui->listID);
                listItem->setFlags(listItem->flags() | Qt::ItemIsUserCheckable); // set checkable flag
                listItem->setCheckState(Qt::Checked); //default all filters to be set active
            }

            //if (operatingState == DWStates::IDLE) stateFrames[0]->append(thisFrame);
            //else stateFrames[currToggleState + 1]->append(thisFrame);
        }
    }
}

void DiscreteStateWindow::refreshFilterList()
{
    int id;

    idFilters.clear();
    ui->listID->clear();

    for (int i = 0; i < modelFrames->length(); i++)
    {
        CANFrame thisFrame = modelFrames->at(i);
        id = thisFrame.frameId();
        if (!idFilters.contains(id))
        {
            idFilters.insert(id, true);
            QListWidgetItem* listItem = new QListWidgetItem(Utility::formatCANID(id, thisFrame.hasExtendedFrameFormat()), ui->listID);
            listItem->setFlags(listItem->flags() | Qt::ItemIsUserCheckable); // set checkable flag
            listItem->setCheckState(Qt::Checked); //default all filters to be set active
        }
    }

    ui->listID->sortItems();
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
        move(Utility::constrainedWindowPos(settings.value("DiscreteState/WindowPos", QPoint(50, 50)).toPoint()));
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
        ui->lblStatus->setText("IDLE");
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
        ui->lblStatus->setText("Wait....");
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
    case DWStates::DONE:
        ui->lblStatus->setText("DONE");
        pal = ui->lblStatus->palette();
        pal.setColor(QPalette::WindowText, Qt::green);
        ui->lblStatus->setPalette(pal);
        break;
    }
}

void DiscreteStateWindow::handleStartButton()
{
    if (isRealtime)
    {
        operatingState = DWStates::COUNTDOWN_SIGNAL;

        ticksPerStateChange = ticksUntilStateChange = ui->spinFreq->value() * 10;
        numToggleStates = ui->spinStates->value();
        numIterations = ui->spinIterations->value();

        currToggleState = 0;
        currIteration = 0;

        for (int i = stateFrames.count() - 1; i > 0; i--)
        {
            stateFrames[i]->clear();
            stateFrames.removeAt(i);
        }

        for (int j = 0; j <= numToggleStates; j++)
        {
            stateFrames.append(new QVector<CANFrame>());
        }

        timer->start();
    }
    else
    {
        calculateResults();
    }
}

void DiscreteStateWindow::handleTick()
{
    switch (operatingState)
    {
    case DWStates::IDLE:
        break;
    case DWStates::DONE:
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
            if (currIteration > numIterations)
            {
                operatingState = DWStates::IDLE;
                timer->stop();
                calculateResults();
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
            if (currToggleState > numToggleStates) currToggleState = 0;
        }
        break;
    }
    updateStateLabel();
}

void DiscreteStateWindow::calculateResults()
{
    int minBits, maxBits;
    if (isRealtime)
    {

    }
    else //use already loaded frames from main cache
    {
        //basic overview: run through all ID filters and see if it is enabled.
        //If so add it to a giant list of messages by ID where each ID has its own
        //list of messages
        //Then, for each ID start a loop that runs from largest bits to smallest bits for scan
        //For each value run through the algorithm for all messages. At the end list any matches
        //the simplest approach seems to be to grab that number of bits and then record every
        //unique value. IF the # of unique values is the same as the number of states then
        //we've got a match.It should be noted that the # of states must be at least 2 - the idle
        //state is 1 and then a second state at the minimum. Turn signals might be 3 states then

        minBits = ui->spinMinBits->value();
        maxBits = ui->spinMaxBits->value();
        QHash<int, bool>::const_iterator it;
        QList<CANFrame> frameCache;
        for (it = idFilters.begin(); it != idFilters.end(); ++it)
        {
            if (it.value())
            {
                frameCache.clear();
                for (int i = 0; i < modelFrames->count(); i++)
                {
                    if (modelFrames->at(i).frameId() == (unsigned int)it.key()) frameCache.append(modelFrames->at(i));
                }
                for (int bits = maxBits; bits >= minBits; bits--)
                {
                    QList<int> values;
                }
            }
        }
    }
}
