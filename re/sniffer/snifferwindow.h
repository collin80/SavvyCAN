#ifndef SNIFFER_H
#define SNIFFER_H

#include <QDialog>
#include <QListWidgetItem>
#include "sniffermodel.h"

namespace Ui {
class snifferWindow;
}

enum tc
{
    DELTA = 0,
    ID,
    DATA_0,
    DATA_1,
    DATA_2,
    DATA_3,
    DATA_4,
    DATA_5,
    DATA_6,
    DATA_7,
    LAST
};


class SnifferWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SnifferWindow(QWidget *parent = 0);
    ~SnifferWindow();

    void showEvent(QShowEvent*);
    void closeEvent(QCloseEvent*);

public slots:
    void update();
    void idChange(int, bool);
    void fltAll();
    void fltNone();
    void itemChanged(QListWidgetItem*);

private:
    void filter(bool pFilter);

    Ui::snifferWindow*          ui;
    SnifferModel                mModel;
    QTimer                      mTimer;
    QMap<int, QListWidgetItem*> mMap;
    bool                        mFilter;
};

#endif // SNIFFER_H
