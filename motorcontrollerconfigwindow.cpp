#include "motorcontrollerconfigwindow.h"
#include "ui_motorcontrollerconfigwindow.h"

#include "mainwindow.h"
#include <QDebug>

MotorControllerConfigWindow::MotorControllerConfigWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MotorControllerConfigWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    modelFrames = frames;

    timer.setInterval(40);
    transmitStep = 0;
    doingRequest = false;

    QStringList headers;
    headers << "Param" << "Value";
    ui->tableParams->setColumnCount(2);
    ui->tableParams->setColumnWidth(0, 350);
    ui->tableParams->setColumnWidth(1, 150);
    ui->tableParams->setHorizontalHeaderLabels(headers);


    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    connect(ui->btnRefresh, SIGNAL(clicked(bool)), this, SLOT(refreshData()));
    connect(ui->btnSave, SIGNAL(clicked(bool)), this, SLOT(saveData()));
    connect(&timer, SIGNAL(timeout()), this, SLOT(timerTick()));
    connect(ui->btnLoadFile, SIGNAL(clicked(bool)), this, SLOT(loadFile()));
}

MotorControllerConfigWindow::~MotorControllerConfigWindow()
{
    delete ui;
}

void MotorControllerConfigWindow::updatedFrames(int numFrames)
{
    CANFrame thisFrame;
    int id;
    int param;
    QTableWidgetItem *item;
    if (numFrames == -1) //all frames deleted
    {

    }
    else if (numFrames == -2) //all new set of frames. Reset
    {

    }
    else //just got some new frames. See if they are relevant.
    {
        if (numFrames > modelFrames->count()) return;
        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);
            id = thisFrame.ID;

            if (id == 0xC2) //response to a query we made
            {
                if (thisFrame.data[2] == 0)
                {
                    int paramID = thisFrame.data[0] + (thisFrame.data[1] * 256);
                    for (int i = 0; i < params.length(); i++)
                    {
                        if (params[i].paramID == paramID)
                        {
                            params[i].value = thisFrame.data[4] + (thisFrame.data[5] * 256);
                            if (params[i].paramType == ASCII) item = new QTableWidgetItem(); //QString::fromUtf8((char *)params[i].value, 2));
                            if (params[i].paramType == HEX) item = new QTableWidgetItem(Utility::formatHexNum(params[i].value));
                            if (params[i].paramType == DEC)
                            {
                                if (params[i].signedType == UNSIGNED) item = new QTableWidgetItem(QString::number(params[i].value));
                                if (params[i].signedType == SIGNED)
                                {
                                    if (params[i].value < 0x8000) item = new QTableWidgetItem(QString::number(params[i].value));
                                    else item = new QTableWidgetItem(QString::number(params[i].value - 0x10000));
                                }
                                if (params[i].signedType == Q15) item = new QTableWidgetItem(QString::number(params[i].value / 32768.0));
                            }
                            ui->tableParams->setItem(i, 1, item);
                            break;
                        }
                    }
                }
            }
        }
    }
}

void MotorControllerConfigWindow::refreshData()
{
    doingRequest = true;
    transmitStep = 0;
    timer.start();
}

void MotorControllerConfigWindow::loadFile()
{
    QString filename;
    QFileDialog dialog;
    QFile *inFile;
    QByteArray line;
    PARAM param;

    QStringList filters;
    filters.append(QString(tr("RMS Definition File (*.txt *.TXT)")));

    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        inFile = new QFile(filename);

        if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
        {
            delete inFile;
            return;
        }

        ui->tableParams->clear();
        params.clear();

        while (!inFile->atEnd())
        {
            line = inFile->readLine().simplified().toUpper();
            if (!line.startsWith("#") && line.length() > 10)
            {
                QList<QByteArray> tokens = line.split(',');
                //Serial_Number_EEPROM                     , 0x0113, uint, dec, 16, 6, spr, spr, spr
                param.paramName = tokens[0].simplified();
                param.paramID = Utility::ParseStringToNum(tokens[1].simplified());
                QString signedStr = tokens[2].simplified().toUpper();
                param.signedType = SIGNED;
                if (signedStr.compare("UINT") == 0) param.signedType = UNSIGNED;
                if (signedStr.compare("Q15") == 0) param.signedType = Q15;
                QString typeStr = tokens[3].simplified().toUpper();
                param.paramType = DEC;
                if (typeStr.compare("HEX") == 0) param.paramType = HEX;
                if (typeStr.compare("ASCII") == 0) param.paramType = ASCII;

                int row = ui->tableParams->rowCount();
                ui->tableParams->insertRow(row);

                QTableWidgetItem *item = new QTableWidgetItem(param.paramName);
                ui->tableParams->setItem(row, 0, item);
                item = new QTableWidgetItem("");
                ui->tableParams->setItem(row, 1, item);

                params.append(param);
            }
        }

        refreshData();

        inFile->close();
        delete inFile;
    }
}

void MotorControllerConfigWindow::saveData()
{
    doingRequest = false;
    transmitStep = 0;
    timer.start();
}

void MotorControllerConfigWindow::timerTick()
{
    if (doingRequest) //send requests for data
    {
        qDebug() << "Request: " << QString::number(transmitStep);

        outFrame.ID = 0xC1;
        outFrame.len = 8;
        outFrame.bus = 0;
        outFrame.extended = false;
        outFrame.data[0] = params[transmitStep].paramID & 0xFF;
        outFrame.data[1] = (params[transmitStep].paramID >> 8) & 0xFF;
        outFrame.data[2] = 0; //0 = read, 1 = write
        outFrame.data[3] = 0; //reserved
        outFrame.data[4] = 0; //value goes in bytes 4,5 when writing
        outFrame.data[5] = 0;
        outFrame.data[6] = 0; //reserved
        outFrame.data[7] = 0; //reserved

        CANConManager::getInstance()->sendFrame(outFrame);

        transmitStep++;
        if (transmitStep == params.length())
        {
            timer.stop();
            return;
        }
    }
    else //save current value of all data items
    {
        uint16_t thisValue;
        qDebug() << "Transmission: " << QString::number(transmitStep);

        QString cellVal = ui->tableParams->item(transmitStep, 1)->text();

        if (params[transmitStep].paramType != HEX)
            thisValue = (uint16_t)Utility::ParseStringToNum(cellVal);
        else
            thisValue = (uint16_t)Utility::ParseStringToNum(cellVal);

        if (thisValue != params[transmitStep].value)
        {
            outFrame.ID = 0xC1;
            outFrame.len = 8;
            outFrame.bus = 0;
            outFrame.extended = false;
            outFrame.data[0] = params[transmitStep].paramID & 0xFF;
            outFrame.data[1] = (params[transmitStep].paramID >> 8) & 0xFF;
            outFrame.data[2] = 1; //0 = read, 1 = write
            outFrame.data[3] = 0; //reserved
            outFrame.data[4] = params[transmitStep].value & 0xFF;
            outFrame.data[5] = (params[transmitStep].value >> 8) & 0xFF;
            outFrame.data[6] = 0; //reserved
            outFrame.data[7] = 0; //reserved

            CANConManager::getInstance()->sendFrame(outFrame);
        }

        transmitStep++;
    }
}
