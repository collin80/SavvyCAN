#ifndef ISOTP_DECODER_H
#define ISOTP_DECODER_H

#include <QObject>
#include <QDebug>
#include "can_structs.h"

enum OBDII_FUNCTS
{
    UDS_OBDII_SHOW_CURRENT = 1,
    UDS_OBDII_SHOW_FREEZE = 2,
    UDS_OBDII_SHOW_STORED_DTC = 3,
    UDS_OBDII_CLEAR_DTC = 4,
    UDS_OBDII_TEST_O2 = 5,
    UDS_OBDII_TEST_RESULTS = 6,
    UDS_OBDII_SHOW_PENDING_DTC = 7,
    UDS_OBDII_CONTROL_DEVICES = 8,
    UDS_OBDII_VEH_INFO = 9,
    UDS_OBDII_PERM_DTC = 0xA
};

enum UDS_FUNCTS
{
    UDS_DIAG_CONTROL=0x10,
    UDS_ECU_RESET=0x11,
    UDS_CLEAR_DIAG=0x14,
    UDS_READ_DTC=0x19,
    UDS_READ_BY_ID=0x22,
    UDS_READ_BY_ADDR=0x23,
    UDS_READ_SCALING_ID=0x24,
    UDS_SECURITY_ACCESS=0x27,
    UDS_COMM_CTRL=0x28,
    UDS_READ_DATA_ID_PERIODIC=0x2A,
    UDS_DYNAMIC_DATA_DEFINE=0x2C,
    UDS_WRITE_BY_ID=0x2E,
    UDS_IO_CTRL=0x2F,
    UDS_ROUTINE_CTRL=0x31,
    UDS_REQUEST_DOWNLOAD=0x34,
    UDS_REQUEST_UPLOAD=0x35,
    UDS_TRANSFER_DATA=0x36,
    UDS_REQ_TRANS_EXIT=0x37,
    UDS_REQ_FILE_TRANS=0x38,
    UDS_WRITE_BY_ADDR=0x3D,
    UDS_TESTER_PRESENT=0x3E,
    UDS_ACCESS_TIMING=0x83,
    UDS_SECURED_DATA_TRANS=0x84,
    UDS_CTRL_DTC_SETTINGS=0x85,
    UDS_RESPONSE_ON_EVENT=0x86,
    UDS_RESPONSE_LINK_CTRL=0x87
};

class ISOTP_DECODER : public QObject
{
    Q_OBJECT

public:
    explicit ISOTP_DECODER(const QVector<CANFrame> *frames, QObject *parent = 0);
    void setExtendedAddressing(bool mode);

public slots:
    void updatedFrames(int);

signals:
    void newISOMessage(ISOTP_MESSAGE &msg);

private:
    QList<ISOTP_MESSAGE> messageBuffer;
    const QVector<CANFrame> *modelFrames;
    bool useExtendedAddressing;

    void processFrame(const CANFrame &frame);
    void checkNeedFlush(uint64_t ID);
};

#endif // ISOTP_DECODER_H
