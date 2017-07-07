#include "isotp_handler.h"
#include "connections/canconmanager.h"

ISOTP_HANDLER* ISOTP_HANDLER::mInstance = NULL;

ISOTP_HANDLER* ISOTP_HANDLER::getInstance()
{
    if(!mInstance)
    {
        mInstance = new ISOTP_HANDLER();
        mInstance->modelFrames = MainWindow::getReference()->getCANFrameModel()->getListReference();
    }

    return mInstance;
}

ISOTP_HANDLER::ISOTP_HANDLER()
{
    useExtendedAddressing = false;
    isReceiving = false;
}

void ISOTP_HANDLER::setExtendedAddressing(bool mode)
{
    useExtendedAddressing = mode;
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

void ISOTP_HANDLER::sendISOTPFrame(int bus, int ID, QVector<unsigned char> data)
{
    CANFrame frame;
    int currByte = 0;
    int index = 0;
    if (bus < 0) return;
    if (bus >= CANConManager::getInstance()->getNumBuses()) return;

    if (data.length() < 8)
    {
        frame.bus = bus;
        frame.extended = false;
        frame.ID = ID;
        frame.len = 8;
        for (int b = 0; b < 8; b++) frame.data[b] = 0xAA;
        frame.data[0] = data.length();
        for (int i = 0; i < frame.data[0]; i++) frame.data[i + 1] = data[i];
        CANConManager::getInstance()->sendFrame(frame);
    }
    else //need to send a multi-part ISO_TP message - no flow control possible right now. TODO - Add flow control
    {
        frame.bus = bus;
        frame.ID = ID;
        frame.extended = false;
        frame.len = 8;
        for (int b = 0; b < 8; b++) frame.data[b] = 0xAA;
        frame.data[0] = 0x10 + (data.length() / 256);
        frame.data[1] = data.length() & 0xFF;
        for (int i = 0; i < 6; i++) frame.data[2 + i] = data[currByte++];
        CANConManager::getInstance()->sendFrame(frame);
        while (currByte < data.length())
        {
            for (int b = 0; b < 8; b++) frame.data[b] = 0xAA;
            frame.data[0] = 0x20 + index;
            index = (index + 1) & 0xF;
            int bytesToGo = data.length() - currByte;
            if (bytesToGo > 7) bytesToGo = 7;
            for (int i = 0; i < bytesToGo; i++) frame.data[1 + i] = data[currByte++];
            frame.len = 8;
            CANConManager::getInstance()->sendFrame(frame);
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
            //processFrame(modelFrames->at(i));
        }
    }
}

void ISOTP_HANDLER::rapidFrames(const CANConnection* conn, const QVector<CANFrame>& pFrames)
{
    Q_UNUSED(conn)
    if (pFrames.length() <= 0) return;

    qDebug() << "received messages in ISOTP handler";

    foreach(const CANFrame& thisFrame, pFrames)
    {
        processFrame(thisFrame);
    }
}

void ISOTP_HANDLER::processFrame(const CANFrame &frame)
{
    uint64_t ID = frame.ID;
    int frameType;
    int frameLen;
    int ln;
    //int offset;
    ISOTP_MESSAGE msg;
    ISOTP_MESSAGE *pMsg;

    frameType = 0;
    frameLen = 0;

    if (useExtendedAddressing)
    {
        ID = ID << 8;
        ID += frame.data[0];
        frameType = frame.data[1] >> 4;
        frameLen = frame.data[1] & 0xF;
    }
    else
    {
        frameType = frame.data[0] >> 4;
        frameLen = frame.data[0] & 0xF;
    }

    switch(frameType)
    {
    case 0: //single frame message
        checkNeedFlush(ID);

        if (frameLen == 0) return; //length of zero isn't valid.
        if (frameLen > 6 && useExtendedAddressing) return; //impossible
        if (frameLen > 7) return;

        msg.bus = frame.bus;
        msg.extended = frame.extended;
        msg.ID = ID;
        msg.isReceived = frame.isReceived;
        msg.len = frameLen;
        msg.data.reserve(frameLen);
        msg.timestamp = frame.timestamp;
        if (useExtendedAddressing) for (int j = 0; j < frameLen; j++) msg.data.append(frame.data[j+2]);
        else for (int j = 0; j < frameLen; j++) msg.data.append(frame.data[j+1]);
        qDebug() << "Emitting single frame ISOTP message";
        emit newISOMessage(msg);
        break;
    case 1: //first frame of a multi-frame message
        checkNeedFlush(ID);
        msg.bus = frame.bus;
        msg.extended = frame.extended;
        msg.ID = ID;
        msg.timestamp = frame.timestamp;
        msg.isReceived = frame.isReceived;
        frameLen = frameLen << 8;
        if (useExtendedAddressing)
        {
            frameLen += frame.data[2];
            frameLen = frameLen & 0xFFF;
            msg.len = frameLen;
            msg.data.reserve(frameLen);
            for (int j = 0; j < 5; j++) msg.data.append(frame.data[3 + j]);
        }
        else
        {
            frameLen += frame.data[1];
            frameLen = frameLen & 0xFFF;
            msg.len = frameLen;
            msg.data.reserve(frameLen);
            for (int j = 0; j < 6; j++) msg.data.append(frame.data[2 + j]);
        }
        messageBuffer.append(msg);
        break;
    case 2: //subsequent frames for multi-frame messages
        pMsg = NULL;
        for (int i = 0; i < messageBuffer.length(); i++)
        {
            if (messageBuffer[i].ID == ID)
            {
                pMsg = &messageBuffer[i];
                break;
            }
        }
        if (!pMsg) return;
        ln = pMsg->len - pMsg->data.count();
        //offset = pMsg->data.count();
        if (useExtendedAddressing)
        {
            if (ln > 6) ln = 6;
            for (int j = 0; j < ln; j++) pMsg->data.append(frame.data[j+2]);
        }
        else
        {
            if (ln > 7) ln = 7;
            for (int j = 0; j < ln; j++) pMsg->data.append(frame.data[j+1]);
        }
        if (pMsg->len <= pMsg->data.count())
        {
            qDebug() << "Emitting multiframe ISOTP message";
            emit newISOMessage(*pMsg);
        }
        break;
    case 3: //flow control messages -ignored for now
        break;
    }
}

void ISOTP_HANDLER::checkNeedFlush(uint64_t ID)
{
    for (int i = 0; i < messageBuffer.length(); i++)
    {
        if (messageBuffer[i].ID == ID)
        {
            //warning... this code will work for direct signals as emit turns into a function call
            //and thus the other side will have time to do its processing before control returns
            //and we delete the message on our side. But, if this code were used cross thread the
            //emit would be a queued message instead and control would immediately return
            //here and then the message would be deleted before being delivered.
            //To fix that the message would have to be passed by value instead which I'd like to avoid.
            //Thus, don't use this across threads unless you like to debug strange issues.
            qDebug() << "Flushing a partial frame";
            emit newISOMessage(messageBuffer[i]);
            messageBuffer.removeAt(i);
            return;
        }
    }
}
