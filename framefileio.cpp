#include "framefileio.h"

#include <QProgressDialog>

FrameFileIO::FrameFileIO()
{

}

bool FrameFileIO::saveFrameFile(QString &fileName, const QVector<CANFrame>* frameCache)
{
    QString filename;
    QFileDialog dialog(qApp->activeWindow());
    bool result = false;

    QStringList filters;
    filters.append(QString(tr("GVRET Logs (*.csv *.CSV)")));
    filters.append(QString(tr("CRTD Logs (*.txt *.TXT)")));
    filters.append(QString(tr("Generic ID/Data CSV (*.csv *.CSV)")));
    filters.append(QString(tr("BusMaster Log (*.log *.LOG)")));
    filters.append(QString(tr("Microchip Log (*.can *.CAN)")));
    filters.append(QString(tr("Vector Trace Files (*.trace *.TRACE)")));
    filters.append(QString(tr("IXXAT MiniLog (*.csv *.CSV)")));
    filters.append(QString(tr("CAN-DO Log (*.can *.avc *.evc *.qcc *.CAN *.AVC *.EVC *.QCC)")));
    filters.append(QString(tr("Vehicle Spy (*.csv *.CSV)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];

        QProgressDialog progress(qApp->activeWindow());
        progress.setWindowModality(Qt::WindowModal);
        progress.setLabelText("Saving file...");
        progress.setCancelButton(0);
        progress.setRange(0,0);
        progress.setMinimumDuration(0);
        progress.show();

        qApp->processEvents();

        if (dialog.selectedNameFilter() == filters[0])
        {
            if (!filename.contains('.')) filename += ".csv";
            result = saveNativeCSVFile(filename, frameCache);
        }
        if (dialog.selectedNameFilter() == filters[1])
        {
            if (!filename.contains('.')) filename += ".txt";
            result = saveCRTDFile(filename, frameCache);
        }
        if (dialog.selectedNameFilter() == filters[2])
        {
            if (!filename.contains('.')) filename += ".csv";
            result = saveGenericCSVFile(filename, frameCache);
        }
        if (dialog.selectedNameFilter() == filters[3])
        {
            if (!filename.contains('.')) filename += ".log";
            result = saveLogFile(filename, frameCache);
        }
        if (dialog.selectedNameFilter() == filters[4])
        {
            if (!filename.contains('.')) filename += ".log";
            result = saveMicrochipFile(filename, frameCache);
        }

        if (dialog.selectedNameFilter() == filters[5])
        {
            if (!filename.contains('.')) filename += ".trace";
            result = saveTraceFile(filename, frameCache);
        }

        if (dialog.selectedNameFilter() == filters[6])
        {
            if (!filename.contains('.')) filename += ".csv";
            result = saveIXXATFile(filename, frameCache);
        }

        if (dialog.selectedNameFilter() == filters[7])
        {
            if (!filename.contains('.')) filename += ".can";
            result = saveCANDOFile(filename, frameCache);
        }

        if (dialog.selectedNameFilter() == filters[8])
        {
            if (!filename.contains('.')) filename += ".csv";
            result = saveVehicleSpyFile(filename, frameCache);
        }

        progress.cancel();

        if (result)
        {
            QStringList fileList = filename.split('/');
            fileName = fileList[fileList.length() - 1];
            return true;
        }
        return false;
    }
    return false;
}

bool FrameFileIO::loadFrameFile(QString &fileName, QVector<CANFrame>* frameCache)
{
    QString filename;
    QFileDialog dialog;
    bool result = false;

    QStringList filters;
    filters.append(QString(tr("GVRET Logs (*.csv *.CSV)")));
    filters.append(QString(tr("CRTD Logs (*.txt *.TXT)")));
    filters.append(QString(tr("Generic ID/Data CSV (*.csv *.CSV)")));
    filters.append(QString(tr("BusMaster Log (*.log *.LOG)")));
    filters.append(QString(tr("Microchip Log (*.can *.CAN)")));
    filters.append(QString(tr("Vector trace files (*.trace *.TRACE)")));
    filters.append(QString(tr("IXXAT MiniLog (*.csv *.CSV)")));
    filters.append(QString(tr("CAN-DO Log (*.avc *.can *.evc *.qcc *.AVC *.CAN *.EVC *.QCC)")));
    filters.append(QString(tr("Vehicle Spy (*.csv *.CSV)")));

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

        if (dialog.selectedNameFilter() == filters[0]) result = loadNativeCSVFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[1]) result = loadCRTDFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[2]) result = loadGenericCSVFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[3]) result = loadLogFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[4]) result = loadMicrochipFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[5]) result = loadTraceFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[6]) result = loadIXXATFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[7]) result = loadCANDOFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[8]) result = loadVehicleSpyFile(filename, frameCache);

        progress.cancel();

        if (result)
        {
            QStringList fileList = filename.split('/');
            fileName = fileList[fileList.length() - 1];
            return true;
        }
        else return false;
    }
    return false;
}


//2,2550.368293675,0.003818174999651092,67371008,F,F,HS CAN $119,HS CAN,,119,F,F,00,00,00,00,00,00,0D,8B,,,
//Line,Abs Time(Sec),Rel Time (Sec),Status,Er,Tx,Description,Network,Node,Arb ID,Remote,Xtd,B1,B2,B3,B4,B5,B6,B7,B8,Value,Trigger,Signals
// 0       1             2             3   4  5   6             7     8     9     10     11 12 13 14 15 16 17 18 19  20     21      22
bool FrameFileIO::loadVehicleSpyFile(QString filename, QVector<CANFrame> *frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    int lineCounter = 0;
    bool pastHeader = false;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    while (!inFile->atEnd() && !pastHeader)
    {
        line = inFile->readLine().simplified().toUpper();
        if (line.startsWith("LINE")) lineCounter++;
        if (lineCounter == 2) pastHeader = true;
    }

    while (!inFile->atEnd()) {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }
        line = inFile->readLine().simplified().toUpper();

        QList<QByteArray> tokens = line.split(',');
        thisFrame.bus = 0;
        thisFrame.timestamp = tokens[1].toDouble() * 1000000.0;
        if (tokens[5].startsWith("T")) thisFrame.isReceived = false;
            else thisFrame.isReceived = true;
        thisFrame.ID = tokens[9].toInt(NULL, 16);
        if (tokens[11].startsWith("T")) thisFrame.extended = true;
            else thisFrame.extended = false;

        thisFrame.len = 0;
        for (int i = 0; i < 8; i++)
        {
            if (tokens[12 + i].length() > 0)
            {
                thisFrame.data[i] = tokens[12 + i].toInt(NULL, 16);
                thisFrame.len++;
            }
            else break;
        }

        frames->append(thisFrame);
    }
    inFile->close();
    delete inFile;
    return true;
}

bool FrameFileIO::saveVehicleSpyFile(QString filename, const QVector<CANFrame> *frames)
{
    return true;
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

    line = inFile->readLine().toUpper(); //read out the header first and discard it.

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
                //int decimalPlaces = tokens[0].length() - tokens[0].indexOf('.') - 1;
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
            char firstChar = tokens[1].left(1)[0];
            if (firstChar == 'R' || firstChar == 'T')
            {
                thisFrame.ID = tokens[2].toInt(NULL, 16);
                if (tokens[1] == "R29" || tokens[1] == "T29") thisFrame.extended = true;
                else thisFrame.extended = false;
                if (firstChar == 'T') thisFrame.isReceived = false;
                else thisFrame.isReceived = true;
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

        if (frames->at(c).isReceived) outFile->putChar('R');
        else outFile->putChar('T');

        if (frames->at(c).extended)
        {
            outFile->write("29 ");
        }
        else outFile->write("11 ");
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
    int fileVersion = 1;
    long long timeStamp = Utility::GetTimeMS();
    int lineCounter = 0;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    line = inFile->readLine().toUpper(); //read out the header first and discard it.
    if (line.at(23) == 'D') fileVersion = 2; //Dir is found starting at position 23 if this is a V2 file

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

            if (fileVersion == 1)
            {
                thisFrame.isReceived = true;
                thisFrame.bus = tokens[3].toInt();
                thisFrame.len = tokens[4].toInt();
                for (int c = 0; c < 8; c++) thisFrame.data[c] = 0;
                for (int d = 0; d < thisFrame.len; d++)
                    thisFrame.data[d] = tokens[5 + d].toInt(NULL, 16);
            }
            else if (fileVersion == 2)
            {
                if (tokens[3].at(0) == 'R') thisFrame.isReceived = true;
                else thisFrame.isReceived = false;
                thisFrame.bus = tokens[4].toInt();
                thisFrame.len = tokens[5].toInt();
                for (int c = 0; c < 8; c++) thisFrame.data[c] = 0;
                for (int d = 0; d < thisFrame.len; d++)
                    thisFrame.data[d] = tokens[6 + d].toInt(NULL, 16);
            }

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

    outFile->write("Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8");
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

        if (frames->at(c).isReceived) outFile->write("Rx,");
        else outFile->write("Tx,");

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

        line = inFile->readLine().toUpper();
        if (line.startsWith("***")) continue;
        if (line.length() > 1)
        {
            QList<QByteArray> tokens = line.split(' ');
            QList<QByteArray> timeToks = tokens[0].split(':');
            timeStamp = (timeToks[0].toInt() * (1000ul * 1000ul * 60ul * 60ul)) + (timeToks[1].toInt() * (1000ul * 1000ul * 60ul))
                      + (timeToks[2].toInt() * (1000ul * 1000ul)) + (timeToks[3].toInt() * 100ul);
            thisFrame.timestamp = timeStamp;
            if (tokens[1].at(0) == 'R') thisFrame.isReceived = true;
            else thisFrame.isReceived = false;
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
        if (frames->at(c).isReceived) outFile->write(" Rx ");
        else outFile->write(" Tx ");
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

//"00:01:03.03","223","Std","","00 00 00 00 49 00 00 01 "
bool FrameFileIO::loadIXXATFile(QString filename, QVector<CANFrame>* frames)
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

    for (int i = 0; i < 7; i++) line = inFile->readLine(); //read out the header first and discard it.

    while (!inFile->atEnd()) {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        line = inFile->readLine().toUpper();
        if (line.length() > 1)
        {
            QList<QByteArray> tokens = line.split(',');
            QString timePortion = unQuote(tokens[0]);
            QStringList timeToks = timePortion.split(':');
            timeStamp = (timeToks[0].toInt() * (1000ul * 1000ul * 60ul * 60ul)) + (timeToks[1].toInt() * (1000ul * 1000ul * 60ul))
                      + (timeToks[2].toDouble() * (1000.0 * 1000.0));
            thisFrame.timestamp = timeStamp;
            thisFrame.ID = unQuote(tokens[1]).toInt(NULL, 16);
            if (unQuote(tokens[2]).toUpper().at(0) == 'S') thisFrame.extended = false;
                else thisFrame.extended = true;

            thisFrame.isReceived = true;
            thisFrame.bus = 0;

            QStringList dataToks = unQuote(tokens[4]).simplified().split(' ');
            thisFrame.len = dataToks.length();
            for (int d = 0; d < thisFrame.len; d++) thisFrame.data[d] = dataToks[d].toInt(NULL, 16);
        }
        frames->append(thisFrame);
    }
    inFile->close();
    delete inFile;
    return true;
}

bool FrameFileIO::saveIXXATFile(QString filename, const QVector<CANFrame>* frames)
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

    outFile->write("ASCII Trace IXXAT SavvyCAN V" + QString::number(VERSION).toUtf8() + "\n");
    outFile->write("Date: " + timestamp.toString("d:M:yyyy").toUtf8() + "\n");
    outFile->write("Start time: " + timestamp.toString("h:m:s").toUtf8() + "\n");
    timestamp.addMSecs((frames->last().timestamp - frames->first().timestamp) / 1000);
    outFile->write("Stop time: " + timestamp.toString("h:m:s").toUtf8() + "\n");
    outFile->write("Overruns: 0\n");
    outFile->write("Baudrate: 500 kbit/s\n"); //could be a lie... this code has no way to know the baud rate (at the moment)
    outFile->write("\"Time\",\"Identifier (hex)\",\"Format\",\"Flags\",\"Data (hex)\"\n");

    for (int c = 0; c < frames->count(); c++)
    {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        timestamp.setTime(QTime());

        tempStamp = timestamp.addMSecs(frames->at(c).timestamp / 1000);
        outFile->write("\"" + tempStamp.toString("h:m:s.").toUtf8() + tempStamp.toString("z").rightJustified(3, '0').toUtf8() + "\"");

        outFile->write(",\"" + QString::number(frames->at(c).ID, 16).toUpper().rightJustified(8, '0').toUtf8() + "\"");
        if (frames->at(c).extended) outFile->write(",\"Ext\"");
            else outFile->write(",\"Std\"");
        outFile->write(",\"\",\"");

        for (int temp = 0; temp < frames->at(c).len; temp++)
        {
            outFile->write(QString::number(frames->at(c).data[temp], 16).toUpper().rightJustified(2, '0').toUtf8());
            outFile->putChar(' ');
        }

        outFile->write("\"\n");

    }
    outFile->close();
    delete outFile;
    return true;
}

bool FrameFileIO::loadCANDOFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    int lineCounter = 0;
    QByteArray data;
    int timeOffset = 0;
    uint64_t lastTimeStamp = 0;

    if (!inFile->open(QIODevice::ReadOnly))
    {
        delete inFile;
        return false;
    }

    //this file format is in static 12 byte blocks.
    //Bytes 0 - 1 are a time stamp
    //Bytes 2 - 3 are the data length (top 4 bits) then ID (bottom 11 bits)
    //Bytes 4 - 11 are the data bytes (padded with FF for bytes not used)

    while (!inFile->atEnd())
    {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        data = inFile->read(12);

        thisFrame.bus = 0;
        thisFrame.isReceived = true;
        thisFrame.extended = false; //format is incapable of extended frames

        thisFrame.timestamp = 1000000ul * ((unsigned char)data[0] >> 2);
        thisFrame.timestamp += (((data[0] & 3) << 8) + (unsigned char)data[1]) * 1000;
        thisFrame.timestamp += timeOffset;
        if (thisFrame.timestamp < lastTimeStamp)
        {
            timeOffset += 60000000ul;
        }
        lastTimeStamp = thisFrame.timestamp;
        thisFrame.ID = ((unsigned char)data[3] * 256 + (unsigned char)data[2]) & 0x7FF;
        thisFrame.len = (unsigned char)data[3] >> 4;

        if (thisFrame.len <= 8 && thisFrame.ID <= 0x7FF)
        {
            for (int d = 0; d < thisFrame.len; d++) thisFrame.data[d] = (unsigned char)data[4 + d];
            frames->append(thisFrame);
        }
    }

    inFile->close();
    delete inFile;
    return true;
}

bool FrameFileIO::saveCANDOFile(QString filename, const QVector<CANFrame>* frames)
{
    QFile *outFile = new QFile(filename);
    int lineCounter = 0;
    QByteArray data;
    CANFrame thisFrame;
    int ms, id;

    if (!outFile->open(QIODevice::WriteOnly))
    {
        delete outFile;
        return false;
    }

    data.reserve(13);

    //The initial frame in official files sets the global time but I don't care so it is set all zeros here.
    thisFrame = frames->at(0);
    ms = (thisFrame.timestamp / 1000);
    data[0] = (((ms / 1000) % 60) << 2) + ((ms % 1000) >> 8);
    data[1] = (char)(ms & 0xFF);
    data[2] = 0xFF;
    data[3] = 0xFF;
    for (int l = 0; l < 8; l++) data[4 + l] = 0;
    outFile->write(data);

    for (int c = 0; c < frames->count(); c++)
    {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }
        for (int j = 0; j < 8; j++) data[4 + j] = 0xFF;        

        thisFrame = frames->at(c);
        if (!thisFrame.extended)
        {
            ms = (thisFrame.timestamp / 1000);
            id = thisFrame.ID & 0x7FF;
            data[0] = (((ms / 1000) % 60) << 2) + ((ms % 1000) >> 8);
            data[1] = (char)(ms & 0xFF);
            data[2] = (char)(id & 0xFF);
            data[3] = (char)((id >> 8) + (thisFrame.len << 4));
            for (int d = 0; d < thisFrame.len; d++) data[4 + d] = (char)thisFrame.data[d];
            outFile->write(data);
        }
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
    long long timeStamp;
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
                    timeStamp = tokens[0].toInt() * 1000;
                    thisFrame.timestamp = timeStamp;
                    if (tokens[1].at(0) == 'R') thisFrame.isReceived = true;
                    else thisFrame.isReceived = false;
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
        if (frames->at(c).isReceived) outFile->write(";RX;");
        else outFile->write(";TX;");
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


/*
;  CAN Logger trace file
;  Device Serial Number :
;  Start Time : Tue, Nov 17, 2015 :: 11:19:59
;
;  Column description :
;  ~~~~~~~~~~~~~~~~~~~~~
;
;   + Message Number
;   |
;   |     	     + Time Stamp (ms)
;   |     	     |
;   |     	     |      	    + Message ID (hex)
;   |     	     |      	    |
;   |     	     |      	    |   	+ Data Length Code
;   |     	     |      	    |   	|
;   |     	     |      	    |   	|	 + Data Bytes (hex)
;   |     	     |      	    |   	|	 |
;---+-----	-----+------	----+---	+	-+ -- -- -- -- -- -- --
         0	00:00:00:0008	00000268	8	55 00 84 03 A8 0D 38 00
         1	00:00:00:0021	0000000E	8	1F D3 3F FF 08 FF E0 CB

It appears that all lines that start in a semicolon are comments
The rest of the lines need the whitespace stripped from beginning and end
and then it appears to be tab delimited. The first field is a sequence number
and irrelevant. The second field is a funky timestamp, then message id, etc as
shown in the file comments. The bytes seem to be space delimited and in hex
*/

bool FrameFileIO::loadTraceFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    long long timeStamp = 0;
    int lineCounter = 0;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    while (!inFile->atEnd()) {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        line = inFile->readLine();
        line = line.trimmed();
        if (line.length() > 2)
        {
            if (line.startsWith(";"))
            {
                // a comment. Ignore it.
            }
            else
            {
                QList<QByteArray> tokens = line.split('\t');

                QList<QByteArray> timestampToks = tokens[1].split(':');

                timeStamp = timestampToks[0].toInt() * 1000000ul * 60 * 60;
                timeStamp += timestampToks[1].toInt() * 1000000ul * 60;
                timeStamp += timestampToks[2].toInt() * 1000000ul;
                timeStamp += timestampToks[3].toInt() * 100;

                thisFrame.timestamp = timeStamp;

                thisFrame.ID = tokens[2].toLong(NULL, 16);
                if (thisFrame.ID <= 0x7FF) thisFrame.extended = false;
                else thisFrame.extended = true;
                thisFrame.bus = 0;
                thisFrame.len = tokens[3].toInt();

                QList<QByteArray> dataToks = tokens[4].split(' ');
                for (int d = 0; d < thisFrame.len; d++) thisFrame.data[d] = (unsigned char)dataToks[d].toInt(NULL, 16);
                frames->append(thisFrame);
            }
        }
    }
    inFile->close();
    delete inFile;
    return true;
}

bool FrameFileIO::saveTraceFile(QString filename, const QVector<CANFrame> * frames)
{
    QFile *outFile = new QFile(filename);
    QDateTime timestamp;
    int lineCounter = 0;
    uint64_t tempTime;
    int tempTimePiece;

    timestamp = QDateTime::currentDateTime();

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete outFile;
        return false;
    }

    outFile->write(";  SavvyCAN CAN Logger trace file\n");
    outFile->write(";  Device Serial Number : 0000 \n");
    outFile->write(";  Start Time : ");
    outFile->write(timestamp.toString("ddd, MMM dd, yyyy :: h:m:s\n").toUtf8());
    outFile->write(";\n");
    outFile->write(";  Column description :\n");
    outFile->write(";  ~~~~~~~~~~~~~~~~~~~~~\n");
    outFile->write(";\n");
    outFile->write(";   + Message Number\n");
    outFile->write(";   |\n");
    outFile->write(";   |     	     + Time Stamp (ms)\n");
    outFile->write(";   |     	     |\n");
    outFile->write(";   |     	     |      	    + Message ID (hex)\n");
    outFile->write(";   |     	     |      	    |\n");
    outFile->write(";   |     	     |      	    |   	+ Data Length Code\n");
    outFile->write(";   |     	     |      	    |   	|\n");
    outFile->write(";   |     	     |      	    |   	|	 + Data Bytes (hex)\n");
    outFile->write(";   |     	     |      	    |   	|	 |\n");
    outFile->write(";---+-----	-----+------	----+---	+	-+ -- -- -- -- -- -- --\n");


    for (int c = 0; c < frames->count(); c++)
    {
        lineCounter++;
        if ((lineCounter % 100) == 0)
        {
            qApp->processEvents();
            //lineCounter = 0;
        }


         //1F D3 3F FF 08 FF E0 CB
        outFile->write(QString::number(lineCounter).rightJustified(10, ' ').toUtf8());
        outFile->write("\t");

        tempTime = frames->at(c).timestamp;
        tempTimePiece = tempTime / 1000000ul / 60 / 60;
        tempTime -= tempTimePiece * 1000000ul * 60 * 60;
        outFile->write(QString::number(tempTimePiece).rightJustified(2, '0').toUtf8());
        outFile->write(":");

        tempTimePiece = tempTime / 1000000ul / 60;
        tempTime -= tempTimePiece * 1000000ul * 60;
        outFile->write(QString::number(tempTimePiece).rightJustified(2, '0').toUtf8());
        outFile->write(":");

        tempTimePiece = tempTime / 1000000ul;
        tempTime -= tempTimePiece * 1000000ul;
        outFile->write(QString::number(tempTimePiece).rightJustified(2, '0').toUtf8());
        outFile->write(":");

        tempTimePiece = tempTime / 100;
        outFile->write(QString::number(tempTimePiece).rightJustified(4, '0').toUtf8());
        outFile->write("\t");

        outFile->write(QString::number(frames->at(c).ID, 16).toUpper().rightJustified(8, '0').toUtf8() + "\t");

        outFile->write(QString::number(frames->at(c).len).toUtf8() + "\t");

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

QString FrameFileIO::unQuote(QString inStr)
{
    return inStr.split('\"')[1];
}
