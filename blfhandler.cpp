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
    }
    else return false;

    while (!inFile->atEnd())
    {
        inFile->read((char *)&objHeader, sizeof(objHeader));
        if (qFromLittleEndian(objHeader.sig) == 0x4A424F4C)
        {
            int readSize = objHeader.objSize - sizeof(BLF_OBJ_HEADER);
            fileData = inFile->read(readSize);
            junk = inFile->read(readSize % 4); //file is padded so sizes must always end up on even multiple of 4
            if (objHeader.objType == BLF_CONTAINER)
            {
                fileData.prepend(objHeader.uncompSize & 0xFF);
                fileData.prepend((objHeader.uncompSize >> 8) & 0xFF);
                fileData.prepend((objHeader.uncompSize >> 16) & 0xFF);
                fileData.prepend((objHeader.uncompSize >> 24) & 0xFF);
                uncompressedData += qUncompress(fileData);
                pos = 0;
                while (pos + sizeof(BLF_OBJ_HEADER) < uncompressedData.count())
                {
                    memcpy(&obj.header, uncompressedData.mid(pos, sizeof(BLF_OBJ_HEADER)).constData(), sizeof(BLF_OBJ_HEADER));
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
                            frame.timestamp = obj.header.uncompSize / 1000000.0; //uncompsize field also used for timestamp oddly enough
                            for (int i = 0; i < 8; i++) frame.data[i] = canObject.data[i];
                            frames->append(frame);
                        }
                        pos += obj.header.objSize + (obj.header.objSize % 4);
                    }
                }
                uncompressedData.remove(0, pos);
            }
        }
        else return false;
    }
    return false;
}
