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
    BLF_CAN_OVERLOAD = 3,
    BLF_STATISTIC = 4,
    BLF_APP_TRIGGER = 5,
    BLF_ENV_INT = 6,
    BLF_ENV_DOUBLE = 7,
    BLF_ENV_STRING = 8,
    BLF_ENV_DATA = 9,
    BLF_CONTAINER = 10,
    BLF_CAN_DRV_ERR = 31, // can driver error
    BLF_CAN_DRV_SYNC = 44,
    BLF_ERROR_EXT = 73,
    BLF_DRV_ERR_EXT = 74,
    BLF_CAN_MSG2 = 86,
    BLF_OVERRUN = 91,
    BLF_EVENT_COMMENT = 92,
    BLF_GLOBAL_MARKER = 96,
    BLF_CAN_FD_MSG = 100,
    BLF_CAN_FD_MSG64 = 101,
    BLF_CAN_FD_ERR64 = 104
};

enum
{
    BLF_CONT_NO_COMPRESSION = 0,
    BLF_CONT_ZLIB_COMPRESSION = 2
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

struct BLF_OBJ_HEADER_BASE
{
    uint32_t sig; //0 offset from start
    uint16_t headerSize; //4
    uint16_t headerVersion; //6
    uint32_t objSize; //8
    uint32_t objType; //12    
}; //16 bytes in common

//the next three are continuations of the above base and so are numbered in comments accordingly
struct BLF_OBJ_HEADER_V1
{
    uint32_t flags; //16
    uint16_t clientIdx; //20
    uint16_t objVer; //22
    uint64_t uncompSize; //24 could be the timestamp too
}; //ends up as 32 bytes for this whole header including base

struct BLF_OBJ_HEADER_V2
{
    uint32_t flags; //16
    uint16_t timestampStatus; //20
    uint16_t objVer; //22
    uint64_t uncompSize; //24
    uint64_t origTimestamp; //32
}; //40 bytes including base header

struct BLF_OBJ_HEADER_CONTAINER
{
    uint16_t compressionMethod; //16
    uint8_t notused[6]; //18
    uint32_t uncompressedSize; //24
    uint8_t notused2[4]; //28
}; //32 bytes for the whole header

struct BLF_OBJ_HEADER
{
    BLF_OBJ_HEADER_BASE base;
    union
    {
        BLF_OBJ_HEADER_V1 v1Obj;
        BLF_OBJ_HEADER_V2 V2Obj;
        BLF_OBJ_HEADER_CONTAINER containerObj;
    };
};

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

struct BLF_CAN_OBJ2
{
    uint16_t channel;
    uint8_t flags; //0 = Tx, 5 = Err 6 = WU, 7 = RTR
    uint8_t dlc;
    uint32_t id;
    uint8_t data[8];
    uint32_t frameLength; //length of transmission in nanoseconds not covering IF or EOF
    uint8_t bitCount; //# of bits in this message including IF and EOF
    uint8_t dummy1;
    uint16_t dummy2;
};

struct BLF_CANFD_OBJ
{
    uint16_t channel;
    uint8_t flags;
    uint8_t dlc;
    uint32_t id;
    uint32_t frameLen;
    uint8_t bitCount;
    uint8_t fdFlags;
    uint8_t validDataBytes;
    uint8_t nonUseful[5];
    uint8_t data[64];
};

struct BLF_ERROR_EXT
{
    uint16_t channel;
    uint16_t len;
    uint32_t flags;
    uint8_t ecc;
    uint8_t position;
    uint8_t dlc;
    uint8_t ignored;
    uint32_t frameLen;
    uint32_t id;
    uint16_t extFlags;
    uint16_t ignore2;
    uint8_t data;
};

struct BLF_GLOBAL_MARKER
{
    uint32_t eventType;
    uint32_t foregroundColor;
    uint32_t backgroundColor;
    uint8_t ignore[3];
    uint8_t relocatable;
    uint32_t groupNameLen;
    uint32_t markerNameLen;
    uint32_t descLen;
    uint8_t ignore2[12];
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
