#ifndef CANCONMANAGER_H
#define CANCONMANAGER_H

#include <QObject>
#include <QTimer>

#include "canconnection.h"

class CANConManager : public QObject
{
    Q_OBJECT

public:
    static CANConManager* getInstance();
    virtual ~CANConManager();

    void add(CANConnection* pConn_p);
    void remove(CANConnection* pConn_p);
    QList<CANConnection*>& getConnections();

    CANConnection* getByName(const QString& pName) const;

signals:
    void framesReceived(CANConnection* pConn_p, QVector<CANFrame>& pFrames);

private slots:
        void refreshCanList();

private:
    explicit CANConManager(QObject *parent = 0);
    void refreshConnection(CANConnection* pConn_p);

    static CANConManager*  mInstance;
    QList<CANConnection*>  mConns;
    QTimer                 mTimer;
};

#endif // CANCONNECTIONMODEL_H

