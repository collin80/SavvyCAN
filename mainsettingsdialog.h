#ifndef MAINSETTINGSDIALOG_H
#define MAINSETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>

namespace Ui {
class MainSettingsDialog;
}

class MainSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MainSettingsDialog(QWidget *parent = 0);
    ~MainSettingsDialog();

private slots:
    void updateSettings();

private:
    Ui::MainSettingsDialog *ui;
    QSettings *settings;

    void closeEvent(QCloseEvent *event);
};

#endif // MAINSETTINGSDIALOG_H
