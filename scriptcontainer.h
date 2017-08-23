#ifndef SCRIPTCONTAINER_H
#define SCRIPTCONTAINER_H

#include "can_structs.h"
#include "canfilter.h"
#include "bus_protocols/isotp_handler.h"
#include "bus_protocols/isotp_message.h"
#include "bus_protocols/uds_handler.h"

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
public:
    ISOTPScriptHelper(QJSEngine *engine);
public slots:
    void setFilter(QJSValue id, QJSValue mask, QJSValue bus);
    void clearFilters();
    void sendISOTP(QJSValue bus, QJSValue id, QJSValue length, QJSValue data);
    void setRxCallback(QJSValue cb);
private slots:
    void newISOMessage(ISOTP_MESSAGE msg);
private:
    QJSValue gotFrameFunction;
    QJSEngine *scriptEngine;
    ISOTP_HANDLER *handler;
};

class UDSScriptHelper: public QObject
{
    Q_OBJECT
public:
    UDSScriptHelper(QJSEngine *engine);
public slots:
    void setFilter(QJSValue id, QJSValue mask, QJSValue bus);
    void clearFilters();
    void sendUDS(QJSValue bus, QJSValue id, QJSValue service, QJSValue sublen, QJSValue subFunc, QJSValue length, QJSValue data);
    void setRxCallback(QJSValue cb);
private slots:
    void newUDSMessage(UDS_MESSAGE msg);
private:
    QJSValue gotFrameFunction;
    QJSEngine *scriptEngine;
    UDS_HANDLER *handler;
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
