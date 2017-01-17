#ifndef FRAMEFILEIO_H
#define FRAMEFILEIO_H

//Class full of static methods that load and save canbus frames to/from various file formats

#include "config.h"
#include <Qt>
#include <QApplication>
#include <QObject>
#include <QVector>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QFileDialog>
#include "can_structs.h"
#include "utility.h"

class FrameFileIO: public QObject
{
    Q_OBJECT

public:
    FrameFileIO();

    //these present a GUI to the user and allow them to pick the file to load/save
    //The QString returns the filename that was selected and so is really a sort of return value
    //The QVector is used as either the target for loading or the source for saving.
    //These routines call the below loading/saving functions so no need to use them directly if you don't want.
    static bool loadFrameFile(QString &, QVector<CANFrame>*);
    static bool saveFrameFile(QString &, const QVector<CANFrame>*);

    //These do the actual loading and saving and can be used directly if you'd prefer
    static bool loadCRTDFile(QString, QVector<CANFrame>*);
    static bool loadNativeCSVFile(QString, QVector<CANFrame>*);
    static bool loadGenericCSVFile(QString, QVector<CANFrame>*);
    static bool loadLogFile(QString, QVector<CANFrame>*);
    static bool loadMicrochipFile(QString, QVector<CANFrame>*);
    static bool loadTraceFile(QString, QVector<CANFrame>*);
    static bool loadIXXATFile(QString, QVector<CANFrame>*);
    static bool loadCANDOFile(QString, QVector<CANFrame>*);
    static bool loadVehicleSpyFile(QString, QVector<CANFrame>*);
    static bool saveCRTDFile(QString, const QVector<CANFrame>*);    
    static bool saveNativeCSVFile(QString, const QVector<CANFrame>*);
    static bool saveGenericCSVFile(QString, const QVector<CANFrame>*);
    static bool saveLogFile(QString, const QVector<CANFrame>*);
    static bool saveMicrochipFile(QString, const QVector<CANFrame>*);
    static bool saveTraceFile(QString, const QVector<CANFrame>*);
    static bool saveIXXATFile(QString, const QVector<CANFrame>*);
    static bool saveCANDOFile(QString, const QVector<CANFrame>*);
    static bool saveVehicleSpyFile(QString, const QVector<CANFrame>*);
    static bool loadCanDumpFile(QString, QVector<CANFrame>*);
    static bool loadPCANFile(QString, QVector<CANFrame>*);
};

#endif // FRAMEFILEIO_H
