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

signals:
    void updatedSettings();

public slots:
    void updateSettings();

private:
    Ui::MainSettingsDialog *ui;

    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
};

#endif // MAINSETTINGSDIALOG_H
