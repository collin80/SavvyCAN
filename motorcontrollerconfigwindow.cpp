#include "motorcontrollerconfigwindow.h"
#include "ui_motorcontrollerconfigwindow.h"

#include "mainwindow.h"
#include <QDebug>

MotorControllerConfigWindow::MotorControllerConfigWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MotorControllerConfigWindow)
{
    ui->setupUi(this);

    modelFrames = frames;

    timer.setInterval(40);
    transmitStep = 0;
    doingRequest = false;

    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    connect(ui->btnRefresh, SIGNAL(clicked(bool)), this, SLOT(refreshData()));
    connect(ui->btnSave, SIGNAL(clicked(bool)), this, SLOT(saveData()));
    connect(&timer, SIGNAL(timeout()), this, SLOT(timerTick()));
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
    float valu;
    uint8_t eightval;
    uint16_t sixteenval;
    uint32_t thirtytwoval;
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

            if (id == 0x420)
            {
                param = thisFrame.data[0];
                eightval = thisFrame.data[1];
                sixteenval = thisFrame.data[1] + (thisFrame.data[2] * 256ul);
                thirtytwoval = sixteenval + (thisFrame.data[3] * 65536ul) + (thisFrame.data[4] * 256ul * 65536ul);

                switch (param)
                {
                case 0: //voltage scale (*65536)
                    valu = thirtytwoval / 65536.0f;
                    ui->editVoltageScale->setText(QString::number(valu, 'g', 4));
                    break;
                case 1: //voltage bias
                    ui->editVoltageOffset->setText(QString::number(sixteenval));
                    break;
                case 2: //current 1 scale (*65536)
                    valu = thirtytwoval / 65536.0f;
                    ui->editCurrent1Scale->setText(QString::number(valu, 'g', 4));
                    break;
                case 3: //current 1 bias
                    ui->editCurrent1Offset->setText(QString::number(sixteenval));
                    break;
                case 4: //current 2 scale (*65536)
                    valu = thirtytwoval / 65536.0f;
                    ui->editCurrent2Scale->setText(QString::number(valu, 'g', 4));
                    break;
                case 5: //current 2 bias
                    ui->editCurrent2Offset->setText(QString::number(sixteenval));
                    break;
                case 6: //Inverter temp 1 scale (*65536)
                    valu = thirtytwoval / 65536.0f;
                    ui->editInvTemp1Scale->setText(QString::number(valu, 'g', 4));
                    break;
                case 7: //Inverter temp 1 bias
                    ui->editInvTemp1Offset->setText(QString::number(sixteenval));
                    break;
                case 8: //Inverter temp 2 scale (*65536)
                    valu = thirtytwoval / 65536.0f;
                    ui->editInvTemp2Scale->setText(QString::number(valu, 'g', 4));
                    break;
                case 9: //Inverter temp 2 bias
                    ui->editInvTemp2Offset->setText(QString::number(sixteenval));
                    break;
                case 10: //Motor temp 1 scale (*65536)
                    valu = thirtytwoval / 65536.0f;
                    ui->editMotorTemp1Scale->setText(QString::number(valu, 'g', 4));
                    break;
                case 11: //Motor temp 1 bias
                    ui->editMotorTemp1Offset->setText(QString::number(sixteenval));
                    break;
                case 12: //Motor temp 2 scale (*65536)
                    valu = thirtytwoval / 65536.0f;
                    ui->editMotorTemp2Scale->setText(QString::number(valu, 'g', 4));
                    break;
                case 13: //Motor temp 2 bias
                    ui->editMotorTemp2Offset->setText(QString::number(sixteenval));
                    break;
                case 14: //motor type (inductive, pmac, bldc)
                    ui->editMotorType->setText(QString::number(eightval));
                    break;
                case 15: //control type (vhz, foc)
                    ui->editControlType->setText(QString::number(eightval));
                    break;
                case 16: //encoder count
                    ui->editEncoderCount->setText(QString::number(sixteenval));
                    break;
                case 17: //encoder direction (0 = normal, 1 = reversed)
                    ui->editEncoderDir->setText(QString::number(eightval));
                    break;
                case 18:
                    ui->editPolePairs->setText(QString::number(eightval));
                    break;
                case 19: //rotor time const (*65536)
                    valu = thirtytwoval / 65536.0f;
                    ui->editRotorTime->setText(QString::number(valu, 'g', 4));
                    break;
                case 20: //Torque KP (*4096)
                    valu = thirtytwoval / 4096.0f;
                    ui->editTorqueKP->setText(QString::number(valu, 'g', 4));
                    break;
                case 21: //Torque KI (*4096)
                    valu = thirtytwoval / 4096.0f;
                    ui->editTorqueKI->setText(QString::number(valu, 'g', 4));
                    break;
                case 22: //Torque KD (*4096)
                    valu = thirtytwoval / 4096.0f;
                    ui->editTorqueKD->setText(QString::number(valu, 'g', 4));
                    break;
                case 23: //Max RPM
                    ui->editMaxRPM->setText(QString::number(sixteenval));
                    break;
                case 24: //Max Torque
                    valu = sixteenval / 10.0f;
                    ui->editMaxTorque->setText(QString::number(valu, 'g', 4));
                    break;
                case 25: //Max Amps in Drive
                    valu = sixteenval / 10.0f;
                    ui->editMaxAmpsDrive->setText(QString::number(valu, 'g', 4));
                    break;
                case 26: //Max Amps in Regen
                    valu = sixteenval / 10.0f;
                    ui->editMaxAmpsRegen->setText(QString::number(valu, 'g', 4));
                    break;
                case 27: //VHz Full Speed
                    ui->editVHZFull->setText(QString::number(sixteenval));
                    break;
                case 28: //Theta offset
                    ui->editThetaOffset->setText(QString::number(sixteenval));
                    break;
                case 29: //Hall AB
                    ui->editHallAB->setText(QString::number(eightval));
                    break;
                case 30: //Hall BC
                    ui->editHallBC->setText(QString::number(eightval));
                    break;
                case 31: //Hall CA
                    ui->editHallCA->setText(QString::number(eightval));
                    break;
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

void MotorControllerConfigWindow::saveData()
{
    doingRequest = false;
    transmitStep = 0;
    timer.start();
}

void MotorControllerConfigWindow::timerTick()
{
    float valu;
    uint32_t intval32;
    uint16_t intval16;
    uint8_t intval8;

    if (doingRequest) //send requests for data
    {
        //qDebug() << "Request: " << QString::number(transmitStep);
        send8BitParam(transmitStep, 0);
        transmitStep++;
        if (transmitStep == 32)
        {
            timer.stop();
            return;
        }
    }
    else //save current value of all data items
    {
        //qDebug() << "Transmission: " << QString::number(transmitStep);
        switch (transmitStep)
        {
        case 0:
            valu = ui->editVoltageScale->text().toFloat();
            intval32 = valu * 65536;
            send32BitParam(0x80 + transmitStep, intval32);
            break;
        case 1:
            intval16 = ui->editVoltageOffset->text().toInt();
            send16BitParam(0x80 + transmitStep, intval16);
            break;
        case 2:
            valu = ui->editCurrent1Scale->text().toFloat();
            intval32 = valu * 65536;
            send32BitParam(0x80 + transmitStep, intval32);
            break;
        case 3:
            intval16 = ui->editCurrent1Offset->text().toInt();
            send16BitParam(0x80 + transmitStep, intval16);
            break;
        case 4:
            valu = ui->editCurrent2Scale->text().toFloat();
            intval32 = valu * 65536;
            send32BitParam(0x80 + transmitStep, intval32);
            break;
        case 5:
            intval16 = ui->editCurrent2Offset->text().toInt();
            send16BitParam(0x80 + transmitStep, intval16);
            break;
        case 6: //Inverter temp 1 scale (*65536)
            valu = ui->editInvTemp1Scale->text().toFloat();
            intval32 = valu * 65536;
            send32BitParam(0x80 + transmitStep, intval32);
            break;
        case 7: //Inverter temp 1 bias
            intval16 = ui->editInvTemp1Offset->text().toInt();
            send16BitParam(0x80 + transmitStep, intval16);
            break;
        case 8: //Inverter temp 2 scale (*65536)
            valu = ui->editInvTemp2Scale->text().toFloat();
            intval32 = valu * 65536;
            send32BitParam(0x80 + transmitStep, intval32);
            break;
        case 9: //Inverter temp 2 bias
            intval16 = ui->editInvTemp2Offset->text().toInt();
            send16BitParam(0x80 + transmitStep, intval16);
            break;
        case 10: //Motor temp 1 scale (*65536)
            valu = ui->editMotorTemp1Scale->text().toFloat();
            intval32 = valu * 65536;
            send32BitParam(0x80 + transmitStep, intval32);
            break;
        case 11: //Motor temp 1 bias
            intval16 = ui->editMotorTemp1Offset->text().toInt();
            send16BitParam(0x80 + transmitStep, intval16);
            break;
        case 12: //Motor temp 2 scale (*65536)
            valu = ui->editMotorTemp2Scale->text().toFloat();
            intval32 = valu * 65536;
            send32BitParam(0x80 + transmitStep, intval32);
            break;
        case 13: //Motor temp 2 bias
            intval16 = ui->editMotorTemp2Offset->text().toInt();
            send16BitParam(0x80 + transmitStep, intval16);
            break;
        case 14: //motor type (inductive, pmac, bldc)
            intval8 = ui->editMotorType->text().toInt();
            send8BitParam(0x80 + transmitStep, intval8);
            break;
        case 15: //control type (vhz, foc)
            intval8 = ui->editControlType->text().toInt();
            send8BitParam(0x80 + transmitStep, intval8);
            break;
        case 16: //encoder count
            intval16 = ui->editEncoderCount->text().toInt();
            send16BitParam(0x80 + transmitStep, intval16);
            break;
        case 17: //encoder direction (0 = normal, 1 = reversed)
            intval8 = ui->editEncoderDir->text().toInt();
            send8BitParam(0x80 + transmitStep, intval8);
            break;
        case 18:
            intval8 = ui->editPolePairs->text().toInt();
            send8BitParam(0x80 + transmitStep, intval8);
            break;
        case 19: //rotor time const (*65536)
            valu = ui->editRotorTime->text().toFloat();
            intval32 = valu * 65536;
            send32BitParam(0x80 + transmitStep, intval32);
            break;
        case 20: //Torque KP (*4096)
            valu = ui->editTorqueKP->text().toFloat();
            intval32 = valu * 4096;
            send32BitParam(0x80 + transmitStep, intval32);
            break;
        case 21: //Torque KI (*4096)
            valu = ui->editTorqueKI->text().toFloat();
            intval32 = valu * 4096;
            send32BitParam(0x80 + transmitStep, intval32);
            break;
        case 22: //Torque KD (*4096)
            valu = ui->editTorqueKD->text().toFloat();
            intval32 = valu * 4096;
            send32BitParam(0x80 + transmitStep, intval32);
            break;
        case 23: //Max RPM
            intval16 = ui->editMaxRPM->text().toInt();
            send16BitParam(0x80 + transmitStep, intval16);
            break;
        case 24: //Max Torque
            valu = ui->editMaxTorque->text().toFloat();
            intval16 =  valu * 10;
            send16BitParam(0x80 + transmitStep, intval16);
            break;
        case 25: //Max Amps in Drive
            valu = ui->editMaxAmpsDrive->text().toFloat();
            intval16 =  valu * 10;
            send16BitParam(0x80 + transmitStep, intval16);
            break;
        case 26: //Max Amps in Regen
            valu = ui->editMaxAmpsRegen->text().toFloat();
            intval16 =  valu * 10;
            send16BitParam(0x80 + transmitStep, intval16);
            break;
        case 27: //VHz Full Speed
            intval16 = ui->editVHZFull->text().toInt();
            send16BitParam(0x80 + transmitStep, intval16);
            break;
        case 28: //Theta offset
            intval16 = ui->editThetaOffset->text().toInt();
            send16BitParam(0x80 + transmitStep, intval16);
            break;
        case 29: //Hall AB
            intval8 = ui->editHallAB->text().toInt();
            send8BitParam(0x80 + transmitStep, intval8);
            break;
        case 30: //Hall BC
            intval8 = ui->editHallBC->text().toInt();
            send8BitParam(0x80 + transmitStep, intval8);
            break;
        case 31: //Hall CA
            intval8 = ui->editHallCA->text().toInt();
            send8BitParam(0x80 + transmitStep, intval8);
            break;
        case 32:
            timer.stop();
            return;
            break;
        }
        transmitStep++;
    }
}

void MotorControllerConfigWindow::send32BitParam(int param, uint32_t valu)
{
    outFrame.ID = 0x230;
    outFrame.len = 5;
    outFrame.bus = 0;
    outFrame.extended = false;
    outFrame.data[0] = param & 0xFF;
    outFrame.data[1] = valu & 0xFF;
    outFrame.data[2] = (valu >> 8) & 0xFF;
    outFrame.data[3] = (valu >> 16) & 0xFF;
    outFrame.data[4] = (valu >> 24) & 0xFF;

    emit sendCANFrame(&outFrame, 0);
}

void MotorControllerConfigWindow::send16BitParam(int param, uint16_t valu)
{
    outFrame.ID = 0x230;
    outFrame.len = 3;
    outFrame.bus = 0;
    outFrame.extended = false;
    outFrame.data[0] = param & 0xFF;
    outFrame.data[1] = valu & 0xFF;
    outFrame.data[2] = (valu >> 8) & 0xFF;

    emit sendCANFrame(&outFrame, 0);
}

void MotorControllerConfigWindow::send8BitParam(int param, uint8_t valu)
{
    outFrame.ID = 0x230;
    outFrame.len = 2;
    outFrame.bus = 0;
    outFrame.extended = false;
    outFrame.data[0] = param & 0xFF;
    outFrame.data[1] = valu;

    emit sendCANFrame(&outFrame, 0);
}
