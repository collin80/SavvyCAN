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
    static bool loadFrameFile(QString &, QVector<CANFrame>*);
    static bool saveFrameFile(QString &, const QVector<CANFrame>*);
    static bool loadCanDumpFile(QString, QVector<CANFrame>*);

private:
    static QString unQuote(QString);
};

#endif // FRAMEFILEIO_H
