#include "canbridgewindow.h"
#include "helpwindow.h"
#include "qevent.h"
#include "ui_canbridgewindow.h"
#include "filterutility.h"
#include "mainwindow.h"

CANBridgeWindow::CANBridgeWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CANBridgeWindow)
{
    ui->setupUi(this);

    modelFrames = frames;

    side1BusNum = 0;
    side2BusNum = 0;

    connect(ui->cbSide1, &QComboBox::currentTextChanged, this, &CANBridgeWindow::recalcSides);
    connect(ui->cbSide2, &QComboBox::currentTextChanged, this, &CANBridgeWindow::recalcSides);
    connect(MainWindow::getReference(), &MainWindow::framesUpdated, this, &CANBridgeWindow::updatedFrames);
    connect(ui->listSide1, &QListWidget::itemChanged,
        [this] (QListWidgetItem *item)
        {
            bool checked = item->checkState() == Qt::Checked ? true:false;
            int IDval = FilterUtility::getIdAsInt(item);
            qDebug() << "ID " << IDval << " set to " << checked;
            foundIDSide1[IDval] = checked;
        });
    connect(ui->listSide2, &QListWidget::itemChanged,
        [this] (QListWidgetItem *item)
        {
            bool checked = item->checkState() == Qt::Checked ? true:false;
            int IDval = FilterUtility::getIdAsInt(item);
            foundIDSide2[IDval] = checked;
        });
}

CANBridgeWindow::~CANBridgeWindow()
{
    delete ui;
}


void CANBridgeWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    //stuff to do every time this form is shown.
    ui->cbSide1->clear();
    ui->cbSide2->clear();
    int numBuses = CANConManager::getInstance()->getNumBuses();
    for (int i = 0; i < numBuses; i++)
    {
        ui->cbSide1->addItem(QString::number(i));
        ui->cbSide2->addItem(QString::number(i));
    }
}

bool CANBridgeWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("canbridge.md");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
}

void CANBridgeWindow::recalcSides()
{
    side1BusNum = ui->cbSide1->currentText().toInt();
    side2BusNum = ui->cbSide2->currentText().toInt();
}


void CANBridgeWindow::updatedFrames(int numFrames)
{
    bool addedSide1 = false;
    bool addedSide2 = false;

    if (numFrames == -1) //all frames deleted. Kill the display
    {
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
    }
    else //just got some new frames. See if they are relevant.
    {
        if (numFrames > modelFrames->count()) return;

        for (int x = modelFrames->count() - numFrames; x < modelFrames->count(); x++)
        {
            CANFrame thisFrame = modelFrames->at(x);
            int32_t id = static_cast<int32_t>(thisFrame.frameId());

            if (thisFrame.bus == side1BusNum)
            {
                if  (!foundIDSide1.contains(id))
                {
                    foundIDSide1.insert(id, true);
                    FilterUtility::createCheckableFilterItem(id, true, ui->listSide1);
                    addedSide1 = true;
                }
                if (ui->ckEnableSide1->isChecked()) //if we're enabled to forward traffic on this bus to the other one
                {
                    if (foundIDSide1[id]) //and the checkbox for this particular ID is checked
                    {
                        thisFrame.bus = side2BusNum;
                        CANConManager::getInstance()->sendFrame(thisFrame);
                    }
                }
            }
            else if (thisFrame.bus == side2BusNum)
            {
                if  (!foundIDSide2.contains(id))
                {
                    foundIDSide2.insert(id, true);
                    FilterUtility::createCheckableFilterItem(id, true, ui->listSide2);
                    addedSide2 = true;
                }
                if (ui->ckEnableSide2->isChecked()) //if we're enabled to forward traffic on this bus to the other one
                {
                    if (foundIDSide2[id]) //and the checkbox for this particular ID is checked
                    {
                        thisFrame.bus = side1BusNum;
                        CANConManager::getInstance()->sendFrame(thisFrame);
                    }
                }
            }
        }
        //default is to sort in ascending order
        if (addedSide1) ui->listSide1->sortItems();
        if (addedSide2) ui->listSide2->sortItems();
    }
}

