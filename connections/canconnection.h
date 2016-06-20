#ifndef CANCONNECTION_H
#define CANCONNECTION_H

#include <Qt>
#include <QObject>
#include "utils/lfqueue.h"
#include "can_structs.h"
#include "canbus.h"
#include "canconconst.h"

struct BusData;

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
    CANConnection(QString pPort,
                  CANCon::type pType,
                  int pNumBuses,
                  int pQueueLen,
                  bool pUseThread);
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
     * @brief getQueue
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

    /**
     * @brief set the callback function, this call has to be placed before calling start
     * @param pCallback
     * @return true if callback can be set
     */
    bool setCallback(std::function<void(CANCon::cbtype)> pCallback);


signals:
    /*not implemented yet */
    void error(const QString &);
    void deviceInfo(int, int); //First param = driver version (or version of whatever you want), second param a status byte

    //bus number, bus speed, status (bit 0 = enabled, 1 = single wire, 2 = listen only)
    //3 = Use value stored for enabled, 4 = use value passed for single wire, 5 = use value passed for listen only
    //6 = use value passed for speed. This allows bus status to be updated but set that some things aren't really
    //being passed. Just set for things that really are being updated.
    void busStatus(int, int, int);

    /**
     * @brief event sent when a frame matching a filter set with notification hs been received
     */
    void notify();

    /**
     * @brief event emitted when the CANCon::status of the connection changes (connected->not_connected or the other way round)
     * @param pStatus: the new status of the device
     */
    void status(CANCon::status pStatus);


public slots:

    /**
     * @brief start the device. This calls piStarted
     * @note starts the working thread if required (piStarted is called in the working thread context)
     */
    void start();

    /**
     * @brief stop the device. This calls piStop
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
     * @note this calls piSuspend (in the working thread context if one has been started)
     * @note the caller shall not access the queue when capture is suspended, it is then safe for callee to flush the queue
     */
    void suspend(bool pSuspend);

    /**
     * @brief provides device with the frame to send
     * @param pFrame: the frame to send
     * @note this calls piSendFrame (in the working thread context if one has been started)
     */
    void sendFrame(const CANFrame& pFrame);

    /**
     * @brief provides device with a list of frames to send
     * @param pFrame: the list of frames to send
     * @note this calls piSendFrameBatch (in the working thread context if one has been started)
     */
    void sendFrameBatch(const QList<CANFrame>& pFrames);

    /**
     * @brief sets a filter list. Filters can be used to send a signal or filter out messages
     * @param pFilters: a vector of can filters
     * @param pFilterOut: if set to true, can frames not matching a filter are discarded
     * @return true if filters have been set, false if busid is invalid
     */
    bool setFilters(int pBusId, const QVector<CANFlt>& pFilters, bool pFilterOut);

protected:

    /**
     * @brief setStatus
     * @param pStatus: the status to set
     */
    void setStatus(CANCon::status pStatus);

    /**
     * @brief isConfigured
     * @param pBusId
     * @return true if bus is configured
     */
    bool isConfigured(int pBusId);

    /**
     * @brief setConfigured
     * @param pBusId
     * @param pConfigured
     * @note it is not necessary to call this function to set pBusId configured, it is enough to call @ref setBusConfig
     */
    void setConfigured(int pBusId, bool pConfigured);

    /**
     * @brief getBusConfig
     * @param pBusId
     * @param pBus
     * @return true if operation succeeds, false if pBusIdx is invalid or bus has not been configured yet
     */
    bool getBusConfig(int pBusId, CANBus& pBus);

    /**
     * @brief setBusConfig
     * @param pBusId: the index of the bus for which settings have to be set
     * @param pBus: the settings to set
     */
    void setBusConfig(int pBusId, CANBus& pBus);

    /**
     * @brief isCapSuspended
     * @return true if the capture is suspended
     */
    bool isCapSuspended();

    /**
     * @brief setCapSuspended
     * @param pIsSuspended
     */
    void setCapSuspended(bool pIsSuspended);

    /**
     * @brief call callback function
     * @param pCbType callback type (read or write)
     */
    void callback(CANCon::cbtype pCbType);

    /**
     * @brief used to check if a message shall be discarded. The function also update pNotify if a notification is expected for that message
     * @param pBusId: the bus id on which the frame has been received
     * @param pId: the id of the message to filter
     * @param pNotify: set to true if a notification is expected, else pNotify is not set
     * @return true if message shall be discarded
     */
    bool discard(int pBusId, quint32 pId, bool& pNotify);


protected:

    /**************************************************************/
    /***********     protected interface to implement       *******/
    /**************************************************************/

    /**
     * @brief starts the device
     */
    virtual void piStarted() = 0;

    /**
     * @brief stops the device
     */
    virtual void piStop() = 0;

    /**
     * @brief piSetBusSettings
     * @param pBusIdx: the index of the bus for which settings have to be set
     * @param pBus: the settings to set
     */
    virtual void piSetBusSettings(int pBusIdx, CANBus pBus) = 0;

    /**
     * @brief piGetBusSettings
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

    /**
     * @brief provides device with the frame to send
     * @param pFrame: the frame to send
     */
    virtual void piSendFrame(const CANFrame&) = 0;

    /**
     * @brief provides device with a list of frames to send
     * @param pFrame: the list of frames to send
     */
    virtual void piSendFrameBatch(const QList<CANFrame>&) = 0;

    /**
     * @brief sets a hardware filter list
     * @param pBusId: the bus id on which filters have to be set
     * @param pFilters: a vector of can filters, the notification flag of CANFilter is ignored
     * @note implementing this function is optional
     */
    virtual void piSetFilters(int pBusId, const QVector<CANFlt>& pFilters);

private:
    LFQueue<CANFrame>   mQueue;
    const int           mNumBuses;
    const QString       mPort;
    const CANCon::type  mType;
    bool                mIsCapSuspended;
    QAtomicInt          mStatus;
    bool                mStarted;
    std::function<void(CANCon::cbtype)> mCallback;
    BusData*            mBusData_p;
    QThread*            mThread_p;
};

#endif // CANCONNECTION_H
