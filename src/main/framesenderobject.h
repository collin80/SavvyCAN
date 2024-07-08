#ifndef FRAMESENDEROBJECT_H
#define FRAMESENDEROBJECT_H

#include "connections/canconmanager.h"
#include "dbc/dbchandler.h"
#include "main/can_structs.h"
#include "main/can_trigger_structs.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QThread>
#include <QTimer>

class FrameSenderObject : public QObject
{
  Q_OBJECT

public:
  FrameSenderObject(const QVector<CANFrame>* frames);
  ~FrameSenderObject();

public slots:
  /**
   * @brief start the device. This calls piStarted
   * @note starts the working thread if required (piStart is called in the working thread context)
   */
  void initialize();

  /**
   * @brief stop the device. This calls piStop
   * @note if a working thread is used, piStop is called before exiting the working thread
   */
  void finalize();

  void startSending();
  void stopSending();

  void addSendRecord(FrameSendData record);
  void removeSendRecord(int idx);
  FrameSendData* getSendRecordRef(int idx);

signals:

private slots:
  void timerTriggered();
  void updatedFrames(int);

private:
  QList<CANFrame> sendingList;
  int currentPosition;
  QTimer* sendingTimer;
  QElapsedTimer sendingElapsed;
  quint64 sendingLastTimeStamp;
  int statusCounter;
  QList<FrameSendData> sendingData;
  QThread* mThread_p;
  QHash<int, CANFrame> frameCache;  // hash with frame ID as the key and the most recent frame as the value
  const QVector<CANFrame>* modelFrames;
  bool inhibitChanged = false;
  QMutex mutex;
  DBCHandler* dbcHandler;

  void doModifiers(int);
  int fetchOperand(int, ModifierOperand);
  CANFrame* lookupFrame(int, int);
  void buildFrameCache();
  void processIncomingFrame(CANFrame* frame);

  /**
   * @brief starts the device
   */
  void piStart();

  /**
   * @brief stops the device
   */
  void piStop();
};

#endif  // FRAMESENDEROBJECT_H
