/*
* gs_usb.cpp
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

/*
* NOTE REGARDING NAMING
*
* I'd like to change the name of the candle_dll repo to gs_usb_dll to be more generic as this driver
* should support all devices following the gs_usb driver protocol. However, I also don't want to take
* away from the original authors contribution to that code and so for now will keep the name.
*
* If development continues to add FD support to the driver etc. I'll make the name switch then.
*
* https://github.com/BennyEvans/candle_dll
*/

#include "gs_usb.h"
#include "gs_usb_definitions.h"
#include "gs_usb_functions.h"
#include "gs_usb_reader.h"
#include <QObject>
#include <QThread>
#include <QDebug>

#define GS_USB_NUM_BUSES        (1)
#define GS_USB_BUS_CHANNEL_ID   (0)


GSUSBDevice::GSUSBDevice(QString portName, int busSpeed, int samplePoint) :
    CANConnection(portName, "win_gs_usb_driver", CANCon::GSUSB, 0, busSpeed, samplePoint, false, 0, GS_USB_NUM_BUSES, 16000, true)
{
    candle_list_handle clist;
    uint8_t num_interfaces;
    candle_handle dev;

    sendDebug("GSUSBDevice()");

    if (candle_list_scan(&clist)) {
        if (candle_list_length(clist, &num_interfaces)) {
            for (uint8_t i = 0; i < num_interfaces; i++) {
                if (candle_dev_get(clist, i, &dev)) {
                    if (QString::compare(portName, QString::fromWCharArray((wchar_t *) candle_dev_get_path(dev))) == 0) {
                        sendDebug("GSUSBDevice found");
                        this->deviceHandle = dev;
                        this->deviceConnected = true;
                        break;
                    }
                }
            }
        }
        candle_list_free(clist);
    }
    if (this->deviceConnected == false) {
        sendDebug("GSUSBDevice not found");
    }

    getSupportedTimings(this->deviceHandle, this->supportedTimings);
}

GSUSBDevice::~GSUSBDevice()
{
    stop();
    sendDebug("~GSUSBDevice()");
}

void GSUSBDevice::sendDebug(const QString debugText)
{
    qDebug() << debugText;
    emit debugOutput(debugText);
}

void GSUSBDevice::piStarted()
{
    uint32_t setupFlags = 0;
    if (this->deviceConnected == true) {

        if (this->reader != NULL) {
            this->reader->stopReading();
        }

        // Get capability
        candle_capability_t capability;
        if (!candle_channel_get_capabilities(this->deviceHandle, GS_USB_BUS_CHANNEL_ID, &capability)) {
            sendDebug("GSUSBDevice failed to get capability");
            return;
        }

        //Connect
        if (!candle_dev_open(this->deviceHandle)) {
            sendDebug("GSUSBDevice could not open");
            return;
        } else {
            sendDebug("GSUSBDevice device open");
        }

        if (mBusData[GS_USB_BUS_CHANNEL_ID].mConfigured == false) {
            // Set default config for unset items
            mBusData[GS_USB_BUS_CHANNEL_ID].mBus.setListenOnly(false);
            mBusData[GS_USB_BUS_CHANNEL_ID].mBus.setActive(true);
            mBusData[GS_USB_BUS_CHANNEL_ID].mConfigured = true;
        }

        // Set the bit timing
        if (!setBitTiming(mBusData[GS_USB_BUS_CHANNEL_ID].mBus.getSpeed(), mBusData[GS_USB_BUS_CHANNEL_ID].mBus.getSamplePoint())) {
            sendDebug("GSUSBDevice could not set bus speed");
            return;
        } else {
            sendDebug("GSUSBDevice bus speed set to " + QString::number(mBusData[GS_USB_BUS_CHANNEL_ID].mBus.getSpeed()) + "bps @ "
                      + QString::number((mBusData[GS_USB_BUS_CHANNEL_ID].mBus.getSamplePoint() / 10.0f), 'f', 1) + "%");
        }

        // Create new reader
        this->reader = new GSUSBReader(NULL, this->deviceHandle);

        // TODO: Maybe this could just be QueuedConnection? If speed up required, check this!
        bool result = connect(this->reader, SIGNAL(newFrame(candle_frame_t)), this, SLOT(readFrame(candle_frame_t)), Qt::BlockingQueuedConnection);
        Q_ASSERT(result);

        // TODO: In the future, add one shot support etc.
        if (mBusData[GS_USB_BUS_CHANNEL_ID].mBus.isListenOnly() && (capability.feature & CANDLE_MODE_LISTEN_ONLY)) {
            setupFlags |= CANDLE_MODE_LISTEN_ONLY;
            sendDebug("GSUSBDevice listen only enabled");
        }
        if (capability.feature & CANDLE_MODE_HW_TIMESTAMP) {
            // Always enable HW timestamps if available
            setupFlags |= CANDLE_MODE_HW_TIMESTAMP;
            sendDebug("GSUSBDevice using HW timestamps");
        } else {
            // Continue to connect, but frame timestamps are going to be all over the place
            sendDebug("GSUSBDevice no HW timestamp support");
        }

        if (!candle_channel_start(this->deviceHandle, GS_USB_BUS_CHANNEL_ID, setupFlags)) {
            sendDebug("GSUSBDevice could not start channel");
            return;
        } else {
            sendDebug("GSUSBDevice channel started");
        }

        // Start listening for messages
       this->reader->startReading();

        setStatus(CANCon::CONNECTED);
        CANConStatus stats;
        stats.conStatus = getStatus();
        stats.numHardwareBuses = 1;
        emit status(stats);
    }
}

void GSUSBDevice::piSuspend(bool pSuspend)
{
    // Update capSuspended
    setCapSuspended(pSuspend);

    // Flush queue if we are suspended
    if (isCapSuspended()) {
        getQueue().flush();
    }
}

void GSUSBDevice::piStop()
{
    if (this->deviceConnected == true) {
        if (this->reader != NULL) {
            this->reader->stopReading();
        }
        this->reader = NULL;
        candle_channel_stop(this->deviceHandle, GS_USB_BUS_CHANNEL_ID);
        candle_dev_close(this->deviceHandle);

        setStatus(CANCon::NOT_CONNECTED);
        CANConStatus stats;
        stats.conStatus = getStatus();
        stats.numHardwareBuses = mNumBuses;
        emit status(stats);
    }
}

bool GSUSBDevice::piGetBusSettings(int pBusIdx, CANBus& pBus)
{
    return getBusConfig(pBusIdx, pBus);
}

void GSUSBDevice::piSetBusSettings(int pBusIdx, CANBus bus)
{
    if ((pBusIdx >= 0) && (pBusIdx <= getNumBuses())) {
        setBusConfig(pBusIdx, bus);
        // stop and re-enable the bus to apply the settings
        this->piStop();
        this->piStarted();
    }
}

bool GSUSBDevice::piSendFrame(const CANFrame& frame)
{
    candle_frame_t sendFrame;
    memset(&sendFrame, 0, sizeof(sendFrame));

    if (frame.hasExtendedFrameFormat()) {
        sendFrame.can_id |= CANDLE_ID_EXTENDED;
        sendFrame.can_id |= frame.frameId() & 0x1FFFFFFF;
    } else {
        sendFrame.can_id |= frame.frameId() & 0x7FF;
    }

    switch (frame.frameType()) {
    case CANFrame::RemoteRequestFrame:
        sendFrame.can_id |= CANDLE_ID_RTR;
        break;
    case CANFrame::DataFrame:
        // Do nothing
        break;
    default:
        return false;
    }

    QByteArray payload = frame.payload();
    if (payload.length() > 8) {
        return false;
    }
    sendFrame.can_dlc = payload.length();
    for (int i = 0; i < payload.length(); i++) {
        sendFrame.data[i] = payload.at(i);
    }

    // Send the frame
    if (candle_frame_send(this->deviceHandle, GS_USB_BUS_CHANNEL_ID, &sendFrame, true, 1000)) {
        return true;
    }
    return false;
}

bool GSUSBDevice::isDeviceConnected()
{
    return this->deviceConnected;
}

candle_handle GSUSBDevice::getDeviceHandle()
{
    return this->deviceHandle;
}

bool GSUSBDevice::setBitTiming(unsigned int bitrate, unsigned int samplePoint)
{
    if (this->deviceConnected) {
        bool timingFound = false;
        candle_bittiming_t bitTiming;
        foreach (const GSUSB_timing_t timing, this->supportedTimings) {
            if ((timing.bitrate ==  bitrate) && (timing.samplePoint == samplePoint)) {
                bitTiming = timingToCandleBitTiming(timing);
                timingFound = true;
                break;
            }
        }
        if (timingFound) {
            return candle_channel_set_timing(this->deviceHandle, GS_USB_BUS_CHANNEL_ID, &bitTiming);
        }
    }
    return false;
}

void GSUSBDevice::readFrame(const candle_frame_t frame)
{
    if (!isCapSuspended()) {
        CANFrame* queuedFrame = getQueue().get();
        if(queuedFrame) {
            queuedFrame->bus = 0;
            queuedFrame->isReceived = true;

            // Don't populate the seconds field as only us is used
            CANFrame::TimeStamp timestamp = QCanBusFrame::TimeStamp(0, frame.timestamp_us);
            queuedFrame->setTimeStamp(timestamp);

            if (frame.can_id & CANDLE_ID_EXTENDED) {
                // Extended ID
                queuedFrame->setExtendedFrameFormat(true);
                queuedFrame->setFrameId(frame.can_id & 0x1FFFFFFF);
            } else {
                // Standard ID
                queuedFrame->setExtendedFrameFormat(false);
                queuedFrame->setFrameId(frame.can_id & 0x7FF);
            }

            if (frame.can_id & CANDLE_ID_ERROR) {
                queuedFrame->setFrameType(QCanBusFrame::FrameType::ErrorFrame);
            } else if (frame.can_id & CANDLE_ID_RTR) {
                queuedFrame->setFrameType(QCanBusFrame::FrameType::RemoteRequestFrame);
            } else{
                queuedFrame->setFrameType(QCanBusFrame::FrameType::DataFrame);
            }

            int length = frame.can_dlc;
            if (length > 8) {
                length = 8;
            }
            QByteArray data = QByteArray(reinterpret_cast<const char *>(frame.data), length);
            queuedFrame->setPayload(data);

            checkTargettedFrame(*queuedFrame);
            getQueue().queue();
        } else {
            sendDebug("ERROR: GSUSBDevice can't get queue frame");
        }
    }
}


// ######Static Functions ######

void GSUSBDevice::getConnectedDevices(QVector<candle_handle> &remoteDeviceGSUSB)
{
    candle_list_handle clist;
    uint8_t num_interfaces;
    candle_handle dev;

    // Get GS USB devices
    remoteDeviceGSUSB.clear();
    if (candle_list_scan(&clist)) {
        if (candle_list_length(clist, &num_interfaces)) {
            for (uint8_t i=0; i<num_interfaces; i++) {
                if (candle_dev_get(clist, i, &dev)) {
                    remoteDeviceGSUSB.append(dev);
                }

            }
        }
        candle_list_free(clist);
    }
}

void GSUSBDevice::getSupportedTimings(candle_handle handle, QList<GSUSB_timing_t> &timings)
{
    // NOTE: In both candlelight and can bus debugger fw, programmed tseg1 is prop_seg + seg1 and thus
    // to simplify things below, prop_seg is always set to 0 as it's incorporated into tseg1
    QList<GSUSB_timing_t> allTimings = {
        // Devices running candleLight_fw with 48Mhz clock - https://github.com/candle-usb/candleLight_fw
        {48000000, 20000, 625, 300, 0, 4, 3, 3},
        {48000000, 50000, 625, 120, 0, 4, 3, 3},
        {48000000, 100000, 625, 60, 0, 4, 3, 3},
        {48000000, 150000, 625, 40, 0, 4, 3, 3},
        {48000000, 200000, 625, 30, 0, 4, 3, 3},
        {48000000, 250000, 625, 24, 0, 4, 3, 3},
        {48000000, 300000, 625, 20, 0, 4, 3, 3},
        {48000000, 500000, 625, 12, 0, 4, 3, 3},
        {48000000, 800000, 625, 6, 0, 5, 4, 4},
        {48000000, 1000000, 625, 6, 0, 4, 3, 3},

        {48000000, 20000, 750, 120, 0, 14, 5, 4},
        {48000000, 50000, 750, 48, 0, 14, 5, 4},
        {48000000, 100000, 750, 24, 0, 14, 5, 4},
        {48000000, 150000, 750, 16, 0, 14, 5, 4},
        {48000000, 200000, 750, 12, 0, 14, 5, 4},
        {48000000, 250000, 750, 12, 0, 11, 4, 4},
        {48000000, 300000, 750, 8, 0, 14, 5, 4},
        {48000000, 500000, 750, 6, 0, 11, 4, 4},
        {48000000, 800000, 750, 3, 0, 14, 5, 4},
        {48000000, 1000000, 750, 3, 0, 11, 4, 4},

        {48000000, 20000, 875, 150, 0, 13, 2, 2},
        {48000000, 50000, 875, 60, 0, 13, 2, 2},
        {48000000, 100000, 875, 30, 0, 13, 2, 2},
        {48000000, 150000, 875, 20, 0, 13, 2, 2},
        {48000000, 200000, 875, 15, 0, 13, 2, 2},
        {48000000, 250000, 875, 12, 0, 13, 2, 2},
        {48000000, 300000, 875, 10, 0, 13, 2, 2},
        {48000000, 500000, 875, 6, 0, 13, 2, 2},
        {48000000, 800000, 875, 4, 0, 12, 2, 2},
        {48000000, 1000000, 875, 3, 0, 13, 2, 2},

        // CAN Bus Debugger device using 80Mhz clock - https://www.canbusdebugger.com
        {80000000, 20000, 625, 100, 0, 24, 15, 15},
        {80000000, 50000, 625, 40, 0, 24, 15, 15},
        {80000000, 100000, 625, 20, 0, 24, 15, 15},
        {80000000, 150000, 625, 13, 0, 25, 15, 15},
        {80000000, 200000, 625, 10, 0, 24, 15, 15},
        {80000000, 250000, 625, 8, 0, 24, 15, 15},
        {80000000, 300000, 625, 7, 0, 23, 14, 14},
        {80000000, 500000, 625, 4, 0, 24, 15, 15},
        {80000000, 800000, 625, 4, 0, 15, 9, 9},
        {80000000, 1000000, 625, 2, 0, 24, 15, 15},

        {80000000, 20000, 750, 100, 0, 29, 10, 10},
        {80000000, 50000, 750, 25, 0, 47, 16, 16},
        {80000000, 100000, 750, 20, 0, 29, 10, 10},
        {80000000, 150000, 750, 13, 0, 30, 10, 10},
        {80000000, 200000, 750, 10, 0, 29, 10, 10},
        {80000000, 250000, 750, 5, 0, 47, 16, 16},
        {80000000, 300000, 750, 7, 0, 27, 10, 10},
        {80000000, 500000, 750, 4, 0, 29, 10, 10},
        {80000000, 800000, 750, 5, 0, 14, 5, 5},
        {80000000, 1000000, 750, 2, 0, 29, 10, 10},

        {80000000, 20000, 875, 100, 0, 34, 5, 5},
        {80000000, 50000, 875, 25, 0, 55, 8, 8},
        {80000000, 100000, 875, 20, 0, 34, 5, 5},
        {80000000, 150000, 875, 13, 0, 35, 5, 5},
        {80000000, 200000, 875, 10, 0, 34, 5, 5},
        {80000000, 250000, 875, 5, 0, 55, 8, 8},
        {80000000, 300000, 875, 7, 0, 32, 5, 5},
        {80000000, 500000, 875, 4, 0, 34, 5, 5},
        {80000000, 800000, 875, 2, 0, 43, 6, 6},
        {80000000, 1000000, 875, 2, 0, 34, 5, 5}
    };

    candle_capability_t capability;
    if (candle_channel_get_capabilities(handle, GS_USB_BUS_CHANNEL_ID, &capability)) {
        foreach (const GSUSB_timing_t timing, allTimings) {
            candle_bittiming_t bitTiming = timingToCandleBitTiming(timing);
            if ((timing.deviceClock == capability.device_clock) &&
                ((bitTiming.phase_seg1 >= capability.tseg1_min) && (bitTiming.phase_seg1 <= capability.tseg1_max)) &&
                ((bitTiming.phase_seg2 >= capability.tseg2_min) && (bitTiming.phase_seg2 <= capability.tseg2_max)))
            {
                timings.append(timing);
            }
        }
    }
}

candle_bittiming_t GSUSBDevice::timingToCandleBitTiming(const GSUSB_timing_t &timing)
{
    candle_bittiming_t timingOut;
    timingOut.brp = timing.prescaler;
    timingOut.prop_seg = timing.propSeg;
    timingOut.phase_seg1 = timing.tseg1;
    timingOut.phase_seg2 = timing.tseg2;
    timingOut.sjw = timing.sjw;
    return timingOut;
}

QString GSUSBDevice::handleToDeviceIDString(candle_handle handle)
{
    wchar_t* path = (wchar_t *) candle_dev_get_path(handle);
    if (path != NULL) {
        return QString::fromWCharArray((wchar_t *) candle_dev_get_path(handle));
    }
    return QString();
}

QString GSUSBDevice::timingToString(const GSUSB_timing_t &timing)
{
    QString str;
    QTextStream(&str) << QString::number(timing.bitrate) << " (SP: " << QString::number((timing.samplePoint / 10.0f), 'f', 1) << "%)";
    return str;
}
