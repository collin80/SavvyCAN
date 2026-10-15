#include <QDateTime>
#include <QDebug>

#include "canconmanager.h"
#include "picoflexray.h"

#ifdef HAVE_LIBUSB
#include <libusb.h>
#endif

namespace {
constexpr unsigned char EP_BULK_IN = 0x81;
constexpr unsigned char EP_BULK_OUT = 0x03; //vendor OUT endpoint: injector override (0x90) and enable (0x91) ops
constexpr int READ_SIZE = 16384;        //bulk IN transfer size; the device ends each transfer with a short packet
constexpr unsigned int READ_TIMEOUT_MS = 10; //keeps the connection thread responsive while the bus is quiet
constexpr unsigned int WRITE_TIMEOUT_MS = 500; //injector writes are tiny; this only bounds a stuck transfer
constexpr int NUM_BUSES = 2;            //FR1, FR2
constexpr int FLEXRAY_BITRATE = 10000000;
constexpr uint8_t PANDA_RESET_CAN_COMMS = 0xc0; //pico-flexray: clears its frame FIFO
constexpr uint8_t PICOFLEX_SET_RECORD_FORMAT = 0xa0; //wValue = record format (1 stock, 2 with device timestamps)
constexpr uint16_t RECORD_FORMAT_STOCK = 1;
constexpr uint16_t RECORD_FORMAT_V2 = 2;

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

    //ask for record format 2 (device timestamps, CRC flags, CRC-failed frames). Stock firmware stalls the request
    //and keeps sending format 1, which the parser reads too.
    const uint8_t vendorOut = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
    rc = libusb_control_transfer(mHandle, vendorOut, PICOFLEX_SET_RECORD_FORMAT, RECORD_FORMAT_V2, 0, nullptr, 0, 500);
    mRecordFormat2 = (rc >= 0);
    emit debugOutput(mRecordFormat2 ? "pico-flexray: firmware sends record format 2 (device timestamps)"
                                    : "pico-flexray: stock firmware (record format 1): frames get host timestamps");

    //clear frames the device queued before we connected; firmware that doesn't know the request just stalls it
    libusb_control_transfer(mHandle, vendorOut, PANDA_RESET_CAN_COMMS, 0, 0, nullptr, 0, 500);
    mParser.reset();
    mReadBuffer.resize(READ_SIZE);
    applyInjectorEnable(); //the device powers up with the injector on; put it in the state the bus settings ask for
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
        //leave the device in the original record format for other tools (it also goes back on its own when unplugged)
        if (mRecordFormat2)
            libusb_control_transfer(mHandle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
                                    PICOFLEX_SET_RECORD_FORMAT, RECORD_FORMAT_STOCK, 0, nullptr, 0, 200);
        libusb_release_interface(mHandle, 0);
        libusb_close(mHandle);
        emit debugOutput(QString("pico-flexray: closed. %1 frames (%2 null, %3 CRC errors, %4 with device time), "
                                 "%5 bytes skipped resyncing")
                             .arg(mParser.framesParsed()).arg(mParser.nullFrames()).arg(mParser.crcErrorFrames())
                             .arg(mParser.format2Records()).arg(mParser.bytesSkipped()));
    }
    mRecordFormat2 = false;
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

    //when this transfer arrived, on the same clocks as QtSerialBus devices (the session's elapsed time, or the system
    //time if that's chosen): the timestamp of record format 1 frames, and what format 2 device times are mapped onto
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
    bus.setSpeed(FLEXRAY_BITRATE); //the device's FlexRay bitrate is fixed
    //listen-only is honoured now: taking a bus out of listen-only turns the firmware injector on (see
    //applyInjectorEnable), which is what lets piSendFrame submit payload overrides
    setBusConfig(pBusIdx, bus);
    applyInjectorEnable();
}

void PicoFlexRay::applyInjectorEnable()
{
    bool enable = false;
    for (int i = 0; i < NUM_BUSES; i++) {
        CANBus bus;
        if (getBusConfig(i, bus) && bus.isActive() && !bus.isListenOnly()) { enable = true; break; }
    }
    mInjectorEnabled = enable;
#ifdef HAVE_LIBUSB
    //push the state whenever we have a device: this both syncs on (re)connect and follows later setting changes
    if (mHandle)
        vendorWrite(injectorEnablePayload(enable), enable ? "enable injector" : "disable injector");
#endif
}

bool PicoFlexRay::vendorWrite(const QByteArray &data, const char *what)
{
#ifdef HAVE_LIBUSB
    if (!mHandle) return false;
    int transferred = 0;
    int rc = libusb_bulk_transfer(mHandle, EP_BULK_OUT,
                                  reinterpret_cast<unsigned char *>(const_cast<char *>(data.constData())),
                                  data.size(), &transferred, WRITE_TIMEOUT_MS);
    if (rc != 0 || transferred != data.size()) {
        emit debugOutput(QString("pico-flexray: USB write (%1) failed (%2, %3/%4 bytes)")
                             .arg(what).arg(libusb_error_name(rc)).arg(transferred).arg(data.size()));
        return false;
    }
    return true;
#else
    (void)data; (void)what;
    return false;
#endif
}

quint8 PicoFlexRay::e2eCrc8(const unsigned char *data, int len, quint8 init)
{
    quint8 crc = init;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x80) ? static_cast<quint8>((crc << 1) ^ 0x1D) : static_cast<quint8>(crc << 1);
    }
    return crc;
}

QByteArray PicoFlexRay::overridePayloadFor(const CommFrame &frame, QString *error)
{
    auto fail = [&](const QString &msg) -> QByteArray { if (error) *error = msg; return QByteArray(); };

    if (!frame.isFlexRay())
        return fail("only FlexRay frames can be sent to a pico-flexray device");
    const QByteArray payload = frame.payload();
    if (payload.size() < 2)
        return fail("a FlexRay frame to send needs a cycle-count byte plus at least one payload byte");
    const quint32 id = frame.frameId();
    if (id > 0x7FFu)
        return fail(QString("FlexRay slot ID 0x%1 is out of range (0-0x7FF)").arg(id, 0, 16));

    const quint8 base = static_cast<quint8>(payload.at(0)); //cycle-count byte -> rule cycle_base
    const QByteArray content = payload.mid(1);              //the slot payload to splice in
    const quint8 crc = e2eCrc8(reinterpret_cast<const unsigned char *>(content.constData()), content.size(), 0xf1);

    //slice submitted to the firmware: [E2E CRC-8][slot payload]; the firmware checks the CRC, then carves the
    //rule's replace window out of this (so the payload length must match what the firmware rule expects)
    QByteArray slice;
    slice.append(static_cast<char>(crc));
    slice.append(content);
    const quint16 sliceLen = static_cast<quint16>(slice.size());

    QByteArray out;
    out.append(static_cast<char>(VENDOR_OUT_OVERRIDE));
    out.append(static_cast<char>(id & 0xFF));
    out.append(static_cast<char>((id >> 8) & 0xFF));
    out.append(static_cast<char>(base));
    out.append(static_cast<char>(sliceLen & 0xFF));
    out.append(static_cast<char>((sliceLen >> 8) & 0xFF));
    out.append(slice);
    return out;
}

QByteArray PicoFlexRay::injectorEnablePayload(bool enabled)
{
    QByteArray out;
    out.append(static_cast<char>(VENDOR_OUT_ENABLE));
    out.append(static_cast<char>(enabled ? 1 : 0));
    return out;
}

bool PicoFlexRay::piSendFrame(const CommFrame &frame)
{
    QString error;
    const QByteArray out = overridePayloadFor(frame, &error);
    if (out.isEmpty()) {
        emit debugOutput("pico-flexray: " + error);
        return false;
    }
    if (!mInjectorEnabled) {
        emit debugOutput("pico-flexray: the injector is off; un-tick listen-only on the bus to enable sending");
        return false;
    }
#ifdef HAVE_LIBUSB
    if (!mHandle) {
        emit debugOutput("pico-flexray: not connected, can't send");
        return false;
    }
    if (!vendorWrite(out, "override")) return false;
    //op 0x90 has no device acknowledgement: the firmware silently drops a slice whose slot ID / cycle base don't
    //match an injection rule (src/flexray_injector_rules.h), so a successful write does not confirm a rule exists
    emit debugOutput(QString("pico-flexray: submitted override for slot 0x%1, cycle-base %2 (%3 bytes); it only "
                             "reaches the bus if the firmware has an injection rule for that slot")
                         .arg(frame.frameId(), 0, 16).arg(static_cast<quint8>(frame.payload().at(0))).arg(out.size()));
    return true;
#else
    emit debugOutput("pico-flexray: built without libusb, can't send");
    return false;
#endif
}
