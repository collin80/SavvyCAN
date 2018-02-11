#include "mainsettingsdialog.h"
#include "ui_mainsettingsdialog.h"
#include "helpwindow.h"

MainSettingsDialog::MainSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainSettingsDialog)
{
    ui->setupUi(this);

    ui->comboSendingBus->addItem(tr("None"));
    ui->comboSendingBus->addItem(tr("0"));
    ui->comboSendingBus->addItem(tr("1"));
    ui->comboSendingBus->addItem(tr("Both"));
    ui->comboSendingBus->addItem(tr("From File"));

    settings = new QSettings();

    //update the GUI with all the settings we have stored giving things
    //defaults if nothing was stored (if this is the first time)
    ui->cbDisplayHex->setChecked(settings->value("Main/UseHex", true).toBool());
    ui->cbFlowAutoRef->setChecked(settings->value("FlowView/AutoRef", false).toBool());
    ui->cbFlowUseTimestamp->setChecked(settings->value("FlowView/UseTimestamp", true).toBool());
    ui->cbInfoAutoExpand->setChecked(settings->value("InfoCompare/AutoExpand", false).toBool());
    ui->cbMainAutoScroll->setChecked(settings->value("Main/AutoScroll", false).toBool());
    ui->cbPlaybackLoop->setChecked(settings->value("Playback/AutoLoop", false).toBool());
    ui->cbRestorePositions->setChecked(settings->value("Main/SaveRestorePositions", true).toBool());
    ui->cbValidate->setChecked(settings->value("Main/ValidateComm", true).toBool());
    ui->spinPlaybackSpeed->setValue(settings->value("Playback/DefSpeed", 5).toInt());
    ui->lineClockFormat->setText(settings->value("Main/TimeFormat", "MMM-dd HH:mm:ss.zzz").toString());
    ui->lineRemoteHost->setText(settings->value("Remote/Host", "api.savvycan.com").toString());
    ui->lineRemotePort->setText(settings->value("Remote/Port", "21315").toString()); // = 0x5343 = SC. Yep, really creative port number
    ui->cbAutoStartRemote->setChecked(settings->value("Remote/AutoStart", false).toBool());

    ui->spinFontSize->setValue(settings->value("Main/FontSize", ui->cbDisplayHex->font().pointSize()).toUInt());

    bool secondsMode = settings->value("Main/TimeSeconds", false).toBool();
    bool clockMode = settings->value("Main/TimeClock", false).toBool();
    if (clockMode)
    {
        ui->rbSeconds->setChecked(false);
        ui->rbMicros->setChecked(false);
        ui->rbSysClock->setChecked(true);
    }
    else
    {
        if (secondsMode)
        {
            ui->rbSeconds->setChecked(true);
            ui->rbMicros->setChecked(false);
            ui->rbSysClock->setChecked(false);
        }
        else
        {
            ui->rbSeconds->setChecked(false);
            ui->rbMicros->setChecked(true);
            ui->rbSysClock->setChecked(false);
        }
    }

    ui->comboSendingBus->setCurrentIndex(settings->value("Playback/SendingBus", 4).toInt());
    ui->cbUseFiltered->setChecked(settings->value("Main/UseFiltered", false).toBool());
    ui->cbUseOpenGL->setChecked(settings->value("Main/UseOpenGL", false).toBool());

    //just for simplicity they all call the same function and that function updates all settings at once
    connect(ui->cbDisplayHex, SIGNAL(toggled(bool)), this, SLOT(updateSettings()));
    connect(ui->cbFlowAutoRef, SIGNAL(toggled(bool)), this, SLOT(updateSettings()));
    connect(ui->cbFlowUseTimestamp, SIGNAL(toggled(bool)), this, SLOT(updateSettings()));
    connect(ui->cbInfoAutoExpand, SIGNAL(toggled(bool)), this, SLOT(updateSettings()));
    connect(ui->cbMainAutoScroll, SIGNAL(toggled(bool)), this, SLOT(updateSettings()));
    connect(ui->cbPlaybackLoop, SIGNAL(toggled(bool)), this, SLOT(updateSettings()));
    connect(ui->cbRestorePositions, SIGNAL(toggled(bool)), this, SLOT(updateSettings()));
    connect(ui->cbValidate, SIGNAL(toggled(bool)), this, SLOT(updateSettings()));
    connect(ui->spinPlaybackSpeed, SIGNAL(valueChanged(int)), this, SLOT(updateSettings()));
    connect(ui->rbSeconds, SIGNAL(toggled(bool)), this, SLOT(updateSettings()));
    connect(ui->rbMicros, SIGNAL(toggled(bool)), this, SLOT(updateSettings()));
    connect(ui->rbSysClock, SIGNAL(toggled(bool)), this, SLOT(updateSettings()));
    connect(ui->comboSendingBus, SIGNAL(currentIndexChanged(int)), this, SLOT(updateSettings()));
    connect(ui->cbUseFiltered, SIGNAL(toggled(bool)), this, SLOT(updateSettings()));
    connect(ui->lineClockFormat, SIGNAL(editingFinished()), this, SLOT(updateSettings()));
    connect(ui->cbUseOpenGL, SIGNAL(toggled(bool)), this, SLOT(updateSettings()));
    connect(ui->cbAutoStartRemote, SIGNAL(toggled(bool)), this, SLOT(updateSettings()));
    connect(ui->lineRemoteHost, SIGNAL(editingFinished()), this, SLOT(updateSettings()));
    connect(ui->lineRemotePort, SIGNAL(editingFinished()), this, SLOT(updateSettings()));
    installEventFilter(this);
}

MainSettingsDialog::~MainSettingsDialog()
{
    delete ui;
    delete settings;
}

void MainSettingsDialog::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    removeEventFilter(this);
    updateSettings();
    settings->sync();
}

bool MainSettingsDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("preferences.html");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}


void MainSettingsDialog::updateSettings()
{
    settings->setValue("Main/UseHex", ui->cbDisplayHex->isChecked());
    settings->setValue("FlowView/AutoRef", ui->cbFlowAutoRef->isChecked());
    settings->setValue("FlowView/UseTimestamp", ui->cbFlowUseTimestamp->isChecked());
    settings->setValue("InfoCompare/AutoExpand", ui->cbInfoAutoExpand->isChecked());
    settings->setValue("Main/AutoScroll", ui->cbMainAutoScroll->isChecked());
    settings->setValue("Playback/AutoLoop", ui->cbPlaybackLoop->isChecked());
    settings->setValue("Main/SaveRestorePositions", ui->cbRestorePositions->isChecked());
    settings->setValue("Main/ValidateComm", ui->cbValidate->isChecked());
    settings->setValue("Playback/DefSpeed", ui->spinPlaybackSpeed->value());
    settings->setValue("Main/TimeSeconds", ui->rbSeconds->isChecked());
    settings->setValue("Main/TimeClock", ui->rbSysClock->isChecked());
    settings->setValue("Playback/SendingBus", ui->comboSendingBus->currentIndex());
    settings->setValue("Main/UseFiltered", ui->cbUseFiltered->isChecked());
    settings->setValue("Main/UseOpenGL", ui->cbUseOpenGL->isChecked());
    settings->setValue("Main/TimeFormat", ui->lineClockFormat->text());
    settings->setValue("Main/FontSize", ui->spinFontSize->value());
    settings->setValue("Remote/Host", ui->lineRemoteHost->text());
    settings->setValue("Remote/Port", ui->lineRemotePort->text());
    settings->setValue("Remote/AutoStart", ui->cbAutoStartRemote->isChecked());

    settings->sync();
    emit updatedSettings();
}
