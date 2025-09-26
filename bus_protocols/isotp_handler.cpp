#include "isotp_handler.h"
#include <QDebug>

#define CAN_FRAME_SIZE  8
#define FF_PCI_SIZE 2

ISOTP_HANDLER::ISOTP_HANDLER(const QVector<CANFrame> &modelFrames,
                             CanSendCallback sendCb,
                             GetNoOfBusesCallback getBusesCb,
                             PendingConnection<ISOTP_HANDLER> pendingConn )
    : modelFrames(modelFrames),
      canSendCallback(sendCb),
      getNoOfBusesCallback(getBusesCb),
      pendingConnection(pendingConn)
{
    useExtendedAddressing = false;
    isReceiving = false;
    issueFlowMsgs = false;
    processAll = false;
    sendPartialMessages = false;
    lastSenderBus = 0;
    lastSenderID = 0;
    padByte = (char)0xAA;

    connect(&frameTimer, &QTimer::timeout, this, &ISOTP_HANDLER::frameTimerTick);
    updateIsoTpCanFrameInfo();
}

ISOTP_HANDLER::~ISOTP_HANDLER()
{
    disconnect(&frameTimer, &QTimer::timeout, this, &ISOTP_HANDLER::frameTimerTick);
}

void ISOTP_HANDLER::setPadByte(char newpad)
{
    padByte = newpad;
}

void ISOTP_HANDLER::setExtendedAddressing(bool mode)
{
    useExtendedAddressing = mode;
    updateIsoTpCanFrameInfo();
}

void ISOTP_HANDLER::setFlowCtrl(bool state)
{
    issueFlowMsgs = state;
}

void ISOTP_HANDLER::setReception(bool mode)
{
    if (isReceiving == mode) return;
    isReceiving = mode;

    if (isReceiving) {
        if (pendingConnection)
            connection = pendingConnection(this);
        qDebug() << "Enabling reception in ISOTP handler";
    } else {
        if (connection)
            QObject::disconnect(connection);
        qDebug() << "Disabling reception in ISOTP handler";
    }
}

void ISOTP_HANDLER::sendISOTPFrame(int bus, int ID, QByteArray data)
{
    CANFrame frame;
    frame.setFrameType(QCanBusFrame::DataFrame);
    int currByte = 0;
    int sequence = 1; //Initial Sequence number is 1
    if (bus < 0) return;
    if (bus >= getNoOfBusesCallback()) return;

    lastSenderID = ID;
    lastSenderBus = bus;

    frame.bus = bus;
    frame.setFrameId(ID);
    if (ID > 0x7FF) frame.setExtendedFrameFormat(true);
    else frame.setExtendedFrameFormat(false);

    if (data.length() < 8)
    {
        QByteArray bytes(8, padByte);
        bytes.resize(8);
        bytes[0] = data.length();
        for (int i = 0; i < data.length(); i++) bytes[i + 1] = data[i];
        frame.setPayload(bytes);
        canSendCallback(frame);
    }
    else //need to send a multi-part ISO_TP message - Respects timing and frame number based flow control
    {
        QByteArray bytes(8, padByte);
        bytes[0] = 0x10 + (data.length() / 256);
        bytes[1] = data.length() & 0xFF;
        for (int i = 0; i < 6; i++) bytes[2 + i] = data[currByte++];
        frame.setPayload(bytes);
        canSendCallback(frame);
        //Queue up the rest of the frames
        waitingForFlow = true;
        frameTimer.setInterval(200); //wait a while for the flow frame to come in
        frameTimer.setTimerType(Qt::PreciseTimer);
        frameTimer.start();
        while (currByte < data.length())
        {
            for (int b = 0; b < 8; b++) bytes[b] = padByte;
            bytes[0] = 0x20 + sequence; //Consecutive Frame starts from 0x20 + 1 (2: Frame type, 1: Sequence number)
            sequence = (sequence + 1) & 0xF;
            int bytesToGo = data.length() - currByte;
            if (bytesToGo > 7) bytesToGo = 7;
            for (int i = 0; i < bytesToGo; i++) bytes[1 + i] = data[currByte++];
            frame.setPayload(bytes);
            sendingFrames.append(frame);
            //canSendCallback(frame);
        }
    }
}

//remember, negative numbers are special -1 = all frames deleted, -2 = totally new set of frames.
void ISOTP_HANDLER::updatedFrames(int numFrames)
{
    if (numFrames == -1) //all frames deleted. Kill the display
    {
        messageBuffer.clear();
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        messageBuffer.clear();
        for (int i = 0; i < modelFrames.length(); i++) processFrame(modelFrames.at(i));
    }
    else //just got some new frames. See if they are relevant.
    {
        for (int i = modelFrames.count() - numFrames; i < modelFrames.count(); i++)
        {
            //processFrame(modelFrames.at(i));  //accepting these frames in rapidFrames instead
        }
    }
}

void ISOTP_HANDLER::rapidFrames(const QVector<CANFrame>& pFrames)
{
    if (pFrames.length() <= 0) return;

    qDebug() << "received " << QString::number(pFrames.count()) << " messages in ISOTP handler";

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
    ISOTP_MESSAGE msg;
    ISOTP_MESSAGE *pMsg;
    QByteArray dataBytes; // collects data bytes to form the ISOTP message

    // Cache the CAN frame payload once
    QByteArray frameDataBytes = frame.payload();
    // Get the raw CAN data bytes
    const unsigned char *canData = reinterpret_cast<const unsigned char *>(frameDataBytes.constData());

    frameType = 0;
    frameLen = 0;

    // Extract frame type and length from PCI bytes
    if (useExtendedAddressing)
    {
        ID = ID << 8;
        ID += canData[0];             // extended address byte
        frameType = canData[1] >> 4;  // PCI first byte upper nibble = frame type
        frameLen = canData[1] & 0xF;  // PCI first byte lower nibble = length high nibble
    }
    else
    {
        frameType = canData[0] >> 4;
        frameLen = canData[0] & 0xF;
    }

    switch(frameType)
    {
    case 0: //single frame message
        checkNeedFlush(ID);

        if (frameLen == 0) return; //length of zero isn't valid.
        if (frameLen > frameInfo.sfFrameLen) return;

        msg.bus = frame.bus;
        msg.setFrameType(QCanBusFrame::FrameType::DataFrame);
        msg.setExtendedFrameFormat(frame.hasExtendedFrameFormat());
        msg.setFrameId(ID);
        msg.isReceived = frame.isReceived;
        dataBytes.reserve(frameLen);
        msg.reportedLength = frameLen;
        msg.setTimeStamp(frame.timeStamp());
        msg.isMultiframe = false;

        qDebug() << "the framelen " << frameLen << " the payloadlen " << frame.payload().length();
        for (int j = 0; j < frameLen; j++)
        {
            if (frameDataBytes.length() > (j + frameInfo.cfAndSfPciOffset))
                dataBytes.append(canData[j + frameInfo.cfAndSfPciOffset]);
        }

        qDebug() << "Emitting single frame ISOTP message";
        msg.setPayload(dataBytes);
        emit newISOMessage(msg);
        break;

    case 1: //first frame of a multi-frame message
        checkNeedFlush(ID);
        msg.bus = frame.bus;
        if (frameDataBytes.length() < CAN_FRAME_SIZE) return; // MUST have all 8 data bytes
        msg.setExtendedFrameFormat(frame.hasExtendedFrameFormat());
        msg.setFrameId(ID);
        msg.setTimeStamp(frame.timeStamp());
        msg.isReceived = frame.isReceived;
        msg.isMultiframe = true;

        // Compute total message length
        frameLen = frameLen << 8; // upper nibble already in frameLen
        frameLen += useExtendedAddressing ? canData[2] : canData[1]; // add lower length byte
        frameLen &= 0xFFF; // mask to 12 bits
        dataBytes.reserve(frameLen);
        msg.reportedLength = frameLen;

        for (int j = 0; j < frameInfo.ffFrameLen; j++)
        {
            dataBytes.append(canData[frameInfo.ffPciOffset + j]);
        }

        msg.lastSequence = -1;
        msg.setPayload(dataBytes);
        messageBuffer.insert(msg.frameId(), msg);

        //The sending ID is set to the last ID we used to send from this class which is
        //very likely to be correct. But, caution, there is a chance that it isn't. Beware.
        if (issueFlowMsgs && lastSenderID > 0 && lastSenderBus==static_cast<uint32_t>(frame.bus))
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
            canSendCallback(outFrame);
        }
        break;

    case 2: //subsequent frames for multi-frame messages
        pMsg = nullptr;
        if (messageBuffer.contains(ID))
        {
            pMsg = &messageBuffer[ID];
        }
        if (!pMsg) return;
        if (!pMsg->isMultiframe) return; //if we didn't get a frame type 1 (start of multiframe) first then ignore this frame.

        dataBytes.clear();
        dataBytes.append(pMsg->payload());
        ln = pMsg->reportedLength - pMsg->payload().length();

        {
            int bytesToCopy = qMin(ln, frameInfo.cfFrameLen);
            for (int j = 0; j < bytesToCopy; j++)
                dataBytes.append(canData[frameInfo.cfAndSfPciOffset + j]);
        }

        pMsg->setPayload(dataBytes);
        if (pMsg->reportedLength <= pMsg->payload().length())
        {
            qDebug() << "Emitting multiframe ISOTP message";
            checkNeedFlush(pMsg->frameId());
        }
        break;

    case 3: //flow control messages
        switch (frameLen) //actually flow control type in this case
        {
        case 0: //continue to send frames but maybe change inter-frame delay
            waitingForFlow = false;
            //canData[1] contains number of frames to send before waiting for next flow control
            framesUntilFlow = canData[1];
            if (framesUntilFlow == 0) framesUntilFlow = -1; //-1 means don't count frames and just keep going
            //canData[2] contains the interframe delay to use (0xF1 through 0xF9 are special through - 100 to 900us)
            if (canData[2] < 0xF1) frameTimer.start(canData[2]); //set proper delay between frames
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
    ISOTP_MESSAGE *msg;
    if (messageBuffer.contains(ID))
    {
        msg = &messageBuffer[ID];
        if (msg->reportedLength <= msg->payload().length())
        {
            qDebug() << "Flushing full frame" << QString::number(msg->frameId(), 16) << "  " << msg->reportedLength << "  " << msg->payload().length();
            if (msg->reportedLength > 0) emit newISOMessage(*msg);
        }
        else
        {
            if (sendPartialMessages)
            {
                qDebug() << "Flushing a partial frame " << QString::number(msg->frameId(), 16) << "  " << msg->reportedLength << "  " << msg->payload().length();
                if (msg->reportedLength > 0) emit newISOMessage(*msg);
            }
            else qDebug() << "Have a partial message but sending of such is disabled. Throwing it away";
        }
        messageBuffer.remove(ID);
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
            canSendCallback(frame);
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
        framesUntilFlow = -1; //don't count frames, just keep sending
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

void ISOTP_HANDLER::updateIsoTpCanFrameInfo()
{
    frameInfo.cfAndSfPciOffset = useExtendedAddressing ? 2 : 1;
    frameInfo.sfFrameLen = CAN_FRAME_SIZE - frameInfo.cfAndSfPciOffset;

    frameInfo.ffPciOffset = useExtendedAddressing ? (1 + FF_PCI_SIZE) : FF_PCI_SIZE;
    frameInfo.ffFrameLen = CAN_FRAME_SIZE - frameInfo.ffPciOffset;

    frameInfo.cfFrameLen = CAN_FRAME_SIZE - frameInfo.cfAndSfPciOffset;
}

void setEmitPartials(bool mode);
