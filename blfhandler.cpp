#include "blfhandler.h"
#include <QDebug>
#include <QFile>
#include <QString>
#include <QtEndian>
#include <cstring>

#define BLF_REMOTE_FLAG 0x80

BLFHandler::BLFHandler()
{

}

/*
 Written while peeking at source code here:
https://python-can.readthedocs.io/en/latest/_modules/can/io/blf.html
https://bitbucket.org/tobylorenz/vector_blf/

All the code actually below is freshly written but heavily based upon things seen in those
two source repos.
*/
namespace
{
const quint32 BLF_FILE_SIGNATURE = 0x47474F4C;
const quint32 BLF_OBJECT_SIGNATURE = 0x4A424F4C;
const qint64 BLF_BASE_HEADER_SIZE = 16;
const qint64 BLF_V1_HEADER_SIZE = 32;
const qint64 BLF_V2_HEADER_SIZE = 40;

quint16 readU16(const quint8 *data)
{
    return quint16(data[0]) | (quint16(data[1]) << 8);
}

quint32 readU32(const quint8 *data)
{
    return quint32(data[0]) |
           (quint32(data[1]) << 8) |
           (quint32(data[2]) << 16) |
           (quint32(data[3]) << 24);
}

quint64 readU64(const quint8 *data)
{
    return quint64(readU32(data)) | (quint64(readU32(data + 4)) << 32);
}

bool hasSignature(const QByteArray &data, qint64 pos)
{
    return pos >= 0 && pos + 4 <= data.size() &&
           memcmp(data.constData() + pos, "LOBJ", 4) == 0;
}

qint64 findSignature(const QByteArray &data, qint64 start, qint64 end)
{
    if (data.size() < 4) return -1;
    end = qMin(end, qint64(data.size()) - 4);
    for (qint64 pos = qMax<qint64>(start, 0); pos <= end; pos++) {
        if (hasSignature(data, pos)) return pos;
    }
    return -1;
}

qint64 timestampMicroseconds(const quint8 *data, quint16 headerVersion)
{
    if (headerVersion != 1 && headerVersion != 2) return 0;
    quint32 flags = readU32(data + 16);
    quint64 timestamp = readU64(data + 24);
    if (flags == 1) return static_cast<qint64>(timestamp / 100ULL);
    return static_cast<qint64>(timestamp / 1000ULL);
}

//CAN FD details of a frame, from the flags of the CAN FD object types (100 and 101)
struct FdFlags
{
    bool fd = false;  //EDL: a CAN FD frame (classic frames on a CAN FD channel are logged with these types too)
    bool brs = false; //bit rate switch
    bool esi = false; //error state indicator
};

//A remote frame has no data; its length is its DLC (0-8).
void setFrame(CommFrame &frame, quint32 id, bool extended, bool remote, quint8 bus,
              const QByteArray &payload, qint64 timestamp, bool received, quint8 dlc,
              FdFlags fdFlags = FdFlags())
{
    frame.setBus(bus);
    frame.setExtendedFrameFormat(extended);
    frame.setFrameId(id & 0x1FFFFFFFull);
    frame.setReceived(received);
    if (remote)
    {
        frame.setFrameType(CommFrame::RemoteRequestFrame);
        frame.setPayload(QByteArray(qMin<int>(dlc, 8), 0));
    }
    else
    {
        frame.setFrameType(fdFlags.fd ? CommFrame::CANFDDataFrame : CommFrame::CANDataFrame);
        frame.setPayload(payload);
        if (fdFlags.fd)
        {
            frame.setBitrateSwitch(fdFlags.brs);
            frame.setErrorStateIndicator(fdFlags.esi);
        }
    }
    frame.setTimeStamp(CommFrame::TimeStamp(0, timestamp));
}

bool parseContainer(const QByteArray &data, QVector<CommFrame> *frames, QByteArray *tail)
{
    if (!frames || !tail) return false;

    QByteArray currentData;
    currentData.reserve(tail->size() + data.size());
    currentData.append(*tail);
    currentData.append(data);
    tail->clear();

    const quint8 *bytes = reinterpret_cast<const quint8 *>(currentData.constData());
    qint64 pos = findSignature(currentData, 0, currentData.size());
    while (pos >= 0)
    {
        if (pos + BLF_BASE_HEADER_SIZE > currentData.size()) {
            *tail = currentData.mid(pos);
            return true;
        }

        quint16 headerSize = readU16(bytes + pos + 4);
        quint16 headerVersion = readU16(bytes + pos + 6);
        quint32 objectSize = readU32(bytes + pos + 8);
        quint32 objectType = readU32(bytes + pos + 12);

        if (headerSize < BLF_BASE_HEADER_SIZE || objectSize < headerSize) return false;
        if (headerVersion == 1 && headerSize < BLF_V1_HEADER_SIZE) return false;
        if (headerVersion == 2 && headerSize < BLF_V2_HEADER_SIZE) return false;
        if (pos + objectSize > currentData.size()) {
            *tail = currentData.mid(pos);
            return true;
        }

        qint64 payloadOffset = pos + headerSize;
        qint64 payloadSize = objectSize - headerSize;
        qint64 timestamp = headerSize >= BLF_V1_HEADER_SIZE
                               ? timestampMicroseconds(bytes + pos, headerVersion)
                               : 0;

        if (objectType == BLF_CAN_MSG)
        {
            if (payloadSize < qint64(sizeof(BLF_CAN_OBJ))) return false;
            BLF_CAN_OBJ canObject;
            memcpy(&canObject, bytes + payloadOffset, sizeof(canObject));
            int payloadLength = qMin<int>(canObject.dlc, sizeof(canObject.data));
            QByteArray payload(reinterpret_cast<const char *>(canObject.data), payloadLength);
            CommFrame frame;
            setFrame(frame, canObject.id, (canObject.id & 0x80000000ull) != 0,
                     (canObject.flags & BLF_REMOTE_FLAG) != 0, canObject.channel,
                     payload, timestamp, true, canObject.dlc);
            frames->append(frame);
        }
        else if (objectType == BLF_CAN_MSG2)
        {
            if (payloadSize < qint64(sizeof(BLF_CAN_OBJ2))) return false;
            BLF_CAN_OBJ2 canObject;
            memcpy(&canObject, bytes + payloadOffset, sizeof(canObject));
            int payloadLength = qMin<int>(canObject.dlc, sizeof(canObject.data));
            QByteArray payload(reinterpret_cast<const char *>(canObject.data), payloadLength);
            CommFrame frame;
            setFrame(frame, canObject.id, (canObject.id & 0x80000000ull) != 0,
                     (canObject.flags & BLF_REMOTE_FLAG) != 0, canObject.channel,
                     payload, timestamp, true, canObject.dlc);
            frames->append(frame);
        }
        else if (objectType == BLF_CAN_FD_MSG)
        {
            if (payloadSize < qint64(sizeof(BLF_CANFD_OBJ))) return false;
            BLF_CANFD_OBJ canObject;
            memcpy(&canObject, bytes + payloadOffset, sizeof(canObject));
            int payloadLength = qMin<int>(canObject.validDataBytes, sizeof(canObject.data));
            QByteArray payload(reinterpret_cast<const char *>(canObject.data), payloadLength);
            FdFlags fdFlags;
            fdFlags.fd = (canObject.fdFlags & 0x01) != 0;
            fdFlags.brs = (canObject.fdFlags & 0x02) != 0;
            fdFlags.esi = (canObject.fdFlags & 0x04) != 0;
            CommFrame frame;
            setFrame(frame, canObject.id, (canObject.id & 0x80000000ull) != 0,
                     (canObject.flags & BLF_REMOTE_FLAG) != 0, canObject.channel,
                     payload, timestamp, true, canObject.dlc, fdFlags);
            frames->append(frame);
        }
        else if (objectType == BLF_CAN_FD_MSG64)
        {
            if (payloadSize < qint64(sizeof(BLF_CANFD64_OBJ))) return false;
            BLF_CANFD64_OBJ canObject;
            memcpy(&canObject, bytes + payloadOffset, sizeof(canObject));
            int payloadLength = qMin<int>(canObject.validDataBytes,
                                          payloadSize - qint64(sizeof(BLF_CANFD64_OBJ)));
            QByteArray payload(reinterpret_cast<const char *>(bytes + payloadOffset + sizeof(BLF_CANFD64_OBJ)), payloadLength);
            FdFlags fdFlags;
            fdFlags.fd = (canObject.flags & 0x1000) != 0;
            fdFlags.brs = (canObject.flags & 0x2000) != 0;
            fdFlags.esi = (canObject.flags & 0x4000) != 0;
            CommFrame frame;
            setFrame(frame, canObject.frameId,
                     (canObject.frameId & 0x80000000ull) != 0,
                     (canObject.flags & 0x10) != 0, canObject.channel,
                     payload, timestamp, canObject.dir == 0, canObject.dlc, fdFlags);
            frames->append(frame);
        }

        qint64 nextPos = pos + objectSize;
        pos = findSignature(currentData, nextPos, nextPos + 8);
        if (pos < 0) {
            *tail = currentData.mid(nextPos);
            return true;
        }
    }

    if (!currentData.isEmpty()) *tail = currentData;
    return true;
}
}

bool BLFHandler::loadBLF(QString filename, QVector<CommFrame>* frames)
{
    if (!frames) return false;

    QFile inFile(filename);
    if (!inFile.open(QIODevice::ReadOnly)) return false;

    BLF_FILE_HEADER fileHeader;
    if (inFile.read(reinterpret_cast<char *>(&fileHeader), sizeof(fileHeader)) != sizeof(fileHeader))
        return false;
    if (qFromLittleEndian(fileHeader.sig) != BLF_FILE_SIGNATURE) return false;

    QByteArray containerTail;
    while (true)
    {
        BLF_OBJ_HEADER_BASE base;
        qint64 readSize = inFile.read(reinterpret_cast<char *>(&base), sizeof(base));
        if (readSize == 0) break;
        if (readSize != sizeof(base)) return false;
        if (qFromLittleEndian(base.sig) != BLF_OBJECT_SIGNATURE) return false;

        quint32 objectSize = qFromLittleEndian(base.objSize);
        quint32 objectType = qFromLittleEndian(base.objType);
        if (objectSize < BLF_BASE_HEADER_SIZE) return false;

        qint64 dataSize = objectSize - BLF_BASE_HEADER_SIZE;
        if (dataSize > inFile.size() - inFile.pos()) return false;
        QByteArray objectData = inFile.read(dataSize);
        if (objectData.size() != dataSize) return false;

        quint32 paddingSize = objectSize % 4;
        if (paddingSize > 0 && inFile.read(paddingSize).size() != qint64(paddingSize))
            return false;

        if (objectType != BLF_CONTAINER) continue;
        if (objectData.size() < qint64(sizeof(BLF_OBJ_HEADER_CONTAINER))) return false;

        BLF_OBJ_HEADER_CONTAINER container;
        memcpy(&container, objectData.constData(), sizeof(container));
        quint16 compressionMethod = qFromLittleEndian(container.compressionMethod);
        quint32 uncompressedSize = qFromLittleEndian(container.uncompressedSize);
        QByteArray uncompressedData = objectData.mid(sizeof(BLF_OBJ_HEADER_CONTAINER));
        if (compressionMethod == BLF_CONT_ZLIB_COMPRESSION)
        {
            QByteArray compressedData;
            compressedData.append(char((uncompressedSize >> 24) & 0xFF));
            compressedData.append(char((uncompressedSize >> 16) & 0xFF));
            compressedData.append(char((uncompressedSize >> 8) & 0xFF));
            compressedData.append(char(uncompressedSize & 0xFF));
            compressedData.append(uncompressedData);
            uncompressedData = qUncompress(compressedData);
        }
        else if (compressionMethod != BLF_CONT_NO_COMPRESSION)
        {
            return false;
        }

        if (uncompressedSize != 0 && uncompressedData.size() != qint64(uncompressedSize))
            return false;
        if (!parseContainer(uncompressedData, frames, &containerTail)) return false;
    }

    return true;
}

bool BLFHandler::saveBLF(QString filename, QVector<CommFrame> *frames)
{
    Q_UNUSED(filename)
    Q_UNUSED(frames)
    return false;
}
