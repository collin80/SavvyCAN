#include "framefileio.h"

#include <QProgressDialog>

FrameFileIO::FrameFileIO()
{

}

QString FrameFileIO::loadFrameFile(QVector<CANFrame>* frameCache)
{
    QString filename;
    QFileDialog dialog;
    bool result = false;

    QStringList filters;
    filters.append(QString(tr("CRTD Logs (*.txt)")));
    filters.append(QString(tr("GVRET Logs (*.csv)")));
    filters.append(QString(tr("Generic ID/Data CSV (*.csv)")));
    filters.append(QString(tr("BusMaster Log (*.log)")));
    filters.append(QString(tr("Microchip Log (*.can)")));

    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];

        QProgressDialog progress(qApp->activeWindow());
        progress.setWindowModality(Qt::WindowModal);
        progress.setLabelText("Loading file...");
        progress.setCancelButton(0);
        progress.setRange(0,0);
        progress.setMinimumDuration(0);
        progress.show();

        qApp->processEvents();

        if (dialog.selectedNameFilter() == filters[0]) result = loadCRTDFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[1]) result = loadNativeCSVFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[2]) result = loadGenericCSVFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[3]) result = loadLogFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[4]) result = loadMicrochipFile(filename, frameCache);

        progress.cancel();

        if (result)
        {
            QStringList fileList = filename.split('/');
            filename = fileList[fileList.length() - 1];
            return filename;
        }
        else return QString("");
    }
    return QString("");
}

//CRTD format from Mark Webb-Johnson / OVMS project
/*
Sample data in CRTD format
1320745424.000 CXX OVMS Tesla Roadster cando2crtd converted log
1320745424.000 CXX OVMS Tesla roadster log: charge.20111108.csv
1320745424.002 R11 402 FA 01 C3 A0 96 00 07 01
1320745424.015 R11 400 02 9E 01 80 AB 80 55 00
1320745424.066 R11 400 01 01 00 00 00 00 4C 1D
1320745424.105 R11 100 A4 53 46 5A 52 45 38 42
1320745424.106 R11 100 A5 31 35 42 33 30 30 30
1320745424.106 R11 100 A6 35 36 39
1320745424.106 CEV Open charge port door
1320745424.106 R11 100 9B 97 A6 31 03 15 05 06
1320745424.107 R11 100 07 64

tokens:
0 = timestamp
1 = line type
2 = ID
3-x = The data bytes
*/

bool FrameFileIO::loadCRTDFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    int lineCounter = 0;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    line = inFile->readLine(); //read out the header first and discard it.

    while (!inFile->atEnd()) {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }
        line = inFile->readLine().simplified();
        if (line.length() > 2)
        {
            QList<QByteArray> tokens = line.split(' ');
            int multiplier;
            int idxOfDecimal = tokens[0].indexOf('.');
            if (idxOfDecimal > -1) {
                int decimalPlaces = tokens[0].length() - tokens[0].indexOf('.') - 1;
                //the result of the above is the # of digits after the decimal.
                //This program deals in microsecond so turn the value into microseconds
                multiplier = 1000000; //turn the decimal into full microseconds
            }
            else
            {
                multiplier = 1; //special case. Assume no decimal means microseconds
            }
            //qDebug() << "decimal places " << decimalPlaces;
            thisFrame.timestamp = (int64_t)(tokens[0].toDouble() * multiplier);
            if (tokens[1] == "R11" || tokens[1] == "R29")
            {
                thisFrame.ID = tokens[2].toInt(NULL, 16);
                if (tokens[1] == "R29") thisFrame.extended = true;
                else thisFrame.extended = false;
                thisFrame.bus = 0;
                thisFrame.len = tokens.length() - 3;
                for (int d = 0; d < thisFrame.len; d++)
                {
                    if (tokens[d + 3] != "")
                    {
                        thisFrame.data[d] = tokens[d + 3].toInt(NULL, 16);
                    }
                    else thisFrame.data[d] = 0;
                }
                frames->append(thisFrame);
            }
        }
    }
    inFile->close();
    delete inFile;
    return true;
}

bool FrameFileIO::saveCRTDFile(QString filename, const QVector<CANFrame>* frames)
{
    QFile *outFile = new QFile(filename);
    int lineCounter = 0;

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete outFile;
        return false;
    }

    //write in float format with 6 digits after the decimal point
    outFile->write(QString::number(frames->at(0).timestamp / 1000000.0, 'f', 6).toUtf8() + tr(" CXX GVRET-PC Reverse Engineering Tool Output V").toUtf8() + QString::number(VERSION).toUtf8());
    outFile->write("\n");

    for (int c = 0; c < frames->count(); c++)
    {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        outFile->write(QString::number(frames->at(c).timestamp / 1000000.0, 'f', 6).toUtf8());
        outFile->putChar(' ');
        if (frames->at(c).extended)
        {
            outFile->write("R29 ");
        }
        else outFile->write("R11 ");
        outFile->write(QString::number(frames->at(c).ID, 16).toUpper().rightJustified(8, '0').toUtf8());
        outFile->putChar(' ');

        for (int temp = 0; temp < frames->at(c).len; temp++)
        {
            outFile->write(QString::number(frames->at(c).data[temp], 16).toUpper().rightJustified(2, '0').toUtf8());
            outFile->putChar(' ');
        }

        outFile->write("\n");
    }
    outFile->close();
    delete outFile;

    return true;
}

//The "native" file format for this program
bool FrameFileIO::loadNativeCSVFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    long long timeStamp = Utility::GetTimeMS();
    int lineCounter = 0;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    line = inFile->readLine(); //read out the header first and discard it.

    while (!inFile->atEnd()) {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        line = inFile->readLine();
        if (line.length() > 2)
        {
            QList<QByteArray> tokens = line.split(',');
            if (tokens[0].length() > 3)
            {
                long long temp = tokens[0].right(10).toLongLong();
                thisFrame.timestamp = temp;
            }
            else
            {
                timeStamp += 5;
                thisFrame.timestamp = timeStamp;
            }

            thisFrame.ID = tokens[1].toInt(NULL, 16);
            if (tokens[2].toUpper().contains("TRUE")) thisFrame.extended = 1;
            else thisFrame.extended = 0;
            thisFrame.bus = tokens[3].toInt();
            thisFrame.len = tokens[4].toInt();
            for (int c = 0; c < 8; c++) thisFrame.data[c] = 0;
            for (int d = 0; d < thisFrame.len; d++)
                thisFrame.data[d] = tokens[5 + d].toInt(NULL, 16);
            frames->append(thisFrame);
        }
    }
    inFile->close();
    delete inFile;
    return true;
}

bool FrameFileIO::saveNativeCSVFile(QString filename, const QVector<CANFrame>* frames)
{
    QFile *outFile = new QFile(filename);
    int lineCounter = 0;

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete outFile;
        return false;
    }

    outFile->write("Time Stamp,ID,Extended,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8");
    outFile->write("\n");

    for (int c = 0; c < frames->count(); c++)
    {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        outFile->write(QString::number(frames->at(c).timestamp).toUtf8());
        outFile->putChar(44);

        outFile->write(QString::number(frames->at(c).ID, 16).toUpper().rightJustified(8, '0').toUtf8());
        outFile->putChar(44);

        if (frames->at(c).extended) outFile->write("true,");
        else outFile->write("false,");

        outFile->write(QString::number(frames->at(c).bus).toUtf8());
        outFile->putChar(44);

        outFile->write(QString::number(frames->at(c).len).toUtf8());
        outFile->putChar(44);

        for (int temp = 0; temp < 8; temp++)
        {
            if (temp < frames->at(c).len)
                outFile->write(QString::number(frames->at(c).data[temp], 16).toUpper().rightJustified(2, '0').toUtf8());
            else
                outFile->write("00");
            outFile->putChar(44);
        }

        outFile->write("\n");

    }
    outFile->close();
    delete outFile;
    return true;
}

bool FrameFileIO::loadGenericCSVFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    long long timeStamp = Utility::GetTimeMS();
    int lineCounter = 0;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    line = inFile->readLine(); //read out the header first and discard it.

    while (!inFile->atEnd()) {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        line = inFile->readLine();
        if (line.length() > 2)
        {
            QList<QByteArray> tokens = line.split(',');

            timeStamp += 5;
            thisFrame.timestamp = timeStamp;
            thisFrame.ID = tokens[0].toInt(NULL, 16);
            if (thisFrame.ID > 0x7FF) thisFrame.extended = true;
            else thisFrame.extended  = false;
            thisFrame.bus = 0;
            QList<QByteArray> dataTok = tokens[1].split(' ');
            thisFrame.len = dataTok.length();
            if (thisFrame.len > 8) thisFrame.len = 8;
            for (int d = 0; d < thisFrame.len; d++) thisFrame.data[d] = dataTok[d].toInt(NULL, 16);

            frames->append(thisFrame);
        }
    }
    inFile->close();
    delete inFile;
    return true;
}

//4f5,ff 34 23 45 24 e4
bool FrameFileIO::saveGenericCSVFile(QString filename, const QVector<CANFrame>* frames)
{
    QFile *outFile = new QFile(filename);
    int lineCounter = 0;

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete outFile;
        return false;
    }

    outFile->write("ID,Data Bytes");
    outFile->write("\n");

    for (int c = 0; c < frames->count(); c++)
    {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        outFile->write(QString::number(frames->at(c).ID, 16).toUpper().rightJustified(8, '0').toUtf8());
        outFile->putChar(44);

        for (int temp = 0; temp < frames->at(c).len; temp++)
        {
            outFile->write(QString::number(frames->at(c).data[temp], 16).toUpper().rightJustified(2, '0').toUtf8());
            outFile->putChar(' ');
        }

        outFile->write("\n");

    }
    outFile->close();
    delete outFile;
    return true;
}

//busmaster log file
/*
tokens:
0 = timestamp
1 = Transmission direction
2 = Channel
3 = ID
4 = Type (s = standard, I believe x = extended)
5 = Data byte length
6-x = The data bytes

Sample chunk of a busmaster log:
***BUSMASTER Ver 2.4.0***
***PROTOCOL CAN***
***NOTE: PLEASE DO NOT EDIT THIS DOCUMENT***
***[START LOGGING SESSION]***
***START DATE AND TIME 8:8:2014 11:49:7:965***
***HEX***
***SYSTEM MODE***
***START CHANNEL BAUD RATE***
***CHANNEL 1 - Kvaser - Kvaser Leaf Light HS #0 (Channel 0), Serial Number- 0, Firmware- 0x00000037 0x00020000 - 500000 bps***
***END CHANNEL BAUD RATE***
***START DATABASE FILES (DBF/DBC)***
***END OF DATABASE FILES (DBF/DBC)***
***<Time><Tx/Rx><Channel><CAN ID><Type><DLC><DataBytes>***
11:49:12:9420 Rx 1 0x023 s 1 40
11:49:12:9440 Rx 1 0x460 s 8 03 E0 00 00 C0 00 00 00
11:49:12:9530 Rx 1 0x023 s 1 40
11:49:12:9680 Rx 1 0x408 s 8 0F 02 00 30 00 00 7F 00
11:49:12:9680 Rx 1 0x40B s 8 00 00 00 00 00 10 60 00
11:49:12:9690 Rx 1 0x045 s 8 40 00 00 00 00 00 00 00
*/
bool FrameFileIO::loadLogFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    uint64_t timeStamp = Utility::GetTimeMS();
    int lineCounter = 0;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    line = inFile->readLine(); //read out the header first and discard it.

    while (!inFile->atEnd()) {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        line = inFile->readLine();
        if (line.startsWith("***")) continue;
        if (line.length() > 1)
        {
            QList<QByteArray> tokens = line.split(' ');
            QList<QByteArray> timeToks = tokens[0].split(':');
            timeStamp = (timeToks[0].toInt() * (1000ul * 1000ul * 60ul * 60ul)) + (timeToks[1].toInt() * (1000ul * 1000ul * 60ul))
                      + (timeToks[2].toInt() * (1000ul * 1000ul)) + (timeToks[3].toInt() * 100ul);
            thisFrame.timestamp = timeStamp;
            thisFrame.ID = tokens[3].right(tokens[3].length() - 2).toInt(NULL, 16);
            if (tokens[4] == "s") thisFrame.extended = false;
            else thisFrame.extended = true;
            thisFrame.bus = tokens[2].toInt() - 1;
            thisFrame.len = tokens[5].toInt();
            for (int d = 0; d < thisFrame.len; d++) thisFrame.data[d] = tokens[d + 6].toInt(NULL, 16);
        }
        frames->append(thisFrame);
    }
    inFile->close();
    delete inFile;
    return true;
}

bool FrameFileIO::saveLogFile(QString filename, const QVector<CANFrame>* frames)
{
    QFile *outFile = new QFile(filename);
    QDateTime timestamp, tempStamp;
    int lineCounter = 0;

    timestamp = QDateTime::currentDateTime();

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete outFile;
        return false;
    }

    outFile->write("***BUSMASTER Ver 2.4.0***\n");
    outFile->write("***PROTOCOL CAN***\n");
    outFile->write("***NOTE: PLEASE DO NOT EDIT THIS DOCUMENT***\n");
    outFile->write("***[START LOGGING SESSION]***\n");
    outFile->write("***START DATE AND TIME ");
    outFile->write(timestamp.toString("d:M:yyyy h:m:s:z").toUtf8());
    outFile->write("***\n");
    outFile->write("***HEX***\n");
    outFile->write("***SYSTEM MODE***\n");
    outFile->write("***START CHANNEL BAUD RATE***\n");
    outFile->write("***CHANNEL 1 - Kvaser - Kvaser Leaf Light HS #0 (Channel 0), Serial Number- 0, Firmware- 0x00000037 0x00020000 - 500000 bps***\n");
    outFile->write("***END CHANNEL BAUD RATE***\n");
    outFile->write("***START DATABASE FILES (DBF/DBC)***\n");
    outFile->write("***END OF DATABASE FILES (DBF/DBC)***\n");
    outFile->write("***<Time><Tx/Rx><Channel><CAN ID><Type><DLC><DataBytes>***\n");

    for (int c = 0; c < frames->count(); c++)
    {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        tempStamp = timestamp.addMSecs(frames->at(c).timestamp / 1000);
        outFile->write(tempStamp.toString("h:m:s:z").toUtf8());
        outFile->write(" Rx ");
        outFile->write(QString::number(frames->at(c).bus).toUtf8() + " ");
        outFile->write(QString::number(frames->at(c).ID, 16).toUpper().rightJustified(8, '0').toUtf8());
        if (frames->at(c).extended) outFile->write(" x ");
            else outFile->write(" s ");
        outFile->write(QString::number(frames->at(c).len).toUtf8() + " ");

        for (int temp = 0; temp < frames->at(c).len; temp++)
        {
            outFile->write(QString::number(frames->at(c).data[temp], 16).toUpper().rightJustified(2, '0').toUtf8());
            outFile->putChar(' ');
        }

        outFile->write("\n");

    }
    outFile->close();
    delete outFile;
    return true;
}

//log file from microchip tool
/*
tokens:
0 = timestamp
1 = Transmission direction
2 = ID
3 = Data byte length
4-x = The data bytes
*/
bool FrameFileIO::loadMicrochipFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    bool inComment = false;
    long long timeStamp = Utility::GetTimeMS();
    int lineCounter = 0;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    //line = inFile->readLine(); //read out the header first and discard it.

    while (!inFile->atEnd()) {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        line = inFile->readLine();
        if (line.length() > 2)
        {
            if (line.startsWith("//"))
            {
                inComment = !inComment;
            }
            else
            {
                if (!inComment)
                {
                    QList<QByteArray> tokens = line.split(';');
                    timeStamp += 5;
                    thisFrame.timestamp = timeStamp;

                    thisFrame.ID = Utility::ParseStringToNum(tokens[2]);
                    if (thisFrame.ID <= 0x7FF) thisFrame.extended = false;
                    else thisFrame.extended = true;
                    thisFrame.bus = 0;
                    thisFrame.len = tokens[3].toInt();
                    for (int d = 0; d < thisFrame.len; d++) thisFrame.data[d] = (unsigned char)Utility::ParseStringToNum(tokens[4 + d]);
                    frames->append(thisFrame);
                }
            }
        }
    }
    inFile->close();
    delete inFile;
    return true;
}

/*
1247;RX;0xC2;8;0x5C;0x87;0x00;0x00;0x01;0x00;0x00;0x1C
1218;RX;0x236;1;0x00;0x00;0x00;0x00;0x00;0x00;0x00;0xF0

tokens:
0 = timestamp in ms
1 = direction (RX, TX)
2 = ID (in hex with 0x prefix)
3 = data length
4-x = data bytes in hex with 0x prefix
*/
bool FrameFileIO::saveMicrochipFile(QString filename, const QVector<CANFrame>* frames)
{
    QFile *outFile = new QFile(filename);
    QDateTime timestamp, tempStamp;
    int lineCounter = 0;

    timestamp = QDateTime::currentDateTime();

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete outFile;
        return false;
    }

    outFile->write("//---------------------------------\n");
    outFile->write("Microchip Technology Inc.\n");
    outFile->write("CAN BUS Analyzer\n");
    outFile->write("SavvyCAN Exporter\n");
    outFile->write("Logging Started: ");
    outFile->write(timestamp.toString("d/M/yyyy h:m:s").toUtf8());
    outFile->write("\n");
    outFile->write("//---------------------------------\n");

    for (int c = 0; c < frames->count(); c++)
    {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        outFile->write(QString::number((int)(frames->at(c).timestamp / 1000)).toUtf8());
        outFile->write(";RX;");
        outFile->write("0x" + QString::number(frames->at(c).ID, 16).toUpper().rightJustified(8, '0').toUtf8() + ";");
        outFile->write(QString::number(frames->at(c).len).toUtf8() + ";");

        for (int temp = 0; temp < frames->at(c).len; temp++)
        {
            outFile->write("0x" + QString::number(frames->at(c).data[temp], 16).toUpper().rightJustified(2, '0').toUtf8());
            outFile->putChar(';');
        }

        outFile->write("\n");

    }
    outFile->close();
    delete outFile;
    return true;
}
