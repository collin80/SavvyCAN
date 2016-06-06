#include "isotp_handler.h"

CODE_STRUCT OBDII_FUNCTS[] =
{
    {1, "UDS_OBDII_SHOW_CURRENT", "OBDII - Show current data"},
    {2, "UDS_OBDII_SHOW_FREEZE", "OBDII - Show freeze data"},
    {3, "UDS_OBDII_SHOW_STORED_DTC", "OBDII - Show stored DTC codes"},
    {4, "UDS_OBDII_CLEAR_DTC", "OBDII - Clear current DTC codes"},
    {5, "UDS_OBDII_TEST_O2", "OBDII - O2 sensor testing"},
    {6, "UDS_OBDII_TEST_RESULTS", "OBDII - Show emissions testing results"},
    {7, "UDS_OBDII_SHOW_PENDING_DTC", "OBDII - Show pending DTC codes"},
    {8, "UDS_OBDII_CONTROL_DEVICES", "OBDII - Control vehicle devices"},
    {9, "UDS_OBDII_VEH_INFO", "OBDII - Retrieve vehicle information"},
    {0xA, "UDS_OBDII_PERM_DTC", "OBDII - Show permanent DTC codes"},
    {0xFF, "UDS_UNKNOWN", "Unknown OBDII code - Likely proprietary"}
};

CODE_STRUCT UDS_FUNCS[] =
{
    {0x10, "UDS_DIAG_CONTROL", "Diagnostic session control"},
    {0x11, "UDS_ECU_RESET", "Reset ECU"},
    {0x14, "UDS_CLEAR_DIAG", "Clear diagnostic trouble codes"},
    {0x19, "UDS_READ_DTC", "Read diagnostic trouble codes"},
    {0x22, "UDS_READ_BY_ID", "Read data by ID"},
    {0x23, "UDS_READ_BY_ADDR", "Read data by address"},
    {0x24, "UDS_READ_SCALING_ID", "Read scaling data by ID"},
    {0x27, "UDS_SECURITY_ACCESS", "Request security access"},
    {0x28, "UDS_COMM_CTRL", "Communication control"},
    {0x2A, "UDS_READ_DATA_ID_PERIODIC", "Read data by ID periodically"},
    {0x2C, "UDS_DYNAMIC_DATA_DEFINE", "Create dynamic data ID"},
    {0x2E, "UDS_WRITE_BY_ID", "Write data by ID"},
    {0x2F, "UDS_IO_CTRL", "Input/Output control (force)"},
    {0x31, "UDS_ROUTINE_CTRL", "Call a service routine"},
    {0x34, "UDS_REQUEST_DOWNLOAD", "Request data download (from PC to ECU)"},
    {0x35, "UDS_REQUEST_UPLOAD", "Request data upload (from ECU to PC)"},
    {0x36, "UDS_TRANSFER_DATA", "Transfer data"},
    {0x37, "UDS_REQ_TRANS_EXIT", "Request that data transfer cease"},
    {0x38, "UDS_REQ_FILE_TRANS", "Request file transfer"},
    {0x3D, "UDS_WRITE_BY_ADDR", "Write data by address"},
    {0x3E, "UDS_TESTER_PRESENT", "Tester is present"},
    {0x83, "UDS_ACCESS_TIMING", "Read or write comm timing parameters"},
    {0x84, "UDS_SECURED_DATA_TRANS", "Secured data transmission"},
    {0x85, "UDS_CTRL_DTC_SETTINGS", "Control DTC settings"},
    {0x86, "UDS_RESPONSE_ON_EVENT", "Request start/stop transmission on event"},
    {0x87, "UDS_RESPONSE_LINK_CTRL", "Control comm link"},
    {0xFF, "UDS_UNKNOWN_CODE", "Unknown, likely proprietary UDS function code"}
};

CODE_STRUCT UDS_NEG_RESPONSE[] =
{
    {0x10, "UDS_NEG_GENERAL_REJECT", "General rejection (no other codes matched)"},
    {0x11, "UDS_NEG_SERVICE_NOTSUPP", "ECU does not support this service code"},
    {0x12, "UDS_NEG_SUBFUNCT_NOTSUPP", "ECU does not support the requested sub function"},
    {0x13, "UDS_NEG_INVALID_FORMAT", "Invalid request length or format error"},
    {0x14, "UDS_NEG_RESPONSE_TOOLONG", "Response would be too long to send"},
    {0x21, "UDS_NEG_BUSY", "ECU is busy. Try again later"},
    {0x22, "UDS_NEG_COND_INCORR", "A prereq. condition was not met"},
    {0x24, "UDS_NEG_REQ_SEQ_ERR", "Invalid sequence of requests"},
    {0x25, "UDS_NEG_SUBNET_NORESP", "ECU tried to gateway request but response timed out"},
    {0x26, "UDS_NEG_FAILURE", "A failure (indicated in a DTC) is preventing a reply"},
    {0x31, "UDS_NEG_REQ_OUTOFRANGE", "A parameter is outside of the valid range"},
    {0x33, "UDS_NEG_SECURITY_DENIED", "Security access was denied. (invalid seq or ECU not unlocked?)"},
    {0x35, "UDS_NEG_INVALID_KEY", "Key passed was invalid. Failure counter has been incremented."},
    {0x36, "UDS_NEG_EXCEED_ATTEMPTS", "Key failed too many times. ECU security access locked out"},
    {0x37, "UDS_NEG_TIMEDELAY", "Security access too soon after last attempt"},
    {0x38, "UDS_NEG_EXT_SECUR_1", "Extended security failure code 1"},
    {0x39, "UDS_NEG_EXT_SECUR_2", "Extended security failure code 2"},
    {0x3A, "UDS_NEG_EXT_SECUR_3", "Extended security failure code 3"},
    {0x3B, "UDS_NEG_EXT_SECUR_4", "Extended security failure code 4"},
    {0x3C, "UDS_NEG_EXT_SECUR_5", "Extended security failure code 5"},
    {0x3D, "UDS_NEG_EXT_SECUR_6", "Extended security failure code 6"},
    {0x3E, "UDS_NEG_EXT_SECUR_7", "Extended security failure code 7"},
    {0x3F, "UDS_NEG_EXT_SECUR_8", "Extended security failure code 8"},
    {0x40, "UDS_NEG_EXT_SECUR_9", "Extended security failure code 9"},
    {0x41, "UDS_NEG_EXT_SECUR_10", "Extended security failure code 10"},
    {0x42, "UDS_NEG_EXT_SECUR_11", "Extended security failure code 11"},
    {0x43, "UDS_NEG_EXT_SECUR_12", "Extended security failure code 12"},
    {0x44, "UDS_NEG_EXT_SECUR_13", "Extended security failure code 13"},
    {0x45, "UDS_NEG_EXT_SECUR_14", "Extended security failure code 14"},
    {0x46, "UDS_NEG_EXT_SECUR_15", "Extended security failure code 15"},
    {0x47, "UDS_NEG_EXT_SECUR_16", "Extended security failure code 16"},
    {0x48, "UDS_NEG_EXT_SECUR_17", "Extended security failure code 17"},
    {0x49, "UDS_NEG_EXT_SECUR_18", "Extended security failure code 18"},
    {0x4A, "UDS_NEG_EXT_SECUR_19", "Extended security failure code 19"},
    {0x4B, "UDS_NEG_EXT_SECUR_20", "Extended security failure code 20"},
    {0x4C, "UDS_NEG_EXT_SECUR_21", "Extended security failure code 21"},
    {0x4D, "UDS_NEG_EXT_SECUR_22", "Extended security failure code 22"},
    {0x4E, "UDS_NEG_EXT_SECUR_23", "Extended security failure code 23"},
    {0x4F, "UDS_NEG_EXT_SECUR_24", "Extended security failure code 24"},
    {0x70, "UDS_NEG_UPLOAD_DOWNLOAD", "Fault when attempting to start upload/download"},
    {0x71, "UDS_NEG_TRX_SUSPENDED", "Transfer aborting due to a fault"},
    {0x72, "UDS_NEG_GEN_PROGRAMMING", "Fault while attempting to write to ECU memory"},
    {0x73, "UDS_NEG_WRONG_BLOCK_SEQ", "Invalid sequence value detected during transfer"},
    {0x78, "UDS_NEG_RESP_PENDING", "Request successful but ECU still busy - Response pending"},
    {0x7E, "UDS_NEG_SUBFUNCT_CURRSESS", "ECU does not support this subfunction in current session type"},
    {0x7F, "UDS_NEG_SERVICE_CURRSESS", "ECU does not support this service in current session type"},
    {0x81, "UDS_NEG_RPM_TOOHIGH", "RPM is too high to execute request"},
    {0x82, "UDS_NEG_RPM_TOOLOW", "RPM is too low to execute request"},
    {0x83, "UDS_NEG_ENGINE_RUNNING", "Cannot execute request while engine is running"},
    {0x84, "UDS_NEG_ENGINE_NOTRUNNING", "Cannot execute request while engine is off"},
    {0x85, "UDS_NEG_ENG_RUNTIME_LOW", "Cannot execute request until engine has run for longer"},
    {0x86, "UDS_NEG_TEMPERATURE_HIGH", "Cannot execute request until temperature is lower"},
    {0x87, "UDS_NEG_TEMPERATURE_LOW", "Cannot execute request until temperature is higher"},
    {0x88, "UDS_NEG_SPEED_HIGH", "Cannot execute request until vehicle slows down"},
    {0x89, "UDS_NEG_SPEED_LOW", "Cannot execute request until vehicle is going faster"},
    {0x8A, "UDS_NEG_PEDAL_HIGH", "Cannot execute request until throttle is lower"},
    {0x8B, "UDS_NEG_PEDAL_LOW", "Cannot execute request until throttle is higher"},
    {0x8C, "UDS_NEG_NOT_NEUTRAL", "Cannot execute request until transmission is in neutral"},
    {0x8D, "UDS_NEG_NOT_INGEAR", "Cannot execute request until vehicle is in gear"},
    {0x8F, "UDS_NEG_BRAKE_NOTPRESSED", "Cannot execute request until brake pedal is pressed (Hold down)"},
    {0x90, "UDS_NEG_NOT_PARK", "Cannot execute request until vehicle is in park"},
    {0x91, "UDS_NEG_CLUTCH_LOCKED", "Cannot execute request while clutch is locked"},
    {0x92, "UDS_NEG_VOLTAGE_HIGH", "Cannot execute request until voltage is lower"},
    {0x93, "UDS_NEG_VOLTAGE_LOW", "Cannot execute request until voltage is higher"},
};

ISOTP_HANDLER::ISOTP_HANDLER(const QVector<CANFrame> *frames, QObject *parent)
    : QObject(parent)
{
    modelFrames = frames;
    useExtendedAddressing = false;
}

void ISOTP_HANDLER::setExtendedAddressing(bool mode)
{
    useExtendedAddressing = mode;
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
            processFrame(modelFrames->at(i));
        }
    }
}

void ISOTP_HANDLER::processFrame(const CANFrame &frame)
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
