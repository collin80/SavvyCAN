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

    uint64_t getTimeBasis();
    void resetTimeBasis();

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

    /**
     * @brief Add a new filter for the targetted frames. If a frame matches it will immediately be sent via the targettedFrameReceived signal
     * @param pBusId - Which bus to bond to. -1 for any, otherwise a bitfield of buses (but 0 = first bus, etc)
     * @param target - The filter to use for selected targetted frames
     * @return true if filter was able to be added, false otherwise.
     */
    bool addTargettedFrame(int pBusId, const CANFlt &target);

    /**
     * @brief Try to find a matching filter in the list and remove it, no longer targetting those frames
     * @param pBusId - Which bus to bond to. Doesn't have to match the call to addTargettedFrame exactly. You could disconnect just one bus for instance.
     * @param target - The filter that was set
     * @return true if filter was found and deleted, false otherwise.
     */
    bool removeTargettedFrame(int pBusId, const CANFlt &target);

signals:
    void framesReceived(CANConnection* pConn_p, QVector<CANFrame>& pFrames);
    void targettedFrameReceived(CANFrame frame);
    void connectionStatusUpdated(int conns);

private slots:
    void refreshCanList();
    void gotTargettedFrame(CANFrame frame);

private:
    explicit CANConManager(QObject *parent = 0);
    void refreshConnection(CANConnection* pConn_p);

    static CANConManager*  mInstance;
    QList<CANConnection*>  mConns;
    QTimer                 mTimer;
    uint64_t               mTimestampBasis;
};

#endif // CANCONNECTIONMODEL_H

