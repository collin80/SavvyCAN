#ifndef SCRIPTCONTAINER_H
#define SCRIPTCONTAINER_H

#include "can_structs.h"
#include "canfilter.h"

#include <QElapsedTimer>
#include <QJSEngine>
#include <QTimer>
#include <qlistwidget.h>

class CANScriptHelper: public QObject
{
    Q_OBJECT
public:
    CANScriptHelper(QJSEngine *engine);

public slots:
    void setFilter(QJSValue id, QJSValue mask, QJSValue bus);
    void clearFilters();
    void sendFrame(QJSValue bus, QJSValue id, QJSValue length, QJSValue data);
    void setRxCallback(QJSValue cb);

private slots:
    void gotTargettedFrame(const CANFrame &frame);

private:
    QList<CANFilter> filters;
    QJSValue gotFrameFunction;
    QJSEngine *scriptEngine;
};

class ISOTPScriptHelper: public QObject
{
    Q_OBJECT
private:
    QJSValue gotFrameFunction;
    QJSEngine *scriptEngine;
};

class UDSScriptHelper: public QObject
{
    Q_OBJECT
private:
    QJSValue gotFrameFunction;
    QJSEngine *scriptEngine;
};

class ScriptContainer : public QObject
{
    Q_OBJECT

public:
    ScriptContainer();
    void setLogWidget(QListWidget *list);

    QString fileName;
    QString filePath;
    QString scriptText;

public slots:
    void compileScript();
    void setTickInterval(QJSValue interval);
    void log(QJSValue logString);

private slots:
    void tick();

private:
    QJSEngine scriptEngine;
    QJSValue compiledScript;
    QJSValue setupFunction;
    QJSValue tickFunction;
    QTimer timer;
    QListWidget *logWidget;
    CANScriptHelper *canHelper;
    ISOTPScriptHelper *isoHelper;
    UDSScriptHelper *udsHelper;
    QElapsedTimer elapsedTime;
};

#endif // SCRIPTCONTAINER_H
