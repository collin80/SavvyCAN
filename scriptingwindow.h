#ifndef SCRIPTINGWINDOW_H
#define SCRIPTINGWINDOW_H

#include "can_structs.h"
#include "scriptcontainer.h"
#include "connections/canconnection.h"

#include <QDialog>
#include <QJSEngine>

namespace Ui {
class ScriptingWindow;
}

class ScriptingWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ScriptingWindow(const QVector<CANFrame> *frames, QWidget *parent = 0);
    void showEvent(QShowEvent*);
    ~ScriptingWindow();

private slots:
    void loadNewScript();
    void createNewScript();
    void deleteCurrentScript();
    void refreshSourceWindow();
    void saveScript();
    void revertScript();
    void recompileScript();
    void newFrames(const CANConnection*, const QVector<CANFrame>&);

private:
    void closeEvent(QCloseEvent *event);
    void readSettings();
    void writeSettings();

    Ui::ScriptingWindow *ui;
    QList<ScriptContainer *> scripts;
    ScriptContainer *currentScript;
    const QVector<CANFrame> *modelFrames;
};

#endif // SCRIPTINGWINDOW_H
