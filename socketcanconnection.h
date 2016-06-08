#ifndef SOCKETCANCONNECTION_H
#define SOCKETCANCONNECTION_H

#include <QObject>
#include <QCanBus>

#include "canframemodel.h"
#include "canconnection.h"

class BUSConfig {
public:
    BUSConfig();
    void    reset();
    bool    operator ==(CANBus&);
    void    operator =(CANBus&);
    bool    isConfigured;
    int     speed;
    bool    listenOnly;
    bool    active;
};


class SocketCanConnection : public CANConnection
{
    Q_OBJECT

public:
    SocketCanConnection(CANFrameModel *, int);
    virtual ~SocketCanConnection() override;

public slots:
    virtual void updateBusSettings(CANBus) override;

private:
    void errorReceived(QCanBusDevice::CanBusError error) const;
    void framesWritten(qint64 count);
    void framesReceived();
    void disconnect();

    QCanBusDevice*  mDev_p;
    BUSConfig       mConf;
};

#endif // SOCKETCANCONNECTION_H
