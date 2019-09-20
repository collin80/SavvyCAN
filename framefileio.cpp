#include "framefileio.h"

#include <QMessageBox>
#include <QProgressDialog>
#include <QDateTime>
#include <QtEndian>
#include <QSettings>
#include <iostream>

#include "utility.h"
#include "blfhandler.h"

QFile FrameFileIO::continuousFile;

FrameFileIO::FrameFileIO()
{
}

bool FrameFileIO::saveFrameFile(QString &fileName, const QVector<CANFrame>* frameCache)
{
    QString filename;
    QFileDialog dialog(qApp->activeWindow());
    QSettings settings;
    bool result = false;

    QStringList filters;
    filters.append(QString(tr("GVRET Logs (*.csv *.CSV)")));
    filters.append(QString(tr("CRTD Logs (*.crt *.crtd *.CRT *.CRTD)")));
    filters.append(QString(tr("Generic ID/Data CSV (*.csv *.CSV)")));
    filters.append(QString(tr("BusMaster Log (*.log *.LOG)")));
    filters.append(QString(tr("Microchip Log (*.can *.CAN)")));
    filters.append(QString(tr("Vector Trace Files (*.trace *.TRACE)")));
    filters.append(QString(tr("IXXAT MiniLog (*.csv *.CSV)")));
    filters.append(QString(tr("CAN-DO Log (*.can *.avc *.evc *.qcc *.CAN *.AVC *.EVC *.QCC)")));
    filters.append(QString(tr("Vehicle Spy (*.csv *.CSV)")));
    filters.append(QString(tr("Candump/Kayak(*.log)")));
    filters.append(QString(tr("Cabana Log(*.csv *.CSV)")));
    filters.append(QString(tr("CANalyzer Ascii Log (*.asc *.ASC)")));

    dialog.setDirectory(settings.value("FileIO/LoadSaveDirectory", dialog.directory().path()).toString());
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
        progress.setCancelButton(nullptr);
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
        if (dialog.selectedNameFilter() == filters[9])
        {
            if (!filename.contains('.')) filename += ".log";
            saveCanDumpFile(filename,frameCache);
        }
        if (dialog.selectedNameFilter() == filters[10])
        {
            if (!filename.contains('.')) filename += ".csv";
            result = saveCabanaFile(filename, frameCache);
        }
        if (dialog.selectedNameFilter() == filters[11])
        {
            if (!filename.contains('.')) filename += ".asc";
            result = saveCanalyzerASC(filename, frameCache);
        }

        progress.cancel();

        if (result)
        {
            QStringList fileList = filename.split('/');
            fileName = fileList[fileList.length() - 1];
            settings.setValue("FileIO/LoadSaveDirectory", dialog.directory().path());
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
    QSettings settings;
    bool result = false;

    QStringList filters;
    filters.append(QString(tr("Autodetect File Type (*.*)")));
    filters.append(QString(tr("GVRET Logs (*.csv *.CSV)")));
    filters.append(QString(tr("CRTD Logs (*.crt *.crtd *.CRT *.CRTD)")));
    filters.append(QString(tr("Generic ID/Data CSV (*.csv *.CSV)")));
    filters.append(QString(tr("BusMaster Log (*.log *.LOG)")));
    filters.append(QString(tr("Microchip Log (*.can *.CAN)")));
    filters.append(QString(tr("Vector trace files (*.trace *.TRACE)")));
    filters.append(QString(tr("IXXAT MiniLog (*.csv *.CSV)")));
    filters.append(QString(tr("CAN-DO Log (*.avc *.can *.evc *.qcc *.AVC *.CAN *.EVC *.QCC)")));
    filters.append(QString(tr("Vehicle Spy (*.csv *.CSV)")));
    filters.append(QString(tr("Candump/Kayak (*.log *.LOG)")));
    filters.append(QString(tr("PCAN Viewer (*.trc *.TRC)")));
    filters.append(QString(tr("Kvaser Log Decimal (*.txt *.TXT)")));
    filters.append(QString(tr("Kvaser Log Hex (*.txt *.TXT)")));
    filters.append(QString(tr("CANalyzer Ascii Log (*.asc *.ASC)")));
    filters.append(QString(tr("CANalyzer Binary Log Files (*.blf *.BLF)")));
    filters.append(QString(tr("CANHacker Trace Files (*.trc *.TRC)")));
    filters.append(QString(tr("Cabana Log (*.csv *.CSV)")));
    filters.append(QString(tr("CANOpen Magic (*.csv *.CSV)")));

    dialog.setDirectory(settings.value("FileIO/LoadSaveDirectory", dialog.directory().path()).toString());
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];

        QProgressDialog progress(qApp->activeWindow());
        progress.setWindowModality(Qt::WindowModal);
        progress.setLabelText("Loading file...");
        progress.setCancelButton(nullptr);
        progress.setRange(0,0);
        progress.setMinimumDuration(0);
        progress.show();

        qApp->processEvents();

        if (dialog.selectedNameFilter() == filters[0]) result = autoDetectLoadFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[1]) result = loadNativeCSVFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[2]) result = loadCRTDFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[3]) result = loadGenericCSVFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[4]) result = loadLogFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[5]) result = loadMicrochipFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[6]) result = loadTraceFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[7]) result = loadIXXATFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[8]) result = loadCANDOFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[9]) result = loadVehicleSpyFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[10]) result = loadCanDumpFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[11]) result = loadPCANFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[12]) result = loadKvaserFile(filename, frameCache, false);
        if (dialog.selectedNameFilter() == filters[13]) result = loadKvaserFile(filename, frameCache, true);
        if (dialog.selectedNameFilter() == filters[14]) result = loadCanalyzerASC(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[15]) result = loadCanalyzerBLF(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[16]) result = loadCANHackerFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[17]) result = loadCabanaFile(filename, frameCache);
        if (dialog.selectedNameFilter() == filters[18]) result = loadCANOpenFile(filename, frameCache);

        progress.cancel();

        if (result)
        {
            QStringList fileList = filename.split('/');
            fileName = fileList[fileList.length() - 1];
            settings.setValue("FileIO/LoadSaveDirectory", dialog.directory().path());
            return true;
        }
        else
        {
            QMessageBox msgBox;
            msgBox.setText("File load completed with errors.\r\nPerhaps you selected the wrong file type?");
            msgBox.exec();
            return false;
        }
    }
    return false;
}


//Try every format by first uses the "is" functions which try to detect whether a given file is a good match to that
//file format or not. Those functions are much less tolerant than the load functions and so should help to discriminate
//whether a file could be loaded or not by a given loader. The loader return is still used in case the guess was wrong.
bool FrameFileIO::autoDetectLoadFile(QString filename, QVector<CANFrame>* frames)
{
    qDebug() << "Attempting Canalyzer BLF";
    if (isCanalyzerBLF(filename))
    {
        if (loadCanalyzerBLF(filename, frames))
        {
            qDebug() << "Loaded as Canalyzer BLF successfully!";
            return true;
        }
    }

    qDebug() << "Attempting native CSV";
    if (isNativeCSVFile(filename))
    {
        if (loadNativeCSVFile(filename, frames))
        {
            qDebug() << "Loaded as native CSV successfully!";
            return true;
        }
    }

    qDebug() << "Attempting canalyzer ASC";
    if (isCanalyzerASC(filename))
    {
        if (loadCanalyzerASC(filename, frames))
        {
            qDebug() << "Loaded as Canalyzer ASC successfully!";
            return true;
        }
    }

    qDebug() << "Attempting CRTD";
    if (isCRTDFile(filename))
    {
        if (loadCRTDFile(filename, frames))
        {
            qDebug() << "Loaded as CRTD successfully!";
            return true;
        }
    }


    qDebug() << "Attempting trace file";
    if (isTraceFile(filename))
    {
        if (loadTraceFile(filename, frames))
        {
            qDebug() << "Loaded as trace successfully!";
            return true;
        }
    }

    qDebug() << "Attempting vehicle spy";
    if (isVehicleSpyFile(filename))
    {
        if (loadVehicleSpyFile(filename, frames))
        {
            qDebug() << "Loaded as vehicle spy successfully!";
            return true;
        }
    }

    qDebug() << "Attempting candump";
    if (isCanDumpFile(filename))
    {
        if (loadCanDumpFile(filename, frames))
        {
            qDebug() << "Loaded as candump successfully!";
            return true;
        }
    }

    qDebug() << "Attempting canhacker";
    if (isCANHackerFile(filename))
    {
        if (loadCANHackerFile(filename, frames))
        {
            qDebug() << "Loaded as CANHacker successfully!";
            return true;
        }
    }

    qDebug() << "Attempting cabana";
    if (isCabanaFile(filename))
    {
        if (loadCabanaFile(filename, frames))
        {
            qDebug() << "Loaded as Cabana successfully!";
            return true;
        }
    }

    qDebug() << "Attempting canopen";
    if (isCANOpenFile(filename))
    {
        if (loadCANOpenFile(filename, frames))
        {
            qDebug() << "Loaded as CANOpen Magic successfully!";
            return true;
        }
    }

    qDebug() << "Attempting busmaster log";
    if (isLogFile(filename))
    {
        if (loadLogFile(filename, frames))
        {
            qDebug() << "Loaded as Busmaster Log successfully!";
            return true;
        }
    }

    qDebug() << "Attempting pcan";
    if (isPCANFile(filename))
    {
        if (loadPCANFile(filename, frames))
        {
            qDebug() << "Loaded as PCAN successfully!";
            return true;
        }
    }

    qDebug() << "Attempting ixxat";
    if (isIXXATFile(filename))
    {
        if (loadIXXATFile(filename, frames))
        {
            qDebug() << "Loaded as IXXAT successfully!";
            return true;
        }
    }

    qDebug() << "Attempting microchip";
    if (isMicrochipFile(filename))
    {
        if (loadMicrochipFile(filename, frames))
        {
            qDebug() << "Loaded as microchip successfully!";
            return true;
        }
    }

    qDebug() << "Attempting CANDo";
    if (isCANDOFile(filename))
    {
        if (loadCANDOFile(filename, frames))
        {
            qDebug() << "Loaded as CANDO successfully!";
            return true;
        }
    }

    qDebug() << "Attempting kvaser";
    if (isKvaserFile(filename))
    {
        if (loadKvaserFile(filename, frames,true))
        {
            qDebug() << "Loaded as Kvaser HEX successfully!";
            return true;
        }
        if (loadKvaserFile(filename, frames,false))
        {
            qDebug() << "Loaded as KVaser Decimal successfully!";
            return true;
        }
    }

    qDebug() << "Attempting generic CSV";
    if (isGenericCSVFile(filename))
    {
        if (loadGenericCSVFile(filename, frames))
        {
            qDebug() << "Loaded as generic CSV successfully!";
            return true;
        }
    }

    qDebug() << "Nothing worked... sorry...";
    return false;
}


bool FrameFileIO::isVehicleSpyFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    bool foundProbableHeader = false;
    bool isMatch = false;
    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }
    try {
        for (int i = 0; i < 10; i++)
        {
            if (!inFile->atEnd())
            {
                line = inFile->readLine().simplified().toUpper();
                if (line.startsWith("LINE") && line.contains("TIME") && line.contains("B1"))
                {
                    foundProbableHeader = true;
                }
            }
        }
        if (foundProbableHeader)
        {
            if (!inFile->atEnd())
            {
                line = inFile->readLine().simplified().toUpper();
                inFile->close();
                QList<QByteArray> tokens = line.split(',');
                if (tokens.length() > 20)
                {
                    if (tokens[9].toInt(nullptr, 16) > 0) isMatch = true;
                }
            }
        }
    }
    catch (...)
    {
        isMatch = false;
    }
    inFile->close();
    return isMatch;
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
    bool foundErrors = false;

    QDateTime now = QDateTime::currentDateTime();
    QDateTime tempTime;

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

    if (inFile->atEnd()) foundErrors = true;

    while (!inFile->atEnd()) {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }
        line = inFile->readLine().simplified().toUpper();

        QList<QByteArray> tokens = line.split(',');
        if (tokens.length() > 20)
        {
            thisFrame.bus = 0;
            thisFrame.remote = false;
            tempTime = now;
            tempTime = tempTime.addMSecs(static_cast<int64_t>(tokens[1].toDouble() * 1000.0));
            thisFrame.timestamp = static_cast<uint64_t>(tempTime.toMSecsSinceEpoch() * 1000);
            if (tokens[5].startsWith("T")) thisFrame.isReceived = false;
                else thisFrame.isReceived = true;
            thisFrame.ID = static_cast<uint32_t>(tokens[9].toInt(nullptr, 16));
            if (tokens[11].startsWith("T")) thisFrame.extended = true;
                else thisFrame.extended = false;

            thisFrame.len = 0;
            for (int i = 0; i < 8; i++)
            {
                if (tokens[12 + i].length() > 0)
                {
                    thisFrame.data[i] = static_cast<unsigned char>(tokens[12 + i].toInt(nullptr, 16));
                    thisFrame.len++;
                }
                else break;
            }
            frames->append(thisFrame);
        }
        else foundErrors = true;
    }
    inFile->close();
    delete inFile;
    return !foundErrors;
}

bool FrameFileIO::saveVehicleSpyFile(QString filename, const QVector<CANFrame> *frames)
{
    Q_UNUSED(filename);
    Q_UNUSED(frames);
    return true;
}

bool FrameFileIO::isCRTDFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    int lineCounter = 0;
    bool isMatch = false;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    try
    {
        line = inFile->readLine().toUpper(); //read out the header first and discard it.

        while (!inFile->atEnd() && lineCounter < 100) {
            lineCounter++;
            line = inFile->readLine().simplified();
            if (line.length() > 2)
            {
                QList<QByteArray> tokens = line.split(' ');
                if (tokens.length() > 2)
                {
                    char firstChar = tokens[1].left(1)[0];
                    if (firstChar >= '1' && firstChar <= '9')
                    {
                        tokens[1].remove(0,1); // Remove leading digit (bus number)
                        firstChar = tokens[1].left(1)[0];
                    }
                    if (firstChar == 'R' || firstChar == 'T')
                    {
                        if (tokens[1] == "R29" || tokens[1] == "T29") isMatch = true;
                        if (tokens[1] == "R11" || tokens[1] == "T11") isMatch = true;
                    }
                }
                else isMatch = false;
            }
            if (lineCounter > 10) break;
        }
    }
    catch (...)
    {
        isMatch = false;
    }
    inFile->close();
    delete inFile;
    return isMatch;
}

//CRTD format from Mark Webb-Johnson / OVMS project
// Specification at: https://docs.openvehicles.com/en/latest/crtd/
/*
Sample data in CRTD format
Timestamp appears to be seconds since the epoch
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
123.431 2R11 27E 00 00 00 00 00 00 00 00
123.441 2R11 7C8 00 00 00 00 00 00 00 00
123.451 2R11 402 FD 04 04 0F 10 00 00 07
123.461 2R11 318 92 08 33 0D 07 00 00 00

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
    bool foundErrors = false;

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
            if (tokens.length() > 2)
            {
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
                thisFrame.timestamp = static_cast<uint64_t>((tokens[0].toDouble() * multiplier));
                thisFrame.bus = 0;
                char firstChar = tokens[1].left(1)[0];
                if (firstChar >= '1' && firstChar <= '9')
                {
                    thisFrame.bus = tokens[1].left(1)[0] - '1';
                    tokens[1].remove(0,1); // Remove leading digit (bus number)
                    firstChar = tokens[1].left(1)[0];
                }
                if (firstChar == 'R' || firstChar == 'T')
                {
                    thisFrame.ID = static_cast<uint32_t>(tokens[2].toInt(nullptr, 16));
                    if (tokens[1] == "R29" || tokens[1] == "T29") thisFrame.extended = true;
                        else thisFrame.extended = false;
                    if (firstChar == 'T') thisFrame.isReceived = false;
                        else thisFrame.isReceived = true;
                    thisFrame.len = tokens.length() - 3;
                    thisFrame.remote = false;
                    for (int d = 0; d < thisFrame.len; d++)
                    {
                        if (tokens[d + 3] != "")
                        {
                            thisFrame.data[d] = static_cast<unsigned char>(tokens[d + 3].toInt(nullptr, 16));
                        }
                        else thisFrame.data[d] = 0;
                    }
                    frames->append(thisFrame);
                }
            }
            else foundErrors = true;
        }
    }
    inFile->close();
    delete inFile;
    return !foundErrors;
}


bool FrameFileIO::isCANHackerFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    int lineCounter = 0;
    bool isMatch = false;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }
    try
    {
        line = inFile->readLine().toUpper(); //read out the header first and discard it.
        if (!line.contains("CANHACKER")) return false;

        while (!inFile->atEnd()) {
            lineCounter++;
            line = inFile->readLine().simplified();
            if (line.length() > 2)
            {
                QList<QByteArray> tokens = line.split(' ');
                if (tokens.length() > 3)
                {
                    if (tokens[1].toInt(nullptr, 16) > 0)
                    {
                        int len = tokens[2].toInt();
                        if (len > -1 && len < 9)
                        {
                            isMatch = true;
                        }
                    }
                }
                else isMatch = false;
            }
            if (lineCounter > 10) break;
        }
    }
    catch (...)
    {
        isMatch = false;
    }
    inFile->close();
    delete inFile;
    return isMatch;
}

// CANHacker trace format
// Time   ID     DLC Data                    Comment
// 00.000 00004000 8 36 47 19 43 01 00 00 80 
bool FrameFileIO::loadCANHackerFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    int lineCounter = 0;
    bool foundErrors = false;

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
            if (tokens.length() > 3)
            {
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
                thisFrame.timestamp = static_cast<uint64_t>(tokens[0].toDouble() * multiplier);
                thisFrame.ID = static_cast<uint32_t>(tokens[1].toInt(nullptr, 16));
                thisFrame.extended = (thisFrame.ID > 0x7FF);
                thisFrame.isReceived = true;
                thisFrame.remote = false;
                thisFrame.bus = 0;
                thisFrame.len = tokens[2].toInt(nullptr, 16);
                for (int d = 0; d < thisFrame.len; d++)
                {
                    if (tokens[d + 3] != "")
                    {
                        thisFrame.data[d] = static_cast<unsigned char>(tokens[d + 3].toInt(nullptr, 16));
                    }
                    else thisFrame.data[d] = 0;
                }
                frames->append(thisFrame);
            }
            else foundErrors = true;
        }
    }
    inFile->close();
    delete inFile;
    return !foundErrors;
}

bool FrameFileIO::isCANOpenFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    int lineCounter = 0;
    bool isMatch = false;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    try
    {
        line = inFile->readLine().toUpper();
        if (!line.contains("CANOPEN MAGIC")) return false;
        line = inFile->readLine();
        line = inFile->readLine();
        line = inFile->readLine();
        line = inFile->readLine();

        while (!inFile->atEnd()) {
            lineCounter++;
            if (lineCounter > 10)
            {
                break;
            }

            line = inFile->readLine().replace('\"', ' ').simplified();
            if (line.length() > 2)
            {
                QList<QByteArray> tokens = line.split(',');
                if (tokens.length() > 11)
                {
                    if (Utility::ParseStringToNum(tokens[5].simplified()) > 0)
                    {
                        QList<QByteArray> dataTok = tokens[11].simplified().split(' ');
                        if ( dataTok.length() > -1 && dataTok.length() < 9) isMatch = true;
                    }
                }
                else isMatch = false;
            }
        }
    }
    catch (...)
    {
        isMatch = false;
    }
    inFile->close();
    delete inFile;
    return isMatch;
}

//"Message Number","Time (ms)","Time","Excel Time","Count","ID","Flags","Message Type","Node","Details","Process Data","Data (Hex)","Data (Text)","Data (Decimal)","Length","Raw Message"
//"0","0.000","8:09:42:48.7953090'",43447.7100146116,"","0x2E1","","Default: PDO","","Default: TPDO 2 of Node 0x61 (97)","","10 21 04 00 00 00 00 00 ",". ! . . . . . . ","U:0 S:0","8","10 21 04 00 00 00 00 00"
bool FrameFileIO::loadCANOpenFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    int lineCounter = 0;
    bool foundErrors = false;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        qDebug() << "Could not open the file!";
        return false;
    }

    line = inFile->readLine(); //read out the header first and discard it.
    line = inFile->readLine();
    line = inFile->readLine();
    line = inFile->readLine();
    line = inFile->readLine();

    while (!inFile->atEnd()) {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        line = inFile->readLine().replace('\"', ' ').simplified();
        if (line.length() > 2)
        {
            QList<QByteArray> tokens = line.split(',');
            if (tokens.length() > 11)
            {
                thisFrame.timestamp = static_cast<uint64_t>(tokens[1].simplified().toDouble() * 1000.0);
                thisFrame.ID = static_cast<uint32_t>(Utility::ParseStringToNum(tokens[5].simplified()));
                thisFrame.extended = (thisFrame.ID > 0x7FF);
                thisFrame.isReceived = true;
                thisFrame.remote = false;
                thisFrame.bus = 0;
                QList<QByteArray> dataTok = tokens[11].simplified().split(' ');
                thisFrame.len = dataTok.length();
                for (int d = 0; d < thisFrame.len; d++)
                {
                    if (dataTok[d] != "")
                    {
                        thisFrame.data[d] = static_cast<unsigned char>(dataTok[d].simplified().toInt(nullptr, 16));
                    }
                    else thisFrame.data[d] = 0;
                }
                frames->append(thisFrame);
            }
            else foundErrors = true;
        }
    }
    inFile->close();
    delete inFile;
    return !foundErrors;
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

        outFile->write(QString::number(frames->at(c).bus + 1).toUtf8());
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


bool FrameFileIO::isPCANFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    int lineCounter = 0;
    int fileVersion = 1;
    bool isMatch = false;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    try
    {
        while (!inFile->atEnd()) {
            lineCounter++;
            if (lineCounter > 10)
            {
                break;
            }
            line = inFile->readLine();
            if (line.startsWith(";$FILEVERSION=2.0")) fileVersion = 2;
            if (line.startsWith(';')) continue;
            if (line.length() > 41)
            {
                if (fileVersion == 1)
                {
                    uint32_t id = line.mid(28, 8).simplified().toUInt(nullptr, 16);
                    if (id > 0 && id < 0x1FFFFFFF)
                    {
                        int len = line.mid(38,1).toInt();
                        if (len > - 1 && len < 9) isMatch = true;
                    }
                    else isMatch = false;
                }

                else if (fileVersion == 2)
                {
                    uint32_t id = line.mid(25, 8).simplified().toUInt(nullptr, 16);
                    if (id > 0 && id < 0x1FFFFFFF)
                    {
                        int len = line.mid(37,2).trimmed().toInt();
                        if (len > 0 && len < 9)
                        {
                            QList<QByteArray> tokens = line.mid(40, len * 3).split(' ');
                            if (tokens.length() == len) isMatch = true;
                        }
                        else isMatch = false;
                    }
                }
            }
            //else foundErrors = true;
        }
    }
    catch (...)
    {
        isMatch = false;
    }
    inFile->close();
    delete inFile;
    return isMatch;
}

/*Fixed length lines
    Version 1.1
//;   Message Number
//;   |         Time Offset (ms)
//;   |         |        Type
//;   |         |        |        ID (hex)
//;   |         |        |        |     Data Length Code
//;   |         |        |        |     |   Data Bytes (hex) ...
//;   |         |        |        |     |   |
//;---+--   ----+----  --+--  ----+---  +  -+ -- -- -- -- -- -- --
// 0-6       10-18     21-25   28-35    38  41-? two chars each + space

    Version 2.0
;   Message   Time    Type ID     Rx/Tx
;   Number    Offset  |    [hex]  |  Data Length
;   |         [ms]    |    |      |  |  Data [hex] ...
;   |         |       |    |      |  |  |
;---+-- ------+------ +- --+----- +- +- +- +- -- -- -- -- -- -- --
  0-6    8-20     22-23  25-32 34-35 37-38 40-? two chars each + space

  Both versions are supported now but rather lazily. Very many aspects of the file are ignored.
  It seems as if Version 2 files might be able to store other protocols like ISO-TP or J1939
*/

bool FrameFileIO::loadPCANFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    int lineCounter = 0;
    int fileVersion = 1;
    bool foundErrors = false;

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
        if (line.startsWith(";$FILEVERSION=2.0")) fileVersion = 2;
        if (line.startsWith(';')) continue;
        if (line.length() > 41)
        {
            if (fileVersion == 1)
            {
                thisFrame.timestamp = (uint64_t)(line.mid(10, 8).simplified().toDouble() * 1000.0);
                thisFrame.ID = line.mid(28, 8).simplified().toUInt(nullptr, 16);
                if (thisFrame.ID < 0x1FFFFFFF)
                {
                    thisFrame.len = line.mid(38,1).toInt();
                    thisFrame.isReceived = true;
                    thisFrame.bus = 0;
                    if (line.at(28) == ' ') {
                        thisFrame.extended = false;
                    } else {
                        thisFrame.extended = true;
                    }

                    if (line.at(41) == 'R') {
                        thisFrame.remote = true;
                    } else {
                        QList<QByteArray> tokens = line.mid(41, thisFrame.len * 3).split(' ');
                        thisFrame.remote = false;
                        for (int d = 0; d < thisFrame.len; d++)
                        {
                            if (tokens[d] != "")
                            {
                                thisFrame.data[d] = static_cast<unsigned char>(tokens[d].toInt(nullptr, 16));
                            }
                            else thisFrame.data[d] = 0;
                        }
                    }
                    frames->append(thisFrame);
                }
            }
            else if (fileVersion == 2)
            {
                thisFrame.timestamp = static_cast<uint64_t>(line.mid(8, 13).simplified().toDouble() * 1000.0);
                thisFrame.ID = line.mid(25, 8).simplified().toUInt(nullptr, 16);
                if (thisFrame.ID < 0x1FFFFFFF)
                {
                    thisFrame.len = line.mid(37,2).trimmed().toInt();
                    qDebug() << thisFrame.len;
                    thisFrame.isReceived = true;
                    thisFrame.bus = 0;
                    if (line.at(25) == ' ') {
                        thisFrame.extended = false;
                    } else {
                        thisFrame.extended = true;
                    }
                    if (line.at(40) == 'R') {
                        thisFrame.remote = true;
                    } else {
                        QList<QByteArray> tokens = line.mid(40, thisFrame.len * 3).split(' ');
                        thisFrame.remote = false;
                        for (int d = 0; d < thisFrame.len; d++)
                        {
                            if (tokens[d] != "")
                            {
                                thisFrame.data[d] = static_cast<unsigned char>(tokens[d].toInt(nullptr, 16));
                            }
                            else thisFrame.data[d] = 0;
                        }
                    }
                    frames->append(thisFrame);
                }
            }
        }
        //else foundErrors = true;
    }
    inFile->close();
    delete inFile;
    return !foundErrors;
}

bool FrameFileIO::isCanalyzerASC(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    int lineCounter = 0;
    bool isMatch = false;
    bool inHeader = true;
    QList<QByteArray> tokens;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }
    try
    {
        while (!inFile->atEnd() && lineCounter < 10) {
            lineCounter++;
            if (lineCounter > 10)
            {
                qApp->processEvents();
                lineCounter = 0;
            }
            line = inFile->readLine();
            if (inHeader)
            {
                if (line.startsWith("//") ||  lineCounter > 4)
                {
                    inHeader = false;
                    continue;
                }
            }
            if (inHeader) continue;
            if (line.length() > 2)
            {
                tokens = line.simplified().split(' ');
                if (tokens.length() > 5)
                {
                    uint64_t time = static_cast<uint64_t>(tokens[0].toDouble() * 1000000.0);
                    if (time > 0) isMatch = true;
                    uint32_t id = static_cast<uint32_t>(tokens[2].toUInt(nullptr, 16));
                    if (id > 0 && id < 0x20000000) isMatch = true;
                    int len = tokens[5].toInt();
                    if (len > 0 && len < 9) isMatch = true;
                    if (tokens.count() >= (len + 6)) isMatch = true;
                }
            }
        }
    }
    catch (...)
    {
        isMatch = false;
    }
    inFile->close();
    delete inFile;
    return isMatch;
}

//There tends to be four lines of header first. The last of which starts with // so first burn off lines
//until a line starting with // is seen, then the rest are formatted like this:
//  47.971842 2  248             Rx   d 8 FF FF FF FF FF FF FF FF
//That can be simplified to look like this:
//47.971842 2 248 Rx d 8 FF FF FF FF FF FF FF FF
//Which is then easy to parse by splitting on the spaces
//Time   bus id dir ? len databytes
bool FrameFileIO::loadCanalyzerASC(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    int lineCounter = 0;
    bool foundErrors = false;
    bool inHeader = true;
    thisFrame.remote = false;
    QList<QByteArray> tokens;

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
        if (inHeader)
        {
            if (line.startsWith("//") ||  lineCounter > 4)
            {
                inHeader = false;
                continue;
            }
        }
        if (inHeader) continue;
        if (line.length() > 2)
        {            
            tokens = line.simplified().split(' ');
            if (tokens.length() > 5)
            {
                thisFrame.timestamp = static_cast<uint64_t>(tokens[0].toDouble() * 1000000.0);
                thisFrame.ID = tokens[2].toUInt(nullptr, 16);
                thisFrame.len = tokens[5].toInt();
                if (thisFrame.len > 8) return false;
                if (thisFrame.len < 0) return false;
                thisFrame.isReceived = tokens[3].toUpper().contains("RX");
                thisFrame.bus = tokens[1].toInt();
                thisFrame.extended = (thisFrame.ID > 0x7FF);
                thisFrame.remote = false;
                for (int d = 6; d < (6 + static_cast<int>(thisFrame.len)); d++)
                {
                    if (tokens.count() > d)
                    {
                        thisFrame.data[d - 6] = static_cast<unsigned char>(tokens[d].toInt(nullptr, 16));
                    }
                    else //expected byte wasn't there to read. Set it zero and set error flag
                    {
                    thisFrame.data[d - 6] = 0;
                    foundErrors = true;
                    }
                }
            }
            frames->append(thisFrame);
        }
        //else foundErrors = true;
    }
    inFile->close();
    delete inFile;
    return !foundErrors;
}

bool FrameFileIO::saveCanalyzerASC(QString filename, const QVector<CANFrame>* frames)
{
    QFile *outFile = new QFile(filename);
    int lineCounter = 0;
    uint64_t offsetTime = frames->at(0).timestamp;
    for (int c = 0; c < frames->count(); c++)
    {
        if (frames->at(c).timestamp < offsetTime) offsetTime = frames->at(c).timestamp;
    }

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete outFile;
        return false;
    }

    QDateTime now;
    now = QDateTime::currentDateTime();
    if (offsetTime > 10000000000) //chances are the input file had times as system time so load it
    {
        now.setMSecsSinceEpoch(offsetTime / 1000); //offsetTime was in microseconds
    }
    outFile->write("date " + now.toString("ddd MMM dd h:mm:ss.zzz a yyyy").toUtf8());

    outFile->write("\nbase hex  timestamps absolute\n");
    outFile->write("no internal event logging\n");
    outFile->write("// version 11.0.0\n");

    for (int c = 0; c < frames->count(); c++)
    {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        uint64_t timeStamp = (frames->at(c).timestamp - offsetTime) / 1000000ull;
        int tsLen = QString::number(timeStamp).length();
        int precision = 6;
        //vector seems to keep 10 bytes at the start of the line for the timestamp. It should never exceed this
        //and there should never be a precision over 6 digits after the decimal
        if (tsLen > 3) precision = 9 - tsLen;
        outFile->write(QString::number((frames->at(c).timestamp - offsetTime) / 1000000.0, 'f', precision).rightJustified(10, ' ').toUtf8());
        outFile->putChar(' ');
        outFile->write(QString::number(frames->at(c).bus + 1).toUtf8());
        outFile->write("  ");
        if (frames->at(c).extended)
            outFile->write(QString::number(frames->at(c).ID, 16).toUpper().rightJustified(8, '0').toUtf8());
        else
        {
            outFile->write(QString::number(frames->at(c).ID, 16).toUpper().rightJustified(3, '0').toUtf8());
            outFile->write("     ");
        }
        outFile->write("   ");

        if (frames->at(c).isReceived) outFile->write("Rx D ");
        else outFile->write("Tx D ");

        outFile->write(QString::number(frames->at(c).len).toUtf8());
        outFile->write("  ");

        for (int temp = 0; temp < frames->at(c).len; temp++)
        {
            outFile->write(QString::number(frames->at(c).data[temp], 16).toUpper().rightJustified(2, '0').toUtf8());
            outFile->write("  ");
        }

        outFile->write("\n");
    }
    outFile->close();
    delete outFile;

    return true;
}

bool FrameFileIO::isCanalyzerBLF(QString filename)
{
    BLF_FILE_HEADER header;

    QFile *inFile = new QFile(filename);
    bool isMatch = false;

    if (!inFile->open(QIODevice::ReadOnly))
    {
        delete inFile;
        return false;
    }
    inFile->read(reinterpret_cast<char *>(&header), sizeof(header));
    if (qFromLittleEndian(header.sig) == 0x47474F4C)
    {
        qDebug() << "Proper BLF file header token";
        isMatch = true;
    }
    else isMatch = false;

    inFile->close();
    delete inFile;

    return isMatch;
}

//this one is pretty complicated and handled by it's own class
bool FrameFileIO::loadCanalyzerBLF(QString filename, QVector<CANFrame> *frames)
{
    BLFHandler blf;
    return blf.loadBLF(filename, frames);
}

bool FrameFileIO::isNativeCSVFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    int fileVersion = 1;
    bool isMatch = true;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }
    try
    {
        line = inFile->readLine().toUpper(); //read out the header first and discard it.
        if (line.length() < 23) isMatch = false;
        else if (line.at(23) == 'D') fileVersion = 2; //Dir is found starting at position 23 if this is a V2 file

        if (!line.contains("TIME STAMP")) isMatch = false;
        if (!line.contains("EXTENDED")) isMatch = false;
        if (!line.contains("D1")) isMatch = false;

        if (!inFile->atEnd()) {
            line = inFile->readLine().simplified();
            if (line.length() > 2)
            {
                QList<QByteArray> tokens = line.split(',');
                if (tokens.length() >= 5)
                {
                    if (fileVersion == 1)
                    {
                        if (tokens[4].toUInt() > 8) isMatch = false;
                    }
                    else if (fileVersion == 2)
                    {
                        if ( tokens[5].toUInt() > 8) isMatch = false;
                    }
                }
                else isMatch = false;
            }
        }
    }
    catch (...)
    {
        isMatch = false;
    }
    inFile->close();
    delete inFile;

    return isMatch;
}

//The "native" file format for this program
//Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8
//39747828,000005EB,false,Rx,0,8,E8,45,85,4B,4A,28,36,69,
bool FrameFileIO::loadNativeCSVFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    int fileVersion = 1;
    uint64_t timeStamp = Utility::GetTimeMS();
    int lineCounter = 0;
    bool foundErrors = false;
    thisFrame.remote = false;

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

        line = inFile->readLine().simplified();
        if (line.length() > 2)
        {
            QList<QByteArray> tokens = line.split(',');
            if (tokens.length() >= 5)
            {
                if (tokens[0].length() > 3)
                {
                    thisFrame.timestamp = tokens[0].toULongLong();
                }
                else
                {
                    timeStamp += 5;
                    thisFrame.timestamp = timeStamp;
                }

                thisFrame.ID = tokens[1].toUInt(nullptr, 16);
                if (tokens[2].toUpper().contains("TRUE")) thisFrame.extended = 1;
                    else thisFrame.extended = 0;

                thisFrame.remote = false;

                if (fileVersion == 1)
                {
                    thisFrame.isReceived = true;
                    thisFrame.bus = tokens[3].toInt();
                    thisFrame.len = tokens[4].toInt();
                    if (thisFrame.len > 8) thisFrame.len = 8;
                    if (thisFrame.len < 0) thisFrame.len = 0;
                    for (int c = 0; c < 8; c++) thisFrame.data[c] = 0;
                    if (thisFrame.len + 5 > tokens.length()) thisFrame.len = tokens.length() - 5;
                    for (int d = 0; d < thisFrame.len; d++)
                        thisFrame.data[d] = static_cast<unsigned char>(tokens[5 + d].toInt(nullptr, 16));
                }
                else if (fileVersion == 2)
                {
                    if (tokens[3].at(0) == 'R') thisFrame.isReceived = true;
                    else thisFrame.isReceived = false;
                    thisFrame.bus = tokens[4].toInt();
                    thisFrame.len = tokens[5].toInt();
                    if (thisFrame.len > 8) thisFrame.len = 8;
                    if (thisFrame.len < 0) thisFrame.len = 0;
                    if (thisFrame.len + 6 > tokens.length()) thisFrame.len = tokens.length() - 6;
                    for (int c = 0; c < 8; c++) thisFrame.data[c] = 0;
                    for (int d = 0; d < thisFrame.len; d++)
                        thisFrame.data[d] = static_cast<unsigned char>(tokens[6 + d].toInt(nullptr, 16));
                }

                frames->append(thisFrame);
            }
            else foundErrors = true;
        }
    }
    inFile->close();
    delete inFile;
    return !foundErrors;
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

bool FrameFileIO::openContinuousNative()
{
    QString filename;
    QFileDialog dialog(qApp->activeWindow());
    QSettings settings;

    QStringList filters;
    filters.append(QString(tr("GVRET Logs (*.csv *.CSV)")));

    dialog.setDirectory(settings.value("FileIO/LoadSaveDirectory", dialog.directory().path()).toString());
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        continuousFile.setFileName(filename);

        if (!continuousFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            return false;
        }
        continuousFile.write("Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8");
        continuousFile.write("\n");
        settings.setValue("FileIO/LoadSaveDirectory", dialog.directory().path());
        return true;
    }
    return false;
}

bool FrameFileIO::closeContinuousNative()
{
    if (continuousFile.isOpen())
    {
        continuousFile.close();
        return true;
    }
    return false;
}

bool FrameFileIO::writeContinuousNative(const QVector<CANFrame>* frames, int beginningFrame)
{
    if (!continuousFile.isOpen()) return false;
    qDebug() << "Bgn: " << beginningFrame << "  Count: " << frames->count();
    for (int c = beginningFrame; c < frames->count(); c++)
    {
        continuousFile.write(QString::number(frames->at(c).timestamp).toUtf8());
        continuousFile.putChar(44);

        continuousFile.write(QString::number(frames->at(c).ID, 16).toUpper().rightJustified(8, '0').toUtf8());
        continuousFile.putChar(44);

        if (frames->at(c).extended) continuousFile.write("true,");
        else continuousFile.write("false,");

        if (frames->at(c).isReceived) continuousFile.write("Rx,");
        else continuousFile.write("Tx,");

        continuousFile.write(QString::number(frames->at(c).bus).toUtf8());
        continuousFile.putChar(44);

        continuousFile.write(QString::number(frames->at(c).len).toUtf8());
        continuousFile.putChar(44);

        for (int temp = 0; temp < 8; temp++)
        {
            if (temp < frames->at(c).len)
                continuousFile.write(QString::number(frames->at(c).data[temp], 16).toUpper().rightJustified(2, '0').toUtf8());
            else
                continuousFile.write("00");
            continuousFile.putChar(44);
        }

        continuousFile.write("\n");
    }
    return true;
}

bool FrameFileIO::flushContinuousNative()
{
    if (continuousFile.isOpen())
    {
        return continuousFile.flush();
    }
    return false;
}


bool FrameFileIO::isGenericCSVFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    bool isMatch = true;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }
    try
    {
        line = inFile->readLine(); //read out the header first and discard it.

        if (!inFile->atEnd()) {
            line = inFile->readLine();
            if (line.length() > 2)
            {
                QList<QByteArray> tokens = line.split(',');

                int ID = tokens[0].toInt(nullptr, 16);
                if (ID < 1 || ID > 0x1FFFFFFF) isMatch = false;

                if (tokens.count() < 2)
                {
                    isMatch = false;
                }

                if (isMatch)
                {
                    QList<QByteArray> dataTok = tokens[1].split(' ');
                    int len = dataTok.length();
                    if (len > 8) isMatch = false;
                    if (len < 2) isMatch = false;
                }
            }
            else isMatch = false;
        }
    }
    catch (...)
    {
        isMatch = false;
    }
    inFile->close();
    delete inFile;
    return isMatch;
}

bool FrameFileIO::loadGenericCSVFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    uint64_t timeStamp = Utility::GetTimeMS();
    int lineCounter = 0;
    bool foundErrors = false;
    thisFrame.remote = false;

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

            timeStamp += 5000;
            thisFrame.timestamp = timeStamp;
            thisFrame.ID = tokens[0].toUInt(nullptr, 16);
            if (thisFrame.ID > 0x7FF) thisFrame.extended = true;
            else thisFrame.extended  = false;
            thisFrame.bus = 0;
            thisFrame.remote = false;
            QList<QByteArray> dataTok = tokens[1].split(' ');
            thisFrame.len = dataTok.length();
            if (thisFrame.len > 8) thisFrame.len = 8;
            for (int d = 0; d < thisFrame.len; d++) thisFrame.data[d] = static_cast<unsigned char>(dataTok[d].toInt(nullptr, 16));

            frames->append(thisFrame);
        }
        else foundErrors = true;
    }
    inFile->close();
    delete inFile;
    return !foundErrors;
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

bool FrameFileIO::isLogFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    bool isMatch = true;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    try
    {
        line = inFile->readLine().toUpper();
        if (!line.contains("BUSMASTER")) isMatch = false;

        while (!inFile->atEnd() && line.startsWith("***")) {
            line = inFile->readLine().toUpper();
        }

        if (!inFile->atEnd())
        {
            line = inFile->readLine().toUpper();
            if (line.length() > 1)
            {
                QList<QByteArray> tokens = line.split(' ');
                if (tokens.length() >= 6)
                {
                    QList<QByteArray> timeToks = tokens[0].split(':');
                    if (timeToks.count() != 4) isMatch = false;

                    int ID = tokens[3].right(tokens[3].length() - 2).toInt(nullptr, 16);
                    if (ID < 1 || ID > 0x1FFFFFFF) isMatch = false;
                    if (tokens[4] != "S" && tokens[4] != "X" && tokens[4] != "SR" && tokens[4] != "XR") isMatch = false;
                    int len = tokens[5].toInt();
                    if (len > 8) isMatch = false;
                }
                else isMatch = false;
            }
        }
    }
    catch (...)
    {
        isMatch = false;
    }

    inFile->close();
    delete inFile;
    return isMatch;
}

//busmaster log file
/*
tokens:
0 = timestamp
1 = Transmission direction
2 = Channel
3 = ID
4 = Type (s = standard, x = extended, sr = standard remote, xr = extended remote)
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
    bool foundErrors = false;

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
            if (tokens.length() >= 6)
            {
                QList<QByteArray> timeToks = tokens[0].split(':');
                timeStamp = (timeToks[0].toUInt() * (1000ul * 1000ul * 60ul * 60ul)) + (timeToks[1].toUInt() * (1000ul * 1000ul * 60ul))
                      + (timeToks[2].toUInt() * (1000ul * 1000ul)) + (timeToks[3].toUInt() * 100ul);
                thisFrame.timestamp = timeStamp;
                if (tokens[1].at(0) == 'R') thisFrame.isReceived = true;
                    else thisFrame.isReceived = false;
                thisFrame.ID = tokens[3].right(tokens[3].length() - 2).toUInt(nullptr, 16);
                if (tokens[4] == "S") {
                    thisFrame.extended = false;
                    thisFrame.remote = false;
                } else if (tokens[4] == "X") {
                    thisFrame.extended = true;
                    thisFrame.remote = false;
                } else if (tokens[4] == "SR") {
                    thisFrame.extended = false;
                    thisFrame.remote = true;
                } else { // XR
                    thisFrame.extended = true;
                    thisFrame.remote = true;
                }
                thisFrame.bus = tokens[2].toInt() - 1;
                thisFrame.len = tokens[5].toInt();
                if (thisFrame.len > 8) thisFrame.len = 8;
                if (thisFrame.len < 0) thisFrame.len = 0;
                if (!thisFrame.remote) {
                    for (int d = 0; d < thisFrame.len; d++)
                        thisFrame.data[d] = static_cast<unsigned char>(tokens[d + 6].toInt(nullptr, 16));
                }
                frames->append(thisFrame);
            }
            else foundErrors = true;
        }
    }
    inFile->close();
    delete inFile;
    return !foundErrors;
}

bool FrameFileIO::saveLogFile(QString filename, const QVector<CANFrame>* frames)
{
    QFile *outFile = new QFile(filename);
    QDateTime timestamp, tempStamp;
    int lineCounter = 0;

    //timestamp = QDateTime::currentDateTime();

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete outFile;
        return false;
    }

    outFile->write("***BUSMASTER Ver 3.2.0***\n");
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
    outFile->write("***START DATABASE FILES***\n");
    outFile->write("***END OF DATABASE FILES***\n");
    outFile->write("***<Time><Tx/Rx><Channel><CAN ID><Type><DLC><DataBytes>***\n");

    for (int c = 0; c < frames->count(); c++)
    {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        tempStamp = QDateTime::fromMSecsSinceEpoch(frames->at(c).timestamp / 1000);
        outFile->write(tempStamp.toString("hh:mm:ss:zzz").toUtf8());
        if (frames->at(c).isReceived) outFile->write(" Rx ");
        else outFile->write(" Tx ");
        // busmaster channel start at 1
        outFile->write(QString::number(frames->at(c).bus+1).toUtf8() + " ");
        outFile->write("0x");
        if (frames->at(c).extended && frames->at(c).ID > 0x7FF) {
            outFile->write(QString::number(frames->at(c).ID, 16).toUpper().rightJustified(8, '0').toUtf8());
        } else {
            outFile->write(QString::number(frames->at(c).ID, 16).toUpper().rightJustified(3, '0').toUtf8());
        }
        if (frames->at(c).extended) outFile->write(" x");
            else outFile->write(" s");
        if (frames->at(c).remote) outFile->write("r ");
            else outFile->write(" ");
        outFile->write(QString::number(frames->at(c).len).toUtf8() + " ");

        if (!frames->at(c).remote) {
            for (int temp = 0; temp < frames->at(c).len; temp++)
            {
                outFile->write(QString::number(frames->at(c).data[temp], 16).toUpper().rightJustified(2, '0').toUtf8());
                outFile->putChar(' ');
            }
        }

        outFile->write("\n");

    }
    outFile->close();
    delete outFile;
    return true;
}

bool FrameFileIO::isIXXATFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    bool isMatch = true;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    try
    {
        line = inFile->readLine().toUpper();
        if (!line.contains("IXXAT")) isMatch = false;

        for (int i = 0; i < 6; i++)
        {
            if (!inFile->atEnd()) line = inFile->readLine().toUpper();
            else isMatch = false;
        }

        if (!line.contains("FORMAT")) isMatch = false;
    }
    catch (...)
    {
        isMatch = false;
    }

    inFile->close();
    delete inFile;
    return isMatch;
}

//"00:01:03.03","223","Std","","00 00 00 00 49 00 00 01 "
bool FrameFileIO::loadIXXATFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    uint64_t timeStamp = Utility::GetTimeMS();
    int lineCounter = 0;
    bool foundErrors = false;
    thisFrame.remote = false;

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
            if (line.length() >= 5)
            {
                QString timePortion = Utility::unQuote(tokens[0]);
                QStringList timeToks = timePortion.split(':');
                if (timeToks.length() >= 3)
                {
                    timeStamp = (timeToks[0].toUInt() * (1000ul * 1000ul * 60ul * 60ul)) + (timeToks[1].toUInt() * (1000ul * 1000ul * 60ul))
                      + static_cast<uint64_t>(timeToks[2].toDouble() * (1000.0 * 1000.0));
                }
                else
                {
                    timeStamp = 0;
                    foundErrors = true;
                    return false;
                }
                thisFrame.timestamp = timeStamp;
                thisFrame.ID = Utility::unQuote(tokens[1]).toUInt(nullptr, 16);
                QString tempStr = Utility::unQuote(tokens[2]).toUpper();
                if (tempStr.length() > 0)
                {
                    if (tempStr.at(0) == 'S') thisFrame.extended = false;
                        else thisFrame.extended = true;
                }
                else
                {
                    thisFrame.extended = false;
                    foundErrors = true;
                    return false;
                }

                thisFrame.isReceived = true;
                thisFrame.bus = 0;
                thisFrame.remote = false;

                QStringList dataToks = Utility::unQuote(tokens[4]).simplified().split(' ');
                thisFrame.len = dataToks.length();
                if (thisFrame.len > 8) return false;
                for (int d = 0; d < thisFrame.len; d++) thisFrame.data[d] = static_cast<unsigned char>(dataToks[d].toInt(nullptr, 16));
                frames->append(thisFrame);
            }
            else return false;
        }
    }
    inFile->close();
    delete inFile;
    return !foundErrors;
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
    timestamp = timestamp.addMSecs((frames->last().timestamp - frames->first().timestamp) / 1000);
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
        tempStamp = QDateTime::fromMSecsSinceEpoch(frames->at(c).timestamp / 1000);
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

bool FrameFileIO::isCANDOFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    int lineCounter = 0;
    QByteArray data;
    bool isMatch = true;

    if (!inFile->open(QIODevice::ReadOnly))
    {
        delete inFile;
        return false;
    }

    //this file format is in static 12 byte blocks.
    //Bytes 0 - 1 are a time stamp
    //Bytes 2 - 3 are the data length (top 4 bits) then ID (bottom 11 bits)
    //Bytes 4 - 11 are the data bytes (padded with FF for bytes not used)
    try
    {
        while (!inFile->atEnd() && lineCounter < 200)
        {
            lineCounter++;

            data = inFile->read(12);

            int ID = ((data[3] & 0x0F) * 256 + data[2]);
            int len = data[3] >> 4;

            if (len <= 8 && ID <= 0x7FF)
            {
                if (len < 8)
                {
                    if (data[4 + len] != static_cast<char>(0xFF)) isMatch = false;
                }
            }
            else isMatch = false;
        }
    }
    catch (...)
    {
        isMatch = false;
    }

    inFile->close();
    delete inFile;
    return isMatch;
}

bool FrameFileIO::loadCANDOFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    int lineCounter = 0;
    QByteArray data;
    int timeOffset = 0;
    uint64_t lastTimeStamp = 0;
    bool foundErrors = false;
    thisFrame.remote = false;

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
        thisFrame.remote = false;

        thisFrame.timestamp = 1000000ul * ((unsigned char)data[0] >> 2);
        thisFrame.timestamp += (((data[0] & 3) << 8) + (unsigned char)data[1]) * 1000;
        thisFrame.timestamp += timeOffset;
        if (thisFrame.timestamp < lastTimeStamp)
        {
            timeOffset += 60000000ul;
        }
        lastTimeStamp = thisFrame.timestamp;
        thisFrame.ID = (((unsigned char)data[3] & 0x0F) * 256 + (unsigned char)data[2]) & 0x7FF;
        thisFrame.len = (unsigned char)data[3] >> 4;

        if (thisFrame.len <= 8 && thisFrame.ID <= 0x7FF)
        {
            for (unsigned int d = 0; d < thisFrame.len; d++) thisFrame.data[d] = (unsigned char)data[4 + d];
            frames->append(thisFrame);
        }
        else foundErrors = true;
    }

    inFile->close();
    delete inFile;
    return !foundErrors;
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
    data[2] = (char)0xFF;
    data[3] = (char)0xFF;
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
        for (int j = 0; j < 8; j++) data[4 + j] = (char)0xFF;

        thisFrame = frames->at(c);
        if (!thisFrame.extended)
        {
            ms = (thisFrame.timestamp / 1000);
            id = thisFrame.ID & 0x7FF;
            data[0] = (((ms / 1000) % 60) << 2) + ((ms % 1000) >> 8);
            data[1] = (char)(ms & 0xFF);
            data[2] = (char)(id & 0xFF);
            data[3] = (char)((id >> 8) + (thisFrame.len << 4));
            for (unsigned int d = 0; d < thisFrame.len; d++) data[4 + d] = (char)thisFrame.data[d];
            outFile->write(data);
        }
    }
    outFile->close();
    delete outFile;
    return true;
}

bool FrameFileIO::isMicrochipFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    bool inComment = false;
    int lineCounter = 0;
    bool isMatch = true;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }
    try
    {
        while (!inFile->atEnd() && lineCounter < 100) {
            lineCounter++;

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
                        if (tokens.length() >= 4)
                        {
                            int ID = Utility::ParseStringToNum(tokens[2]);
                            if (ID < 1 || ID > 0x1FFFFFFF) isMatch = false;
                            int len = tokens[3].toInt();
                            if ( len > 8 || len < 0 ) isMatch = false;
                            if ( (len + 4)  < tokens.length() ) isMatch = false;
                        }
                        else isMatch = false;
                    }
                }
            }
        }
    }
    catch (...)
    {
        isMatch = false;
    }
    inFile->close();
    delete inFile;
    return isMatch;
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
    bool foundErrors = false;
    thisFrame.remote = false;

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
                    if (tokens.length() >= 4)
                    {
                        timeStamp = tokens[0].toInt() * 1000;
                        thisFrame.timestamp = timeStamp;
                        if (tokens[1].at(0) == 'R') thisFrame.isReceived = true;
                            else thisFrame.isReceived = false;
                        thisFrame.remote = false;
                        thisFrame.ID = Utility::ParseStringToNum(tokens[2]);
                        if (thisFrame.ID <= 0x7FF) thisFrame.extended = false;
                            else thisFrame.extended = true;
                        thisFrame.bus = 0;
                        thisFrame.len = tokens[3].toUInt();
                        if (thisFrame.len > 8) thisFrame.len = 8;
                        if (thisFrame.len + 4 > (unsigned int) tokens.length()) thisFrame.len = tokens.length() - 4;
                        for (unsigned int d = 0; d < thisFrame.len; d++) thisFrame.data[d] = (unsigned char)Utility::ParseStringToNum(tokens[4 + d]);
                        frames->append(thisFrame);
                    }
                    else foundErrors = true;
                }
            }
        }
    }
    inFile->close();
    delete inFile;
    return !foundErrors;
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

        for (unsigned int temp = 0; temp < frames->at(c).len; temp++)
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

bool FrameFileIO::isTraceFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    int lineCounter = 0;
    bool isMatch = true;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    try
    {
        while (!inFile->atEnd() && lineCounter < 100) {
            lineCounter++;

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
                    if (tokens.length() > 3)
                    {
                        QList<QByteArray> timestampToks = tokens[1].split(':');
                        if (timestampToks.count() != 4) isMatch = false;

                        long ID = tokens[2].toLong(nullptr, 16);
                        if (ID < 1 || ID > 0x1FFFFFFF) isMatch = false;
                        int len = tokens[3].toInt();
                        if (len > 8 || len < 0) isMatch = false;
                        QList<QByteArray> dataToks = tokens[4].split(' ');
                        if (len > dataToks.length()) isMatch = false;
                    }
                    else isMatch = false;
                }
            }
        }
    }
    catch (...)
    {
        isMatch = false;
    }

    inFile->close();
    delete inFile;
    return isMatch;
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
    bool foundErrors = false;
    thisFrame.remote = false;

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
                if (tokens.length() > 3)
                {
                    QList<QByteArray> timestampToks = tokens[1].split(':');

                    timeStamp = timestampToks[0].toInt() * 1000000l * 60 * 60;
                    timeStamp += timestampToks[1].toInt() * 1000000l * 60;
                    timeStamp += timestampToks[2].toInt() * 1000000l;
                    timeStamp += timestampToks[3].toInt() * 100;

                    thisFrame.timestamp = static_cast<uint32_t>(timeStamp);

                    thisFrame.ID = static_cast<uint32_t>(tokens[2].toLong(nullptr, 16));
                    if (thisFrame.ID <= 0x7FF) thisFrame.extended = false;
                        else thisFrame.extended = true;
                    thisFrame.bus = 0;
                    thisFrame.remote = false;
                    thisFrame.len = tokens[3].toUInt();
                    if (thisFrame.len > 8) thisFrame.len = 8;
                    QList<QByteArray> dataToks = tokens[4].split(' ');
                    if (thisFrame.len > (unsigned int) dataToks.length()) thisFrame.len = static_cast<unsigned int>(dataToks.length());
                    for (unsigned int d = 0; d < thisFrame.len; d++) thisFrame.data[d] = (unsigned char)dataToks[d].toInt(nullptr, 16);
                    frames->append(thisFrame);
                }
                else foundErrors = true;
            }
        }
    }
    inFile->close();
    delete inFile;
    return !foundErrors;
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

        for (unsigned int temp = 0; temp < frames->at(c).len; temp++)
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

bool FrameFileIO::saveCanDumpFile(QString filename, const QVector<CANFrame> * frames)
{
    QFile *outFile = new QFile(filename);
    QDateTime timestamp;
    int lineCounter = 0;
    double tempTime;

    timestamp = QDateTime::currentDateTime();

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete outFile;
        return false;
    }

    for (int c = 0; c < frames->count(); c++)
    {
        lineCounter++;
        if ((lineCounter % 100) == 0)
        {
            qApp->processEvents();
        }

        outFile->write("(");

        tempTime = frames->at(c).timestamp / 1000000.0;
        outFile->write(QString::number(tempTime,'f', 6).rightJustified(17, '0').toUtf8());
        outFile->write(") vcan0 ");

        if (frames->at(c).extended) {
            outFile->write(QString::number(frames->at(c).ID, 16).rightJustified(8,'0').toUpper().toUtf8());
        } else {
            outFile->write(QString::number(frames->at(c).ID, 16).rightJustified(3,'0').toUpper().toUtf8());
        }

        outFile->write("#");

        if (frames->at(c).remote) {
            outFile->write("R");
            outFile->write(QString::number(frames->at(c).len).toUtf8());
        } else {
            for (unsigned int temp = 0; temp < frames->at(c).len; temp++)
            {
                outFile->write(QString::number(frames->at(c).data[temp], 16).rightJustified(2,'0').toUpper().toUtf8());
            }
        }

        outFile->write("\n");

    }
    outFile->close();
    delete outFile;
    return true;
}

bool FrameFileIO::isCanDumpFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    QList<QByteArray> tokens;
    QRegExp timeExp("^\\((\\S+)\\)$");
    QRegExp IdValExp("^(\\S+)#(\\S+)$");
    QRegExp valExp("(\\S{2})");
    int lineCounter = 0;
    int pos = 0;
    bool isMatch = true;
    bool ret;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    try
    {
        while (!inFile->atEnd() && lineCounter < 100) {
            lineCounter++;

            line = inFile->readLine().toUpper();
            if (line.length() > 1)
            {
                /* tokenize */
                tokens.clear();
                tokens = line.simplified().split(' ');
                if(tokens.count() < 3) isMatch = false;

                /* timestamp */
                ret = timeExp.exactMatch(tokens[0]);
                if(!ret) isMatch = false;

                /*uint64_t timestamp = (uint64_t)*/(timeExp.cap(1).toDouble(&ret) /** (double)1000000.0*/);
                if(!ret) isMatch = false;

                if (line.contains('[')) //the expanded format
                {
                    //(1551774790.942758) can1 7A8 [8] F4 DC D1 83 0E 02 00 00
                    //     0               1     2   3  4 5  6  7  8  9  10 11
                    if (tokens.count() < 4)
                    {
                        isMatch = false;
                        continue;
                    }
                    int ID = tokens[2].toULong(nullptr, 16);
                    if (ID > 0x1FFFFFFF || ID == 0) isMatch = false;
                    int len = tokens[3].at(1) - '0';
                    if (len < 0 || len > 8) isMatch = false;
                }
                else  //the more concise format
                {
                    /* ID & value */
                    //qDebug() << tokens[2];
                    if (tokens.count() < 3)
                    {
                        isMatch = false;
                        continue;
                    }
                    ret = IdValExp.exactMatch(tokens[2]);
                    if(!ret)
                    {
                        isMatch = false;
                        continue;
                    }

                    /* ID */
                    /*int ID = */IdValExp.cap(1).toInt(&ret, 16);

                    QString val= IdValExp.cap(2);

                    pos = 0;
                    int len = 0;
                    if (val.startsWith("R") && val.at(1).isDigit()) {
                        len = val.at(1).toLatin1() - '0';
                        if (len < 0 || len > 8)
                        {
                            isMatch = false;
                            continue;
                        }
                    } else {
                        /* val byte per byte */
                        int lng = 0;
                        while ((pos = valExp.indexIn(val, pos)) != -1)
                        {
                            lng++;
                            if (lng > 8)
                            {
                                isMatch = false;
                                break;
                            }
                            /*int data = */valExp.cap(1).toInt(&ret, 16);
                            if(!ret)
                            {
                                isMatch = false;
                                break;
                            }

                            pos += valExp.matchedLength();
                        }
                    }
                }
            }
        }
    }
    catch (...)
    {
        isMatch = false;
    }
    inFile->close();
    delete inFile;
    return isMatch;
}

/*
   (0.003800) vcan0 164#0000c01aa8000013
                       or
   (1551774790.942758) can1 7A8 [8] F4 DC D1 83 0E 02 00 00
*/
bool FrameFileIO::loadCanDumpFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    QList<QByteArray> tokens;
    QRegExp timeExp("^\\((\\S+)\\)$");
    QRegExp IdValExp("^(\\S+)#(\\S+)$");
    QRegExp valExp("(\\S{2})");
    int lineCounter = 0;
    int pos = 0;
    bool ret;

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

        line = inFile->readLine().toUpper();
        if (line.length() > 1)
        {
            /* tokenize */
            tokens.clear();
            tokens = line.simplified().split(' ');
            if(tokens.count()<3) continue;

            /* timestamp */
            ret = timeExp.exactMatch(tokens[0]);
            if(!ret) continue;

            thisFrame.timestamp = (uint64_t)(timeExp.cap(1).toDouble(&ret) * (double)1000000.0);
            if(!ret) continue;

            if (line.contains('[')) //the expanded format (second one from the above list)
            {
                //(1551774790.942758) can1 7A8 [8] F4 DC D1 83 0E 02 00 00
                //     0               1     2   3  4 5  6  7  8  9  10 11
                thisFrame.ID = tokens[2].toULong(nullptr, 16);
                if (thisFrame.ID > 0x7FF) thisFrame.extended = true;
                else thisFrame.extended = false;
                thisFrame.remote = false;
                thisFrame.len = tokens[3].at(1) - '0';
                for (uint32_t c = 0; c < thisFrame.len; c++)
                {
                    thisFrame.data[c] = static_cast<unsigned char>(tokens[4 + c].toInt(nullptr, 16));
                }
            }
            else  //the more concise format (first one from list above)
            {
                /* ID & value */
                //qDebug() << tokens[2];
                ret = IdValExp.exactMatch(tokens[2]);
                if(!ret)
                {
                    qDebug() << "ID regex didn't match!";
                    continue;
                }

                /* ID */
                thisFrame.ID = static_cast<uint32_t>(IdValExp.cap(1).toInt(&ret, 16));
                if (IdValExp.cap(1).length() > 3) {
                    thisFrame.extended = true;
                } else {
                    thisFrame.extended = false;
                }

                QString val= IdValExp.cap(2);

                pos = 0;
                thisFrame.len = 0;
                if (val.startsWith("R") && val.at(1).isDigit()) {
                    thisFrame.len = val.at(1).toLatin1() - '0';
                    thisFrame.remote = true;
                } else {
                    thisFrame.remote = false;
                    /* val byte per byte */
                    while ((pos = valExp.indexIn(val, pos)) != -1)
                    {
                        thisFrame.data[thisFrame.len] = valExp.cap(1).toInt(&ret, 16);
                        if(!ret) continue;

                        thisFrame.len++;
                        pos += valExp.matchedLength();
                    }
                }
            }

            /*NB: should we make sure len <= 8? */
            thisFrame.isReceived = true;
            thisFrame.bus = 0;
       }
       frames->append(thisFrame);
    }
    inFile->close();
    delete inFile;
    return true;
}


bool FrameFileIO::isKvaserFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    int lineCounter = 0;
    bool isMatch = true;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }
    try
    {
        line = inFile->readLine().simplified().toUpper();
        if (!line.contains("CHN")) isMatch = false;
        if (!line.contains("FLG")) isMatch = false;
        if (!line.contains("D0")) isMatch = false;

        if (inFile->atEnd()) isMatch = false;

        while (!inFile->atEnd() && lineCounter < 10) {
            lineCounter++;

            line = inFile->readLine().toUpper();

            if (line.length() > 70) {
                //Chn Identifier Flg   DLC  D0...1...2...3...4...5...6..D7       Time     Dir
                // 0    000000AD         8  FF  FF  00  00  00  00  00  00     154.266550 R
                int len = line.mid(21, 3).simplified().toInt();
                if (len > 8 || len < 0) isMatch = false;
            }
            else isMatch = false;
        }
    }
    catch (...)
    {
        isMatch = false;
    }
    inFile->close();
    delete inFile;
    return isMatch;
}

//Chn Identifier Flg   DLC  D0...1...2...3...4...5...6..D7       Time     Dir
// 0    000000AD         8  FF  FF  00  00  00  00  00  00     154.266550 R
bool FrameFileIO::loadKvaserFile(QString filename, QVector<CANFrame> *frames, bool useHex)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    int lineCounter = 0;
    int base = 10;
    bool foundErrors = false;
    thisFrame.remote = false;

    if (useHex) base = 16;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }

    //ignore header
    line = inFile->readLine().simplified().toUpper();

    if (inFile->atEnd()) foundErrors = true;

    while (!inFile->atEnd()) {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }
        line = inFile->readLine().toUpper();

        qDebug() << line.length();

        if (line.length() > 70) {
            //Chn Identifier Flg   DLC  D0...1...2...3...4...5...6..D7       Time     Dir
            // 0    000000AD         8  FF  FF  00  00  00  00  00  00     154.266550 R
            thisFrame.bus = line.mid(0,3).simplified().toInt();
            thisFrame.ID = line.mid(4,10).simplified().toInt(nullptr, base);
            if (thisFrame.ID > 0x7FF) thisFrame.extended = true;
                else thisFrame.extended = false;
            thisFrame.remote = false;
            thisFrame.len = line.mid(21, 3).simplified().toInt();
            for (int i = 0; i < 8; i++) {
                thisFrame.data[i] = line.mid(25 + i * 4, 3).simplified().toInt(nullptr, base);
            }
            thisFrame.timestamp = line.mid(57, 14).simplified().toDouble() * 1000000;
            if (line.mid(72, 1).toUpper() == "R") thisFrame.isReceived = true;
                else thisFrame.isReceived = false;

            frames->append(thisFrame);
        }
        //else foundErrors = true;
    }
    inFile->close();
    delete inFile;
    return !foundErrors;
}

bool FrameFileIO::isCabanaFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QByteArray line;
    int lineCounter = 0;
    bool isMatch = true;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return false;
    }
    try
    {
        line = inFile->readLine().toUpper(); //read out the header first and discard it.
        if (!line.contains("TIME")) isMatch = false;
        if (!line.contains("ADDR")) isMatch = false;

        while (!inFile->atEnd() || lineCounter < 100) {
            lineCounter++;

            line = inFile->readLine().simplified();
            if (line.length() > 2)
            {
                QList<QByteArray> tokens = line.split(',');
                if (tokens.length() >= 3 && tokens.length() < 5)
                {
                    int ID = tokens[1].toInt();
                    if (ID < 1 || ID > 0x1FFFFFFF) isMatch = false;
                }
                else isMatch = false;
            }
        }
    }
    catch (...)
    {
        isMatch = false;
    }

    inFile->close();
    delete inFile;
    return isMatch;
}

//Cabana uses a CSV file with four columns
//time,addr,bus,data
//time is in seconds, with quite a bit of resolution after the decimal point. Does not start at 0, probably time-since-boot?
//addr is the arbitrarion ID in decimal, not hex
//bus is which CAN bus this was captured on
//data is the data in hex
//There also may or may not be some blank lines in between the csv column headers and the beginning of data.
bool FrameFileIO::loadCabanaFile(QString filename, QVector<CANFrame>* frames)
{
    QFile *inFile = new QFile(filename);
    CANFrame thisFrame;
    QByteArray line;
    bool timeStampBaseSet = false;
    uint64_t timeStampBase = 0;
    uint64_t lastTimeStamp = 0;
    int lineCounter = 0;
    bool foundErrors = false;
    thisFrame.remote = false;

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
            QList<QByteArray> tokens = line.split(',');
            if (tokens.length() >= 3)
            {
                if (tokens[0].length() > 1)
                {
                    double temp = tokens[0].toDouble();
                    temp = temp * 1000000;

                    if(!timeStampBaseSet)
                    {
                        timeStampBase = (uint64_t)(temp);
                        timeStampBaseSet = true;
                    }

                    if(timeStampBaseSet) {
                        thisFrame.timestamp = ((uint64_t)(temp) - timeStampBase);
                        lastTimeStamp = thisFrame.timestamp;
                    } else {
                        thisFrame.timestamp = (lastTimeStamp + 1);
                        lastTimeStamp = thisFrame.timestamp;
                    }

                }
                else
                {
                    thisFrame.timestamp = (lastTimeStamp + 1);
                    lastTimeStamp = thisFrame.timestamp;
                }

                thisFrame.ID = tokens[1].toInt();
                if (thisFrame.ID > 0x7ff) thisFrame.extended = 1;
                    else thisFrame.extended = 0;

                thisFrame.remote = false;

                thisFrame.isReceived = true;
                thisFrame.bus = tokens[2].toInt();
                thisFrame.len = 8;
                
                {
                    unsigned long long int tempData = tokens[3].toULongLong(nullptr, 16);
                    thisFrame.data[0] = ((tempData >> 56) & 0xFF);
                    thisFrame.data[1] = ((tempData >> 48) & 0xFF);
                    thisFrame.data[2] = ((tempData >> 40) & 0xFF);
                    thisFrame.data[3] = ((tempData >> 32) & 0xFF);
                    thisFrame.data[4] = ((tempData >> 24) & 0xFF);
                    thisFrame.data[5] = ((tempData >> 16) & 0xFF);
                    thisFrame.data[6] = ((tempData >> 8) & 0xFF);
                    thisFrame.data[7] = (tempData & 0xFF);
                }

                frames->append(thisFrame);
            }
            else foundErrors = true;
        }
    }
    inFile->close();
    delete inFile;
    return !foundErrors;
}

bool FrameFileIO::saveCabanaFile(QString filename, const QVector<CANFrame>* frames)
{
    QFile *outFile = new QFile(filename);
    int lineCounter = 0;

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete outFile;
        return false;
    }
    
    outFile->write("time,addr,bus,data");
    outFile->write("\n");

    for (int c = 0; c < frames->count(); c++)
    {
        lineCounter++;
        if (lineCounter > 100)
        {
            qApp->processEvents();
            lineCounter = 0;
        }

        double tempTimeStamp = frames->at(c).timestamp;
        tempTimeStamp /= 1000000;

        outFile->write(QString::number(tempTimeStamp, 'f').toUtf8());
        outFile->write(".0");
        outFile->putChar(44);

        outFile->write(QString::number(frames->at(c).ID, 10).toUpper().toUtf8());
        outFile->putChar(44);

        outFile->write(QString::number(frames->at(c).bus).toUtf8());
        outFile->putChar(44);

        for (unsigned int temp = 0; temp < 8; temp++)
        {
            if (temp < frames->at(c).len)
                outFile->write(QString::number(frames->at(c).data[temp], 16).toUpper().rightJustified(2, '0').toUtf8());
            else
                outFile->write("00");
        }

        outFile->write("\n");

    }
    outFile->close();
    delete outFile;
    return true;
}
