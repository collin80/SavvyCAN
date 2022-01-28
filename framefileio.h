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
    static bool autoDetectLoadFile(QString, QVector<CANFrame>*);
    static bool loadCRTDFile(QString, QVector<CANFrame>*);
    static bool loadNativeCSVFile(QString, QVector<CANFrame>*);
    static bool loadGenericCSVFile(QString, QVector<CANFrame>*);
    static bool loadLogFile(QString, QVector<CANFrame>*);
    static bool loadMicrochipFile(QString, QVector<CANFrame>*);
    static bool loadTraceFile(QString, QVector<CANFrame>*);
    static bool loadIXXATFile(QString, QVector<CANFrame>*);
    static bool loadCANDOFile(QString, QVector<CANFrame>*);
    static bool loadVehicleSpyFile(QString, QVector<CANFrame>*);
    static bool loadCanDumpFile(QString, QVector<CANFrame>*);
    static bool loadLawicelFile(QString, QVector<CANFrame>*);
    static bool loadPCANFile(QString, QVector<CANFrame>*);
    static bool loadKvaserFile(QString, QVector<CANFrame>*, bool);
    static bool loadCanalyzerASC(QString, QVector<CANFrame>*);
    static bool loadCanalyzerBLF(QString, QVector<CANFrame>*);
    static bool loadCARBUSAnalyzerFile(QString filename, QVector<CANFrame>* frames);
    static bool loadCANHackerFile(QString filename, QVector<CANFrame>* frames);
    static bool loadCabanaFile(QString filename, QVector<CANFrame>* frames);
    static bool loadCANOpenFile(QString filename, QVector<CANFrame>* frames);
    static bool loadTeslaAPFile(QString filename, QVector<CANFrame>* frames);
    static bool loadCLX000File(QString filename, QVector<CANFrame>* frames);
    static bool loadCANServerFile(QString filename, QVector<CANFrame>* frames);
    static bool loadWiresharkFile(QString filename, QVector<CANFrame>* frames);

    //functions that pre-scan a file to try to figure out if they could read it. Used to automatically determine
    //file type and load it.
    static bool isCRTDFile(QString);
    static bool isNativeCSVFile(QString);
    static bool isGenericCSVFile(QString);
    static bool isLogFile(QString);
    static bool isMicrochipFile(QString);
    static bool isTraceFile(QString);
    static bool isIXXATFile(QString);
    static bool isCANDOFile(QString);
    static bool isVehicleSpyFile(QString);
    static bool isCanDumpFile(QString);
    static bool isLawicelFile(QString);
    static bool isPCANFile(QString);
    static bool isKvaserFile(QString);
    static bool isCanalyzerASC(QString);
    static bool isCanalyzerBLF(QString);
    static bool isCARBUSAnalyzerFile(QString filename);
    static bool isCANHackerFile(QString filename);
    static bool isCabanaFile(QString filename);
    static bool isCANOpenFile(QString filename);
    static bool isTeslaAPFile(QString filename);
    static bool isCLX000File(QString filename);
    static bool isCANServerFile(QString filename);
    static bool isWiresharkFile(QString filename);

    static bool saveCRTDFile(QString, const QVector<CANFrame>*);
    static bool saveNativeCSVFile(QString, const QVector<CANFrame>*);
    static bool saveGenericCSVFile(QString, const QVector<CANFrame>*);
    static bool saveLogFile(QString, const QVector<CANFrame>*);
    static bool saveMicrochipFile(QString, const QVector<CANFrame>*);
    static bool saveTraceFile(QString, const QVector<CANFrame>*);
    static bool saveIXXATFile(QString, const QVector<CANFrame>*);
    static bool saveCANDOFile(QString, const QVector<CANFrame>*);
    static bool saveVehicleSpyFile(QString, const QVector<CANFrame>*);
    static bool saveCanDumpFile(QString filename, const QVector<CANFrame> * frames);
    static bool saveCabanaFile(QString filename, const QVector<CANFrame>* frames);
    static bool saveCanalyzerASC(QString filename, const QVector<CANFrame>* frames);
    static bool saveCARBUSAnalzyer(QString filename, const QVector<CANFrame>* frames);

    static bool openContinuousNative();
    static bool closeContinuousNative();
    static bool writeContinuousNative(const QVector<CANFrame>*, int);
    static bool flushContinuousNative();

private:
    static QFile continuousFile;
};

#endif // FRAMEFILEIO_H
