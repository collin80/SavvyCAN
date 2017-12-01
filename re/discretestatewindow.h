#ifndef DISCRETESTATEWINDOW_H
#define DISCRETESTATEWINDOW_H

#include <QDialog>
#include <QTimer>
#include "can_structs.h"

namespace Ui {
class DiscreteStateWindow;
}

namespace DWStates {
enum DiscreteWindowState
{
    IDLE,
    COUNTDOWN_SIGNAL,
    COUNTDOWN_WAITING,
    GETTING_SIGNAL,
    DONE
};
}

using namespace DWStates;
class DiscreteStateWindow : public QDialog
{
    Q_OBJECT

public:
    explicit DiscreteStateWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    ~DiscreteStateWindow();
    void showEvent(QShowEvent*);

private slots:
    void updatedFrames(int);
    void handleStartButton();
    void handleTick();
    void typeChanged();

private:
    Ui::DiscreteStateWindow *ui;
    const QVector<CANFrame> *modelFrames;
    QList< QVector<CANFrame> *> stateFrames;
    QTimer *timer;
    DiscreteWindowState operatingState;
    int ticksUntilStateChange;
    int ticksPerStateChange;
    int numToggleStates;
    int currToggleState;
    int numIterations;
    int currIteration;
    bool isRealtime;
    QHash<int, bool> idFilters;

    void refreshFilterList();
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    void readSettings();
    void writeSettings();
    void updateStateLabel();
    void calculateResults();
};

#endif // DISCRETESTATEWINDOW_H
