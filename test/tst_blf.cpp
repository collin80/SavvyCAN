#include <QtTest>
#include <QByteArray>
#include <QTemporaryFile>

#include "blfhandler.h"
#include "tst_blf.h"

namespace
{
void appendU8(QByteArray &data, quint8 value)
{
    data.append(static_cast<char>(value));
}

void appendU16(QByteArray &data, quint16 value)
{
    appendU8(data, value & 0xFF);
    appendU8(data, (value >> 8) & 0xFF);
}

void appendU32(QByteArray &data, quint32 value)
{
    appendU16(data, value & 0xFFFF);
    appendU16(data, (value >> 16) & 0xFFFF);
}

void appendU64(QByteArray &data, quint64 value)
{
    appendU32(data, value & 0xFFFFFFFFULL);
    appendU32(data, (value >> 32) & 0xFFFFFFFFULL);
}

void setU32(QByteArray &data, int offset, quint32 value)
{
    data[offset] = static_cast<char>(value & 0xFF);
    data[offset + 1] = static_cast<char>((value >> 8) & 0xFF);
    data[offset + 2] = static_cast<char>((value >> 16) & 0xFF);
    data[offset + 3] = static_cast<char>((value >> 24) & 0xFF);
}

QByteArray makeObject(quint32 type, const QByteArray &payload)
{
    QByteArray object;
    appendU32(object, 0x4A424F4C);
    appendU16(object, 32);
    appendU16(object, 1);
    appendU32(object, 32 + payload.size());
    appendU32(object, type);
    appendU32(object, 2);
    appendU16(object, 0);
    appendU16(object, 0);
    appendU64(object, 0);
    object.append(payload);
    return object;
}

QByteArray makeContainer(const QByteArray &uncompressedData)
{
    QByteArray compressedData = qCompress(uncompressedData);
    QByteArray containerData;
    appendU16(containerData, BLF_CONT_ZLIB_COMPRESSION);
    containerData.append(QByteArray(6, '\0'));
    appendU32(containerData, uncompressedData.size());
    containerData.append(QByteArray(4, '\0'));
    containerData.append(compressedData.constData() + 4, compressedData.size() - 4);

    QByteArray container;
    appendU32(container, 0x4A424F4C);
    appendU16(container, 16);
    appendU16(container, 1);
    appendU32(container, 16 + containerData.size());
    appendU32(container, BLF_CONTAINER);
    container.append(containerData);
    return container;
}

QByteArray makeBlf(const QList<QByteArray> &uncompressedData, quint32 objectCount)
{
    quint64 totalUncompressedSize = 0;
    for (const QByteArray &data : uncompressedData) totalUncompressedSize += data.size();

    QByteArray file;
    appendU32(file, 0x47474F4C);
    appendU32(file, 144);
    file.append(QByteArray(8, '\0'));
    appendU64(file, 0);
    appendU64(file, totalUncompressedSize);
    appendU32(file, objectCount);
    appendU32(file, 0);
    file.append(QByteArray(16, '\0'));
    file.append(QByteArray(16, '\0'));
    file.append(QByteArray(72, '\0'));
    for (const QByteArray &data : uncompressedData) {
        QByteArray container = makeContainer(data);
        file.append(container);
        file.append(QByteArray(container.size() % 4, '\0'));
    }
    setU32(file, 16, file.size());
    return file;
}

QByteArray makeBlf(const QByteArray &uncompressedData, quint32 objectCount)
{
    QList<QByteArray> chunks;
    chunks.append(uncompressedData);
    return makeBlf(chunks, objectCount);
}

QByteArray makeCanMessage2Payload()
{
    QByteArray payload;
    appendU16(payload, 1);
    appendU8(payload, 0);
    appendU8(payload, 8);
    appendU32(payload, 0x123);
    for (int i = 0; i < 8; i++) appendU8(payload, 0x10 + i);
    appendU32(payload, 0);
    appendU8(payload, 0);
    appendU8(payload, 0);
    appendU16(payload, 0);
    return payload;
}

QByteArray makeCanFd64Payload()
{
    QByteArray payload;
    appendU8(payload, 1);
    appendU8(payload, 15);
    appendU8(payload, 16);
    appendU8(payload, 0);
    appendU32(payload, 0x123);
    appendU32(payload, 1234);
    appendU32(payload, 0x1000);
    appendU32(payload, 0);
    appendU32(payload, 0);
    appendU32(payload, 0);
    appendU32(payload, 0);
    appendU16(payload, 0);
    appendU8(payload, 0);
    appendU8(payload, 0);
    appendU32(payload, 0);
    for (int i = 0; i < 16; i++) appendU8(payload, 0x20 + i);
    return payload;
}

bool writeFixture(const QByteArray &data, QTemporaryFile *file)
{
    if (!file->open()) return false;
    if (file->write(data) != data.size()) return false;
    file->close();
    return true;
}
}

void TestBLF::loadObjectBoundary()
{
    QByteArray uncompressedData;
    uncompressedData.append(makeObject(65, QByteArray(49, 'x')));
    uncompressedData.append(makeObject(BLF_CAN_MSG2, makeCanMessage2Payload()));

    QTemporaryFile file;
    QVERIFY(writeFixture(makeBlf(uncompressedData, 2), &file));
    QString filename = file.fileName();

    QVector<CANFrame> frames;
    BLFHandler handler;
    QVERIFY(handler.loadBLF(filename, &frames));
    QCOMPARE(frames.count(), 1);
    QCOMPARE(frames[0].frameId(), 0x123u);
    QCOMPARE(frames[0].payload(), QByteArray::fromHex("1011121314151617"));
}

void TestBLF::loadSplitObject()
{
    QByteArray object = makeObject(BLF_CAN_MSG2, makeCanMessage2Payload());
    QList<QByteArray> chunks;
    chunks.append(object.left(40));
    chunks.append(object.mid(40));

    QTemporaryFile file;
    QVERIFY(writeFixture(makeBlf(chunks, 1), &file));
    QString filename = file.fileName();

    QVector<CANFrame> frames;
    BLFHandler handler;
    QVERIFY(handler.loadBLF(filename, &frames));
    QCOMPARE(frames.count(), 1);
    QCOMPARE(frames[0].frameId(), 0x123u);
    QCOMPARE(frames[0].payload(), QByteArray::fromHex("1011121314151617"));
}

void TestBLF::loadCanFd64()
{
    QTemporaryFile file;
    QVERIFY(writeFixture(makeBlf(makeObject(BLF_CAN_FD_MSG64, makeCanFd64Payload()), 1), &file));
    QString filename = file.fileName();

    QVector<CANFrame> frames;
    BLFHandler handler;
    QVERIFY(handler.loadBLF(filename, &frames));
    QCOMPARE(frames.count(), 1);
    QCOMPARE(frames[0].frameId(), 0x123u);
    QCOMPARE(frames[0].payload(), QByteArray::fromHex("202122232425262728292a2b2c2d2e2f"));
}
