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

    int getNumBuses();

    /**
     * @brief sendFrame sends a single frame out the desired bus
     * @param pFrame - reference to a CANFrame struct that has been filled out for sending
     * @return bool specifying whether the send succeeded or not
     * @note Finds which CANConnection object is responsible for this bus and automatically converts bus number to pass properly to CANConnection
     */
    bool sendFrame(const CANFrame& pFrame);

    //just the multi-frame version of above function.
    bool sendFrames(const QList<CANFrame>& pFrames);

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

