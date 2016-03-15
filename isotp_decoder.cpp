#include "isotp_decoder.h"

//in order descriptions based on the list above. But, just in order one after the other
QString UDS_SERVICE_DESCRIPT[] =
{
    "Diagnostic session control",
    "Reset ECU",
    "Clear diagnostic trouble codes",
    "Read diagnostic trouble codes",
    "Read data by ID",
    "Read data by address",
    "Read scaling data by ID",
    "Request security access",
    "Communication control",
    "Read data by ID periodically",
    "Create dynamic data ID",
    "Write data by ID",
    "Input/Output control (force)",
    "Call a service routine",
    "Request data download (from client to server)",
    "Request data upload (from server to client)",
    "Transfer data",
    "Request that data transfer cease",
    "Request file transfer",
    "Write data by address",
    "Tester is present",
    "Read or write comm timing parameters",
    "Secured data transmission",
    "Control DTC settings",
    "Request start/stop transmission on event",
    "Control comm link"
};

ISOTP_DECODER::ISOTP_DECODER(const QVector<CANFrame> *frames, QObject *parent)
    : QObject(parent)
{
    modelFrames = frames;
    useExtendedAddressing = false;
}

void ISOTP_DECODER::setExtendedAddressing(bool mode)
{
    useExtendedAddressing = mode;
}

//remember, negative numbers are special -1 = all frames deleted, -2 = totally new set of frames.
void ISOTP_DECODER::updatedFrames(int numFrames)
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
            processFrame(modelFrames->at(i));
        }
    }
}

void ISOTP_DECODER::processFrame(const CANFrame &frame)
{
    uint64_t ID = frame.ID;
    int frameType;
    int frameLen;
    int ln;
    int offset;
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
        offset = pMsg->data.count();
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

void ISOTP_DECODER::checkNeedFlush(uint64_t ID)
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
