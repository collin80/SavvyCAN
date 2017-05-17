#ifndef ISOTP_HANDLER_H
#define ISOTP_HANDLER_H

#include <Qt>
#include <QObject>
#include <QDebug>
#include "can_structs.h"
#include "mainwindow.h"
#include "canframemodel.h"
#include "isotp_message.h"

class ISOTP_HANDLER : public QObject
{
    Q_OBJECT

public:
    ISOTP_HANDLER();
    void setExtendedAddressing(bool mode);
    static ISOTP_HANDLER* getInstance();
    void setReception(bool mode); //set whether to accept and forward frames or not
    void sendISOTPFrame(int bus, int ID, QVector<unsigned char> data);

public slots:
    void updatedFrames(int);
    void rapidFrames(const CANConnection* conn, const QVector<CANFrame>& pFrames);

signals:
    void newISOMessage(ISOTP_MESSAGE &msg);

private:
    QList<ISOTP_MESSAGE> messageBuffer;
    const QVector<CANFrame> *modelFrames;
    bool useExtendedAddressing;
    bool isReceiving;

    void processFrame(const CANFrame &frame);
    void checkNeedFlush(uint64_t ID);

    static ISOTP_HANDLER*  mInstance;
};

#endif // ISOTP_HANDLER_H
