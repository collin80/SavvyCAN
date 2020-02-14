#ifndef UDS_HANDLER_H
#define UDS_HANDLER_H

#include <Qt>
#include <QObject>
#include <QDebug>
#include "can_structs.h"
#include "isotp_message.h"

class ISOTP_HANDLER;

namespace UDS_SERVICES
{
    enum
    {
    OBDII_SHOW_CURRENT = 1,
    OBDII_SHOW_FREEZE = 2,
    OBDII_SHOW_STORED_DTC = 3,
    OBDII_CLEAR_DTC = 4,
    OBDII_TEST_O2 = 5,
    OBDII_TEST_RESULTS = 6,
    OBDII_SHOW_PENDING_DTC = 7,
    OBDII_CONTROL_DEVICES = 8,
    OBDII_VEH_INFO = 9,
    OBDII_PERM_DTC = 0xA,
    DIAG_CONTROL = 0x10,
    ECU_RESET = 0x11,
    GMLAN_READ_FAILURE_RECORD = 0x12,
    CLEAR_DIAG = 0x14,
    READ_DTC = 0x19,
    GMLAN_READ_DIAGNOSTIC_ID = 0x1A,
    RETURN_TO_NORMAL = 0x20,
    READ_BY_ID = 0x22,
    READ_BY_ADDR = 0x23,
    READ_SCALING_ID = 0x24,
    SECURITY_ACCESS = 0x27,
    COMM_CTRL = 0x28,
    READ_DATA_ID_PERIODIC = 0x2A,
    DYNAMIC_DATA_DEFINE = 0x2C,
    DEFINE_PID_BY_ADDR = 0x2D,
    WRITE_BY_ID = 0x2E,
    IO_CTRL = 0x2F,
    ROUTINE_CTRL = 0x31,
    REQUEST_DOWNLOAD = 0x34,
    REQUEST_UPLOAD = 0x35,
    TRANSFER_DATA = 0x36,
    REQ_TRANS_EXIT = 0x37,
    REQ_FILE_TRANS = 0x38,
    GMLAN_WRITE_DID = 0x3B,
    WRITE_BY_ADDR = 0x3D,
    TESTER_PRESENT = 0x3E,
    NEG_RESPONSE = 0x7F,
    ACCESS_TIMING = 0x83,
    SECURED_DATA_TRANS = 0x84,
    CTRL_DTC_SETTINGS = 0x85,
    RESPONSE_ON_EVENT = 0x86,
    RESPONSE_LINK_CTRL = 0x87,
    GMLAN_REPORT_PROG_STATE = 0xA2,
    GMLAN_ENTER_PROG_MODE = 0xA5,
    GMLAN_CHECK_CODES = 0xA9,
    GMLAN_READ_DPID = 0xAA,
    GMLAN_DEVICE_CTRL = 0xAE
    };
}

struct CODE_STRUCT
{
    int code;
    QString shortDesc;
    QString longDesc;
};

class UDS_MESSAGE: public ISOTP_MESSAGE
{
public:
    int service;
    int subFunc;
    int subFuncLen;
    bool isErrorReply;

    UDS_MESSAGE();
};

class UDS_HANDLER : public QObject
{
    Q_OBJECT

public:
    UDS_HANDLER();
    ~UDS_HANDLER();
    void setExtendedAddressing(bool mode);
    static UDS_HANDLER* getInstance();
    void setReception(bool mode); //set whether to accept and forward frames or not
    void sendUDSFrame(const UDS_MESSAGE &msg);
    void setProcessAllIDs(bool state);
    void setFlowCtrl(bool state);
    void addFilter(uint32_t pBusId, uint32_t ID, uint32_t mask);
    void removeFilter(uint32_t pBusId, uint32_t ID, uint32_t mask);
    void clearAllFilters();

    QString getServiceShortDesc(int service);
    QString getServiceLongDesc(int service);
    QString getNegativeResponseShort(int respCode);
    QString getNegativeResponseLong(int respCode);
    QString getShortDesc(QVector<CODE_STRUCT> &codeVector, int code);
    QString getLongDesc(QVector<CODE_STRUCT> &codeVector, int code);
    QString getDetailedMessageAnalysis(const UDS_MESSAGE &msg);

public slots:
    void gotISOTPFrame(ISOTP_MESSAGE msg);

signals:
    void newUDSMessage(UDS_MESSAGE msg);

private:
    QList<ISOTP_MESSAGE> messageBuffer;
    const QVector<CANFrame> *modelFrames;
    bool isReceiving;
    bool useExtendedAddressing;

    void processFrame(const CANFrame &frame);

    ISOTP_HANDLER *isoHandler;
};


#endif // UDS_HANDLER_H
