/*
* gs_usb.h
*
* Ben Evans <ben@canbusdebugger.com>
*
* MIT License
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#ifndef GSUSB_H
#define GSUSB_H

#include "gs_usb_definitions.h"
#include "gs_usb_reader.h"
#include "connections/canconnection.h"

typedef struct _GSUSB_timing_t_s
{
    unsigned int deviceClock;
    unsigned int bitrate;
    unsigned int samplePoint;
    unsigned int prescaler;
    unsigned int propSeg;
    unsigned int tseg1;
    unsigned int tseg2;
    unsigned int sjw;
} GSUSB_timing_t;


class GSUSBDevice : public CANConnection
{
    Q_OBJECT

public:
    GSUSBDevice(QString portName, int busSpeed, int samplePoint);
    virtual ~GSUSBDevice();
    bool isDeviceConnected();
    candle_handle getDeviceHandle();

    static void getConnectedDevices(QVector<candle_handle> &remoteDeviceGSUSB);
    static void getSupportedTimings(candle_handle handle, QList<GSUSB_timing_t> &timings);

    static candle_bittiming_t timingToCandleBitTiming(const GSUSB_timing_t &timing);
    static QString handleToDeviceIDString(candle_handle handle);
    static QString timingToString(const GSUSB_timing_t &timing);

protected:
    virtual void piStarted();
    virtual void piStop();
    virtual void piSetBusSettings(int pBusIdx, CANBus pBus);
    virtual bool piGetBusSettings(int pBusIdx, CANBus& pBus);
    virtual void piSuspend(bool pSuspend);
    virtual bool piSendFrame(const CANFrame&);

private slots:
    void readFrame(const candle_frame_t frame);

private:
    bool setBitTiming(unsigned int bitrate, unsigned int samplePoint);
    void sendDebug(const QString debugText);

protected:
    bool deviceConnected = false;
    candle_handle deviceHandle;
    GSUSBReader* reader = NULL;
    QList<GSUSB_timing_t> supportedTimings;

};

#endif // GSUSB_H
