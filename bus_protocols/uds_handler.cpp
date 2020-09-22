#include "uds_handler.h"
#include "connections/canconmanager.h"
#include "mainwindow.h"
#include "isotp_handler.h"
#include <QDebug>

static QVector<CODE_STRUCT> UDS_DIAG_CTRL_SUB = {
    {1,"DFLT_SESS", "Default session"},
    {2,"PROG_SESS", "Programming Session"},
    {3,"EXT_SESS", "Extended Diagnostics Session"},
    {4,"SAFETY_SESS", "Safety System Diagnostics Session"},
};

static QVector<CODE_STRUCT> UDS_ECU_RESET_SUB = {
    {1,"HARD_RESET", "Hard reset of ECU"},
    {2,"KEYOFFON_RESET", "Simulated key off then on reset"},
    {3,"SOFT_RESET", "Soft reset - leaving RAM intact"},
    {4,"EN_POWERDOWN_RESET", "Enable sleep mode"},
    {5,"DIS_POWERDOWN_RESET", "Disable sleep mode"},
};

static QVector<CODE_STRUCT> UDS_COMM_CTRL_SUB = {
    {0,"COMM_NORMAL", "Enable both Rx and Tx of normal messages"},
    {1,"COMM_DIS_TX", "Enable reception of normal messages but don't Tx them"},
    {3,"COMM_DIS_ALL", "Disable both Rx and Tx of non-diagnostics messages"},
    {4,"COMM_DIS_TX_ENH", "Addressed bus master should turn off TX on related sub-bus"},
    {5,"COMM_ENHANC", "Addressed bus master should set related sub-bus to app scheduling mode"},
};

static QVector<CODE_STRUCT> UDS_ROUTINE_SUB = {
    {1,"START_ROUTINE", "Start routine by given ID"},
    {2,"STOP_ROUTINE", "Stop routine by given ID"},
    {3,"GET_ROUTINE_RESULTS", "Get results from routine specified by ID"},
};

static QVector<CODE_STRUCT> UDS_FILE_MODEOFOP = {
    {1, "ADDFILE", "Add file to file system"},
    {2, "DELETEFILE", "Add file to file system"},
    {3, "REPLACEFILE", "Add file to file system"},
    {4, "READFILE", "Add file to file system"},
    {5, "READDIR", "Add file to file system"}
};

static QVector<CODE_STRUCT> UDS_SERVICE_DESC = {
    {1, "OBDII_SHOW_CURRENT", "OBDII - Show current data"},
    {2, "OBDII_SHOW_FREEZE", "OBDII - Show freeze data"},
    {3, "OBDII_SHOW_STORED_DTC", "OBDII - Show stored DTC codes"},
    {4, "OBDII_CLEAR_DTC", "OBDII - Clear current DTC codes"},
    {5, "OBDII_TEST_O2", "OBDII - O2 sensor testing"},
    {6, "OBDII_TEST_RESULTS", "OBDII - Show emissions testing results"},
    {7, "OBDII_SHOW_PENDING_DTC", "OBDII - Show pending DTC codes"},
    {8, "OBDII_CONTROL_DEVICES", "OBDII - Control vehicle devices"},
    {9, "OBDII_VEH_INFO", "OBDII - Retrieve vehicle information"},
    {0xA, "OBDII_PERM_DTC", "OBDII - Show permanent DTC codes"},
    {0x10, "DIAG_CONTROL", "Diagnostic session control"},
    {0x11, "ECU_RESET", "Reset ECU"},
    {0x12, "GMLAN_READ_FAILURE_RECORD", "GMLAN - Read Fail"},
    {0x14, "CLEAR_DIAG", "Clear diagnostic trouble codes"},
    {0x19, "READ_DTC", "Read diagnostic trouble codes"},
    {0x1A, "GMLAN_READ_DIAGNOSTIC_ID", "GMLAN - Read diagnostics ID"},
    {0x20, "RETURN_TO_NORMAL", "Return to normal mode"},    
    {0x21, "READ_BY_LOCALID", "Read data by Local ID"},
    {0x22, "READ_BY_ID", "Read data by ID"},
    {0x23, "READ_BY_ADDR", "Read data by address"},
    {0x24, "READ_SCALING_ID", "Read scaling data by ID"},
    {0x27, "SECURITY_ACCESS", "Request security access"},
    {0x28, "COMM_CTRL", "Communication control"},
    {0x2A, "READ_DATA_ID_PERIODIC", "Read data by ID periodically"},
    {0x2C, "DYNAMIC_DATA_DEFINE", "Create dynamic data ID"},
    {0x2D, "DEFINE_PID_BY_ADDR", "Create a PID for a given memory address"},
    {0x2E, "WRITE_BY_ID", "Write data by ID"},
    {0x2F, "IO_CTRL", "Input/Output control (force)"},
    {0x31, "ROUTINE_CTRL", "Call a service routine"},
    {0x34, "REQUEST_DOWNLOAD", "Request data download (from PC to ECU)"},
    {0x35, "REQUEST_UPLOAD", "Request data upload (from ECU to PC)"},
    {0x36, "TRANSFER_DATA", "Transfer data"},
    {0x37, "REQ_TRANS_EXIT", "Request that data transfer cease"},
    {0x38, "REQ_FILE_TRANS", "Request file transfer"},
    {0x3B, "GMLAN_WRITE_DID", "GMLAN - Write DID"},
    {0x3D, "WRITE_BY_ADDR", "Write data by address"},
    {0x3E, "TESTER_PRESENT", "Tester is present"},
    {0x7F, "NEG_RESPONSE","Negative Response"},
    {0x83, "ACCESS_TIMING", "Read or write comm timing parameters"},
    {0x84, "SECURED_DATA_TRANS", "Secured data transmission"},
    {0x85, "CTRL_DTC_SETTINGS", "Control DTC settings"},
    {0x86, "RESPONSE_ON_EVENT", "Request start/stop transmission on event"},
    {0x87, "RESPONSE_LINK_CTRL", "Control comm link"},
    {0xA2, "GMLAN_REPORT_PROG_STATE", "GMLAN - Report programming state"},
    {0xA5, "GMLAN_ENTER_PROG_MODE", "GMLAN - Enter programming mode"},
    {0xA9, "GMLAN_CHECK_CODES", "GMLAN - Check codes"},
    {0xAA, "GMLAN_READ_DPID", "GMLAN - Read dynamic PID"},
    {0xAE, "GMLAN_DEVICE_CTRL", "GMLAN - Device control"},
    {0xFF, "UNKNOWN_CODE", "Unknown, likely proprietary UDS function code"}
};

static QVector<CODE_STRUCT> UDS_NEG_RESPONSE =
{
    {0x10, "GENERAL_REJECT", "General rejection (no other codes matched)"},
    {0x11, "SERVICE_NOTSUPP", "ECU does not support this service code"},
    {0x12, "SUBFUNCT_NOTSUPP", "ECU does not support the requested sub function"},
    {0x13, "INVALID_FORMAT", "Invalid request length or format error"},
    {0x14, "RESPONSE_TOOLONG", "Response would be too long to send"},
    {0x21, "BUSY", "ECU is busy. Try again later"},
    {0x22, "COND_INCORR", "A prereq. condition was not met"},
    {0x24, "REQ_SEQ_ERR", "Invalid sequence of requests"},
    {0x25, "SUBNET_NORESP", "ECU tried to gateway request but response timed out"},
    {0x26, "FAILURE", "A failure (indicated in a DTC) is preventing a reply"},
    {0x31, "REQ_OUTOFRANGE", "A parameter is outside of the valid range"},
    {0x33, "SECURITY_DENIED", "Security access was denied. (invalid seq or ECU not unlocked?)"},
    {0x35, "INVALID_KEY", "Key passed was invalid. Failure counter has been incremented."},
    {0x36, "EXCEED_ATTEMPTS", "Key failed too many times. ECU security access locked out"},
    {0x37, "TIMEDELAY", "Security access too soon after last attempt"},
    {0x38, "EXT_SECUR_1", "Extended security failure code 1"},
    {0x39, "EXT_SECUR_2", "Extended security failure code 2"},
    {0x3A, "EXT_SECUR_3", "Extended security failure code 3"},
    {0x3B, "EXT_SECUR_4", "Extended security failure code 4"},
    {0x3C, "EXT_SECUR_5", "Extended security failure code 5"},
    {0x3D, "EXT_SECUR_6", "Extended security failure code 6"},
    {0x3E, "EXT_SECUR_7", "Extended security failure code 7"},
    {0x3F, "EXT_SECUR_8", "Extended security failure code 8"},
    {0x40, "EXT_SECUR_9", "Extended security failure code 9"},
    {0x41, "EXT_SECUR_10", "Extended security failure code 10"},
    {0x42, "EXT_SECUR_11", "Extended security failure code 11"},
    {0x43, "EXT_SECUR_12", "Extended security failure code 12"},
    {0x44, "EXT_SECUR_13", "Extended security failure code 13"},
    {0x45, "EXT_SECUR_14", "Extended security failure code 14"},
    {0x46, "EXT_SECUR_15", "Extended security failure code 15"},
    {0x47, "EXT_SECUR_16", "Extended security failure code 16"},
    {0x48, "EXT_SECUR_17", "Extended security failure code 17"},
    {0x49, "EXT_SECUR_18", "Extended security failure code 18"},
    {0x4A, "EXT_SECUR_19", "Extended security failure code 19"},
    {0x4B, "EXT_SECUR_20", "Extended security failure code 20"},
    {0x4C, "EXT_SECUR_21", "Extended security failure code 21"},
    {0x4D, "EXT_SECUR_22", "Extended security failure code 22"},
    {0x4E, "EXT_SECUR_23", "Extended security failure code 23"},
    {0x4F, "EXT_SECUR_24", "Extended security failure code 24"},
    {0x70, "UPLOAD_DOWNLOAD", "Fault when attempting to start upload/download"},
    {0x71, "TRX_SUSPENDED", "Transfer aborting due to a fault"},
    {0x72, "GEN_PROGRAMMING", "Fault while attempting to write to ECU memory"},
    {0x73, "WRONG_BLOCK_SEQ", "Invalid sequence value detected during transfer"},
    {0x78, "RESP_PENDING", "Request successful but ECU still busy - Response pending"},
    {0x7E, "SUBFUNCT_CURRSESS", "ECU does not support this subfunction in current session type"},
    {0x7F, "SERVICE_CURRSESS", "ECU does not support this service in current session type"},
    {0x81, "RPM_TOOHIGH", "RPM is too high to execute request"},
    {0x82, "RPM_TOOLOW", "RPM is too low to execute request"},
    {0x83, "ENGINE_RUNNING", "Cannot execute request while engine is running"},
    {0x84, "ENGINE_NOTRUNNING", "Cannot execute request while engine is off"},
    {0x85, "ENG_RUNTIME_LOW", "Cannot execute request until engine has run for longer"},
    {0x86, "TEMPERATURE_HIGH", "Cannot execute request until temperature is lower"},
    {0x87, "TEMPERATURE_LOW", "Cannot execute request until temperature is higher"},
    {0x88, "SPEED_HIGH", "Cannot execute request until vehicle slows down"},
    {0x89, "SPEED_LOW", "Cannot execute request until vehicle is going faster"},
    {0x8A, "PEDAL_HIGH", "Cannot execute request until throttle is lower"},
    {0x8B, "PEDAL_LOW", "Cannot execute request until throttle is higher"},
    {0x8C, "NOT_NEUTRAL", "Cannot execute request until transmission is in neutral"},
    {0x8D, "NOT_INGEAR", "Cannot execute request until vehicle is in gear"},
    {0x8F, "BRAKE_NOTPRESSED", "Cannot execute request until brake pedal is pressed (Hold down)"},
    {0x90, "NOT_PARK", "Cannot execute request until vehicle is in park"},
    {0x91, "CLUTCH_LOCKED", "Cannot execute request while clutch is locked"},
    {0x92, "VOLTAGE_HIGH", "Cannot execute request until voltage is lower"},
    {0x93, "VOLTAGE_LOW", "Cannot execute request until voltage is higher"},
};

UDS_MESSAGE::UDS_MESSAGE()
{
    subFunc = 0;
    service = 0;
    subFuncLen = 1;
    isErrorReply = false;
}

UDS_HANDLER::UDS_HANDLER()
{
    isReceiving = false;
    useExtendedAddressing = false;
    modelFrames = MainWindow::getReference()->getCANFrameModel()->getListReference();
    isoHandler = new ISOTP_HANDLER();
}

UDS_HANDLER::~UDS_HANDLER()
{
    delete isoHandler;
}

void UDS_HANDLER::gotISOTPFrame(ISOTP_MESSAGE msg)
{
    qDebug() << "UDS handler got ISOTP frame";
    const unsigned char *data = reinterpret_cast<const unsigned char *>(msg.payload().constData());
    int dataLen = msg.payload().count();
    UDS_MESSAGE udsMsg;
    udsMsg.bus = msg.bus;
    udsMsg.setExtendedFrameFormat(msg.hasExtendedFrameFormat());
    udsMsg.setFrameId(msg.frameId());
    udsMsg.isReceived = msg.isReceived;
    udsMsg.setTimeStamp(msg.timeStamp());
    udsMsg.reportedLength = msg.reportedLength;
    udsMsg.service = 0;
    udsMsg.subFunc = 0;
    udsMsg.subFuncLen = 0;
    udsMsg.isErrorReply = false;
    udsMsg.setPayload(msg.payload());
    if (dataLen > 0) {
        udsMsg.service = data[0];
        if (udsMsg.service == 0x7F)
        {
            udsMsg.isErrorReply = true;
            if (dataLen > 1)
            {
                udsMsg.service = data[1];
                if (dataLen > 2) udsMsg.subFunc = data[2];
                else return;
            }
            else return;
            udsMsg.payload().remove(0, 2);
        }
        else
        {
            udsMsg.isErrorReply = false;
            if (dataLen > 1) udsMsg.subFunc = data[1];
            udsMsg.payload().remove(0, 1);
        }
    }
    else return;

    emit newUDSMessage(udsMsg);
}

void UDS_HANDLER::setFlowCtrl(bool state)
{
    isoHandler->setFlowCtrl(state);
}

void UDS_HANDLER::setReception(bool mode)
{
    if (isReceiving == mode) return;

    isReceiving = mode;

    if (isReceiving)
    {
        connect(isoHandler, SIGNAL(newISOMessage(ISOTP_MESSAGE)), this, SLOT(gotISOTPFrame(ISOTP_MESSAGE)));
        isoHandler->setReception(true); //must enable ISOTP reception too.
        qDebug() << "Enabling reception of ISO-TP frames in UDS handler";
    }
    else
    {
        disconnect(isoHandler, SIGNAL(newISOMessage(ISOTP_MESSAGE)), this, SLOT(gotISOTPFrame(ISOTP_MESSAGE)));
        isoHandler->setReception(false);
        qDebug() << "Disabling reception of ISOTP frames in UDS handler";
    }
}

void UDS_HANDLER::sendUDSFrame(const UDS_MESSAGE &msg)
{
    QByteArray data;
    if (msg.bus < 0) return;
    if (msg.bus >= CANConManager::getInstance()->getNumBuses()) return;
    if (msg.service > 0xFF) return;

    data.append(msg.service);
    for (int b = msg.subFuncLen - 1; b >= 0; b--)
    {
        data.append((msg.subFunc >> (8 * b)) & 0xFF);
    }

    data.append(msg.payload());
    isoHandler->sendISOTPFrame(msg.bus, msg.frameId(), data);

    //qDebug() << "Data sending: " << data;

    qDebug() << "Sent UDS service: " << getServiceShortDesc(msg.service) << " on bus " << msg.bus;
}

QString UDS_HANDLER::getShortDesc(QVector<CODE_STRUCT> &codeVector, int code)
{
    foreach (CODE_STRUCT codeRec, codeVector)
    {
        if (codeRec.code == code) return codeRec.shortDesc;
    }
    return QString();
}

QString UDS_HANDLER::getLongDesc(QVector<CODE_STRUCT> &codeVector, int code)
{
    foreach (CODE_STRUCT codeRec, codeVector)
    {
        if (codeRec.code == code) return codeRec.longDesc;
    }
    return QString();
}

QString UDS_HANDLER::getServiceShortDesc(int service)
{
    QString ret;
    ret = getShortDesc(UDS_SERVICE_DESC, service);
    if (ret.length() == 0)
        ret = getShortDesc(UDS_SERVICE_DESC, service + 0x40);
    return ret;
}

QString UDS_HANDLER::getServiceLongDesc(int service)
{
    QString ret;
    ret = getLongDesc(UDS_SERVICE_DESC, service);
    if (ret.length() == 0)
        ret = getLongDesc(UDS_SERVICE_DESC, service + 0x40);
    return ret;
}

QString UDS_HANDLER::getNegativeResponseShort(int respCode)
{
    QString ret;
    ret = getShortDesc(UDS_NEG_RESPONSE, respCode);
    return ret;
}

QString UDS_HANDLER::getNegativeResponseLong(int respCode)
{
    QString ret;
    ret = getLongDesc(UDS_NEG_RESPONSE, respCode);
    return ret;
}

/*
 * This function kind of breaks the hands off approach of the rest of the class. In here
 * we dig deep into nitty gritty details of UDS and much more of this is hardcoded
 * to the UDS standard than the rest of the code where the code doesn't really know anything
 * about what it is doing - it just reads arrays and such.
 * No, in here we end up with hard coded handlers to figure out each type of message and how to
 * interpret it to a fine level of detail. Look away code purists, this is likely to be a mess.
 * The output here is potentially multi-line and is useful for debugging a UDS problem. You'll
 * get a finely detailed dump of exactly what the message means.
*/
QString UDS_HANDLER::getDetailedMessageAnalysis(const UDS_MESSAGE &msg)
{
    QString buildString;
    int dataSize;
    int addrSize;
    int compType, encType;
    const unsigned char *data = reinterpret_cast<const unsigned char *>(msg.payload().constData());
    int dataLen = msg.payload().length();

    if (msg.isErrorReply)
    {
        buildString.append("UDS ERROR Response\n");
        buildString.append("Service: " + getServiceLongDesc(msg.service) + "\n");
    }
    else if (msg.service < 0x3F || (msg.service > 0x7F && msg.service < 0xAF)) {
        buildString.append("UDS Request\n");
        buildString.append("Service: " + getServiceLongDesc(msg.service) + "\n");
    }
    else
    {
        buildString.append("UDS Positive Response\n");
        buildString.append("Service: " + getServiceLongDesc(msg.service - 0x40) + "\n");
    }

    if (msg.isErrorReply)
    {
        //Negative responses replace the sub function with an error code instead
        buildString.append("Negative response: " + getLongDesc(UDS_NEG_RESPONSE, msg.subFunc) + "\n");
    }
    else
    {
        switch (msg.service)
        {
        case UDS_SERVICES::DIAG_CONTROL:
            //diag control requests have one parameter - which type of session we want.
            buildString.append("Session Request: " + getLongDesc(UDS_DIAG_CTRL_SUB, msg.subFunc));
            break;
        case UDS_SERVICES::DIAG_CONTROL + 0x40: //positive response
            buildString.append("Session Request: " + getLongDesc(UDS_DIAG_CTRL_SUB, msg.subFunc));
            //there should be four extra bytes now
            if (dataLen < 5) //5 because subfunc codes are left in data so it starts with one subfunc byte
            {
                //buildString.append("\nReturned data payload wasn't at least \n4 bytes like it should have been");
            }
            else
            {
                int p2 = data[1] * 256 + data[2];
                buildString.append("\nP2MAX (Max Wait / Resp Time): " + QString::number(p2) + "ms");
                p2 = (data[3] * 256 + data[4]) * 10;
                buildString.append("\nP2 Ext MAX: " + QString::number(p2) + "ms");
            }
            break;
        case UDS_SERVICES::ECU_RESET:
            //ECU reset has one parameter - which reset type to ask for
            buildString.append("Reset Type: " + getLongDesc(UDS_ECU_RESET_SUB, msg.subFunc));
            break;
        case UDS_SERVICES::ECU_RESET + 0x40:
            buildString.append("Reset Type: " + getLongDesc(UDS_ECU_RESET_SUB, msg.subFunc));
            //There should be one additional byte which encodes power down time
            if (dataLen > 1)
            {
                if (data[1] < 0xFF)
                {
                    buildString.append("\nMinimum powered down time: " + QString::number(data[1]));
                }
                else buildString.append("\nPowerdown time not available");
            }
            else buildString.append("\nNo powerdown time returned");
            break;
        case UDS_SERVICES::COMM_CTRL:
            //Comm control has potentially a lot of parameters. control type, comm type, nodeID
            buildString.append("Control type: " + getLongDesc(UDS_COMM_CTRL_SUB, msg.subFunc));
            if (dataLen > 1)
                buildString.append("\nComm Type: " + QString::number(data[1])); //TODO: no attempt to interpret yet
            if (dataLen > 3)
            {
                int nodeID = (data[2] * 256 + data[3]);
                buildString.append("\nNode ID: " + Utility::formatHexNum(nodeID));
            }
            break;
        case UDS_SERVICES::SECURITY_ACCESS:
            if ((msg.subFunc % 2) == 1)
            {
                buildString.append("Seed request for security level: " + QString::number(msg.subFunc) + "\n");
                if (dataLen > 1)
                {
                    buildString.append("Data payload: ");
                    for (int j = 2; j < dataLen; j++) buildString.append(Utility::formatHexNum(data[j]) + " ");
                }
            }
            else
            {
                buildString.append("Key sending for security level: " + QString::number(msg.subFunc - 1));
                if (dataLen > 1) //and it sure as hell should be!
                {
                    buildString.append("  KEY: ");
                    for (int j = 2; j < dataLen; j++) buildString.append(Utility::formatHexNum(data[j]) + " ");
                }
            }
            break;
        case UDS_SERVICES::SECURITY_ACCESS + 0x40:
            if ((msg.subFunc % 2) == 1)
            {
                buildString.append("Seed response for security level: " + QString::number(msg.subFunc) + "\n");
                if (dataLen > 1) //be kinda pointless if it weren't
                {
                    buildString.append("SEED: ");
                    for (int j = 2; j < dataLen; j++) buildString.append(Utility::formatHexNum(data[j]) + " ");
                }
            }
            else
            {
                buildString.append("Positive response to key for security level: " + QString::number(msg.subFunc - 1));
                buildString.append("\nECU is now unlocked");
            }
            break;
        case UDS_SERVICES::READ_BY_ID:
            //parameter is groups of two bytes, each of which specify an ID to read
            if (dataLen > 2)
            {
                uint32_t id;
                for (int i = 1; i < dataLen; i = i + 2)
                {
                    id = (data[i] * 256) + data[i+1];
                    buildString.append("\nID to read: " + Utility::formatHexNum(id));
                }
            }
            break;
        case UDS_SERVICES::READ_BY_ID + 0x40: //reply
            buildString.append("Reply is non-standard and so no decoding is done. The format is (ID) followed by how ever much data that ID returns, followed by more ID/data pairs if applicable.\nPayload: ");
            for (int i = 1; i < dataLen; i++)
            {
                buildString.append(Utility::formatHexNum(data[i]) + " ");
            }
            break;
        case UDS_SERVICES::READ_BY_ADDR:
            //subfunc byte specifies address and length format, then address, then size
            dataSize = msg.subFunc >> 4;
            addrSize = msg.subFunc & 0xF;
            if (dataLen > (dataSize + addrSize))
            {
                buildString.append("Address: 0x");
                for (int i = 0; i < addrSize; i++) buildString.append(QString::number(data[1+i], 16).toUpper().rightJustified(2,'0'));
                buildString.append("\nSize: 0x");
                for (int i = 0; i < dataSize; i++) buildString.append(QString::number(data[1+i+addrSize], 16).toUpper().rightJustified(2,'0'));
            }
            else
            {
                buildString.append("Message has insufficient bytes to properly decode address and size!");
            }
            break;
        case UDS_SERVICES::READ_BY_ADDR + 0x40:
            buildString.append("Reply is a raw packet of data of the size requested.\nPayload: ");
            for (int i = 1; i < dataLen; i++)
            {
                buildString.append(Utility::formatHexNum(data[i]) + " ");
            }
            break;
        case UDS_SERVICES::WRITE_BY_ID:            
            if (dataLen > 3)
            {
                int writeID = (data[1] * 256 + data[2]);
                buildString.append("ID to write to: " + Utility::formatHexNum(writeID) + "\nPayload: ");
                for (int i = 3; i < dataLen; i++)
                {
                    buildString.append(Utility::formatHexNum(data[i]) + " ");
                }
            }
            break;
        case UDS_SERVICES::WRITE_BY_ID + 0x40:
            if (dataLen > 2)
            {
                int writeID = (data[1] * 256 + data[2]);
                buildString.append("ID written to: " + Utility::formatHexNum(writeID));
            }
            break;
        case UDS_SERVICES::ROUTINE_CTRL:
            buildString.append("Routine Control: " + getLongDesc(UDS_ROUTINE_SUB, msg.subFunc));
            if (dataLen > 3)
            {
                int routineID;
                routineID = (data[2] * 256 + data[3]);
                buildString.append("\nRoutine ID: " + Utility::formatHexNum(routineID));
            }
            if (dataLen > 4)
            {
                buildString.append("\nParameter bytes to routine: ");
                for (int i = 4; i < dataLen; i++)
                {
                    buildString.append(Utility::formatHexNum(data[i]) + " ");
                }
            }
            break;
        case UDS_SERVICES::ROUTINE_CTRL + 0x40:
            buildString.append("Routine Control: " + getLongDesc(UDS_ROUTINE_SUB, msg.subFunc));
            if (dataLen > 2)
            {
                int routineID;
                routineID = (data[2] * 256 + data[3]);
                buildString.append("\nRoutine ID: " + Utility::formatHexNum(routineID));
            }
            if (dataLen > 4)
            {
                buildString.append("\nBytes returned by routine: ");
                for (int i = 4; i < dataLen; i++)
                {
                    buildString.append(Utility::formatHexNum(data[i]) + " ");
                }
            }
            break;
        case UDS_SERVICES::REQUEST_DOWNLOAD:
        case UDS_SERVICES::REQUEST_UPLOAD:
            compType = data[1] >> 4;
            encType = data[1] & 0xF;
            buildString.append("Compression Type: " + QString::number(compType));
            if (compType == 0) buildString.append(" (none)");
            buildString.append("\n");
            buildString.append("Encryption Type: " + QString::number(encType));
            if (encType == 0) buildString.append(" (none)");
            buildString.append("\n");
            //subfunc byte specifies address and length format, then address, then size
            dataSize = data[2] >> 4;
            addrSize = data[2] & 0xF;
            if (dataLen > (dataSize + addrSize))
            {
                buildString.append("Address: 0x");
                for (int i = 0; i < addrSize; i++) buildString.append(QString::number(data[3 + i], 16).toUpper().rightJustified(2,'0'));
                buildString.append("\nSize: 0x");
                for (int i = 0; i < dataSize; i++) buildString.append(QString::number(data[3 + i + addrSize], 16).toUpper().rightJustified(2,'0'));
            }
            else
            {
                buildString.append("Message has insufficient bytes to properly decode address and size!");
            }
            break;
        case UDS_SERVICES::REQUEST_DOWNLOAD + 0x40:
        case UDS_SERVICES::REQUEST_UPLOAD + 0x40:
            dataSize = data[1] >> 4;
            buildString.append("\nMax Size of data block: 0x");
            for (int i = 0; i < dataSize; i++) buildString.append(QString::number(data[2 + i], 16).toUpper().rightJustified(2,'0'));
            break;
        case UDS_SERVICES::TRANSFER_DATA:
        case UDS_SERVICES::TRANSFER_DATA + 0x40:
            buildString.append("\nBlock Sequence: " + QString::number(data[1], 16) + "\nPayload: ");
            for (int i = 2; i < dataLen; i++)
            {
                buildString.append(Utility::formatHexNum(data[i]) + " ");
            }
            break;
        case UDS_SERVICES::REQ_TRANS_EXIT:
        case UDS_SERVICES::REQ_TRANS_EXIT + 0x40:
            buildString.append("\nPayload: ");
            for (int i = 1; i < dataLen; i++)
            {
                buildString.append(Utility::formatHexNum(data[i]) + " ");
            }
            break;
        case UDS_SERVICES::REQ_FILE_TRANS:
            break;
        case UDS_SERVICES::WRITE_BY_ADDR:
            break;
        }
    }

    //qDebug() << buildString;
    return buildString;
}

//Little shim functions that drop straight through to the ISO_TP handler
void UDS_HANDLER::setProcessAllIDs(bool state)
{
    isoHandler->setProcessAll(state);
}

void UDS_HANDLER::addFilter(uint32_t pBusId, uint32_t ID, uint32_t mask)
{
    isoHandler->addFilter(pBusId, ID, mask);
}

void UDS_HANDLER::removeFilter(uint32_t pBusId, uint32_t ID, uint32_t mask)
{
    isoHandler->removeFilter(pBusId, ID, mask);
}

void UDS_HANDLER::clearAllFilters()
{
    isoHandler->clearAllFilters();
}

