#include <QDateTime>
#include <QDebug>
#include <QThread>
#include <QSettings>
#include <QDataStream>
#include <QIODevice>

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
constexpr uint8_t PICOFLEX_SET_RECORD_FORMAT = 0xa0; //wValue = record format (1 stock, 2 device us, 3 + fine clock/seq)
constexpr uint8_t PICOFLEX_GET_TS_INFO = 0xa2;       //only the timestamped-capture firmware answers this (64-byte struct)
constexpr uint16_t RECORD_FORMAT_STOCK = 1;
constexpr uint16_t RECORD_FORMAT_V2 = 2;
constexpr uint16_t RECORD_FORMAT_V3 = 3;

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
    //so the config apply slots can be invoked across the worker-thread boundary (BlockingQueuedConnection)
    qRegisterMetaType<PicoFlexRay::InjectRule>("PicoFlexRay::InjectRule");
    qRegisterMetaType<PicoFlexRay::SourceSlot>("PicoFlexRay::SourceSlot");
    qRegisterMetaType<PicoFlexRay::SourceParams>("PicoFlexRay::SourceParams");
    qRegisterMetaType<QList<PicoFlexRay::InjectRule>>("QList<PicoFlexRay::InjectRule>");
    qRegisterMetaType<QList<PicoFlexRay::SourceSlot>>("QList<PicoFlexRay::SourceSlot>");

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
    mStatusTimer = new QTimer(this);
    mStatusTimer->setInterval(1000); //poll the injector status (0xa3) once a second while connected
    connect(mStatusTimer, &QTimer::timeout, this, &PicoFlexRay::pollInjectStatus);

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
    if (mStatusTimer) mStatusTimer->stop();
    closeDevice();
#ifdef HAVE_LIBUSB
    if (mContext) libusb_exit(mContext);
#endif
    mContext = nullptr;
    delete mReadTimer;
    mReadTimer = nullptr;
    delete mReconnectTimer;
    mReconnectTimer = nullptr;
    delete mStatusTimer;
    mStatusTimer = nullptr;
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

    //Ask for the richest record format the firmware offers: 3 (device timestamps + a sequence number that exposes
    //USB frame loss) on the timestamped-capture firmware, else 2 (device timestamps). The timestamped-capture build
    //is the only one that answers GET_TS_INFO, so probe that first and only request format 3 from firmware that
    //advertises it; that keeps format-2-only firmware on format 2 exactly as before. Stock firmware stalls both
    //requests and keeps sending format 1, which the parser also reads. The parser detects the format per record, so
    //whatever the device actually sends is handled regardless of what was granted here.
    const uint8_t vendorOut = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
    const uint8_t vendorIn = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
    unsigned char tsInfo[64] = {0};
    const bool tsCapable = libusb_control_transfer(mHandle, vendorIn, PICOFLEX_GET_TS_INFO, 0, 0,
                                                   tsInfo, sizeof(tsInfo), 500) > 0;
    const uint16_t wanted = tsCapable ? RECORD_FORMAT_V3 : RECORD_FORMAT_V2;
    rc = libusb_control_transfer(mHandle, vendorOut, PICOFLEX_SET_RECORD_FORMAT, wanted, 0, nullptr, 0, 500);
    mRecordFormat = (rc >= 0) ? wanted : RECORD_FORMAT_STOCK;
    switch (mRecordFormat) {
    case RECORD_FORMAT_V3:
        emit debugOutput("pico-flexray: firmware sends record format 3 (device timestamps + USB-loss sequence)"); break;
    case RECORD_FORMAT_V2:
        emit debugOutput("pico-flexray: firmware sends record format 2 (device timestamps)"); break;
    default:
        emit debugOutput("pico-flexray: stock firmware (record format 1): frames get host timestamps"); break;
    }

    //clear frames the device queued before we connected; firmware that doesn't know the request just stalls it
    libusb_control_transfer(mHandle, vendorOut, PANDA_RESET_CAN_COMMS, 0, 0, nullptr, 0, 500);
    mParser.reset();
    mReadBuffer.resize(READ_SIZE);
    applyInjectorEnable(); //the device powers up with the injector on; put it in the state the bus settings ask for
    //Only re-push rules/schedule if the user explicitly applied a config in THIS session. Merely connecting must
    //not clear or overwrite the device's boot defaults (or a free-running generator): a board that was happily
    //sourcing frames kept running when we leave its config alone. The editor still loads its last-saved config
    //from QSettings for editing; it reaches the device only when the user clicks an Apply button.
    if (mUserAppliedConfig) pushAllConfig();
    mLastStatusSummary.clear();
    if (mStatusTimer) mStatusTimer->start();
    pollInjectStatus();       //one read now so the editor shows device state immediately
    pollMode();               //read (never set) the device's current run mode so the editor shows it
    emit debugOutput("pico-flexray: connected");
    return true;
#else
    return false;
#endif
}

void PicoFlexRay::closeDevice()
{
#ifdef HAVE_LIBUSB
    if (mStatusTimer) mStatusTimer->stop();
    if (mHandle) {
        //leave the device in the original record format for other tools (it also goes back on its own when unplugged)
        if (mRecordFormat >= RECORD_FORMAT_V2)
            libusb_control_transfer(mHandle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
                                    PICOFLEX_SET_RECORD_FORMAT, RECORD_FORMAT_STOCK, 0, nullptr, 0, 200);
        libusb_release_interface(mHandle, 0);
        libusb_close(mHandle);
        emit debugOutput(QString("pico-flexray: closed. %1 frames (%2 null, %3 CRC errors, %4 with device time), "
                                 "%5 frames lost to USB in %6 gaps, %7 bytes skipped resyncing")
                             .arg(mParser.framesParsed()).arg(mParser.nullFrames()).arg(mParser.crcErrorFrames())
                             .arg(mParser.deviceTimeRecords()).arg(mParser.seqFramesLost()).arg(mParser.seqGaps())
                             .arg(mParser.bytesSkipped()));
    }
    mRecordFormat = RECORD_FORMAT_STOCK;
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
    const QByteArray content = frame.payload();            //the slot payload to splice in
    if (content.isEmpty())
        return fail("a FlexRay frame to send needs at least one payload byte");
    const quint32 id = frame.frameId();
    if (id > 0x7FFu)
        return fail(QString("FlexRay slot ID 0x%1 is out of range (0-0x7FF)").arg(id, 0, 16));

    const quint8 base = frame.flexRayCycle(); //first-class cycle count -> rule cycle_base
    return overridePayload(static_cast<quint16>(id), base, content);
}

QByteArray PicoFlexRay::overridePayload(quint16 slotId, quint8 cycleBase, const QByteArray &payload)
{
    if (slotId > 0x7FFu || payload.isEmpty()) return QByteArray();

    const quint8 crc = e2eCrc8(reinterpret_cast<const unsigned char *>(payload.constData()), payload.size(), 0xf1);

    //slice submitted to the firmware: [E2E CRC-8][slot payload]; the firmware checks the CRC, then carves the
    //rule's replace window out of this (so the payload length must match what the firmware rule expects)
    QByteArray slice;
    slice.append(static_cast<char>(crc));
    slice.append(payload);
    const quint16 sliceLen = static_cast<quint16>(slice.size());

    QByteArray out;
    out.append(static_cast<char>(VENDOR_OUT_OVERRIDE));
    out.append(static_cast<char>(slotId & 0xFF));
    out.append(static_cast<char>((slotId >> 8) & 0xFF));
    out.append(static_cast<char>(cycleBase));
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
                         .arg(frame.frameId(), 0, 16).arg(frame.flexRayCycle()).arg(out.size()));
    return true;
#else
    emit debugOutput("pico-flexray: built without libusb, can't send");
    return false;
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// Host-configurable injection rules + generator schedule (ops 0x92-0x96, status 0xa3).
//
// The byte-builders below are pure functions that mirror overridePayloadFor(): each returns exactly the bytes
// written to EP_BULK_OUT, so they can be checked byte-for-byte against the frozen USB contract. All multi-byte
// fields are little-endian.
// ---------------------------------------------------------------------------------------------------------------

namespace {
//append a little-endian unsigned 32-bit value
void appendU32LE(QByteArray &out, quint32 v)
{
    out.append(static_cast<char>(v & 0xFF));
    out.append(static_cast<char>((v >> 8) & 0xFF));
    out.append(static_cast<char>((v >> 16) & 0xFF));
    out.append(static_cast<char>((v >> 24) & 0xFF));
}
}

//0x92 SET_INJECT_RULE -> [0x92][slot_lo][slot_hi][cycle_base][cycle_mask][win_off][win_len] (7 bytes)
QByteArray PicoFlexRay::setInjectRulePayload(const InjectRule &rule)
{
    QByteArray out;
    out.append(static_cast<char>(VENDOR_OUT_SET_INJECT_RULE));
    out.append(static_cast<char>(rule.slotId & 0xFF));
    out.append(static_cast<char>((rule.slotId >> 8) & 0xFF));
    out.append(static_cast<char>(rule.cycleBase));
    out.append(static_cast<char>(rule.cycleMask));
    out.append(static_cast<char>(rule.winOffset));
    out.append(static_cast<char>(rule.winLength));
    return out;
}

//0x93 CLEAR_INJECT_RULES -> [0x93] (1 byte)
QByteArray PicoFlexRay::clearInjectRulesPayload()
{
    QByteArray out;
    out.append(static_cast<char>(VENDOR_OUT_CLEAR_INJECT_RULES));
    return out;
}

//0x94 SET_SOURCE_SLOT -> [0x94][slot_lo][slot_hi][chan_mask][len=N][flags][payload x N] (6+N bytes)
QByteArray PicoFlexRay::setSourceSlotPayload(const SourceSlot &slot)
{
    QByteArray payload = slot.payload;
    if (payload.size() > CommFrame::FLEXRAY_MAX_PAYLOAD) //clamp to a FlexRay payload; len field is one byte
        payload.truncate(CommFrame::FLEXRAY_MAX_PAYLOAD);
    const quint8 flags = slot.lastByteTracksCycle ? 0x01 : 0x00;

    QByteArray out;
    out.append(static_cast<char>(VENDOR_OUT_SET_SOURCE_SLOT));
    out.append(static_cast<char>(slot.slotId & 0xFF));
    out.append(static_cast<char>((slot.slotId >> 8) & 0xFF));
    out.append(static_cast<char>(slot.chanMask));
    out.append(static_cast<char>(payload.size() & 0xFF));
    out.append(static_cast<char>(flags));
    out.append(payload);
    return out;
}

//0x95 CLEAR_SOURCE_SCHEDULE -> [0x95] (1 byte)
QByteArray PicoFlexRay::clearSourceSchedulePayload()
{
    QByteArray out;
    out.append(static_cast<char>(VENDOR_OUT_CLEAR_SOURCE));
    return out;
}

//0x96 SET_SOURCE_PARAMS -> [0x96][period_us u32 LE][cycle_gap_us u32 LE][enable] (10 bytes)
QByteArray PicoFlexRay::setSourceParamsPayload(const SourceParams &params)
{
    QByteArray out;
    out.append(static_cast<char>(VENDOR_OUT_SET_SOURCE_PARAMS));
    appendU32LE(out, params.periodUs);
    appendU32LE(out, params.cycleGapUs);
    out.append(static_cast<char>(params.enable ? 1 : 0));
    return out;
}

//0xa3 GET_INJECT_STATUS reply: 16 bytes, little-endian:
// [0]='P' [1]='F' [2]=version=1 [3]=injector_enabled [4]=inject_rule_count [5]=source_slot_count
// [6]=source_running [7]=source_slot_drops [8..11]=u32 total_inject_matches [12..15]=u32 total_inject_drops
bool PicoFlexRay::parseInjectStatus(const QByteArray &reply, InjectStatus &out)
{
    out = InjectStatus();
    if (reply.size() != 16) return false;
    const unsigned char *b = reinterpret_cast<const unsigned char *>(reply.constData());
    if (b[0] != 0x50 || b[1] != 0x46) return false; //'P','F'
    if (b[2] != 1) return false;                     //version
    out.version = b[2];
    out.injectorEnabled = b[3] != 0;
    out.ruleCount = b[4];
    out.sourceSlotCount = b[5];
    out.sourceRunning = b[6] != 0;
    out.sourceSlotDrops = b[7]; //generator slots the device rejected (e.g. odd-length payload)
    out.totalMatches = static_cast<quint32>(b[8]) | (static_cast<quint32>(b[9]) << 8)
                       | (static_cast<quint32>(b[10]) << 16) | (static_cast<quint32>(b[11]) << 24);
    out.totalDrops = static_cast<quint32>(b[12]) | (static_cast<quint32>(b[13]) << 8)
                     | (static_cast<quint32>(b[14]) << 16) | (static_cast<quint32>(b[15]) << 24);
    out.valid = true;
    return true;
}

// --- QSettings (de)serialisation: a versioned blob, independent of the wire format ---
QByteArray PicoFlexRay::serialiseInjectRules(const QList<InjectRule> &rules)
{
    QByteArray blob;
    QDataStream s(&blob, QIODevice::WriteOnly);
    s.setVersion(QDataStream::Qt_6_5);
    s << static_cast<quint8>(1) << static_cast<quint32>(rules.size());
    for (const InjectRule &r : rules)
        s << r.slotId << r.cycleBase << r.cycleMask << r.winOffset << r.winLength;
    return blob;
}

QList<PicoFlexRay::InjectRule> PicoFlexRay::deserialiseInjectRules(const QByteArray &blob)
{
    QList<InjectRule> rules;
    if (blob.isEmpty()) return rules;
    QDataStream s(blob);
    s.setVersion(QDataStream::Qt_6_5);
    quint8 ver = 0; quint32 count = 0;
    s >> ver >> count;
    if (ver != 1) return rules;
    for (quint32 i = 0; i < count && s.status() == QDataStream::Ok; i++) {
        InjectRule r;
        s >> r.slotId >> r.cycleBase >> r.cycleMask >> r.winOffset >> r.winLength;
        if (s.status() == QDataStream::Ok) rules.append(r);
    }
    return rules;
}

QByteArray PicoFlexRay::serialiseSourceSlots(const QList<SourceSlot> &slotList)
{
    QByteArray blob;
    QDataStream s(&blob, QIODevice::WriteOnly);
    s.setVersion(QDataStream::Qt_6_5);
    s << static_cast<quint8>(1) << static_cast<quint32>(slotList.size());
    for (const SourceSlot &sl : slotList)
        s << sl.slotId << sl.chanMask << sl.lastByteTracksCycle << sl.payload;
    return blob;
}

QList<PicoFlexRay::SourceSlot> PicoFlexRay::deserialiseSourceSlots(const QByteArray &blob)
{
    QList<SourceSlot> slotList;
    if (blob.isEmpty()) return slotList;
    QDataStream s(blob);
    s.setVersion(QDataStream::Qt_6_5);
    quint8 ver = 0; quint32 count = 0;
    s >> ver >> count;
    if (ver != 1) return slotList;
    for (quint32 i = 0; i < count && s.status() == QDataStream::Ok; i++) {
        SourceSlot sl;
        s >> sl.slotId >> sl.chanMask >> sl.lastByteTracksCycle >> sl.payload;
        if (s.status() == QDataStream::Ok) slotList.append(sl);
    }
    return slotList;
}

bool PicoFlexRay::inWorkerThread()
{
    return thread() == QThread::currentThread();
}

QString PicoFlexRay::settingsGroup() const
{
    QString port = const_cast<PicoFlexRay *>(this)->getPort();
    if (port.isEmpty()) port = "default";
    port.replace('/', '_');
    return "PicoFlex/" + port;
}

void PicoFlexRay::saveConfigToSettings()
{
    QSettings settings;
    const QString base = settingsGroup();
    settings.setValue(base + "/injectRules", serialiseInjectRules(mInjectRules));
    settings.setValue(base + "/sourceSlots", serialiseSourceSlots(mSourceSlots));
    settings.setValue(base + "/srcPeriodUs", mSourceParams.periodUs);
    settings.setValue(base + "/srcCycleGapUs", mSourceParams.cycleGapUs);
    settings.setValue(base + "/srcEnable", mSourceParams.enable);
}

QStringList PicoFlexRay::describeConfig() const
{
    const QString label = getName();
    QString tag = label.isEmpty() ? const_cast<PicoFlexRay *>(this)->getPort() : label;
    if (tag.isEmpty()) tag = QStringLiteral("pico-flexray");

    QStringList lines;

    //run mode (live, cached from the 0xa5 GET_MODE reads)
    const int mode = lastKnownMode();
    lines.append(QString("%1: mode %2").arg(tag,
        mode < 0 ? QStringLiteral("unknown (not connected / firmware without mode support)") : modeName(mode)));

    //persisted injection rules + generator schedule (the same stored config the editor loads)
    QSettings settings;
    const QString base = settingsGroup();

    const QList<InjectRule> rules = deserialiseInjectRules(settings.value(base + "/injectRules").toByteArray());
    for (const InjectRule &r : rules)
        lines.append(QString("%1: inject rule slot 0x%2 cycle %3 mask 0x%4 window off %5 len %6")
                         .arg(tag)
                         .arg(r.slotId, 3, 16, QLatin1Char('0'))
                         .arg(r.cycleBase)
                         .arg(r.cycleMask, 2, 16, QLatin1Char('0'))
                         .arg(r.winOffset).arg(r.winLength));

    const QList<SourceSlot> srcSlots = deserialiseSourceSlots(settings.value(base + "/sourceSlots").toByteArray());
    for (const SourceSlot &s : srcSlots)
    {
        QStringList chans;
        if (s.chanMask & 0x1) chans << QStringLiteral("FR1");
        if (s.chanMask & 0x2) chans << QStringLiteral("FR2");
        QString line = QString("%1: source slot 0x%2 ch %3 payload %4")
                           .arg(tag)
                           .arg(s.slotId, 3, 16, QLatin1Char('0'))
                           .arg(chans.isEmpty() ? QStringLiteral("none") : chans.join('+'),
                                s.payload.isEmpty() ? QStringLiteral("(none)")
                                                    : QString::fromLatin1(s.payload.toHex(' ')));
        if (s.lastByteTracksCycle) line += QStringLiteral(", last byte tracks cycle");
        lines.append(line);
    }

    if (!srcSlots.isEmpty())
    {
        const quint32 periodUs = settings.value(base + "/srcPeriodUs").toUInt();
        const quint32 gapUs = settings.value(base + "/srcCycleGapUs").toUInt();
        const bool running = settings.value(base + "/srcEnable").toBool();
        lines.append(QString("%1: generator period %2us gap %3us %4")
                         .arg(tag).arg(periodUs).arg(gapUs).arg(running ? QStringLiteral("running")
                                                                        : QStringLiteral("stopped")));
    }

    return lines;
}

void PicoFlexRay::pushInjectRules()
{
#ifdef HAVE_LIBUSB
    if (!mHandle) return;
    vendorWrite(clearInjectRulesPayload(), "clear inject rules");
    for (const InjectRule &r : mInjectRules)
        vendorWrite(setInjectRulePayload(r), "set inject rule");
    emit debugOutput(QString("pico-flexray: pushed %1 injection rule(s) to the device").arg(mInjectRules.size()));
#endif
}

void PicoFlexRay::pushSourceSchedule()
{
#ifdef HAVE_LIBUSB
    if (!mHandle) return;
    vendorWrite(clearSourceSchedulePayload(), "clear source schedule");
    for (const SourceSlot &sl : mSourceSlots)
        vendorWrite(setSourceSlotPayload(sl), "set source slot");
    vendorWrite(setSourceParamsPayload(mSourceParams), "set source params");
    emit debugOutput(QString("pico-flexray: pushed %1 generator slot(s), period %2 us, gap %3 us, %4")
                         .arg(mSourceSlots.size()).arg(mSourceParams.periodUs).arg(mSourceParams.cycleGapUs)
                         .arg(mSourceParams.enable ? "running" : "stopped"));
#endif
}

void PicoFlexRay::pushAllConfig()
{
#ifdef HAVE_LIBUSB
    if (!mHandle) return;
    //only touch the device tables we actually have config for, so firmware defaults are left alone otherwise
    if (!mInjectRules.isEmpty()) pushInjectRules();
    if (!mSourceSlots.isEmpty() || mSourceParams.enable || mSourceParams.periodUs != 0)
        pushSourceSchedule();
#endif
}

void PicoFlexRay::applyInjectRules(QList<PicoFlexRay::InjectRule> rules)
{
    if (!inWorkerThread()) {
        QMetaObject::invokeMethod(this, "applyInjectRules", Qt::BlockingQueuedConnection,
                                  Q_ARG(QList<PicoFlexRay::InjectRule>, rules));
        return;
    }
    mInjectRules = rules;
    mUserAppliedConfig = true; //an explicit Apply: it's now safe to re-push on reconnect
    saveConfigToSettings();
    pushInjectRules();
}

void PicoFlexRay::clearInjectRules()
{
    if (!inWorkerThread()) {
        QMetaObject::invokeMethod(this, "clearInjectRules", Qt::BlockingQueuedConnection);
        return;
    }
    mInjectRules.clear();
    saveConfigToSettings();
#ifdef HAVE_LIBUSB
    if (mHandle) vendorWrite(clearInjectRulesPayload(), "clear inject rules");
#endif
}

void PicoFlexRay::applySourceSchedule(QList<PicoFlexRay::SourceSlot> slotList, PicoFlexRay::SourceParams params)
{
    if (!inWorkerThread()) {
        QMetaObject::invokeMethod(this, "applySourceSchedule", Qt::BlockingQueuedConnection,
                                  Q_ARG(QList<PicoFlexRay::SourceSlot>, slotList),
                                  Q_ARG(PicoFlexRay::SourceParams, params));
        return;
    }
    mSourceSlots = slotList;
    mSourceParams = params;
    mUserAppliedConfig = true; //an explicit Apply: it's now safe to re-push on reconnect
    saveConfigToSettings();
    pushSourceSchedule();
}

void PicoFlexRay::clearSourceSchedule()
{
    if (!inWorkerThread()) {
        QMetaObject::invokeMethod(this, "clearSourceSchedule", Qt::BlockingQueuedConnection);
        return;
    }
    mSourceSlots.clear();
    saveConfigToSettings();
#ifdef HAVE_LIBUSB
    if (mHandle) vendorWrite(clearSourceSchedulePayload(), "clear source schedule");
#endif
}

void PicoFlexRay::applySourceParams(PicoFlexRay::SourceParams params)
{
    if (!inWorkerThread()) {
        QMetaObject::invokeMethod(this, "applySourceParams", Qt::BlockingQueuedConnection,
                                  Q_ARG(PicoFlexRay::SourceParams, params));
        return;
    }
    mSourceParams = params;
    mUserAppliedConfig = true; //an explicit Apply: it's now safe to re-push on reconnect
    saveConfigToSettings();
#ifdef HAVE_LIBUSB
    if (mHandle) vendorWrite(setSourceParamsPayload(mSourceParams), "set source params");
#endif
}

void PicoFlexRay::sendOverride(int slotId, int cycleBase, QByteArray payload)
{
    if (!inWorkerThread()) {
        QMetaObject::invokeMethod(this, "sendOverride", Qt::BlockingQueuedConnection,
                                  Q_ARG(int, slotId), Q_ARG(int, cycleBase), Q_ARG(QByteArray, payload));
        return;
    }
    if (slotId < 0 || slotId > 0x7FF || payload.isEmpty()) {
        emit debugOutput("pico-flexray: can't send override (slot ID out of range or empty payload)");
        return;
    }
#ifdef HAVE_LIBUSB
    if (!mHandle) { emit debugOutput("pico-flexray: not connected, can't send override"); return; }
    if (!mInjectorEnabled)
        emit debugOutput("pico-flexray: note - the injector is off (bus is listen-only); this override will be "
                         "dropped by the device until you un-tick listen-only to enable it");
    const QByteArray out = overridePayload(static_cast<quint16>(slotId), static_cast<quint8>(cycleBase), payload);
    if (out.isEmpty() || !vendorWrite(out, "override")) return;
    //op 0x90 has no device ACK: it only reaches the bus if the firmware has a matching injection rule for this slot
    //and cycle base. The 0xa3 counters tell the story - matches climb on a hit, drops climb on a miss.
    emit debugOutput(QString("pico-flexray: sent override for slot 0x%1, cycle-base %2 (%3 payload bytes)")
                         .arg(slotId, 0, 16).arg(cycleBase).arg(payload.size()));
    pollInjectStatus(); //refresh the 0xa3 match/drop counters so the editor shows the result
#else
    (void)slotId; (void)cycleBase; (void)payload;
#endif
}

void PicoFlexRay::requestInjectStatus()
{
    if (!inWorkerThread()) {
        QMetaObject::invokeMethod(this, "requestInjectStatus", Qt::QueuedConnection);
        return;
    }
    pollInjectStatus();
}

void PicoFlexRay::pollInjectStatus()
{
#ifdef HAVE_LIBUSB
    if (!mHandle) return;
    const uint8_t vendorIn = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
    unsigned char buf[16] = {0};
    int rc = libusb_control_transfer(mHandle, vendorIn, PICOFLEX_GET_INJECT_STATUS, 0, 0,
                                     buf, sizeof(buf), 200);
    if (rc < 16) return; //firmware without the status request stalls it; nothing to report

    QByteArray reply(reinterpret_cast<const char *>(buf), 16);
    InjectStatus st;
    if (!parseInjectStatus(reply, st)) return;

    emit injectStatus(reply); //for the config editor

    //surface a summary in the debug log, but only when it changes so the log isn't spammed at 1 Hz
    const QString summary = QString("pico-flexray inject status: injector %1, %2 rule(s), %3 source slot(s), "
                                    "source %4, %5 source drops, %6 matches, %7 drops")
                                .arg(st.injectorEnabled ? "on" : "off").arg(st.ruleCount).arg(st.sourceSlotCount)
                                .arg(st.sourceRunning ? "running" : "stopped").arg(st.sourceSlotDrops)
                                .arg(st.totalMatches).arg(st.totalDrops);
    if (summary != mLastStatusSummary) {
        emit debugOutput(summary);
        mLastStatusSummary = summary;
    }
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// Runtime mode selector (0xa4 SET_MODE control OUT, 0xa5 GET_MODE control IN). Frozen contract, identical to the
// unified firmware; modelled on the existing 0xa2 GET_TS_INFO / 0xa3 status control transfers.
// ---------------------------------------------------------------------------------------------------------------

QString PicoFlexRay::modeName(int mode)
{
    switch (mode) {
    case MODE_SNIFF:  return "Sniff";
    case MODE_BRIDGE: return "Bridge";
    case MODE_INJECT: return "Inject";
    case MODE_SOURCE: return "Source";
    default:          return "unknown";
    }
}

void PicoFlexRay::setMode(int mode)
{
    if (!inWorkerThread()) {
        QMetaObject::invokeMethod(this, "setMode", Qt::BlockingQueuedConnection, Q_ARG(int, mode));
        return;
    }
    if (!isValidMode(mode)) {
        emit debugOutput(QString("pico-flexray: ignoring invalid mode %1").arg(mode));
        return;
    }
#ifdef HAVE_LIBUSB
    if (!mHandle) { emit debugOutput("pico-flexray: not connected, can't set mode"); return; }
    //0xa4 SET_MODE: vendor control OUT, wValue = mode (0..3), no data stage
    const uint8_t vendorOut = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
    int rc = libusb_control_transfer(mHandle, vendorOut, PICOFLEX_SET_MODE,
                                     static_cast<uint16_t>(mode), 0, nullptr, 0, WRITE_TIMEOUT_MS);
    if (rc < 0) {
        emit debugOutput(QString("pico-flexray: SET_MODE(%1) failed (%2)").arg(modeName(mode)).arg(libusb_error_name(rc)));
        return;
    }
    emit debugOutput(QString("pico-flexray: set mode to %1").arg(modeName(mode)));
    pollMode(); //read it back so the editor reflects the device's actual mode
#else
    (void)mode;
#endif
}

void PicoFlexRay::requestMode()
{
    if (!inWorkerThread()) {
        QMetaObject::invokeMethod(this, "requestMode", Qt::QueuedConnection);
        return;
    }
    pollMode();
}

void PicoFlexRay::pollMode()
{
#ifdef HAVE_LIBUSB
    if (!mHandle) return;
    //0xa5 GET_MODE: vendor control IN, returns one byte = active mode. Firmware without it stalls the request.
    const uint8_t vendorIn = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
    unsigned char buf[1] = {0};
    int rc = libusb_control_transfer(mHandle, vendorIn, PICOFLEX_GET_MODE, 0, 0, buf, sizeof(buf), 200);
    if (rc < 1) { mLastKnownMode.storeRelaxed(-1); emit deviceMode(-1); return; } //unsupported / no reply
    const int mode = buf[0];
    const int resolved = isValidMode(mode) ? mode : -1;
    mLastKnownMode.storeRelaxed(resolved);
    emit deviceMode(resolved);
#else
    mLastKnownMode.storeRelaxed(-1);
    emit deviceMode(-1);
#endif
}
