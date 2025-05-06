#include "framesenderobject.h"
#include "mainwindow.h"

FrameSenderObject::FrameSenderObject(const QVector<CANFrame> *frames)
{
    mThread_p = new QThread();

    statusCounter = 0;
    modelFrames = frames;
    dbcHandler = DBCHandler::getReference();
}

FrameSenderObject::~FrameSenderObject()
{
    mThread_p->quit();
    mThread_p->wait();
    delete mThread_p;
}

void FrameSenderObject::piStart()
{
    sendingTimer = new QTimer();
    sendingTimer->setTimerType(Qt::PreciseTimer);
    sendingTimer->setInterval(1);

    connect(sendingTimer, &QTimer::timeout, this, &FrameSenderObject::timerTriggered);
}

void FrameSenderObject::piStop()
{
    sendingTimer->stop();
    delete sendingTimer;
}

void FrameSenderObject::initialize()
{
    if( mThread_p && (mThread_p != QThread::currentThread()) )
    {
        /* move ourself to the thread */
        moveToThread(mThread_p); /*TODO handle errors */
        /* connect started() */
        connect(mThread_p, SIGNAL(started()), this, SLOT(initialize()));
        /* start the thread */
        mThread_p->start(QThread::HighPriority);
        return;
    }

    /* set started flag */
    //mStarted = true;

    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));

    /* in multithread case, this will be called before entering thread event loop */
    return piStart();

}

void FrameSenderObject::finalize()
{
    /* 1) execute in mThread_p context */
    if( mThread_p && (mThread_p != QThread::currentThread()) )
    {
        /* if thread is finished, it means we call this function for the second time so we can leave */
        if( !mThread_p->isFinished() )
        {
            /* we need to call piStop() */
            QMetaObject::invokeMethod(this, "finalize",
                                      Qt::BlockingQueuedConnection);
            /* 3) stop thread */
            mThread_p->quit();
            if(!mThread_p->wait()) {
                qDebug() << "can't stop thread";
            }
        }
        return;
    }

    /* 2) call piStop in mThread context */
    return piStop();
}

void FrameSenderObject::startSending()
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "startSending",
                                  Qt::BlockingQueuedConnection);
        return;
    }

    sendingElapsed.start();
    sendingTimer->start();
}

void FrameSenderObject::stopSending()
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "stopPlayback",
                                  Qt::BlockingQueuedConnection);
        return;
    }

    sendingTimer->stop(); //pushing this button halts automatic playback
    //emit statusUpdate(currentPosition);
}

void FrameSenderObject::addSendRecord(FrameSendData record)
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "addSendRecord",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(FrameSendData, record));
        return;
    }
    sendingData.append(record);
}

void FrameSenderObject::removeSendRecord(int idx)
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "removeSendRecord",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(int, idx));
        return;
    }
    sendingData.removeAt(idx);
}

/*
 * Note: Getting a reference to an item in a QList is a dangerous thing. If anyone
 * or anything happens to reallocate the list you're screwed. So, call this, do the
 * work, and drop the reference as soon as possible.
*/
FrameSendData *FrameSenderObject::getSendRecordRef(int idx)
{
    if (idx < 0 || idx >= sendingData.count()) return nullptr;
    return &sendingData[idx];
}

/// <summary>
/// Called every millisecond to set the system update figures and send frames if necessary.
/// </summary>
/// <param name="sender"></param>
/// <param name="timerEventArgs"></param>
///
void FrameSenderObject::timerTriggered()
{
    FrameSendData *sendData;
    Trigger *trigger;

    sendingList.clear();
    if(mutex.tryLock())
    {
        /*
         * Requested tick interval was 1ms but the actual interval could be wildly different. So, we track
         * by counting microseconds and accumulating. This creates decent stability even for long intervals
         */
        quint64 elapsed = sendingElapsed.nsecsElapsed() / 1000ul;
        if (elapsed == 0) elapsed = 1;
        sendingElapsed.start();
        sendingLastTimeStamp += elapsed;
        //qDebug() << sendingLastTimeStamp;
        //qDebug() << "El: " << elapsed;
        statusCounter++;
        for (int i = 0; i < sendingData.count(); i++)
        {
            sendData = &sendingData[i];
            if (!sendData->enabled)
            {
                if (sendData->triggers.count() > 0)
                {
                    for (int j = 0; j < sendData->triggers.count(); j++)    //resetting currCount when line is disabled
                    {
                        sendData->triggers[j].currCount = 0;
                    }
                }
                continue; //abort any processing on this if it is not enabled.
            }
            if (sendData->triggers.count() == 0)
            {
                //qDebug() << "No triggers to process";
                break;
            }
            for (int j = 0; j < sendData->triggers.count(); j++)
            {
                trigger = &sendData->triggers[j];
                //if ( (trigger->currCount >= trigger->maxCount) || (trigger->maxCount == -1) ) continue; //don't process if we've sent max frames we were supposed to
                if (!trigger->readyCount) continue; //don't tick if not ready to tick
                //is it time to fire?
                trigger->msCounter += elapsed; //gives proper tracking even if timer doesn't fire as fast as it should
                //qDebug() << trigger->msCounter;
                if (trigger->msCounter >= (trigger->milliseconds * 1000))
                {
                    trigger->msCounter -= (trigger->milliseconds * 1000);
                    sendData->count++;
                    trigger->currCount++;
                    doModifiers(i);
                    //updateGridRow(i);
                    //qDebug() << "About to try to send a frame";
                    //CANConManager::getInstance()->sendFrame(sendingData[i]);
                    sendingList.append(sendingData[i]); //queue it instead of immediate sending
                    if (trigger->ID > 0) trigger->readyCount = false; //reset flag if this is a timed ID trigger
                }
            }
        }

        //if we have any frames to send after the above then send as a batch
        if (sendingList.count() > 0) CANConManager::getInstance()->sendFrames(sendingList);
        mutex.unlock();
    }
    else
    {
        qDebug() << "framesenderobject::handleTick() couldn't get mutex, elapsed is: " << sendingElapsed.elapsed();
    }
}

void FrameSenderObject::buildFrameCache()
{
    CANFrame thisFrame;
    frameCache.clear();
    for (int i = 0; i < modelFrames->count(); i++)
    {
        thisFrame = modelFrames->at(i);
        if (!frameCache.contains(thisFrame.frameId()))
        {
            frameCache.insert(thisFrame.frameId(), thisFrame);
        }
        else
        {
            frameCache[thisFrame.frameId()] = thisFrame;
        }
    }
}

//remember, negative numbers are special -1 = all frames deleted, -2 = totally new set of frames.
void FrameSenderObject::updatedFrames(int numFrames)
{
    CANFrame thisFrame;
    if (numFrames == -1) //all frames deleted.
    {
    }
    else if (numFrames == -2) //all new set of frames.
    {
        buildFrameCache();
    }
    else //just got some new frames. See if they are relevant.
    {
        if (numFrames > modelFrames->count()) return;
        qDebug() << "New frames in sender window";
        //run through the supposedly new frames in order
        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);
            if (!frameCache.contains(thisFrame.frameId()))
            {
                frameCache.insert(thisFrame.frameId(), thisFrame);
            }
            else
            {
                frameCache[thisFrame.frameId()] = thisFrame;
            }
            processIncomingFrame(&thisFrame);
        }
    }
}

void FrameSenderObject::processIncomingFrame(CANFrame *frame)
{
    for (int sd = 0; sd < sendingData.count(); sd++)
    {
        if (sendingData[sd].triggers.count() == 0) continue;
        bool passedChecks = true;
        for (int trig = 0; trig < sendingData[sd].triggers.count(); trig++)
        {
            Trigger *thisTrigger = &sendingData[sd].triggers[trig];
            //need to ensure that this trigger is actually related to frames incoming.
            //Otherwise ignore it here. Only frames that have BUS and/or ID set as trigger will work here.
            if (!(thisTrigger->triggerMask & (TriggerMask::TRG_BUS | TriggerMask::TRG_ID) ) ) continue;

            passedChecks = true;
            //qDebug() << "Trigger ID: " << thisTrigger->ID;
            //qDebug() << "Frame ID: " << frame->frameId();

            //Check to see if we have a bus trigger condition and if so does it match
            if (thisTrigger->bus != frame->bus && (thisTrigger->triggerMask & TriggerMask::TRG_BUS) )
                passedChecks = false;

            //check to see if we have an ID trigger condition and if so does it match
            if ((thisTrigger->triggerMask & TriggerMask::TRG_ID) && (uint32_t)thisTrigger->ID != frame->frameId() )
                passedChecks = false;

            //check to see if we're limiting the trigger by max count and have we reached that count?
            if ( (thisTrigger->triggerMask & TriggerMask::TRG_COUNT) && (thisTrigger->currCount >= thisTrigger->maxCount) )
                passedChecks = false;

            //if the above passed then are we triggering not only on ID but also signal?
            if (passedChecks && (thisTrigger->triggerMask & (TriggerMask::TRG_SIGNAL | TriggerMask::TRG_ID) ) )
            {
                bool sigCheckPassed = false;
                DBC_MESSAGE *msg = dbcHandler->findMessage(thisTrigger->ID);
                if (msg)
                {
                    DBC_SIGNAL *sig = msg->sigHandler->findSignalByName(thisTrigger->sigName);
                    if (sig)
                    {
                        //first of all, is this signal really in this message we got?
                        if (sig->isSignalInMessage(*frame)) sigCheckPassed = true;
                        //if it was and we're also filtering on value then try that next
                        if (sigCheckPassed && (thisTrigger->triggerMask & TriggerMask::TRG_SIGVAL))
                        {
                            double sigval = 0.0;
                            if (sig->processAsDouble(*frame, sigval))
                            {
                                if (abs(sigval - thisTrigger->sigValueDbl) > 0.001)
                                {
                                    sigCheckPassed = false;
                                }
                            }
                            else sigCheckPassed = false;
                        }
                    }
                    else sigCheckPassed = false;
                }
                else sigCheckPassed = false;
                passedChecks &= sigCheckPassed; //passedChecks can only be true if both are
            }

            //if all the above says it's OK then we'll go ahead and send that message.
            //If a message that comes through here has a MS value then we use it as a delay
            //after the check passes. This allows for delaying the sending of the frame if that
            //is required. Otherwise, just send it immediately.
            if (passedChecks)
            {
                if (thisTrigger->milliseconds == 0) //immediate reply
                {
                    thisTrigger->currCount++;
                    sendingData[sd].count++;
                    doModifiers(sd);
                    //updateGridRow(sd);
                    CANConManager::getInstance()->sendFrame(sendingData[sd]);
                }
                else //delayed sending frame
                {
                    thisTrigger->readyCount = true;
                }
            }
        }
    }
}


/// <summary>
/// given an index into the sendingData list we run the modifiers that it has set up
/// </summary>
/// <param name="idx">The index into the sendingData list</param>
void FrameSenderObject::doModifiers(int idx)
{
    int shadowReg = 0; //shadow register we use to accumulate results
    int first=0, second=0;

    FrameSendData *sendData = &sendingData[idx];
    Modifier *mod;
    ModifierOp op;

    if (sendData->modifiers.count() == 0) return; //if no modifiers just leave right now

    //qDebug() << "Executing mods";

    for (int i = 0; i < sendData->modifiers.count(); i++)
    {
        mod = &sendData->modifiers[i];
        for (int j = 0; j < mod->operations.count(); j++)
        {
            op = mod->operations.at(j);
            if (op.first.ID == -1)
            {
                first = shadowReg;
            }
            else first = fetchOperand(idx, op.first);
            second = fetchOperand(idx, op.second);
            switch (op.operation)
            {
            case ADDITION:
                shadowReg = first + second;
                break;
            case AND:
                shadowReg = first & second;
                break;
            case DIVISION:
                shadowReg = first / second;
                break;
            case MULTIPLICATION:
                shadowReg = first * second;
                break;
            case OR:
                shadowReg = first | second;
                break;
            case SUBTRACTION:
                shadowReg = first - second;
                break;
            case XOR:
                shadowReg = first ^ second;
                break;
            case MOD:
                shadowReg = first % second;
            }
        }
        //Finally, drop the result into the proper data byte
        QByteArray newArr(sendData->payload());
        newArr[mod->destByte] = (char) shadowReg;
        sendData->setPayload(newArr);
    }
}

int FrameSenderObject::fetchOperand(int idx, ModifierOperand op)
{
    CANFrame *tempFrame = nullptr;
    if (op.ID == 0) //numeric constant
    {
        if (op.notOper) return ~op.databyte;
        else return op.databyte;
    }
    else if (op.ID == -2) //fetch data from a data byte within the output frame
    {
        if (op.notOper) return ~((unsigned char)sendingData.at(idx).payload()[op.databyte]);
        else return (unsigned char)sendingData.at(idx).payload()[op.databyte];
    }
    else //look up external data byte
    {
        tempFrame = lookupFrame(op.ID, op.bus);
        if (tempFrame != nullptr)
        {
            if (op.notOper) return ~((unsigned char)tempFrame->payload()[op.databyte]);
            else return (unsigned char)tempFrame->payload()[op.databyte];
        }
        else return 0;
    }
}

/// <summary>
/// Try to find the most recent frame given the input criteria
/// </summary>
/// <param name="ID">The ID to find</param>
/// <param name="bus">Which bus to look on (-1 if you don't care)</param>
/// <returns></returns>
CANFrame* FrameSenderObject::lookupFrame(int ID, int bus)
{
    if (!frameCache.contains(ID)) return nullptr;

    if (bus == -1 || frameCache[ID].bus == bus) return &frameCache[ID];

    return nullptr;
}

