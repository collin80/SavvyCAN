#include "isotp_handler.h"
#include "connections/canconmanager.h"

ISOTP_HANDLER::ISOTP_HANDLER()
{
    useExtendedAddressing = false;
    isReceiving = false;
    issueFlowMsgs = false;
    processAll = false;
    lastSenderBus = 0;
    lastSenderID = 0;

    modelFrames = MainWindow::getReference()->getCANFrameModel()->getListReference();

    connect(&frameTimer, SIGNAL(timeout()), this, SLOT(frameTimerTick()));
}

ISOTP_HANDLER::~ISOTP_HANDLER()
{
    disconnect(&frameTimer, SIGNAL(timeout()), this, SLOT(frameTimerTick()));
}

void ISOTP_HANDLER::setExtendedAddressing(bool mode)
{
    useExtendedAddressing = mode;
}

void ISOTP_HANDLER::setFlowCtrl(bool state)
{
    issueFlowMsgs = state;
}

void ISOTP_HANDLER::setReception(bool mode)
{
    if (isReceiving == mode) return;
    isReceiving = mode;

    if (isReceiving)
    {
        connect(CANConManager::getInstance(), &CANConManager::framesReceived, this, &ISOTP_HANDLER::rapidFrames);
        qDebug() << "Enabling reception in ISOTP handler";
    }
    else
    {
        disconnect(CANConManager::getInstance(), &CANConManager::framesReceived, this, &ISOTP_HANDLER::rapidFrames);
        qDebug() << "Disabling reception in ISOTP handler";
    }
}

void ISOTP_HANDLER::sendISOTPFrame(int bus, int ID, QByteArray data)
{
    CANFrame frame;
    frame.setFrameType(QCanBusFrame::DataFrame);
    int currByte = 0;
    int index = 0;
    if (bus < 0) return;
    if (bus >= CANConManager::getInstance()->getNumBuses()) return;

    lastSenderID = ID;
    lastSenderBus = bus;

    frame.bus = bus;
    frame.setFrameId(ID);
    if (ID > 0x7FF) frame.setExtendedFrameFormat(true);
    else frame.setExtendedFrameFormat(false);

    if (data.length() < 8)
    {
        QByteArray bytes(8,0);
        bytes.resize(8);
        bytes[0] = data.length();
        for (int i = 0; i < data.length(); i++) bytes[i + 1] = data[i];
        frame.setPayload(bytes);
        CANConManager::getInstance()->sendFrame(frame);
    }
    else //need to send a multi-part ISO_TP message - Respects timing and frame number based flow control
    {
        QByteArray bytes(8, 0);
        bytes[0] = 0x10 + (data.length() / 256);
        bytes[1] = data.length() & 0xFF;
        for (int i = 0; i < 6; i++) bytes[2 + i] = data[currByte++];
        frame.setPayload(bytes);
        CANConManager::getInstance()->sendFrame(frame);
        //Queue up the rest of the frames
        waitingForFlow = true;
        frameTimer.setInterval(200); //wait a while for the flow frame to come in
        frameTimer.setTimerType(Qt::PreciseTimer);
        frameTimer.start();
        while (currByte < data.length())
        {
            for (int b = 0; b < 8; b++) bytes[b] = 0x00;
            bytes[0] = 0x20 + index;
            index = (index + 1) & 0xF;
            int bytesToGo = data.length() - currByte;
            if (bytesToGo > 7) bytesToGo = 7;
            for (int i = 0; i < bytesToGo; i++) bytes[1 + i] = data[currByte++];
            frame.setPayload(bytes);
            sendingFrames.append(frame);
            //CANConManager::getInstance()->sendFrame(frame);
        }
    }
}

//remember, negative numbers are special -1 = all frames deleted, -2 = totally new set of frames.
void ISOTP_HANDLER::updatedFrames(int numFrames)
{
    if (numFrames == -1) //all frames deleted. Kill the display
    {
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        for (int i = 0; i < modelFrames->length(); i++) processFrame(modelFrames->at(i));
    }
    else //just got some new frames. See if they are relevant.
    {
        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            //processFrame(modelFrames->at(i));  //accepting these frames in rapidFrames instead
        }
    }
}

void ISOTP_HANDLER::rapidFrames(const CANConnection* conn, const QVector<CANFrame>& pFrames)
{
    Q_UNUSED(conn)
    if (pFrames.length() <= 0) return;

    //qDebug() << "received messages in ISOTP handler";

    foreach(const CANFrame& thisFrame, pFrames)
    {
        //only process frames that we've marked are ISOTP frames
        //unless processAll is true
        if (processAll) processFrame(thisFrame);
        else
        {
            for (int i = 0; i < filters.count(); i++)
            {
                if ((thisFrame.bus == filters[i].bus) && ((thisFrame.frameId() & filters[i].mask) == filters[i].ID))
                {
                    processFrame(thisFrame);
                    break;
                }
            }

        }
    }
}

void ISOTP_HANDLER::processFrame(const CANFrame &frame)
{
    uint64_t ID = frame.frameId();
    int frameType;
    int frameLen;
    int ln;
    //int offset;
    ISOTP_MESSAGE msg;
    ISOTP_MESSAGE *pMsg;
    const unsigned char *data = reinterpret_cast<const unsigned char *>(frame.payload().constData());
    //int dataLen = frame.payload().count();

    frameType = 0;
    frameLen = 0;

    if (useExtendedAddressing)
    {
        ID = ID << 8;
        ID += data[0];
        frameType = data[1] >> 4;
        frameLen = data[1] & 0xF;
    }
    else
    {
        frameType = data[0] >> 4;
        frameLen = data[0] & 0xF;
    }

    switch(frameType)
    {
    case 0: //single frame message
        checkNeedFlush(ID);

        if (frameLen == 0) return; //length of zero isn't valid.
        if (frameLen > 6 && useExtendedAddressing) return; //impossible
        if (frameLen > 7) return;

        msg.bus = frame.bus;
        msg.setExtendedFrameFormat( frame.hasExtendedFrameFormat() );
        msg.setFrameId(ID);
        msg.isReceived = frame.isReceived;
        msg.payload().reserve(frameLen);
        msg.reportedLength = frameLen;
        msg.setTimeStamp(frame.timeStamp());
        msg.isMultiframe = false;
        if (useExtendedAddressing) for (int j = 0; j < frameLen; j++) msg.payload().append(frame.payload()[j+2]);
        else for (int j = 0; j < frameLen; j++) msg.payload().append(frame.payload()[j+1]);
        //qDebug() << "Emitting single frame ISOTP message";
        emit newISOMessage(msg);
        break;
    case 1: //first frame of a multi-frame message
        checkNeedFlush(ID);
        msg.bus = frame.bus;
        msg.setExtendedFrameFormat( frame.hasExtendedFrameFormat() );
        msg.setFrameId(ID);
        msg.setTimeStamp(frame.timeStamp());
        msg.isReceived = frame.isReceived;
        msg.isMultiframe = true;
        frameLen = frameLen << 8;
        if (useExtendedAddressing)
        {
            frameLen += data[2];
            frameLen = frameLen & 0xFFF;
            msg.payload().reserve(frameLen);
            msg.reportedLength = frameLen;
            for (int j = 0; j < 5; j++) msg.payload().append(frame.payload()[3 + j]);
        }
        else
        {
            frameLen += data[1];
            frameLen = frameLen & 0xFFF;
            msg.payload().reserve(frameLen);
            msg.reportedLength = frameLen;
            for (int j = 0; j < 6; j++) msg.payload().append(frame.payload()[2 + j]);
        }
        msg.lastSequence = -1;
        messageBuffer.append(msg);
        //The sending ID is set to the last ID we used to send from this class which is
        //very likely to be correct. But, caution, there is a chance that it isn't. Beware.
        if (issueFlowMsgs && lastSenderID > 0)
        {
            CANFrame outFrame;
            outFrame.bus = lastSenderBus;
            outFrame.setExtendedFrameFormat(false);
            outFrame.setFrameId(lastSenderID);
            QByteArray bytes(8, 0);
            bytes[0] = 0x30; //flow control, go ahead and send
            bytes[1] = 0; //dont ask again about flow control
            bytes[2] = 3; //separation time in milliseconds between messages.
            outFrame.setPayload(bytes);
            CANConManager::getInstance()->sendFrame(outFrame);
        }
        break;
    case 2: //subsequent frames for multi-frame messages
        pMsg = nullptr;
        for (int i = 0; i < messageBuffer.length(); i++)
        {
            if (messageBuffer[i].frameId() == ID)
            {
                pMsg = &messageBuffer[i];
                break;
            }
        }
        if (!pMsg) return;
        if (!pMsg->isMultiframe) return; //if we didn't get a frame type 1 (start of multiframe) first then ignore this frame.
        ln = pMsg->payload().length() - pMsg->payload().count();
        //offset = pMsg->data.count();
        if (useExtendedAddressing)
        {
            if (ln > 6) ln = 6;
            for (int j = 0; j < ln; j++) pMsg->payload().append(frame.payload()[j+2]);
        }
        else
        {
            if (ln > 7) ln = 7;
            for (int j = 0; j < ln; j++) pMsg->payload().append(frame.payload()[j+1]);
        }
        if (pMsg->reportedLength <= pMsg->payload().count())
        {
            //qDebug() << "Emitting multiframe ISOTP message";
            checkNeedFlush(pMsg->frameId());
        }
        break;
    case 3: //flow control messages
        switch (frameLen) //actually flow control type in this case
        {
        case 0: //continue to send frames but maybe change inter-frame delay
            waitingForFlow = false;
            //data[1] contains number of frames to send before waiting for next flow control
            framesUntilFlow = data[1];
            if (framesUntilFlow == 0) framesUntilFlow = -1; //-1 means don't count frames and just keep going
            //data[2] contains the interframe delay to use (0xF1 through 0xF9 are special through)
            if (data[2] < 0xF1) frameTimer.start(data[2]); //set proper delay between frames
            else frameTimer.start(1); //can't do sub-millisecond sending with this code so just use 1ms timing
            break;
        case 1: //wait - do not send any more frames until other side says so
            waitingForFlow = true;
            frameTimer.stop(); //quit sending frames for now
            break;
        case 2: //overflow or abort. Assume this means abort and quit sending
            frameTimer.stop();
            sendingFrames.clear();
            waitingForFlow = false;
            break;
        }
        waitingForFlow = false;

        break;
    }
}

void ISOTP_HANDLER::checkNeedFlush(uint64_t ID)
{
    for (int i = 0; i < messageBuffer.length(); i++)
    {
        if (messageBuffer[i].frameId() == ID)
        {
            //used to pass by reference but now newISOMessage should pass by value which makes it easier to use cross thread
            if (messageBuffer[i].frameId() > 0x600 && messageBuffer[i].frameId() < 0x630)
            {
                if (messageBuffer[i].reportedLength <= messageBuffer[i].payload().count())
                {
                    qDebug() << "Flushing full frame" << QString::number(messageBuffer[i].frameId(), 16) << "  " << messageBuffer[i].reportedLength << "  " << messageBuffer[i].payload().count();
                }
                else
                {
                    qDebug() << "Flushing a partial frame " << QString::number(messageBuffer[i].frameId(), 16) << "  " << messageBuffer[i].reportedLength << "  " << messageBuffer[i].payload().count();
                }
            }
            emit newISOMessage(messageBuffer[i]);
            messageBuffer.removeAt(i);
            return;
        }
    }
}

void ISOTP_HANDLER::frameTimerTick()
{
    CANFrame frame;
    if (!waitingForFlow)
    {
        if (!sendingFrames.isEmpty())
        {
            frame = sendingFrames.takeFirst();
            CANConManager::getInstance()->sendFrame(frame);
            if (framesUntilFlow > -1) framesUntilFlow--;
            if (framesUntilFlow == 0) //stop sending and wait for another flow control message
            {
                frameTimer.stop(); //we absolutely will not send anything until other side says to.
                waitingForFlow = true;
            }
        }
        else //no more frames to send
        {
            frameTimer.stop();
        }
    }
    else //while waiting for a flow frame we didn't get one during timeout period. Try to send anyway with default timeout
    {
        waitingForFlow = false;
        frameTimer.setInterval(20); //pretty slow sending which should be OK as a default
    }
}

void ISOTP_HANDLER::setProcessAll(bool state)
{
    processAll = state;
}

void ISOTP_HANDLER::addFilter(int pBusId, uint32_t ID, uint32_t mask)
{
    CANFilter filt;
    filt.ID = ID;
    filt.bus = pBusId;
    filt.mask = mask;

    filters.append(filt);
}

void ISOTP_HANDLER::removeFilter(int pBusId, uint32_t ID, uint32_t mask)
{
    for (int i = 0; i < filters.count(); i++)
    {
        if (filters[i].bus == pBusId && filters[i].ID == ID && filters[i].mask == mask) filters.removeAt(i);
    }
}

void ISOTP_HANDLER::clearAllFilters()
{
    filters.clear();
}


