#include "scriptcontainer.h"

#include <QJSValueIterator>
#include <QDebug>

ScriptContainer::ScriptContainer()
{
    fileName = QString();
    filePath = QString();
    scriptText = QString();

    connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void ScriptContainer::compileScript()
{
    QJSValue result = scriptEngine.evaluate(scriptText, fileName);
    if (result.isError() && errorWidget)
    {
        errorWidget->clear();
        errorWidget->addItem("SCRIPT EXCEPTION!");
        errorWidget->addItem("Line: " + result.property("lineNumber").toString());
        errorWidget->addItem(result.property("message").toString());
        errorWidget->addItem("Stack:");
        errorWidget->addItem(result.property("stack").toString());
    }
    else
    {
        compiledScript = result;

        QJSValue hostObj = scriptEngine.newQObject(this);
        scriptEngine.globalObject().setProperty("host", hostObj);

        setupFunction = scriptEngine.globalObject().property("setup");
        gotFrameFunction = scriptEngine.globalObject().property("gotFrame");
        tickFunction = scriptEngine.globalObject().property("tick");

        if (setupFunction.isCallable())
        {
            qDebug() << "setup exists";
            QJSValue res = setupFunction.call();
            if (res.isError())
            {
                errorWidget->addItem("Error in setup function on line " + res.property("lineNumber").toString());
                errorWidget->addItem(res.property("message").toString());
            }
        }

        if (gotFrameFunction.isCallable()) qDebug() << "gotFrame exists";
        if (tickFunction.isCallable()) qDebug() << "tick exists";
    }
}

void ScriptContainer::setErrorWidget(QListWidget *list)
{
    errorWidget = list;
}

void ScriptContainer::setFilter(QJSValue id, QJSValue mask, QJSValue bus)
{
    qDebug() << "Called set filter";
    qDebug() << id.toInt() << "*" << mask.toInt() << "*" << bus.toInt();
}

void ScriptContainer::setTickInterval(QJSValue interval)
{
    int intervalValue = interval.toInt();
    qDebug() << "called set tick interval with value " << intervalValue;
    if (intervalValue > 0)
    {
        timer.setInterval(intervalValue);
        timer.start();
    }
    else timer.stop();
}

void ScriptContainer::clearFilters()
{
    qDebug() << "Called clear filters";
}

void ScriptContainer::sendFrame(QJSValue id, QJSValue length, QJSValue data)
{
    qDebug() << "called send frame";
}

void ScriptContainer::tick()
{
    if (tickFunction.isCallable())
    {
        qDebug() << "Calling tick function";
        QJSValue res = tickFunction.call();
        if (res.isError())
        {
            errorWidget->addItem("Error in tick function on line " + res.property("lineNumber").toString());
            errorWidget->addItem(res.property("message").toString());
        }
    }
}
