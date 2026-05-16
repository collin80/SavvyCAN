/*
* gs_usb_reader.cpp
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

#include "gs_usb_reader.h"
#include "gs_usb_definitions.h"
#include "gs_usb_functions.h"
#include <QObject>
#include <QThread>

#define GS_USB_READER_BLOCK_TIME_MS  (100U)


GSUSBReader::GSUSBReader(QObject *parent, candle_handle deviceHandle) : QObject(parent), deviceHandle(deviceHandle)
{
    this->thread = new QThread();
}

GSUSBReader::~GSUSBReader()
{
    stopReading();
    if (this->thread != NULL) {
        delete this->thread;
    }
}

void GSUSBReader::run()
{
    // NOTE: In the future, maybe we could add a list here to reduce the number of signals being fired
    candle_frame_t frame;
    while (!QThread::currentThread()->isInterruptionRequested()) {
        if (candle_frame_read(this->deviceHandle, &frame, GS_USB_READER_BLOCK_TIME_MS)) {
            if (candle_frame_type(&frame) == CANDLE_FRAMETYPE_RECEIVE) {
                emit newFrame(frame);
            }
        }
    }
    this->thread->quit();
}

void GSUSBReader::stopReading()
{
    if ((this->thread != NULL) && (this->thread->isRunning())) {
        this->thread->requestInterruption();
        this->thread->wait();
    }
}

void GSUSBReader::startReading()
{
    moveToThread(this->thread);
    connect(this->thread, SIGNAL(started()), this, SLOT(run()));
    this->thread->start(QThread::HighPriority);
}
