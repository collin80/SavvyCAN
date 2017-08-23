#include "uds_handler.h"
#include "connections/canconmanager.h"
#include "mainwindow.h"
#include "isotp_handler.h"
#include <QDebug>

QVector<CODE_STRUCT> UDS_SERVICE_DESC = {
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

QVector<CODE_STRUCT> UDS_NEG_RESPONSE =
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
    extended = false;
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
    UDS_MESSAGE udsMsg;
    udsMsg.bus = msg.bus;
    udsMsg.extended = msg.extended;
    udsMsg.ID = msg.ID;
    udsMsg.isReceived = msg.isReceived;
    udsMsg.timestamp = msg.timestamp;
    udsMsg.actualSize = msg.actualSize;
    udsMsg.len = msg.len;
    if (msg.data.length() > 0) {
        udsMsg.service = msg.data.at(0);
        if (udsMsg.service == 0x7F)
        {
            udsMsg.isErrorReply = true;
            if (msg.data.length() > 1)
            {
                udsMsg.service = msg.data.at(1);
                if (msg.data.length() > 2) udsMsg.subFunc = msg.data.at(2);
                else return;
            }
            else return;
            udsMsg.data = msg.data.mid(2, -1); //don't copy error byte nor service byte
        }
        else
        {
            udsMsg.isErrorReply = false;
            if (msg.data.length() > 1) udsMsg.subFunc = msg.data.at(1);
            udsMsg.data = msg.data.mid(1, -1); //don't copy service byte
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
    QVector<unsigned char> data;
    if (msg.bus < 0) return;
    if (msg.bus >= CANConManager::getInstance()->getNumBuses()) return;
    if (msg.service < 0 || msg.service > 0xFF) return;

    data.append(msg.service);
    for (int b = msg.subFuncLen - 1; b >= 0; b--)
    {
        data.append((msg.subFunc >> (8 * b)) & 0xFF);
    }

    data.append(msg.data);
    isoHandler->sendISOTPFrame(msg.bus, msg.ID, data);

    qDebug() << "Sent UDS service: " << getServiceShortDesc(msg.service) << " on bus " << msg.bus;
}

QString UDS_HANDLER::getServiceShortDesc(int service)
{
    foreach (CODE_STRUCT code, UDS_SERVICE_DESC)
    {
        if (code.code == service) return code.shortDesc;
        if (code.code == (service + 0x40)) return code.shortDesc;
    }
    return QString();
}

QString UDS_HANDLER::getServiceLongDesc(int service)
{
    foreach (CODE_STRUCT code, UDS_SERVICE_DESC)
    {
        if (code.code == service) return code.longDesc;
        if (code.code == (service + 0x40)) return code.longDesc;
    }
    return QString();
}

QString UDS_HANDLER::getNegativeResponseShort(int respCode)
{
    foreach (CODE_STRUCT code, UDS_NEG_RESPONSE)
    {
        if (code.code == respCode) return code.shortDesc;
    }
    return QString();
}

QString UDS_HANDLER::getNegativeResponseLong(int respCode)
{
    foreach (CODE_STRUCT code, UDS_NEG_RESPONSE)
    {
        if (code.code == respCode) return code.longDesc;
    }
    return QString();
}

//Little shim functions that drop straight through to the ISO_TP handler
void UDS_HANDLER::setProcessAllIDs(bool state)
{
    isoHandler->setProcessAll(state);
}

void UDS_HANDLER::addFilter(int pBusId, uint32_t ID, uint32_t mask)
{
    isoHandler->addFilter(pBusId, ID, mask);
}

void UDS_HANDLER::removeFilter(int pBusId, uint32_t ID, uint32_t mask)
{
    isoHandler->removeFilter(pBusId, ID, mask);
}

void UDS_HANDLER::clearAllFilters()
{
    isoHandler->clearAllFilters();
}

