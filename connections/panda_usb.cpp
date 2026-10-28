#ifdef WITH_PANDA

#include <QDebug>
#include <QSettings>
#include <QCanBusFrame>
#include "panda_usb.h"

// USB bulk endpoints
#define PANDA_EP_RX  0x81
#define PANDA_EP_TX  0x03

// Control transfer request codes
#define PANDA_REQ_RESET_COMMS  0xC0
#define PANDA_REQ_SET_SAFETY   0xDC
#define PANDA_REQ_SET_SPEED    0xDE
#define PANDA_REQ_HEARTBEAT    0xF3

// Safety modes
#define PANDA_SAFETY_NOOUTPUT   0x0000
#define PANDA_SAFETY_ALLOUTPUT  0x0001

#define PANDA_NUM_BUSES     3
#define PANDA_TX_TIMEOUT_MS 10
#define PANDA_RX_TIMEOUT_MS 5

// libusb bmRequestType constants
#define USB_CTRL_OUT  0x40   // host-to-device | vendor | device
#define USB_CTRL_IN   0xC0   // device-to-host | vendor | device

const int PandaUSB::dlcToLen[16] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};

PandaUSB::PandaUSB(QString portName, int busSpeed) :
    CANConnection(portName, "PANDA", CANCon::PANDA, 0, busSpeed, false, 0,
                  PANDA_NUM_BUSES, 4000, true),
    mHeartbeatTimer(this),
    mRxTimer(this)
{
    sendDebug("PandaUSB()");
}

PandaUSB::~PandaUSB()
{
    stop();
    sendDebug("~PandaUSB()");
}

void PandaUSB::sendDebug(const QString& text)
{
    qDebug() << text;
    emit debugOutput(text);
}

bool PandaUSB::isPandaVidPid(uint16_t vid, uint16_t pid)
{
    return (vid == VID1 && pid == PID1) || (vid == VID2 && pid == PID2);
}

QStringList PandaUSB::connected()
{
    QStringList result;
    libusb_context* ctx = nullptr;
    if (libusb_init(&ctx) != LIBUSB_SUCCESS)
        return result;

    libusb_device** devs = nullptr;
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt >= 0) {
        for (ssize_t i = 0; i < cnt; i++) {
            struct libusb_device_descriptor desc;
            if (libusb_get_device_descriptor(devs[i], &desc) != LIBUSB_SUCCESS)
                continue;
            if (!isPandaVidPid(desc.idVendor, desc.idProduct))
                continue;
            libusb_device_handle* h = nullptr;
            if (libusb_open(devs[i], &h) != LIBUSB_SUCCESS) {
                result.append("panda (no access - check udev rules)");
                continue;
            }
            unsigned char serial[64] = {};
            libusb_get_string_descriptor_ascii(h, desc.iSerialNumber,
                                               serial, sizeof(serial));
            libusb_close(h);
            QString s = QString::fromLatin1(reinterpret_cast<const char*>(serial));
            if (s.isEmpty())
                s = QString("panda-%1").arg(i);
            result.append(s);
        }
        libusb_free_device_list(devs, 1);
    }

    libusb_exit(ctx);
    return result;
}

void PandaUSB::piStarted()
{
    connectDevice();
}

void PandaUSB::piStop()
{
    mHeartbeatTimer.stop();
    mRxTimer.stop();
    disconnectDevice();
}

void PandaUSB::piSuspend(bool pSuspend)
{
    setCapSuspended(pSuspend);
    if (isCapSuspended())
        getQueue().flush();
}

bool PandaUSB::piGetBusSettings(int pBusIdx, CANBus& pBus)
{
    return getBusConfig(pBusIdx, pBus);
}

void PandaUSB::piSetBusSettings(int pBusIdx, CANBus bus)
{
    if (pBusIdx < 0 || pBusIdx >= getNumBuses())
        return;

    setBusConfig(pBusIdx, bus);

    if (mHandle && bus.isActive()) {
        int speed = bus.getSpeed();
        if (speed > 0) {
            // wIndex = kbps * 10; speed is in Hz so divide by 100
            uint16_t kbps10 = static_cast<uint16_t>(speed / 100);
            ctrlWrite(PANDA_REQ_SET_SPEED, static_cast<uint16_t>(pBusIdx), kbps10);
        }
    }
}

bool PandaUSB::piSendFrame(const CANFrame& frame)
{
    if (!mHandle) return false;

    QByteArray buf;
    if (!packFrame(frame, buf)) return false;

    int transferred = 0;
    int ret = libusb_bulk_transfer(mHandle, PANDA_EP_TX,
                                   reinterpret_cast<unsigned char*>(buf.data()),
                                   buf.length(), &transferred,
                                   PANDA_TX_TIMEOUT_MS);
    if (ret != LIBUSB_SUCCESS) {
        sendDebug(QString("Panda TX error: %1").arg(libusb_strerror(static_cast<libusb_error>(ret))));
        return false;
    }
    return true;
}

bool PandaUSB::packFrame(const CANFrame& frame, QByteArray& out)
{
    int dlc = frame.payload().length();
    if (dlc > 8) return false; // non-FD only

    int bus = frame.bus;
    if (bus < 0 || bus >= PANDA_NUM_BUSES) bus = 0;

    // Byte 0: [dlc_index:4 | bus:3 | fd:1]
    uint8_t header[6];
    header[0] = (static_cast<uint8_t>(dlc) << 4)
              | (static_cast<uint8_t>(bus & 0x7) << 1);
    // fd bit = 0 for standard CAN

    // Bytes 1-4: (addr << 3) | (extended << 2) as little-endian uint32
    uint32_t addr = frame.frameId();
    uint32_t word4b = (addr << 3) | (frame.hasExtendedFrameFormat() ? 0x4u : 0u);
    header[1] = static_cast<uint8_t>(word4b);
    header[2] = static_cast<uint8_t>(word4b >> 8);
    header[3] = static_cast<uint8_t>(word4b >> 16);
    header[4] = static_cast<uint8_t>(word4b >> 24);

    // Byte 5: XOR checksum of header bytes 0-4 and all data bytes
    uint8_t cs = 0;
    for (int i = 0; i < 5; i++) cs ^= header[i];
    for (int i = 0; i < dlc; i++) cs ^= static_cast<uint8_t>(frame.payload().at(i));
    header[5] = cs;

    out.append(reinterpret_cast<const char*>(header), 6);
    out.append(frame.payload());
    return true;
}

void PandaUSB::parseRxBuffer(const uint8_t* buf, int len)
{
    int pos = 0;
    while (pos + 6 <= len) {
        int dlc_index = (buf[pos] >> 4) & 0xF;
        int bus       = (buf[pos] >> 1) & 0x7;
        // bit 0 of byte 0 is fd flag - reserved for future FD support

        int dataLen = dlcToLen[dlc_index];
        if (pos + 6 + dataLen > len) break;

        // Verify XOR checksum: XOR of all bytes (header + data) must be 0
        uint8_t cs = 0;
        for (int i = 0; i < 6 + dataLen; i++) cs ^= buf[pos + i];
        if (cs != 0) {
            pos += 6 + dataLen;
            continue;
        }

        // Decode the 32-bit address word from bytes 1-4
        uint32_t word4b = static_cast<uint32_t>(buf[pos + 1])
                        | (static_cast<uint32_t>(buf[pos + 2]) << 8)
                        | (static_cast<uint32_t>(buf[pos + 3]) << 16)
                        | (static_cast<uint32_t>(buf[pos + 4]) << 24);

        // Bits 0-1 of word4b are rejected/returned flags for loopback frames
        bool rejected = (word4b & 0x1) != 0;
        bool returned = (word4b & 0x2) != 0;

        if (rejected || returned || bus >= PANDA_NUM_BUSES) {
            pos += 6 + dataLen;
            continue;
        }

        uint32_t addr    = word4b >> 3;
        bool     extended = (word4b & 0x4) != 0;

        if (!isCapSuspended()) {
            CANFrame frame;
            frame.setFrameId(addr);
            frame.setExtendedFrameFormat(extended);
            frame.setFrameType(QCanBusFrame::DataFrame);
            frame.bus = bus;
            frame.isReceived = true;

            QByteArray payload(reinterpret_cast<const char*>(buf + pos + 6), dataLen);
            frame.setPayload(payload);

            qint64 elapsedUs = (QDateTime::currentMSecsSinceEpoch() - mTimeBasis) * 1000ll;
            frame.setTimeStamp(QCanBusFrame::TimeStamp(0, elapsedUs));

            CANFrame* frame_p = getQueue().get();
            if (frame_p) {
                *frame_p = frame;
                checkTargettedFrame(frame);
                getQueue().queue();
            }
        }

        pos += 6 + dataLen;
    }
}

void PandaUSB::rxPoll()
{
    if (!mHandle) return;

    int transferred = 0;
    int ret = libusb_bulk_transfer(mHandle, PANDA_EP_RX,
                                   mRxBuf, sizeof(mRxBuf),
                                   &transferred, PANDA_RX_TIMEOUT_MS);

    if (ret == LIBUSB_SUCCESS || ret == LIBUSB_ERROR_TIMEOUT) {
        if (transferred > 0)
            parseRxBuffer(mRxBuf, transferred);
    } else {
        sendDebug(QString("Panda RX error: %1").arg(
            libusb_strerror(static_cast<libusb_error>(ret))));
        disconnectDevice();
    }
}

void PandaUSB::sendHeartbeat()
{
    ctrlWrite(PANDA_REQ_HEARTBEAT, 0, 0);
}

bool PandaUSB::ctrlWrite(uint8_t request, uint16_t value, uint16_t index)
{
    if (!mHandle) return false;
    int ret = libusb_control_transfer(mHandle, USB_CTRL_OUT,
                                      request, value, index,
                                      nullptr, 0,
                                      PANDA_TX_TIMEOUT_MS);
    return ret >= 0;
}

bool PandaUSB::ctrlRead(uint8_t request, uint16_t value, uint16_t index,
                        uint8_t* buf, int len)
{
    if (!mHandle) return false;
    int ret = libusb_control_transfer(mHandle, USB_CTRL_IN,
                                      request, value, index,
                                      buf, static_cast<uint16_t>(len),
                                      PANDA_TX_TIMEOUT_MS);
    return ret > 0;
}

void PandaUSB::connectDevice()
{
    if (libusb_init(&mCtx) != LIBUSB_SUCCESS) {
        sendDebug("Panda: failed to initialize libusb");
        return;
    }

    QString targetSerial = getPort();

    libusb_device** devs = nullptr;
    ssize_t cnt = libusb_get_device_list(mCtx, &devs);
    if (cnt >= 0) {
        for (ssize_t i = 0; i < cnt && !mHandle; i++) {
            struct libusb_device_descriptor desc;
            if (libusb_get_device_descriptor(devs[i], &desc) != LIBUSB_SUCCESS)
                continue;
            if (!isPandaVidPid(desc.idVendor, desc.idProduct))
                continue;

            libusb_device_handle* h = nullptr;
            if (libusb_open(devs[i], &h) != LIBUSB_SUCCESS) {
                sendDebug("Panda: could not open device (check udev rules)");
                continue;
            }

            if (!targetSerial.isEmpty() && targetSerial != "auto") {
                unsigned char serial[64] = {};
                libusb_get_string_descriptor_ascii(h, desc.iSerialNumber,
                                                   serial, sizeof(serial));
                if (QString::fromLatin1(reinterpret_cast<const char*>(serial)) != targetSerial) {
                    libusb_close(h);
                    continue;
                }
            }

            mHandle = h;
        }
        libusb_free_device_list(devs, 1);
    }

    if (!mHandle) {
        sendDebug("Panda: no device found");
        libusb_exit(mCtx);
        mCtx = nullptr;
        return;
    }

    // Detach any kernel driver on Linux so we can claim the interface
    if (libusb_kernel_driver_active(mHandle, 0) == 1)
        libusb_detach_kernel_driver(mHandle, 0);

    if (libusb_claim_interface(mHandle, 0) != LIBUSB_SUCCESS) {
        sendDebug("Panda: failed to claim USB interface");
        libusb_close(mHandle);
        mHandle = nullptr;
        libusb_exit(mCtx);
        mCtx = nullptr;
        return;
    }

    // Reset communication state and enable CAN TX
    ctrlWrite(PANDA_REQ_RESET_COMMS, 0, 0);
    ctrlWrite(PANDA_REQ_SET_SAFETY, PANDA_SAFETY_ALLOUTPUT, 0);

    // Configure CAN speed for all buses; propagate bus 0 speed as default
    int defaultSpeed = mBusData[0].mBus.getSpeed();
    if (defaultSpeed <= 0) defaultSpeed = 500000;

    for (int bus = 0; bus < PANDA_NUM_BUSES; bus++) {
        int speed = mBusData[bus].mBus.getSpeed();
        if (speed <= 0) speed = defaultSpeed;

        uint16_t kbps10 = static_cast<uint16_t>(speed / 100);
        ctrlWrite(PANDA_REQ_SET_SPEED, static_cast<uint16_t>(bus), kbps10);

        mBusData[bus].mConfigured = true;
        mBusData[bus].mBus.setActive(true);
        mBusData[bus].mBus.setSpeed(speed);
    }

    mTimeBasis = QDateTime::currentMSecsSinceEpoch();

    connect(&mHeartbeatTimer, &QTimer::timeout, this, &PandaUSB::sendHeartbeat,
            Qt::UniqueConnection);
    mHeartbeatTimer.setInterval(500);
    mHeartbeatTimer.setSingleShot(false);
    mHeartbeatTimer.start();

    connect(&mRxTimer, &QTimer::timeout, this, &PandaUSB::rxPoll,
            Qt::UniqueConnection);
    mRxTimer.setInterval(10);
    mRxTimer.setSingleShot(false);
    mRxTimer.start();

    setStatus(CANCon::CONNECTED);
    CANConStatus stats;
    stats.conStatus = getStatus();
    stats.numHardwareBuses = PANDA_NUM_BUSES;
    emit status(stats);

    sendDebug("Panda connected");
}

void PandaUSB::disconnectDevice()
{
    if (mHandle) {
        libusb_release_interface(mHandle, 0);
        libusb_close(mHandle);
        mHandle = nullptr;
    }
    if (mCtx) {
        libusb_exit(mCtx);
        mCtx = nullptr;
    }

    setStatus(CANCon::NOT_CONNECTED);
    CANConStatus stats;
    stats.conStatus = getStatus();
    stats.numHardwareBuses = PANDA_NUM_BUSES;
    emit status(stats);
}

#endif // WITH_PANDA
