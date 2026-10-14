#ifndef UDSFIRMWAREUPLOADERWINDOW_H
#define UDSFIRMWAREUPLOADERWINDOW_H

#include <QDialog>
#include <QTimer>
#include <QElapsedTimer>
#include "can_structs.h"
#include "bus_protocols/handler_factory.h"
#include "bus_protocols/uds_handler.h"

namespace Ui {
class UDSFirmwareUploaderWindow;
}

enum UPLOAD_STATE
{
    STATE_IDLE,
    STATE_SEND_DIAG_SESSION,
    STATE_WAIT_DIAG_RESP,
    STATE_SEND_SECURITY_SEED,
    STATE_WAIT_SEED,
    STATE_SEND_SECURITY_KEY,
    STATE_WAIT_KEY_RESP,
    STATE_SEND_REQUEST_DOWNLOAD,
    STATE_WAIT_DOWNLOAD_RESP,
    STATE_SEND_DATA,
    STATE_WAIT_DATA_RESP,
    STATE_SEND_TRANSFER_EXIT,
    STATE_WAIT_EXIT_RESP,
    STATE_SEND_ECU_RESET,
    STATE_WAIT_RESET_RESP,
    STATE_COMPLETE,
    STATE_ERROR
};

class UDSFirmwareUploaderWindow : public QDialog
{
    Q_OBJECT

public:
    explicit UDSFirmwareUploaderWindow(const QVector<CommFrame> *frames, QWidget *parent = 0);
    ~UDSFirmwareUploaderWindow();

private slots:
    void handleLoadFile();
    void handleStartStop();
    void handleAbort();
    void gotUDSReply(UDS_MESSAGE msg);
    void timeoutTriggered();
    void testerPresentTick();

private:
    void logMessage(const QString &msg);
    void setState(UPLOAD_STATE newState);
    void startUpload();
    void stopUpload(bool resetState = true);
    void sendDiagSession();
    void sendSecuritySeed();
    void sendSecurityKey(const QByteArray &seed);
    void sendRequestDownload();
    void sendDataBlock();
    void sendTransferExit();
    void sendECUReset();
    void sendTesterPresent();
    void handleNRC(int nrc);
    void advanceState();

    Ui::UDSFirmwareUploaderWindow *ui;
    const QVector<CommFrame> *modelFrames;
    UDS_HANDLER *udsHandler;
    UPLOAD_STATE currentState;
    QTimer *timeoutTimer;
    QTimer *testerTimer;
    QElapsedTimer *progressTimer;

    QByteArray firmwareData;
    int firmwareSize;
    int busId;
    uint32_t targetId;
    uint32_t responseId;
    int sessionType;
    int securityLevel;
    uint32_t flashAddr;
    int blockSequenceCounter;
    int maxBlockLength;
    int currentDataPos;
    bool useSecurity;
};

#endif // UDSFIRMWAREUPLOADERWINDOW_H
