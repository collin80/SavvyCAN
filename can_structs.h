#ifndef CAN_STRUCTS_H
#define CAN_STRUCTS_H

#include <QObject>
#include <QtCore/qmetatype.h>
#include <QtCore/qglobal.h>
#include <QVector>
#include <stdint.h>

using Qt::staticMetaObject;

#include <QCanBusFrame>

//Now instead of inheriting from builtin QT class, we just copy it and modify it to support more cooler stuff bro
//The idea now is to support LIN, CAN, CAN-FD, and FlexRay all in the same class structure here. The payload
//for this traffic is 0 - 254 bytes and the max ID size is 29 bits.
//There is a lot of heavy copying from the QT supplied version, just extended. This should be fine as this code is
//fully open source but do note that this could some affect licensing restrictions if you try to bring this into other projects.



struct CommFrame
{
    class TimeStamp {
    public:
        constexpr TimeStamp(qint64 s = 0, qint64 usec = 0) noexcept
            : secs(s), usecs(usec) {}

        constexpr static TimeStamp fromMicroSeconds(qint64 usec) noexcept
        { return TimeStamp(usec / 1000000, usec % 1000000); }

        constexpr qint64 seconds() const noexcept { return secs; }
        constexpr qint64 microSeconds() const noexcept { return usecs; }

    private:
        qint64 secs;
        qint64 usecs;
    };

    enum FrameType {
        CANDataFrame        = 0x0,
        CANFDDataFrame      = 0x1,
        FLEXRAYDataFrame    = 0x2,
        LINDataFrame        = 0x3,
        ErrorFrame          = 0x4,
        RemoteRequestFrame  = 0x5,
        InvalidFrame        = 0x6
    };
    Q_ENUM(FrameType)

    enum FrameError {
        NoError                     = 0,
        TransmissionTimeoutError    = (1 << 0),
        LostArbitrationError        = (1 << 1),
        ControllerError             = (1 << 2),
        ProtocolViolationError      = (1 << 3),
        TransceiverError            = (1 << 4),
        MissingAcknowledgmentError  = (1 << 5),
        BusOffError                 = (1 << 6),
        BusError                    = (1 << 7),
        ControllerRestartError      = (1 << 8),
        UnknownError                = (1 << 9),
        AnyError                    = 0x1FFFFFFFU
        //only 29 bits usable
    };
    Q_DECLARE_FLAGS(FrameErrors, FrameError)
    Q_ENUM(FrameError)
    Q_FLAG(FrameErrors)

public:
    friend bool operator<(const CommFrame& l, const CommFrame& r)
    {
        qint64 lStamp = l.timeStamp().seconds() * 1000000 + l.timeStamp().microSeconds();
        qint64 rStamp = r.timeStamp().seconds() * 1000000 + r.timeStamp().microSeconds();
        return lStamp < rStamp;
    }

    explicit CommFrame(FrameType type = CANDataFrame) noexcept :
        isExtendedFrame(0x0),
        isFlexibleDataRate(0x0),
        isBitrateSwitch(0x0),
        isErrorStateIndicator(0x0),
        isLocalEcho(0x0),
        reserved0(0x0),
        bus(0),
        timedelta(0),
        frameCount(0)
    {
        Q_UNUSED(reserved0);
        setFrameId(0x0);
        setFrameType(type);
    }


    explicit CommFrame(quint32 identifier, const QByteArray &data) :
        format(data.size() > 8 ? CANFDDataFrame : CANDataFrame), //more than 8 bytes can only be CAN FD
        isExtendedFrame(0x0),
        isFlexibleDataRate(data.size() > 8 ? 0x1 : 0x0),
        isBitrateSwitch(0x0),
        isErrorStateIndicator(0x0),
        isLocalEcho(0x0),
        reserved0(0x0),
        dataBytes(data)
    {
        setFrameId(identifier);
    }

    constexpr FrameType frameType() const noexcept
    {
        switch (format) {
        case 0x0: return CANDataFrame;
        case 0x1: return CANFDDataFrame;
        case 0x2: return FLEXRAYDataFrame;
        case 0x3: return LINDataFrame;
        case 0x4: return ErrorFrame;
        case 0x5: return RemoteRequestFrame;
        case 0x6: return InvalidFrame;
            // no default to trigger warning
        }

        return InvalidFrame;
    }

    /*
     * CAN FD is represented in two places: the frame type (CANFDDataFrame) and the FD flag
     * (hasFlexibleDataRateFormat()). The setters keep them in step, so code can use either:
     *   frameType() == CANFDDataFrame  <=>  hasFlexibleDataRateFormat()
     * Setting the type to CANFDDataFrame sets the FD flag; any other type clears it (and BRS/ESI).
     * Setting the FD flag (directly, or via BRS/ESI) turns a CAN data frame into a CAN FD data frame,
     * and clearing it turns a CAN FD data frame back into a CAN data frame.
     */
    constexpr void setFrameType(FrameType newFormat) noexcept
    {
        switch (newFormat) {
        case CANDataFrame:
            format = 0x0; break;
        case CANFDDataFrame:
            format = 0x1; break;
        case FLEXRAYDataFrame:
            format = 0x2; break;
        case LINDataFrame:
            format = 0x3; break;
        case ErrorFrame:
            format = 0x4; break;
        case RemoteRequestFrame:
            format = 0x5; break;
        case InvalidFrame:
            format = 0x6; break;
        }
        isFlexibleDataRate = (format == 0x1) ? 0x1 : 0x0;
        if (!isFlexibleDataRate) {
            isBitrateSwitch = 0x0;
            isErrorStateIndicator = 0x0;
        }
    }

    //QCanBusFrame has no separate CAN FD type: a FD data frame is a DataFrame with the FD flag set,
    //so call setFlexibleDataRateFormat() (and BRS/ESI) after this to carry the FD information over.
    constexpr void setFrameTypeFromQCan(QCanBusFrame::FrameType newFormat) noexcept
    {
        switch (newFormat) {
        case QCanBusFrame::FrameType::DataFrame:
            setFrameType(CANDataFrame); return;
        case QCanBusFrame::FrameType::ErrorFrame:
            setFrameType(ErrorFrame); return;
        case QCanBusFrame::FrameType::UnknownFrame:
            setFrameType(InvalidFrame); return;
        case QCanBusFrame::FrameType::RemoteRequestFrame:
            setFrameType(RemoteRequestFrame); return;
        case QCanBusFrame::FrameType::InvalidFrame:
            setFrameType(InvalidFrame); return;

        }
    }

    //A frame carrying data: classic CAN, CAN FD or FlexRay, but not a FlexRay null frame (as opposed to remote,
    //error, LIN, ...)
    constexpr bool isDataFrame() const noexcept
    {
        return format == 0x0 || format == 0x1 || (format == 0x2 && !isFlexRayNullFrame());
    }

    /*
     * FlexRay frames: the frame ID is the 11-bit slot ID, byte 0 of the payload is the cycle count (0-63) and the
     * FlexRay payload (0-254 bytes) follows. Keeping the cycle count in the payload lets DBC files multiplex on it,
     * as pico-flexray's and openpilot cabana's FlexRay DBC files do.
     */
    static constexpr int FLEXRAY_MAX_PAYLOAD = 254;
    constexpr bool isFlexRay() const noexcept { return format == 0x2; }

    /*
     * FlexRay header details from the capture device, kept with the frame (none for frames loaded from most logs).
     * Indicators are the header's 5 bits: reserved, payload preamble, null frame (0 = null frame), sync, startup.
     * The CRC status is what the device checked; header and frame CRCs are the values received.
     */
    static constexpr quint8 FLEXRAY_IND_NULL_FRAME = 0x04; //null frame indicator: set = data valid
    void setFlexRayHeader(quint8 indicators, quint16 headerCrc, quint32 frameCrc)
    {
        flexRayInfo = (flexRayInfo & (FR_CRC_CHECKED | FR_HEADER_CRC_OK | FR_FRAME_CRC_OK | FR_DEVICE_TIME))
                      | FR_HAS_HEADER | (quint64(indicators & 0x1F) << FR_IND_SHIFT)
                      | (quint64(headerCrc & 0x7FF) << FR_HCRC_SHIFT) | (quint64(frameCrc & 0xFFFFFF) << FR_FCRC_SHIFT);
    }
    //for sources that only know whether it was a null frame (e.g. native CSV)
    void setFlexRayNullFrame(bool isNull)
    {
        quint8 ind = hasFlexRayHeader() ? flexRayIndicators() : FLEXRAY_IND_NULL_FRAME;
        ind = isNull ? quint8(ind & ~FLEXRAY_IND_NULL_FRAME) : quint8(ind | FLEXRAY_IND_NULL_FRAME);
        setFlexRayHeader(ind, flexRayHeaderCrc(), flexRayFrameCrc());
    }
    void setFlexRayCrcStatus(bool headerCrcOk, bool frameCrcOk)
    {
        flexRayInfo = (flexRayInfo & ~(FR_HEADER_CRC_OK | FR_FRAME_CRC_OK)) | FR_CRC_CHECKED
                      | (headerCrcOk ? FR_HEADER_CRC_OK : 0) | (frameCrcOk ? FR_FRAME_CRC_OK : 0);
    }
    void setFlexRayDeviceTimestamp(bool fromDevice)
    {
        flexRayInfo = fromDevice ? (flexRayInfo | FR_DEVICE_TIME) : (flexRayInfo & ~FR_DEVICE_TIME);
    }
    void clearFlexRayInfo() { flexRayInfo = 0; }

    constexpr bool hasFlexRayHeader() const noexcept { return flexRayInfo & FR_HAS_HEADER; }
    constexpr quint8 flexRayIndicators() const noexcept { return quint8((flexRayInfo >> FR_IND_SHIFT) & 0x1F); }
    constexpr quint16 flexRayHeaderCrc() const noexcept { return quint16((flexRayInfo >> FR_HCRC_SHIFT) & 0x7FF); }
    constexpr quint32 flexRayFrameCrc() const noexcept { return quint32((flexRayInfo >> FR_FCRC_SHIFT) & 0xFFFFFF); }
    constexpr bool isFlexRayNullFrame() const noexcept
    {
        return format == 0x2 && hasFlexRayHeader() && !(flexRayIndicators() & FLEXRAY_IND_NULL_FRAME);
    }
    constexpr bool flexRayCrcChecked() const noexcept { return flexRayInfo & FR_CRC_CHECKED; }
    constexpr bool flexRayHeaderCrcOk() const noexcept { return flexRayInfo & FR_HEADER_CRC_OK; }
    constexpr bool flexRayFrameCrcOk() const noexcept { return flexRayInfo & FR_FRAME_CRC_OK; }
    //checked by the device and failed
    constexpr bool hasFlexRayCrcError() const noexcept
    {
        return flexRayCrcChecked() && !(flexRayHeaderCrcOk() && flexRayFrameCrcOk());
    }
    constexpr bool hasFlexRayDeviceTimestamp() const noexcept { return flexRayInfo & FR_DEVICE_TIME; }

    constexpr QCanBusFrame::FrameType toQCanBusFrameType() noexcept
    {
        switch (format) {
        case CANDataFrame:
            return QCanBusFrame::FrameType::DataFrame;
        case CANFDDataFrame:
            return QCanBusFrame::FrameType::DataFrame;
        case FLEXRAYDataFrame:
            return QCanBusFrame::FrameType::DataFrame;
        case LINDataFrame:
            return QCanBusFrame::FrameType::DataFrame;
        case ErrorFrame:
            return QCanBusFrame::FrameType::ErrorFrame;
        case RemoteRequestFrame:
            return QCanBusFrame::FrameType::RemoteRequestFrame;
        case InvalidFrame:
            return QCanBusFrame::FrameType::InvalidFrame;
        }
        return QCanBusFrame::FrameType::UnknownFrame;
    }

    constexpr bool hasExtendedFrameFormat() const noexcept { return (isExtendedFrame & 0x1); }
    constexpr void setExtendedFrameFormat(bool isExtended) noexcept
    {
        isExtendedFrame = (isExtended & 0x1);
    }

    constexpr quint32 frameId() const noexcept
    {
        if (Q_UNLIKELY(format == ErrorFrame))
            return 0;
        return (msgId & 0x1FFFFFFFU);
    }

    constexpr void setFrameId(quint32 newFrameId)
    {
        if (Q_LIKELY(newFrameId < 0x20000000U)) {
            isValidFrameId = true;
            msgId = newFrameId;
            setExtendedFrameFormat(isExtendedFrame || (newFrameId & 0x1FFFF800U));
        } else {
            isValidFrameId = false;
            msgId = 0;
        }
    }

    constexpr quint8 getBus() const noexcept {return bus;}

    constexpr void setBus(quint8 busNum) noexcept
    {
        bus = busNum;
    }

    constexpr quint64 getTimeDelta() const noexcept {return timedelta;}

    constexpr void setTimeDelta(quint64 d) noexcept
    {
        timedelta = d;
    }

    constexpr quint32 getFrameCount() const noexcept {return frameCount;}

    constexpr void setFrameCount(quint32 cnt) noexcept
    {
        frameCount = cnt;
    }

    constexpr bool hasFlexibleDataRateFormat() const noexcept { return (isFlexibleDataRate & 0x1); }
    //keeps the frame type in step with the FD flag (see setFrameType())
    constexpr void setFlexibleDataRateFormat(bool isFlexibleData) noexcept
    {
        if (isFlexibleData) {
            if (format != 0x0 && format != 0x1) return; //only data frames can be CAN FD (there are no FD remote frames)
            format = 0x1; //CAN data frame -> CAN FD data frame
            isFlexibleDataRate = 0x1;
        } else {
            isFlexibleDataRate = 0x0;
            if (format == 0x1) format = 0x0; //CAN FD data frame -> CAN data frame
            isBitrateSwitch = 0x0;
            isErrorStateIndicator = 0x0;
        }
    }

    constexpr bool hasBitrateSwitch() const noexcept { return (isBitrateSwitch & 0x1); }
    constexpr void setBitrateSwitch(bool bitrateSwitch) noexcept
    {
        if (bitrateSwitch) setFlexibleDataRateFormat(true); //only CAN FD frames have BRS
        isBitrateSwitch = (bitrateSwitch && isFlexibleDataRate) ? 0x1 : 0x0;
    }

    constexpr bool hasErrorStateIndicator() const noexcept { return (isErrorStateIndicator & 0x1); }
    constexpr void setErrorStateIndicator(bool errorStateIndicator) noexcept
    {
        if (errorStateIndicator) setFlexibleDataRateFormat(true); //only CAN FD frames have ESI
        isErrorStateIndicator = (errorStateIndicator && isFlexibleDataRate) ? 0x1 : 0x0;
    }

    constexpr bool hasLocalEcho() const noexcept { return (isLocalEcho & 0x1); }
    constexpr void setLocalEcho(bool localEcho) noexcept
    {
        isLocalEcho = (localEcho & 0x1);
    }

    constexpr bool isReceived() const noexcept { return (!(isLocalEcho & 0x1)); }
    constexpr void setReceived(bool received) noexcept
    {
        isLocalEcho = !(received & 0x1);
    }

    //more than 8 bytes makes a CAN frame CAN FD; other frame types (FlexRay, LIN, ...) keep their type
    void setPayload(const QByteArray &data)
    {
        dataBytes = data;
        if (data.size() > 8)
            setFlexibleDataRateFormat(true);
    }

    QByteArray payload() const { return dataBytes; }

    constexpr void setTimeStamp(TimeStamp ts) noexcept { stamp = ts; }

    constexpr TimeStamp timeStamp() const noexcept { return stamp; }

    constexpr FrameErrors error() const noexcept
    {
        if (format != ErrorFrame)
            return NoError;

        return FrameErrors(msgId & 0x1FFFFFFFU);
    }

    constexpr void setError(FrameErrors e)
    {
        if (format != ErrorFrame)
            return;
        msgId = (e & AnyError).toInt();
    }

    bool isValid() const noexcept
    {
        if (format == InvalidFrame)
            return false;

        // long id used, but extended flag not set
        if (!isExtendedFrame && (msgId & 0x1FFFF800U))
            return false;

        if (!isValidFrameId)
            return false;

        // maximum permitted payload size in CAN or CAN FD
        const qsizetype length = dataBytes.size();
        if (format == FLEXRAYDataFrame) // 11-bit slot ID, cycle count byte + up to 254 bytes
            return !isExtendedFrame && msgId <= 0x7FFU && length >= 1 && length <= 1 + FLEXRAY_MAX_PAYLOAD;

        if (isFlexibleDataRate) {
            if (format == RemoteRequestFrame)
                return false;

            return length <= 8 || length == 12 || length == 16 || length == 20
                   || length == 24 || length == 32 || length == 48 || length == 64;
        }

        return length <= 8;
    }

private:
    quint32 msgId:29; // acts as container for error codes too
    quint8 format:3; // max of 8 frame types

    quint8 isExtendedFrame:1;
    quint8 isValidFrameId:1;
    quint8 isFlexibleDataRate:1;
    quint8 isBitrateSwitch:1;
    quint8 isErrorStateIndicator:1;
    quint8 isLocalEcho:1;
    quint8 reserved0:2;

    QByteArray dataBytes;
    TimeStamp stamp;
    int bus;
    uint64_t timedelta;
    uint32_t frameCount; //used in overwrite mode

    //FlexRay header details (see setFlexRayHeader()), packed: flags in bits 0-4, indicators 5-9,
    //header CRC 10-20, frame CRC 21-44
    static constexpr quint64 FR_HAS_HEADER = 1ull << 0;
    static constexpr quint64 FR_CRC_CHECKED = 1ull << 1;
    static constexpr quint64 FR_HEADER_CRC_OK = 1ull << 2;
    static constexpr quint64 FR_FRAME_CRC_OK = 1ull << 3;
    static constexpr quint64 FR_DEVICE_TIME = 1ull << 4;
    static constexpr int FR_IND_SHIFT = 5;
    static constexpr int FR_HCRC_SHIFT = 10;
    static constexpr int FR_FCRC_SHIFT = 21;
    quint64 flexRayInfo = 0;
};


class CANFltObserver
{
public:
    quint32 id;
    quint32 mask;
    QObject * observer; //used to target the specific object that setup this filter

    bool operator ==(const CANFltObserver &b) const
    {
        if ( (id == b.id) && (mask == b.mask) && (observer == b.observer) ) return true;

        return false;
    }
};


Q_DECLARE_TYPEINFO(CommFrame, Q_RELOCATABLE_TYPE);
Q_DECLARE_TYPEINFO(CommFrame::FrameError, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(CommFrame::FrameType, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(CommFrame::TimeStamp, Q_PRIMITIVE_TYPE);

Q_DECLARE_OPERATORS_FOR_FLAGS(CommFrame::FrameErrors)
Q_DECLARE_METATYPE(CommFrame::FrameType)
Q_DECLARE_METATYPE(CommFrame::FrameErrors)

#endif // CAN_STRUCTS_H
