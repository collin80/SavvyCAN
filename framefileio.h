#ifndef FRAMEFILEIO_H
#define FRAMEFILEIO_H

//Class full of static methods that load and save canbus frames to/from various file formats

#include "config.h"
#include <Qt>
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
    static bool saveCRTDFile(QString, QVector<CANFrame>*);
    static bool saveNativeCSVFile(QString, QVector<CANFrame>*);
    static bool saveGenericCSVFile(QString, QVector<CANFrame>*);
    static bool saveLogFile(QString, QVector<CANFrame>*);
    static bool saveMicrochipFile(QString, QVector<CANFrame>*);
    static QString loadFrameFile(QVector<CANFrame>*);
};

#endif // FRAMEFILEIO_H
