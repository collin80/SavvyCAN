#ifndef SNIFFER_H
#define SNIFFER_H

#include <QDialog>
#include <QListWidgetItem>
#include "sniffermodel.h"
#include "SnifferDelegate.h"

namespace Ui {
class snifferWindow;
}

enum tc
{
    DELTA = 0,
    FREQUENCY,
    ID,
    DATA_0,
    DATA_1,
    DATA_2,
    DATA_3,
    DATA_4,
    DATA_5,
    DATA_6,
    DATA_7,
    DATA_8,
    DATA_9,
    DATA_10,
    DATA_11,
    DATA_12,
    DATA_13,
    DATA_14,
    DATA_15,
    DATA_16,
    DATA_17,
    DATA_18,
    DATA_19,
    DATA_20,
    DATA_21,
    DATA_22,
    DATA_23,
    DATA_24,
    DATA_25,
    DATA_26,
    DATA_27,
    DATA_28,
    DATA_29,
    DATA_30,
    DATA_31,
    DATA_32,
    DATA_33,
    DATA_34,
    DATA_35,
    DATA_36,
    DATA_37,
    DATA_38,
    DATA_39,
    DATA_40,
    DATA_41,
    DATA_42,
    DATA_43,
    DATA_44,
    DATA_45,
    DATA_46,
    DATA_47,
    DATA_48,
    DATA_49,
    DATA_50,
    DATA_51,
    DATA_52,
    DATA_53,
    DATA_54,
    DATA_55,
    DATA_56,
    DATA_57,
    DATA_58,
    DATA_59,
    DATA_60,
    DATA_61,
    DATA_62,
    DATA_63,
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
    void notchTick();
    void idChange(int, bool);
    void fltAll();
    void fltNone();
    void itemChanged(QListWidgetItem*);

private:
    void filter(bool pFilter);
    bool eventFilter(QObject *obj, QEvent *event);
    void readSettings();
    void writeSettings();

    Ui::snifferWindow*          ui;
    SnifferModel                mModel;
    QTimer                      mGUITimer;
    QTimer                      mNotchTimer;
    QMap<int, QListWidgetItem*> mMap;
    bool                        mFilter;
    SnifferDelegate             *sniffDel;
    QAbstractItemDelegate       *defaultDel;
    bool                        notchPingPong;
};

#endif // SNIFFER_H
