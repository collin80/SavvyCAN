#include <QJSValueIterator>
#include <QDebug>

#include "scriptcontainer.h"
#include "connections/canconmanager.h"

ScriptContainer::ScriptContainer()
{
    canHelper = new CANScriptHelper(&scriptEngine);
    isoHelper = new ISOTPScriptHelper(&scriptEngine);
    udsHelper = new UDSScriptHelper(&scriptEngine);
    elapsedTime.start();
    connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void ScriptContainer::compileScript()
{
    QJSValue result = scriptEngine.evaluate(scriptText, fileName);

    if (logWidget)
    {
        logWidget->addItem("");
        logWidget->addItem("Compiling script " + fileName);
    }

    if (result.isError() && logWidget)
    {
        logWidget->addItem("SCRIPT EXCEPTION!");
        logWidget->addItem("Line: " + result.property("lineNumber").toString());
        logWidget->addItem(result.property("message").toString());
        logWidget->addItem("Stack:");
        logWidget->addItem(result.property("stack").toString());
    }
    else
    {
        compiledScript = result;

        //Add a bunch of helper objects into javascript that the scripts
        //can use to interact with the CAN buses
        QJSValue hostObj = scriptEngine.newQObject(this);
        scriptEngine.globalObject().setProperty("host", hostObj);
        QJSValue canObj = scriptEngine.newQObject(canHelper);
        scriptEngine.globalObject().setProperty("can", canObj);
        QJSValue isoObj = scriptEngine.newQObject(isoHelper);
        scriptEngine.globalObject().setProperty("isotp", isoObj);
        QJSValue udsObj = scriptEngine.newQObject(udsHelper);
        scriptEngine.globalObject().setProperty("uds", udsObj);

        //Find out which callbacks the script has created.
        setupFunction = scriptEngine.globalObject().property("setup");
        canHelper->setRxCallback(scriptEngine.globalObject().property("gotCANFrame"));

        tickFunction = scriptEngine.globalObject().property("tick");

        if (setupFunction.isCallable())
        {
            qDebug() << "setup exists";
            QJSValue res = setupFunction.call();
            if (res.isError())
            {
                logWidget->addItem("Error in setup function on line " + res.property("lineNumber").toString());
                logWidget->addItem(res.property("message").toString());
            }
        }
        if (tickFunction.isCallable()) qDebug() << "tick exists";
    }
}

void ScriptContainer::setLogWidget(QListWidget *list)
{
    logWidget = list;
}

void ScriptContainer::log(QJSValue logString)
{
    QString val = logString.toString();
    logWidget->addItem(QString::number(elapsedTime.elapsed()) + ": " + val);
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

void ScriptContainer::tick()
{
    if (tickFunction.isCallable())
    {
        //qDebug() << "Calling tick function";
        QJSValue res = tickFunction.call();
        if (res.isError())
        {
            logWidget->addItem("Error in tick function on line " + res.property("lineNumber").toString());
            logWidget->addItem(res.property("message").toString());
        }
    }
}





/* CANScriptHandler Methods */

CANScriptHelper::CANScriptHelper(QJSEngine *engine)
{
    scriptEngine = engine;
}

void CANScriptHelper::setRxCallback(QJSValue cb)
{
    gotFrameFunction = cb;
}

void CANScriptHelper::setFilter(QJSValue id, QJSValue mask, QJSValue bus)
{
    uint32_t idVal = id.toUInt();
    uint32_t maskVal = mask.toUInt();
    int busVal = bus.toInt();
    qDebug() << "Called set filter";
    qDebug() << idVal << "*" << maskVal << "*" << busVal;
    CANFilter filter;
    filter.setFilter(idVal, maskVal, busVal);
    filters.append(filter);

    CANConManager::getInstance()->addTargettedFrame(busVal, idVal, maskVal, this);
}

void CANScriptHelper::clearFilters()
{
    qDebug() << "Called clear filters";
    foreach (CANFilter filter, filters)
    {
        CANConManager::getInstance()->removeTargettedFrame(filter.bus, filter.ID, filter.mask, this);
    }

    filters.clear();
}

void CANScriptHelper::sendFrame(QJSValue bus, QJSValue id, QJSValue length, QJSValue data)
{
    CANFrame frame;
    frame.extended = false;
    frame.ID = id.toInt();
    frame.len = length.toUInt();
    if (frame.len > 8) frame.len = 8;

    if (!data.isArray()) qDebug() << "data isn't an array";

    for (unsigned int i = 0; i < frame.len; i++)
    {
        frame.data[i] = (uint8_t)data.property(i).toInt();
    }

    frame.bus = (uint32_t)bus.toInt();
    //if (frame.bus > 1) frame.bus = 1;

    if (frame.ID > 0x7FF) frame.extended = true;

    qDebug() << "sending frame from script";
    CANConManager::getInstance()->sendFrame(frame);
}

void CANScriptHelper::gotTargettedFrame(const CANFrame &frame)
{
    if (!gotFrameFunction.isCallable()) return; //nothing to do if we can't even call the function
    //qDebug() << "Got frame in script interface";
    for (int i = 0; i < filters.length(); i++)
    {
        if (filters[i].checkFilter(frame.ID, frame.bus))
        {
            QJSValueList args;
            args << frame.bus << frame.ID << frame.len;
            QJSValue dataBytes = scriptEngine->newArray(frame.len);

            for (unsigned int j = 0; j < frame.len; j++) dataBytes.setProperty(j, QJSValue(frame.data[j]));
            args.append(dataBytes);
            gotFrameFunction.call(args);
            return; //as soon as one filter matches we jump out
        }
    }
}




/* ISOTPScriptHelper methods */
ISOTPScriptHelper::ISOTPScriptHelper(QJSEngine *engine)
{
    scriptEngine = engine;
    handler = new ISOTP_HANDLER;
    connect(handler, SIGNAL(newISOMessage(ISOTP_MESSAGE)), this, SLOT(newISOMessage(ISOTP_MESSAGE)));
    handler->setReception(true);
}

void ISOTPScriptHelper::clearFilters()
{
    handler->clearAllFilters();
}

void ISOTPScriptHelper::setFilter(QJSValue id, QJSValue mask, QJSValue bus)
{
    uint32_t idVal = id.toUInt();
    uint32_t maskVal = mask.toUInt();
    int busVal = bus.toInt();
    qDebug() << "Called isotp set filter";
    qDebug() << idVal << "*" << maskVal << "*" << busVal;

    handler->addFilter(busVal, idVal, maskVal);
}

void ISOTPScriptHelper::sendISOTP(QJSValue bus, QJSValue id, QJSValue length, QJSValue data)
{
    ISOTP_MESSAGE msg;
    msg.extended = false;
    msg.ID = id.toInt();
    msg.len = length.toUInt();

    if (!data.isArray()) qDebug() << "data isn't an array";

    for (unsigned int i = 0; i < msg.len; i++)
    {
        msg.data[i] = (uint8_t)data.property(i).toInt();
    }

    msg.bus = (uint32_t)bus.toInt();

    if (msg.ID > 0x7FF) msg.extended = true;

    qDebug() << "sending isotp message from script";
    handler->sendISOTPFrame(msg.bus, msg.ID, msg.data);
}

void ISOTPScriptHelper::setRxCallback(QJSValue cb)
{
    gotFrameFunction = cb;
}

void ISOTPScriptHelper::newISOMessage(ISOTP_MESSAGE msg)
{
    if (!gotFrameFunction.isCallable()) return; //nothing to do if we can't even call the function
    //qDebug() << "Got frame in script interface";

    QJSValueList args;
    args << msg.bus << msg.ID << msg.len;
    QJSValue dataBytes = scriptEngine->newArray(msg.len);

    for (unsigned int j = 0; j < msg.len; j++) dataBytes.setProperty(j, QJSValue(msg.data[j]));
    args.append(dataBytes);
    gotFrameFunction.call(args);
}




/* UDSScriptHelper methods */
UDSScriptHelper::UDSScriptHelper(QJSEngine *engine)
{
    scriptEngine = engine;
    handler = new UDS_HANDLER;
    connect(handler, SIGNAL(newUDSMessage(UDS_MESSAGE)), this, SLOT(newUDSMessage(UDS_MESSAGE)));
    handler->setReception(true);
}

void UDSScriptHelper::clearFilters()
{
    handler->clearAllFilters();
}

void UDSScriptHelper::setFilter(QJSValue id, QJSValue mask, QJSValue bus)
{
    uint32_t idVal = id.toUInt();
    uint32_t maskVal = mask.toUInt();
    int busVal = bus.toInt();
    qDebug() << "Called uds set filter";
    qDebug() << idVal << "*" << maskVal << "*" << busVal;

    handler->addFilter(busVal, idVal, maskVal);
}

void UDSScriptHelper::sendUDS(QJSValue bus, QJSValue id, QJSValue service, QJSValue sublen, QJSValue subFunc, QJSValue length, QJSValue data)
{
    UDS_MESSAGE msg;
    msg.extended = false;
    msg.ID = id.toInt();
    msg.len = length.toUInt();
    msg.service = service.toUInt();
    msg.subFuncLen = sublen.toUInt();
    msg.subFunc = subFunc.toUInt();

    if (!data.isArray()) qDebug() << "data isn't an array";

    for (unsigned int i = 0; i < msg.len; i++)
    {
        msg.data[i] = (uint8_t)data.property(i).toInt();
    }

    msg.bus = (uint32_t)bus.toInt();

    if (msg.ID > 0x7FF) msg.extended = true;

    qDebug() << "sending UDS message from script";

    handler->sendUDSFrame(msg);
}

void UDSScriptHelper::setRxCallback(QJSValue cb)
{
    gotFrameFunction = cb;
}

void UDSScriptHelper::newUDSMessage(UDS_MESSAGE msg)
{
    if (!gotFrameFunction.isCallable()) return; //nothing to do if we can't even call the function
    //qDebug() << "Got frame in script interface";

    QJSValueList args;
    args << msg.bus << msg.ID << msg.service << msg.subFunc << msg.len;
    QJSValue dataBytes = scriptEngine->newArray(msg.len);

    for (unsigned int j = 0; j < msg.len; j++) dataBytes.setProperty(j, QJSValue(msg.data[j]));
    args.append(dataBytes);
    gotFrameFunction.call(args);
}

