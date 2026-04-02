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
    static bool loadFrameFile(QString &, QVector<CommFrame>*);
    static bool saveFrameFile(QString &, const QVector<CommFrame>*);

    //These do the actual loading and saving and can be used directly if you'd prefer
    static bool autoDetectLoadFile(QString, QVector<CommFrame>*);
    static bool loadCRTDFile(QString, QVector<CommFrame>*);
    static bool loadNativeCSVFile(QString, QVector<CommFrame>*);
    static bool loadGenericCSVFile(QString, QVector<CommFrame>*);
    static bool loadLogFile(QString, QVector<CommFrame>*);
    static bool loadMicrochipFile(QString, QVector<CommFrame>*);
    static bool loadTraceFile(QString, QVector<CommFrame>*);
    static bool loadIXXATFile(QString, QVector<CommFrame>*);
    static bool loadCANDOFile(QString, QVector<CommFrame>*);
    static bool loadVehicleSpyFile(QString, QVector<CommFrame>*);
    static bool loadCanDumpFile(QString, QVector<CommFrame>*);
    static bool loadLawicelFile(QString, QVector<CommFrame>*);
    static bool loadPCANFile(QString, QVector<CommFrame>*);
    static bool loadKvaserFile(QString, QVector<CommFrame>*, bool);
    static bool loadCanalyzerASC(QString, QVector<CommFrame>*);
    static bool loadCanalyzerBLF(QString, QVector<CommFrame>*);
    static bool loadCARBUSAnalyzerFile(QString filename, QVector<CommFrame>* frames);
    static bool loadCANHackerFile(QString filename, QVector<CommFrame>* frames);
    static bool loadCabanaFile(QString filename, QVector<CommFrame>* frames);
    static bool loadCANOpenFile(QString filename, QVector<CommFrame>* frames);
    static bool loadTeslaAPFile(QString filename, QVector<CommFrame>* frames);
    static bool loadCLX000File(QString filename, QVector<CommFrame>* frames);
    static bool loadCANServerFile(QString filename, QVector<CommFrame>* frames);
    static bool loadWiresharkFile(QString filename, QVector<CommFrame>* frames);
    static bool loadWiresharkSocketCANFile(QString filename, QVector<CommFrame>* frames);

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
    static bool isWiresharkSocketCANFile(QString filename);

    static bool saveCRTDFile(QString, const QVector<CommFrame>*);
    static bool saveNativeCSVFile(QString, const QVector<CommFrame>*);
    static bool saveGenericCSVFile(QString, const QVector<CommFrame>*);
    static bool saveLogFile(QString, const QVector<CommFrame>*);
    static bool saveMicrochipFile(QString, const QVector<CommFrame>*);
    static bool saveTraceFile(QString, const QVector<CommFrame>*);
    static bool saveIXXATFile(QString, const QVector<CommFrame>*);
    static bool saveCANDOFile(QString, const QVector<CommFrame>*);
    static bool saveVehicleSpyFile(QString, const QVector<CommFrame>*);
    static bool saveCanDumpFile(QString filename, const QVector<CommFrame> * frames);
    static bool saveCabanaFile(QString filename, const QVector<CommFrame>* frames);
    static bool saveCanalyzerASC(QString filename, const QVector<CommFrame>* frames);
    static bool saveCARBUSAnalzyer(QString filename, const QVector<CommFrame>* frames);

    static bool openContinuousNative();
    static bool closeContinuousNative();
    static bool writeContinuousNative(const QVector<CommFrame>*, int);
    static bool flushContinuousNative();

private:
    static QFile continuousFile;
};

#endif // FRAMEFILEIO_H
