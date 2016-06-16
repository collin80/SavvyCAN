#ifndef CANCONNECTION_H
#define CANCONNECTION_H

#include <Qt>
#include <QObject>
#include "utils/lfqueue.h"
#include "can_structs.h"
#include "canbus.h"
#include "canconconst.h"


class CANConnection : public QObject
{
    Q_OBJECT

public:

    /**
     * @brief CANConnection constructor
     * @param pPort: string containing port name
     * @param pType: the type of connection @ref CANCon::type
     * @param pNumBuses: the number of buses the device has
     * @param pQueueLen: the length of the lock free queue to use
     * @param pUseThread: if set to true, object will be execute in a dedicated thread
     */
    CANConnection(QString pPort, CANCon::type pType, int pNumBuses,
                  int pQueueLen, bool pUseThread);
    /**
     * @brief CANConnection destructor
     */

    virtual ~CANConnection();

    /**
     * @brief getNumBuses
     * @return returns the number of buses of the device
     */
    int getNumBuses();

    /**
     * @brief getPort
     * @return returns the port name of the device
     */
    QString getPort();

    /**
     * @brief getQueue is call by reader to get a reference on the queue to monitor
     * @return the lock free queue of the device
     */
    LFQueue<CANFrame>& getQueue();

    /**
     * @brief getType
     * @return the @ref CANCon::type of the device
     */
    CANCon::type getType();

    /**
     * @brief getStatus
     * @return the @ref CANCon::status of the device (either connected or not)
     */
    CANCon::status getStatus();


signals:
    void error(const QString &);
    void deviceInfo(int, int); //First param = driver version (or version of whatever you want), second param a status byte

    //bus number, bus speed, status (bit 0 = enabled, 1 = single wire, 2 = listen only)
    //3 = Use value stored for enabled, 4 = use value passed for single wire, 5 = use value passed for listen only
    //6 = use value passed for speed. This allows bus status to be updated but set that some things aren't really
    //being passed. Just set for things that really are being updated.
    void busStatus(int, int, int);

    /**
     * @brief event sent when the CANCon::status of the connection changes (connected->not_connected or the other way round)
     * @param pStatus: the new status of the device
     */
    void status(CANCon::status pStatus);


public slots:

    /**
     * @brief start the device, this calls piStarted
     * @note starts the working thread if required (piStarted in the working thread context)
     */
    void start();

    /**
     * @brief stop the device, this calls piStop
     * @note if a working thread is used, piStop is called before exiting the working thread
     */
    void stop();

    /**
     * @brief setBusSettings
     * @param pBusIdx: the index of the bus for which settings have to be set
     * @param pBus: the settings to set
     * @note this calls piSetBusSettings in the working thread context (if one has been started)
     */
    void setBusSettings(int pBusIdx, CANBus pBus);

    /**
     * @brief getBusSettings
     * @param pBusIdx: the index of the bus for which settings have to be retrieved
     * @param pBus: the CANBus struct to fill with information
     * @return true if operation succeeds, false if pBusIdx is invalid or bus has not been configured yet
     * @note this calls piGetBusSettings in the working thread context (if one has been started)
     */
    bool getBusSettings(int pBusIdx, CANBus& pBus);

    /**
     * @brief suspends/restarts data capture
     * @param pSuspend: suspends capture if true else restarts it
     * @note this calls piSuspend in the working thread context (if one has been started)
     * @note the caller will not access the queue when capture is suspended, so it is safe for callee to flush the queue
     */
    void suspend(bool pSuspend);

    void sendFrame(const CANFrame&);
    void sendFrameBatch(const QList<CANFrame>&);

protected:

    /**
     * @brief setStatus
     * @param pStatus: the status to set
     * @note multithread safe (can be used while another thread calls @ref getStatus)
     */
    void setStatus(CANCon::status pStatus);

    /**
     * @brief isConfigured
     * @param pBusId
     * @return true if bus is configured
     * @note NOT multithread safe
     */
    bool isConfigured(int pBusId);

    /**
     * @brief setConfigured
     * @param pBusId
     * @param pConfigured
     * @note it is not necessary to call this function to set pBusId configured, it is enough to call @ref setBusConfig
     * @note NOT multithread safe
     */
    void setConfigured(int pBusId, bool pConfigured);

    /**
     * @brief getBusConfig
     * @param pBusId
     * @param pBus
     * @return true if operation succeeds, false if pBusIdx is invalid or bus has not been configured yet
     * @note NOT multithread safe
     */
    bool getBusConfig(int pBusId, CANBus& pBus);

    /**
     * @brief setBusConfig
     * @param pBusId: the index of the bus for which settings have to be set
     * @param pBus: the settings to set
     * @note NOT multithread safe
     */
    void setBusConfig(int pBusId, CANBus& pBus);

    /**
     * @brief isCapSuspended
     * @return true if the capture is suspended
     * @note NOT multithread safe
     */
    bool isCapSuspended();

    /**
     * @brief setCapSuspended
     * @param pIsSuspended
     * @note NOT multithread safe
     */
    void setCapSuspended(bool pIsSuspended);

protected:

    /**
     * @brief start the device
     * @note start a working thread here if needed
     */
    virtual void piStarted() = 0;

    /**
     * @brief stop the device
     * @note stop the working thread here if one has been started
     */
    virtual void piStop() = 0;

    /**
     * @brief setBusSettings
     * @param pBusIdx: the index of the bus for which settings have to be set
     * @param pBus: the settings to set
     */
    virtual void piSetBusSettings(int pBusIdx, CANBus pBus) = 0;

    /**
     * @brief getBusSettings
     * @param pBusIdx: the index of the bus for which settings have to be retrieved
     * @param pBus: the CANBus struct to fill with information
     * @return true if operation succeeds, false if pBusIdx is invalid or bus has not been configured yet
     */
    virtual bool piGetBusSettings(int pBusIdx, CANBus& pBus) = 0;

    /**
     * @brief suspends/restarts data capture
     * @param pSuspend: suspends capture if true else restarts it
     * @note the caller will not access the queue when capture is suspended, so it is safe for callee to flush the queue
     */
    virtual void piSuspend(bool pSuspend) = 0;

    virtual void piSendFrame(const CANFrame&) = 0;
    virtual void piSendFrameBatch(const QList<CANFrame>&) = 0;


private:
    CANBus*             mBus;
    bool*               mConfigured;
    LFQueue<CANFrame>   mQueue;
    const int           mNumBuses;
    const QString       mPort;
    const CANCon::type  mType;
    bool                mIsCapSuspended;
    QAtomicInt          mStatus;
    QThread*            mThread_p;
};

#endif // CANCONNECTION_H
