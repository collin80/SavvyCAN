#include "frameplaybackobject.h"

FramePlaybackObject::FramePlaybackObject()
{
    mThread_p = new QThread();

    currentPosition = 0;
    playbackInterval = 1;
    playbackBurst = 1;
    statusCounter = 0;
    numBuses = 0;
    playbackActive = false;
    playbackForward = true;
    useOrigTiming = false;
    whichBusSend = 0;
    currentSeqItem = nullptr;
}

FramePlaybackObject::~FramePlaybackObject()
{
    mThread_p->quit();
    mThread_p->wait();
    delete mThread_p;
}

quint64 FramePlaybackObject::updatePosition(bool forward)
{
    //qDebug() << "updatePosition";
    if (!currentSeqItem) {
        playbackTimer->stop(); //pushing this button halts automatic playback
        playbackActive = false;
        currentPosition = 0;
        return 0;
    }
    if (forward)
    {
        if (currentPosition < (currentSeqItem->data.count() - 1)) currentPosition++; //still in same file so keep going
        else //hit the end of the current file
        {
            qDebug() << "hit end of current sequence";
            currentSeqItem->currentLoopCount++;
            currentPosition = 0;
            if (currentSeqItem->currentLoopCount == currentSeqItem->maxLoops) //have we looped enough times?
            {
                playbackActive = false;
                playbackTimer->stop();
                emit EndOfFrameCache();
            }
        }
    }
    else
    {
        if (currentPosition > 0) currentPosition--;
        else //hit the beginning of the current sequence
        {
            qDebug() << "hit start of current sequence";
            currentSeqItem->currentLoopCount++;
            currentPosition = currentSeqItem->data.count() - 1;
            if (currentSeqItem->currentLoopCount == currentSeqItem->maxLoops) //have we looped enough times?
            {
                playbackActive = false;
                playbackTimer->stop();
                emit EndOfFrameCache();
            }
        }
    }

    //only send frame out if its ID is checked in the list. Otherwise discard it.
    CANFrame *thisFrame = &currentSeqItem->data[currentPosition];
    uint32_t originalBus = thisFrame->bus;
    if (currentSeqItem->idFilters.find(thisFrame->frameId()).value())
    {
        if (whichBusSend > -1)
        {
            thisFrame->bus = whichBusSend;
            sendingBuffer.append(*thisFrame);
        }
        else if (whichBusSend == -1)
        {
            for (int c = 0; c < numBuses; c++)
            {
                thisFrame->bus = c;
                sendingBuffer.append(*thisFrame);
            }
        }
        else //from file so retain original bus and send as-is
        {
            sendingBuffer.append(*thisFrame);
        }

        thisFrame->bus = originalBus;
    }
    return thisFrame->timeStamp().microSeconds();
}

quint64 FramePlaybackObject::peekPosition(bool forward)
{
    int peekCurrentPosition = currentPosition;
    if (forward)
    {
        if (peekCurrentPosition < (currentSeqItem->data.count() - 1)) peekCurrentPosition++; //still in same file so keep going
        else //hit the end of the current file
        {
            return 0xFFFFFFFFFFFFFFFFull;
        }
    }
    else
    {
        if (peekCurrentPosition > 0) peekCurrentPosition--;
        else //hit the beginning of the current sequence
        {
            return 0xFFFFFFFFFFFFFFFFull;
        }
    }
    CANFrame *thisFrame = &currentSeqItem->data[peekCurrentPosition];
    return thisFrame->timeStamp().microSeconds();
}

void FramePlaybackObject::piStart()
{
    playbackTimer = new QTimer();
    playbackTimer->setTimerType(Qt::PreciseTimer);
    playbackTimer->setInterval(1);

    currentPosition = 0;
    playbackActive = false;
    playbackForward = true;
    whichBusSend = 0;

    connect(playbackTimer, &QTimer::timeout, this, &FramePlaybackObject::timerTriggered);
}

void FramePlaybackObject::piStop()
{
    playbackTimer->stop();
    delete playbackTimer;
}

void FramePlaybackObject::initialize()
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

    /* in multithread case, this will be called before entering thread event loop */
    return piStart();

}

void FramePlaybackObject::finalize()
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

void FramePlaybackObject::startPlaybackForward()
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "startPlaybackForward",
                                  Qt::BlockingQueuedConnection);
        return;
    }

    playbackActive = true;
    playbackForward = true;

    if (useOrigTiming)
    {
        playbackTimer->setInterval(1);
        playbackElapsed.start();
        if (currentSeqItem->data[currentPosition].timeStamp().microSeconds() > 2000)
            playbackLastTimeStamp = currentSeqItem->data[currentPosition].timeStamp().microSeconds() - 2000;
        else playbackLastTimeStamp = 0;
    }
    playbackTimer->start();
}

void FramePlaybackObject::startPlaybackBackward()
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "startPlaybackBackward",
                                  Qt::BlockingQueuedConnection);
        return;
    }

    playbackActive = true;
    playbackForward = false;
    if (useOrigTiming)
    {
        playbackElapsed.start();
        playbackTimer->setInterval(1);
        playbackLastTimeStamp = currentSeqItem->data[currentPosition].timeStamp().microSeconds() + 2000;
    }
    playbackTimer->start();
}

void FramePlaybackObject::stepPlaybackForward()
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "stepPlaybackForward",
                                  Qt::BlockingQueuedConnection);
        return;
    }

    sendingBuffer.clear();
    playbackTimer->stop();
    playbackActive = false;
    updatePosition(true);
    CANConManager::getInstance()->sendFrames(sendingBuffer);
    emit statusUpdate(currentPosition);
}

void FramePlaybackObject::stepPlaybackBackward()
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "stepPlaybackBackward",
                                  Qt::BlockingQueuedConnection);
        return;
    }

    sendingBuffer.clear();
    playbackTimer->stop(); //pushing this button halts automatic playback
    playbackActive = false;

    updatePosition(false);
    CANConManager::getInstance()->sendFrames(sendingBuffer);
    emit statusUpdate(currentPosition);
}

void FramePlaybackObject::stopPlayback()
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "stopPlayback",
                                  Qt::BlockingQueuedConnection);
        return;
    }

    playbackTimer->stop(); //pushing this button halts automatic playback
    playbackActive = false;
    currentPosition = 0;
    emit statusUpdate(currentPosition);
}

void FramePlaybackObject::pausePlayback()
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "pausePlayback",
                                  Qt::BlockingQueuedConnection);
        return;
    }

    playbackActive = false;
    playbackTimer->stop();
    emit statusUpdate(currentPosition);
}

void FramePlaybackObject::setSequenceObject(SequenceItem *item)
{
    currentSeqItem = item;
}

void FramePlaybackObject::setUseOriginalTiming(bool state)
{
    useOrigTiming = state;
}

void FramePlaybackObject::setSendingBus(int bus)
{
    qDebug() << "Setting sending bus to " << bus;
    whichBusSend = bus;
}

void FramePlaybackObject::setPlaybackBurst(int burst)
{
    playbackBurst = burst;
}

void FramePlaybackObject::setNumBuses(int buses)
{
    numBuses = buses;
}

//any time you touch objects that live in a potentially different thread you must switch into that thread.
//So this function is a bit different than the rest of the simple setters for that reason.
void FramePlaybackObject::setPlaybackInterval(int interval)
{
    /* make sure we execute in mThread context */
    if( mThread_p && (mThread_p != QThread::currentThread()) ) {
        QMetaObject::invokeMethod(this, "setPlaybackInterval",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(int , interval) );
        return;
    }
    playbackInterval = interval;
    playbackTimer->setInterval(interval);
}

void FramePlaybackObject::timerTriggered()
{
    sendingBuffer.clear();

    if (useOrigTiming)
    {
        //get elapsed microseconds since last tick (in case timer skips or is otherwise inaccurate, though there are no guarantees about elapsed timer either)
        quint64 elapsed = playbackElapsed.nsecsElapsed() / 1000;
        playbackElapsed.start();
        if (playbackForward)
        {
            playbackLastTimeStamp += elapsed;
            //qDebug() << playbackLastTimeStamp;
            //qDebug() << "El: " << elapsed;
            while (peekPosition(true) <= playbackLastTimeStamp)
            {
                updatePosition(true);
            }
            if (peekPosition(true) == 0xFFFFFFFFFFFFFFFFull)
            {
                updatePosition(true); //this'll go to the next log (if there is one)
                if (currentSeqItem->data[currentPosition].timeStamp().microSeconds() > 1000)
                    playbackLastTimeStamp = currentSeqItem->data[currentPosition].timeStamp().microSeconds() - 1000;
                else playbackLastTimeStamp = 0;
            }
        }
        else
        {
            if (playbackLastTimeStamp > elapsed)
                playbackLastTimeStamp -= elapsed;
            else playbackLastTimeStamp = 0;
            while (peekPosition(false) >= playbackLastTimeStamp && peekPosition(false) != 0xFFFFFFFFFFFFFFFFull)
            {
                updatePosition(false);
            }
            if (peekPosition(false) == 0xFFFFFFFFFFFFFFFFull)
            {
                updatePosition(false); //this'll go to the next log (if there is one)
                playbackLastTimeStamp = currentSeqItem->data[currentPosition].timeStamp().microSeconds() + 1000;
            }
        }
        statusCounter++;
    }
    else
    {
        for (int count = 0; count < playbackBurst; count++)
        {
            if (!playbackActive)
            {
                playbackTimer->stop();
                return;
            }
            if (playbackForward)
            {
                updatePosition(true);
            }
            else
            {
                updatePosition(false);
            }
        }
        statusCounter += playbackInterval;
    }

    if (statusCounter > 249)
    {
        statusCounter = 0;
        emit statusUpdate(currentPosition);
    }

    //qDebug() << "sb: " << sendingBuffer.count();
    if (sendingBuffer.count() > 0) CANConManager::getInstance()->sendFrames(sendingBuffer);
}







