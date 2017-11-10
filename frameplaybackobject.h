#ifndef FRAMEPLAYBACKOBJECT_H
#define FRAMEPLAYBACKOBJECT_H

#include <QElapsedTimer>
#include <QTimer>
#include <QHash>
#include <QThread>
#include <QDebug>
#include "can_structs.h"
#include "connections/canconmanager.h"

//one entry in the sequence of data to use
struct SequenceItem
{
    QString filename;
    QVector<CANFrame> data;
    QHash<int, bool> idFilters;
    int maxLoops;
    int currentLoopCount;
};

/*
  broken out functionality that used to be in frameplaybackwindow. That mixed code for Model/View/Controller into one giant
  class. I wasn't too concerned about that but the problem is that all code within a GUI object runs in GUI context and that
  is most certainly not a feature, especially if you have a multi-core CPU. This object lives in a separate thread from the GUI
  and thus is better scheduled and doesn't block the GUI thread. Really all functionality in this program should be broken into
  a separate thread from GUI if it is prone to running a long time and/or taking up a lot of CPU time (unless it really does
  have to interface with the GUI in some way. All gui touching code must run on its thread).
*/
class FramePlaybackObject : public QObject
{
    Q_OBJECT

public:
    FramePlaybackObject();
    ~FramePlaybackObject();

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

    void startPlaybackForward();
    void startPlaybackBackward();
    void stepPlaybackForward();
    void stepPlaybackBackward();
    void stopPlayback();
    void pausePlayback();

    void setSequenceObject(SequenceItem *item);
    void setUseOriginalTiming(bool state);
    void setSendingBus(int bus);
    void setPlaybackInterval(int interval);
    void setPlaybackBurst(int burst);
    void setNumBuses(int buses);

signals:
    void EndOfFrameCache(); //we hit the end/beginning of the frame cache (depending on direction of playback)
    void statusUpdate(int frameNum);

private slots:
    void timerTriggered();

private:
     QList<CANFrame> sendingBuffer;
     SequenceItem *currentSeqItem;
     int currentPosition;
     QTimer *playbackTimer;
     QElapsedTimer playbackElapsed;
     quint64 playbackLastTimeStamp;
     int playbackInterval;
     int playbackBurst;
     int numBuses;
     int statusCounter;
     bool playbackActive;
     bool playbackForward;
     bool useOrigTiming;
     int whichBusSend;
     QThread*            mThread_p;

     quint64 updatePosition(bool forward);
     quint64 peekPosition(bool forward);
     /**
      * @brief starts the device
      */
     void piStart();

     /**
      * @brief stops the device
      */
     void piStop();
};

#endif // FRAMEPLAYBACKOBJECT_H
