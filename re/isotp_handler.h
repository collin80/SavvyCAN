#ifndef ISOTP_DECODER_H
#define ISOTP_DECODER_H

#include <Qt>
#include <QObject>
#include <QDebug>
#include "can_structs.h"

struct CODE_STRUCT
{
    int code;
    QString shortDesc;
    QString longDesc;
};

class ISOTP_HANDLER : public QObject
{
    Q_OBJECT

public:
    explicit ISOTP_HANDLER(const QVector<CANFrame> *frames, QObject *parent = 0);
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
