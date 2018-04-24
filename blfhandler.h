#ifndef BLFHANDLER_H
#define BLFHANDLER_H

#include <Qt>
#include <QByteArray>
#include <QList>
#include "can_structs.h"

enum
{
    BLF_CAN_MSG = 1,
    BLF_CAN_ERR = 2,
    BLF_CONTAINER = 10,
    BLF_GLOBAL_MARKER = 96,
};

struct BLF_FILE_HEADER
{
    uint32_t sig;
    uint32_t headerSize;
    uint8_t  appID;
    uint8_t appVerMajor;
    uint8_t appVerMinor;
    uint8_t appVerBuild;
    uint8_t binLogVerMajor;
    uint8_t binLogVerMinor;
    uint8_t binLogVerBuild;
    uint8_t binLogVerPatch;
    uint64_t fileSize;
    uint64_t uncompressedFileSize;
    uint32_t countObjs;
    uint32_t countObjsRead;
    uint8_t startTime[16];
    uint8_t stopTime[16];
    uint8_t unused[72];
}; //144 bytes

struct BLF_OBJ_HEADER
{
    uint32_t sig;
    uint16_t headerSize;
    uint16_t headerVersion;
    uint32_t objSize;
    uint32_t objType;
    uint32_t flags;
    uint16_t nothing;
    uint16_t objVer;
    uint64_t uncompSize;
}; //32 bytes

struct BLF_OBJECT
{
    BLF_OBJ_HEADER header;
    QByteArray data;
};

struct BLF_CAN_OBJ
{
    uint16_t channel;
    uint8_t flags;
    uint8_t dlc;
    uint32_t id;
    uint8_t data[8];
};


class BLFHandler
{
public:
    BLFHandler();
    bool loadBLF(QString filename, QVector<CANFrame>* frames);
    bool saveBLF(QString filename, QVector<CANFrame>* frames);

private:
    BLF_FILE_HEADER header;
    QList<BLF_OBJECT> objects;
};

#endif // BLFHANDLER_H
