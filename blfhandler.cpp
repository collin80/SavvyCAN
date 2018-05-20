#include "blfhandler.h"
#include <QDebug>
#include <QFile>
#include <QString>
#include <QtEndian>

BLFHandler::BLFHandler()
{

}

/*
 Written while peeking at source code here:
https://python-can.readthedocs.io/en/latest/_modules/can/io/blf.html

All the code actually below is freshly written though.
*/
bool BLFHandler::loadBLF(QString filename, QVector<CANFrame>* frames)
{
    BLF_OBJ_HEADER objHeader;
    QByteArray fileData;
    QByteArray uncompressedData;
    QByteArray junk;
    BLF_OBJECT obj;
    uint32_t pos;
    BLF_CAN_OBJ canObject;

    QFile *inFile = new QFile(filename);

    if (!inFile->open(QIODevice::ReadOnly))
    {
        delete inFile;
        return false;
    }
    inFile->read((char *)&header, sizeof(header));
    if (qFromLittleEndian(header.sig) == 0x47474F4C)
    {
        qDebug() << "Proper BLF file header token";
    }
    else return false;

    while (!inFile->atEnd())
    {
        qDebug() << "Position within file: " << inFile->pos();
        inFile->read((char *)&objHeader, sizeof(objHeader));
        if (qFromLittleEndian(objHeader.sig) == 0x4A424F4C)
        {
            int readSize = objHeader.objSize - sizeof(BLF_OBJ_HEADER);
            qDebug() << "Proper object header token. Read Size: " << readSize;
            fileData = inFile->read(readSize);
            junk = inFile->read(readSize % 4); //file is padded so sizes must always end up on even multiple of 4
            //qDebug() << "Fudge bytes in readSize: " << (readSize % 4);
            if (objHeader.objType == BLF_CONTAINER)
            {
                qDebug() << "Object is a container. Uncompressing it";
                fileData.prepend(objHeader.uncompSize & 0xFF);
                fileData.prepend((objHeader.uncompSize >> 8) & 0xFF);
                fileData.prepend((objHeader.uncompSize >> 16) & 0xFF);
                fileData.prepend((objHeader.uncompSize >> 24) & 0xFF);
                uncompressedData += qUncompress(fileData);
                qDebug() << "Uncompressed size: " << uncompressedData.count();
                pos = 0;
                bool foundHeader = false;
                //first skip forward to find a header signature - usually not necessary
                while ( (int)(pos + sizeof(BLF_OBJ_HEADER)) < uncompressedData.count())
                {
                    int32_t *headerSig = (int32_t *)(uncompressedData.constData() + pos);
                    if (*headerSig == 0x4A424F4C) break;
                    pos += 4;
                }
                //then process all the objects
                while ( (int)(pos + sizeof(BLF_OBJ_HEADER)) < uncompressedData.count())
                {                    
                    memcpy(&obj.header, (uncompressedData.constData() + pos), sizeof(BLF_OBJ_HEADER));
                    qDebug() << "Pos: " << pos << " Type: " << obj.header.objType << "Obj Size: " << obj.header.objSize;
                    if (qFromLittleEndian(objHeader.sig) == 0x4A424F4C)
                    {
                        fileData = uncompressedData.mid(pos + sizeof(BLF_OBJ_HEADER), obj.header.objSize - sizeof(BLF_OBJ_HEADER));
                        if (obj.header.objType == BLF_CAN_MSG)
                        {
                            memcpy(&canObject, fileData.constData(), sizeof(BLF_CAN_OBJ));
                            CANFrame frame;
                            frame.bus = canObject.channel;
                            frame.extended = (canObject.id & 0x80000000ull)?true:false;
                            frame.ID = canObject.id & 0x1FFFFFFFull;
                            frame.isReceived = true;
                            frame.len = canObject.dlc;
                            frame.timestamp = obj.header.uncompSize / 100000.0; //uncompsize field also used for timestamp oddly enough
                            for (int i = 0; i < 8; i++) frame.data[i] = canObject.data[i];
                            frames->append(frame);
                        }
                        else
                        {
                            //qDebug() << "Not a can frame! ObjType: " << obj.header.objType;
                            if (obj.header.objType > 0xFFFF)
                                return false;
                        }
                        pos += obj.header.objSize + (obj.header.objSize % 4);
                    }
                    else
                    {
                        qDebug() << "Unexpected object header signature, aborting";
                        return false;
                    }
                }
                uncompressedData.remove(0, pos);
                qDebug() << "After removing used data uncompressedData is now this big: " << uncompressedData.count();
            }
        }
        else return false;
    }
    return true;
}

bool BLFHandler::saveBLF(QString filename, QVector<CANFrame> *frames)
{
    return false;
}
