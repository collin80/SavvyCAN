#include <QDateTime>
#include <QDebug>

#include "canconmanager.h"
#include "picoflexray.h"

#ifdef HAVE_LIBUSB
#include <libusb.h>
#endif

namespace {
constexpr unsigned char EP_BULK_IN = 0x81;
constexpr int READ_SIZE = 16384;        //bulk IN transfer size; the device ends each transfer with a short packet
constexpr unsigned int READ_TIMEOUT_MS = 10; //keeps the connection thread responsive while the bus is quiet
constexpr int NUM_BUSES = 2;            //FR1, FR2
constexpr int FLEXRAY_BITRATE = 10000000;
constexpr uint8_t PANDA_RESET_CAN_COMMS = 0xc0; //pico-flexray: clears its frame FIFO

#ifdef HAVE_LIBUSB
QString serialNumberOf(libusb_device_handle *handle, const libusb_device_descriptor &desc)
{
    unsigned char serial[64] = {0};
    if (desc.iSerialNumber == 0) return QString();
    int len = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, serial, sizeof(serial) - 1);
    if (len <= 0) return QString();
    return QString::fromLatin1(reinterpret_cast<const char *>(serial), len);
}

//Opens the first pico-flexray device whose serial number matches (any pico-flexray device if wantedSerial is
//empty), or lists the serial numbers of all of them when found is given. A Panda shares the VID/PID, so the
//serial number prefix is what picks out pico-flexray.
libusb_device_handle *findDevice(libusb_context *ctx, const QString &wantedSerial, QStringList *found, QString *error)
{
    libusb_device **list = nullptr;
    ssize_t count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        if (error) *error = QString("libusb: can't list USB devices (%1)").arg(libusb_error_name((int)count));
        return nullptr;
    }

    libusb_device_handle *result = nullptr;
    int openError = 0;
    for (ssize_t i = 0; i < count && !result; i++)
    {
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(list[i], &desc) != 0) continue;
        if (desc.idVendor != PicoFlexRay::USB_VID || desc.idProduct != PicoFlexRay::USB_PID) continue;

        libusb_device_handle *handle = nullptr;
        int rc = libusb_open(list[i], &handle);
        if (rc != 0) {
            openError = rc;
            continue;
        }
        QString serial = serialNumberOf(handle, desc);
        bool isPicoFlexRay = serial.startsWith(PicoFlexRay::SERIAL_PREFIX);
        if (found && isPicoFlexRay) found->append(serial);
        if (!found && isPicoFlexRay && (wantedSerial.isEmpty() || serial == wantedSerial)) result = handle;
        else libusb_close(handle);
    }
    libusb_free_device_list(list, 1);

    if (!result && error) {
        if (openError == LIBUSB_ERROR_NOT_SUPPORTED || openError == LIBUSB_ERROR_ACCESS)
            *error = QString("found a device with VID 0x3801/PID 0xddcc but can't open it (%1). On Windows, install "
                             "the WinUSB driver for it with Zadig; on Linux, check the udev permissions.")
                         .arg(libusb_error_name(openError));
        else if (wantedSerial.isEmpty()) *error = "no pico-flexray device found";
        else *error = QString("pico-flexray device %1 not found").arg(wantedSerial);
    }
    return result;
}
#endif
}

PicoFlexRay::PicoFlexRay(QString serialNumber) :
    CANConnection(serialNumber, "pico-flexray", CANCon::PICOFLEXRAY, 0, FLEXRAY_BITRATE, false, 0, NUM_BUSES, 16000, true,
                  true /* listen only */, true)
{
    CANBus bus;
    bus.setActive(true);
    bus.setListenOnly(true);
    bus.setSpeed(FLEXRAY_BITRATE);
    for (int i = 0; i < NUM_BUSES; i++) setBusConfig(i, bus);
}

PicoFlexRay::~PicoFlexRay()
{
    stop();
}

bool PicoFlexRay::isAvailable()
{
#ifdef HAVE_LIBUSB
    return true;
#else
    return false;
#endif
}

QStringList PicoFlexRay::availableDevices()
{
    QStringList serials;
#ifdef HAVE_LIBUSB
    libusb_context *ctx = nullptr;
    if (libusb_init(&ctx) != 0) return serials;
    findDevice(ctx, QString(), &serials, nullptr);
    libusb_exit(ctx);
#endif
    return serials;
}

void PicoFlexRay::piStarted()
{
    //created here, in the connection's thread
    mReadTimer = new QTimer(this);
    mReadTimer->setInterval(0);
    connect(mReadTimer, &QTimer::timeout, this, &PicoFlexRay::readDevice);
    mReconnectTimer = new QTimer(this);
    mReconnectTimer->setInterval(1000);
    connect(mReconnectTimer, &QTimer::timeout, this, &PicoFlexRay::tryConnect);

#ifdef HAVE_LIBUSB
    int rc = libusb_init(&mContext);
    if (rc != 0) {
        mContext = nullptr;
        emit debugOutput(QString("pico-flexray: libusb_init failed (%1)").arg(libusb_error_name(rc)));
        reportStatus();
        return;
    }
    tryConnect();
#else
    emit debugOutput("pico-flexray: this SavvyCAN was built without libusb, so pico-flexray devices aren't supported");
    reportStatus();
#endif
}

void PicoFlexRay::piStop()
{
    if (mReadTimer) mReadTimer->stop();
    if (mReconnectTimer) mReconnectTimer->stop();
    closeDevice();
#ifdef HAVE_LIBUSB
    if (mContext) libusb_exit(mContext);
#endif
    mContext = nullptr;
    delete mReadTimer;
    mReadTimer = nullptr;
    delete mReconnectTimer;
    mReconnectTimer = nullptr;
}

void PicoFlexRay::tryConnect()
{
    if (mHandle) return;
    if (openDevice()) {
        if (mReconnectTimer) mReconnectTimer->stop();
        if (mReadTimer) mReadTimer->start();
    }
    else if (mReconnectTimer && !mReconnectTimer->isActive()) mReconnectTimer->start(); //keep looking
    reportStatus();
}

bool PicoFlexRay::openDevice()
{
#ifdef HAVE_LIBUSB
    if (!mContext) return false;
    QString error;
    mHandle = findDevice(mContext, getPort(), nullptr, &error);
    if (!mHandle) {
        if (error != mLastError) emit debugOutput("pico-flexray: " + error); //once, not on every retry
        mLastError = error;
        return false;
    }
    mLastError.clear();

    libusb_set_auto_detach_kernel_driver(mHandle, 1); //Linux; harmless elsewhere
    int rc = libusb_claim_interface(mHandle, 0);
    if (rc != 0) {
        emit debugOutput(QString("pico-flexray: can't claim the USB interface (%1)").arg(libusb_error_name(rc)));
        libusb_close(mHandle);
        mHandle = nullptr;
        return false;
    }

    //clear frames the device queued before we connected; firmware that doesn't know the request just stalls it
    libusb_control_transfer(mHandle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
                            PANDA_RESET_CAN_COMMS, 0, 0, nullptr, 0, 500);
    mParser.reset();
    mReadBuffer.resize(READ_SIZE);
    emit debugOutput("pico-flexray: connected");
    return true;
#else
    return false;
#endif
}

void PicoFlexRay::closeDevice()
{
#ifdef HAVE_LIBUSB
    if (mHandle) {
        libusb_release_interface(mHandle, 0);
        libusb_close(mHandle);
        emit debugOutput(QString("pico-flexray: closed. %1 frames, %2 null frames dropped, %3 bytes skipped resyncing")
                             .arg(mParser.framesParsed()).arg(mParser.nullFrames()).arg(mParser.bytesSkipped()));
    }
#endif
    mHandle = nullptr;
}

void PicoFlexRay::readDevice()
{
#ifdef HAVE_LIBUSB
    if (!mHandle) return;

    int transferred = 0;
    int rc = libusb_bulk_transfer(mHandle, EP_BULK_IN, reinterpret_cast<unsigned char *>(mReadBuffer.data()),
                                  READ_SIZE, &transferred, READ_TIMEOUT_MS);
    if (rc != 0 && rc != LIBUSB_ERROR_TIMEOUT && rc != LIBUSB_ERROR_OVERFLOW) {
        emit debugOutput(QString("pico-flexray: USB read failed (%1), reconnecting").arg(libusb_error_name(rc)));
        mReadTimer->stop();
        closeDevice();
        reportStatus();
        mReconnectTimer->start();
        return;
    }
    if (transferred <= 0 || isCapSuspended()) return;

    //the device sends no timestamps: use the time of this transfer, on the same clocks as QtSerialBus devices
    //without hardware timestamps (the session's elapsed time, or the system time if that's chosen)
    CommFrame::TimeStamp ts = useSystemTime
        ? CommFrame::TimeStamp::fromMicroSeconds(QDateTime::currentMSecsSinceEpoch() * 1000ll)
        : CommFrame::TimeStamp(0, CANConManager::getInstance()->getElapsedUs());
    mFrames.clear();
    mParser.feed(mReadBuffer.constData(), transferred, ts, mFrames);
    for (CommFrame &frame : mFrames)
    {
        CANBus bus;
        if (getBusConfig(frame.getBus(), bus) && !bus.isActive()) continue;

        CommFrame *frame_p = getQueue().get();
        if (!frame_p) {
            qDebug() << "pico-flexray: frame queue full, dropping frames";
            break;
        }
        *frame_p = frame;
        checkTargettedFrame(*frame_p);
        getQueue().queue();
    }
#endif
}

void PicoFlexRay::reportStatus()
{
    CANCon::status newStatus = mHandle ? CANCon::CONNECTED : CANCon::NOT_CONNECTED;
    if (newStatus == getStatus()) return;
    setStatus(newStatus);
    CANConStatus stats;
    stats.conStatus = newStatus;
    stats.numHardwareBuses = NUM_BUSES;
    emit status(stats);
}

void PicoFlexRay::piSuspend(bool pSuspend)
{
    setCapSuspended(pSuspend);
    if (isCapSuspended()) getQueue().flush();
}

bool PicoFlexRay::piGetBusSettings(int pBusIdx, CANBus& pBus)
{
    return getBusConfig(pBusIdx, pBus);
}

void PicoFlexRay::piSetBusSettings(int pBusIdx, CANBus bus)
{
    if ((pBusIdx < 0) || pBusIdx >= getNumBuses()) return;
    bus.setListenOnly(true); //capture only
    bus.setSpeed(FLEXRAY_BITRATE);
    setBusConfig(pBusIdx, bus);
}

bool PicoFlexRay::piSendFrame(const CommFrame &)
{
    //pico-flexray can only replace the payload of slots set up in its firmware; sending isn't supported yet
    return false;
}
