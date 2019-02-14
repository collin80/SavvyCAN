#include <QDebug>
#include <QListWidgetItem>
#include "snifferwindow.h"
#include "ui_snifferwindow.h"
#include "helpwindow.h"
#include "connections/canconmanager.h"
#include "SnifferDelegate.h"

SnifferWindow::SnifferWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::snifferWindow),
    mModel(this),
    mTimer(this),
    mFilter(false)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);
    ui->treeView->setModel(&mModel);

    /* set column width */
    ui->treeView->setColumnWidth(tc::ID, 80);
    ui->treeView->setColumnWidth(tc::LAST, 1);
    for(int i=tc::DATA_0 ; i<=tc::DATA_7 ; i++)
        ui->treeView->setColumnWidth(i, 92);
    ui->treeView->setUniformRowHeights(true);
    ui->treeView->header()->setDefaultAlignment(Qt::AlignCenter);
    ui->treeView->setItemDelegate(new SnifferDelegate());

    /* activate sorting */
    ui->listWidget->setSortingEnabled(true);

    /* connect timer */
    connect(&mTimer, &QTimer::timeout, this, &SnifferWindow::update);
    mTimer.setInterval(200);

    /* connect buttons */
    connect(ui->btNotch, &QPushButton::clicked, &mModel, &SnifferModel::notch);
    connect(ui->btUnNotch, &QPushButton::clicked, &mModel, &SnifferModel::unNotch);
    connect(ui->btAll, &QPushButton::clicked, this, &SnifferWindow::fltAll);
    connect(ui->btNone, &QPushButton::clicked, this, &SnifferWindow::fltNone);
    connect(&mModel, &SnifferModel::idChange, this, &SnifferWindow::idChange);
    connect(ui->listWidget, &QListWidget::itemChanged, this, &SnifferWindow::itemChanged);

    connect(ui->cbFadeInactive, &QCheckBox::stateChanged, this, [this](int val){mModel.setFadeInactive(val);});
    connect(ui->cbMuteNotched, &QCheckBox::stateChanged, this, [this](int val){mModel.setMuteNotched(val);});
    connect(ui->cbNoExpire, &QCheckBox::stateChanged, this, [this](int val){mModel.setNeverExpire(val);});
}

SnifferWindow::~SnifferWindow()
{
    closeEvent(NULL);
    delete ui;
}


void SnifferWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    connect(CANConManager::getInstance(), &CANConManager::framesReceived, &mModel, &SnifferModel::update);
    mTimer.start();
    installEventFilter(this);
    qDebug() << "show";
}


void SnifferWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    /* stop timer */
    mTimer.stop();
    /* disconnect reception of frames */
    disconnect(CANConManager::getInstance(), 0, this, 0);
    /* clear model */
    mModel.clear();
    /* clean list */
    qDeleteAll(mMap);
    mMap.clear();
    /* reset filtering */
    mFilter = false;
    removeEventFilter(this);
}

bool SnifferWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("sniffer.html");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void SnifferWindow::update()
{
    mModel.refresh();
}


void SnifferWindow::fltAll()
{
    filter(false);
}


void SnifferWindow::fltNone()
{
    filter(true);
}

void SnifferWindow::filter(bool pFilter)
{
    mFilter = pFilter;
    mModel.filter(mFilter ? fltType::NONE : fltType::ALL);

    foreach(QListWidgetItem* item, mMap)
        item->setCheckState(mFilter ? Qt::Unchecked : Qt::Checked);
}

void SnifferWindow::idChange(int pId, bool pAdd)
{
    QListWidgetItem* item;

    if(pAdd)
    {
        QString text = QString("0x") + QString("%1").arg(pId, 3, 16, QLatin1Char('0')).toUpper();
        item = new QListWidgetItem(text);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        //item->setCheckState(mFilter ? Qt::Unchecked : Qt::Checked);
        //always check new IDs now. Otherwise any that expire then come back will not be selected
        //and that might be a bigger issue than defaulting them unselected.
        item->setCheckState(Qt::Checked);
        ui->listWidget->addItem(item);
        mMap[pId] = item;
    }
    else
    {
        item = mMap.take(pId);
        ui->listWidget->removeItemWidget(item);
        delete item;
    }
}


void SnifferWindow::itemChanged(QListWidgetItem * item)
{
    bool checked = (Qt::Checked == item->checkState());

    if( !mFilter && checked )
        return;

    mModel.filter(checked ? fltType::ADD : fltType::REMOVE, mMap.key(item));
    mFilter = true;
}
