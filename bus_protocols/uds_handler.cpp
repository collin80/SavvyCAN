#include "uds_handler.h"
#include "connections/canconmanager.h"
#include "mainwindow.h"
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

UDS_HANDLER* UDS_HANDLER::mInstance = NULL;

UDS_HANDLER* UDS_HANDLER::getInstance()
{
    if(!mInstance)
    {
        mInstance = new UDS_HANDLER();
        mInstance->modelFrames = MainWindow::getReference()->getCANFrameModel()->getListReference();
    }

    return mInstance;
}

UDS_HANDLER::UDS_HANDLER()
{
    isReceiving = false;
    useExtendedAddressing = false;
}

void UDS_HANDLER::gotISOTPFrame(ISOTP_MESSAGE &msg)
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
    udsMsg.service = msg.data.at(0);
    udsMsg.subFunc = msg.data.at(1);
    udsMsg.data = msg.data.mid(1, -1); //don't copy data[0] which was service number
    emit newUDSMessage(udsMsg);
}

void UDS_HANDLER::setReception(bool mode)
{
    if (isReceiving == mode) return;

    isReceiving = mode;

    if (isReceiving)
    {
        connect(ISOTP_HANDLER::getInstance(), SIGNAL(newISOMessage(ISOTP_MESSAGE&)), this, SLOT(gotISOTPFrame(ISOTP_MESSAGE&)));
        ISOTP_HANDLER::getInstance()->setReception(true); //must enable ISOTP reception too.
        qDebug() << "Enabling reception of ISO-TP frames in UDS handler";
    }
    else
    {
        disconnect(ISOTP_HANDLER::getInstance(), SIGNAL(newISOMessage(ISOTP_MESSAGE&)), this, SLOT(gotISOTPFrame(ISOTP_MESSAGE&)));
        //can't disable ISOTP reception because something else might be using it.
        qDebug() << "Disabling reception of ISOTP frames in UDS handler";
    }
}

void UDS_HANDLER::sendUDSFrame(int bus, int ID, int service, QVector<unsigned char> payload)
{
    QVector<unsigned char> data;
    if (bus < 0) return;
    if (bus >= CANConManager::getInstance()->getNumBuses()) return;
    if (service < 0 || service > 0xFF) return;
    data.append(service);
    data.append(payload);
    ISOTP_HANDLER::getInstance()->sendISOTPFrame(bus, ID, data);
    qDebug() << "Sent UDS service: " << getServiceShortDesc(service) << " on bus " << bus;
}

void UDS_HANDLER::sendUDSFrame(const UDS_MESSAGE &msg)
{
    QVector<unsigned char> data;
    if (msg.bus < 0) return;
    if (msg.bus >= CANConManager::getInstance()->getNumBuses()) return;
    if (msg.service < 0 || msg.service > 0xFF) return;
    data.append(msg.service);
    data.append(msg.data);
    ISOTP_HANDLER::getInstance()->sendISOTPFrame(msg.bus, msg.ID, data);
    qDebug() << "Sent UDS service: " << getServiceShortDesc(msg.service) << " on bus " << msg.bus;
}

QString UDS_HANDLER::getServiceShortDesc(int service)
{
    foreach (CODE_STRUCT code, UDS_SERVICE_DESC)
    {
        if (code.code == service) return code.shortDesc;
    }
    return QString();
}

QString UDS_HANDLER::getServiceLongDesc(int service)
{
    foreach (CODE_STRUCT code, UDS_SERVICE_DESC)
    {
        if (code.code == service) return code.longDesc;
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

