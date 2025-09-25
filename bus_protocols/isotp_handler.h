#pragma once

#include <Qt>
#include <QObject>
#include <QTimer>
#include <QHash>
#include "can_structs.h"
#include "isotp_message.h"
#include "canfilter.h"


class ISOTP_HANDLER : public QObject
{
    Q_OBJECT

    using CanSendCallback = std::function<void(const CANFrame& pFrame)>;
    using GetNoOfBusesCallback = std::function<int(void)>;
    using PendingConnection = std::function<QMetaObject::Connection(ISOTP_HANDLER*)>; // lambda to call connect() in setReception()

public:
    ISOTP_HANDLER(const QVector<CANFrame> &modelFrames,
                  CanSendCallback sendCb,
                  GetNoOfBusesCallback getBusesCb,
                  PendingConnection pendingConn
                );
    ~ISOTP_HANDLER();

    void setExtendedAddressing(bool mode);
    void setReception(bool mode); //set whether to accept and forward frames or not
    void setEmitPartials(bool mode);
    void sendISOTPFrame(int bus, int ID, QByteArray data);
    void setProcessAll(bool state);
    void setFlowCtrl(bool state);
    void addFilter(int pBusId, uint32_t ID, uint32_t mask);
    void removeFilter(int pBusId, uint32_t ID, uint32_t mask);
    void clearAllFilters();
    void setPadByte(char newpad);

public slots:
    void updatedFrames(int);
    void rapidFrames(const QVector<CANFrame>& pFrames);
    void frameTimerTick();

signals:
    void newISOMessage(ISOTP_MESSAGE msg);

private:
    QHash<uint32_t, ISOTP_MESSAGE> messageBuffer;
    QList<CANFrame> sendingFrames;
    QList<CANFilter> filters;
    const QVector<CANFrame> &modelFrames;
    bool useExtendedAddressing;
    bool isReceiving;
    bool waitingForFlow;
    int framesUntilFlow;
    bool processAll;
    bool issueFlowMsgs;
    bool sendPartialMessages;
    QTimer frameTimer;
    uint32_t lastSenderID;
    uint32_t lastSenderBus;
    char padByte;

    void processFrame(const CANFrame &frame);
    void checkNeedFlush(uint64_t ID);
    void updateIsoTpCanFrameInfo();

    CanSendCallback canSendCallback;
    GetNoOfBusesCallback getNoOfBusesCallback;

    PendingConnection pendingConnection;
    QMetaObject::Connection connection;

    // Info about ISO-TP frame formats
    struct FrameInfo {
        int cfAndSfPciOffset;
        int sfFrameLen;
        int ffPciOffset;
        int ffFrameLen;
        int cfFrameLen;
    } frameInfo;
};
