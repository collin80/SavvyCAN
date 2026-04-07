#include <QDebug>
#include <QListWidgetItem>
#include <qevent.h>
#include "snifferwindow.h"
#include "ui_snifferwindow.h"
#include "helpwindow.h"
#include "connections/canconmanager.h"
#include "SnifferDelegate.h"
#include "utility.h"

SnifferWindow::SnifferWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::snifferWindow),
    mModel(this),
    mGUITimer(this),
    mFilter(false)
{

    inhibitSectionResizeEvent = false;

    ui->setupUi(this);
    setWindowFlags(Qt::Window);
    ui->treeView->setModel(&mModel);

    sniffDel = new SnifferDelegate();
    defaultDel = ui->treeView->itemDelegate();

    /* set column width */
    ui->treeView->setColumnWidth(tc::ID, 80);
    ui->treeView->setColumnWidth(tc::LAST, 1);
    for(int i=tc::DATA_0 ; i<=tc::LAST ; i++)
        ui->treeView->setColumnWidth(i, 50);
    ui->treeView->setUniformRowHeights(true);
    ui->treeView->header()->setDefaultAlignment(Qt::AlignCenter);
    //ui->treeView->setItemDelegate(new SnifferDelegate());

    QHeaderView *header = ui->treeView->header();
    connect(header, &QHeaderView::sectionResized, this, &SnifferWindow::sectionResized);

    /* activate sorting */
    ui->listWidget->setSortingEnabled(true);

    /* connect timer */
    connect(&mGUITimer, &QTimer::timeout, this, &SnifferWindow::update);
    mGUITimer.setInterval(200);
    connect(&mNotchTimer, &QTimer::timeout,this, &SnifferWindow::notchTick);
    mNotchTimer.setInterval(ui->spinNotchInterval->value());

    notchPingPong = false;

    /* connect buttons */
    connect(ui->btNotch, &QPushButton::clicked, &mModel, &SnifferModel::notch);
    connect(ui->btUnNotch, &QPushButton::clicked, &mModel, &SnifferModel::unNotch);
    connect(ui->btAll, &QPushButton::clicked, this, &SnifferWindow::fltAll);
    connect(ui->btNone, &QPushButton::clicked, this, &SnifferWindow::fltNone);
    connect(&mModel, &SnifferModel::idChange, this, &SnifferWindow::idChange);
    connect(ui->listWidget, &QListWidget::itemChanged, this, &SnifferWindow::itemChanged);

    connect(ui->cbFadeInactive, &QCheckBox::checkStateChanged, this, [this](int val){mModel.setFadeInactive(val);sniffDel->setFadeInactive(val);});
    connect(ui->cbMuteNotched, &QCheckBox::checkStateChanged, this, [this](int val){mModel.setMuteNotched(val);});
    connect(ui->cbNoExpire, &QCheckBox::checkStateChanged, this, [this](int val){mModel.setNeverExpire(val);});
    connect(ui->cbViewBits, &QCheckBox::checkStateChanged, this,
            [this](int val)
            {
                if (val) ui->treeView->setItemDelegate(sniffDel);
                else
                {
                    ui->treeView->setItemDelegate(defaultDel);
                }
            }
    );
    connect(ui->spinNotchInterval, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int i){mNotchTimer.setInterval(i);});
    connect(ui->spinExpireInterval, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int i){mModel.setExpireInterval(i);});
}

SnifferWindow::~SnifferWindow()
{
    closeEvent(nullptr);
    delete sniffDel;
    delete ui;
}

void SnifferWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("Sniffer/WindowSize", QSize(1100, 750)).toSize());
        move(Utility::constrainedWindowPos(settings.value("Sniffer/WindowPos", QPoint(50, 50)).toPoint()));
        ui->treeView->setColumnWidth(0, settings.value("Sniffer/DeltaColumn", 110).toUInt());
        ui->treeView->setColumnWidth(1, settings.value("Sniffer/FrequencyColumn", 110).toUInt());
        ui->treeView->setColumnWidth(2, settings.value("Sniffer/IDColumn", 70).toUInt());
        if (!settings.value("Main/EqualDataSniff", false).toBool())
        {
            for (int i = 0; i < MAX_BYTES; i++)
            {
                QString key = "Sniffer/Data" + QString::number(i).trimmed() + "Column";
                ui->treeView->setColumnWidth(3+i, settings.value(key, 50).toUInt());
            }
        }
        else //set all columns equal to column Data0 size
        {
            int colSize = settings.value("Sniffer/Data0Column", 50).toUInt();
            for (int i = 0; i < MAX_BYTES; i++)
            {
                ui->treeView->setColumnWidth(3+i, colSize);
            }
        }
    }
}

void SnifferWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("Sniffer/WindowSize", size());
        settings.setValue("Sniffer/WindowPos", pos());
        settings.setValue("Sniffer/DeltaColumn", ui->treeView->columnWidth(0));
        settings.setValue("Sniffer/FrequencyColumn", ui->treeView->columnWidth(1));
        settings.setValue("Sniffer/IDColumn", ui->treeView->columnWidth(2));

        for (int i = 0; i < MAX_BYTES; i++)
        {
            QString key = "Sniffer/Data" + QString::number(i).trimmed() + "Column";
            settings.setValue(key, ui->treeView->columnWidth(i+3));
        }
    }
}

void SnifferWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    connect(CANConManager::getInstance(), &CANConManager::framesReceived, &mModel, &SnifferModel::update);
    mGUITimer.start();
    mNotchTimer.start();
    readSettings();
    installEventFilter(this);
    qDebug() << "show";
}


void SnifferWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    /* stop timer */
    mGUITimer.stop();
    /* disconnect reception of frames */
    disconnect(CANConManager::getInstance(), 0, this, 0);
    writeSettings();
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
            HelpWindow::getRef()->showHelp("sniffer.md");
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

void SnifferWindow::sectionResized(int idx, int oldSize, int newSize)
{
    QSettings settings;

    //nothing to do if we're not enforcing equal column sizes
    if (!settings.value("Main/EqualDataSniff", false).toBool()) return;
    if (inhibitSectionResizeEvent) return;

    //we're enforcing equal sizes so change the size of all columns to match.
    inhibitSectionResizeEvent = true;
    for (int i = 0; i < MAX_BYTES; i++)
    {
        ui->treeView->setColumnWidth(3+i, newSize);
    }
    inhibitSectionResizeEvent = false;
}

void SnifferWindow::notchTick()
{
    notchPingPong = !notchPingPong;
    if (notchPingPong)
    {
        ui->lblNotch->setBackgroundRole(QPalette::Link);
        ui->lblNotch->repaint();
        //qDebug() << "Tick";
    }
    else
    {
        ui->lblNotch->setBackgroundRole(QPalette::Window);
        ui->lblNotch->repaint();
        //qDebug() << "Tock";
    }
    mModel.updateNotchPoint();
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
