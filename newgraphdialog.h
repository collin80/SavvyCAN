#ifndef NEWGRAPHDIALOG_H
#define NEWGRAPHDIALOG_H

#include <QDialog>
#include "graphingwindow.h"

namespace Ui {
class NewGraphDialog;
}

class NewGraphDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewGraphDialog(QWidget *parent = 0);
    ~NewGraphDialog();
    void getParams(GraphParams &);

private slots:
    void addButtonClicked();
    void colorSwatchClick();

private:
    Ui::NewGraphDialog *ui;
};

#endif // NEWGRAPHDIALOG_H
