#ifndef PICOFLEXRAYPARSER_H
#define PICOFLEXRAYPARSER_H

#include <QByteArray>
#include <QList>
#include "can_structs.h"

/*
 * Parser for the frame stream a pico-flexray device (https://github.com/dynm/pico-flexray) sends on its bulk IN
 * endpoint. The stream is a sequence of variable-length records, which can be split across USB transfers, in one of
 * two formats:
 *
 *   format 1 (stock firmware, and every firmware until the host asks for format 2):
 *     [u16 body length LE][u8 source][5-byte FlexRay header][payload][3-byte frame CRC, big endian]
 *   format 2 (bit 15 of the length set; firmware with record format support, after PICOFLEX_SET_RECORD_FORMAT 2):
 *     [u16 0x8000|body length LE][u8 version = 2][u8 flags][u8 source][u32 device time, us, LE][header][payload][CRC]
 *     flags: bit 0 = frame CRC correct, bit 1 = header CRC correct
 *
 * The source is the capture channel as decimal digits: 1 = FR1, 2 = FR2, and an FR3/FR4 tag can follow
 * (13 = seen on FR1, tagged FR3). Frames are captured on FR1 (bus 0) or FR2 (bus 1); the FR3/FR4 tag isn't kept.
 *
 * Each record becomes a CommFrame::FLEXRAYDataFrame (see CommFrame::isFlexRay()): ID = slot ID, payload = cycle count
 * followed by the FlexRay payload. The header's indicators and CRCs and the device's CRC check go into the frame's
 * FlexRay header details (CommFrame::setFlexRayHeader()). Null frames are kept: they're marked by their indicators
 * (CommFrame::isFlexRayNullFrame()) and aren't data frames. Stock firmware only sends frames that passed both CRCs.
 *
 * Timestamps: format 1 frames get the host timestamp passed to feed(). Format 2 frames get their device time, mapped
 * onto the host clock: the device clock is unwrapped to 64 bits and offset so that the latest frame of each transfer
 * is never stamped later than the transfer reached the host. The offset follows the lowest latency seen, and starts
 * again if the two clocks are more than a second apart (e.g. after a device reset). Frames keep their device
 * spacing, so slot and cycle timing is the device's.
 */
class PicoFlexRayParser
{
public:
    static constexpr int V1_MIN_BODY_LEN = 1 + 5 + 3; //source + header + CRC, no payload
    static constexpr int V1_MAX_BODY_LEN = V1_MIN_BODY_LEN + CommFrame::FLEXRAY_MAX_PAYLOAD;
    static constexpr int V2_HEADER_LEN = 7;           //version, flags, source, u32 device time
    static constexpr int V2_MIN_BODY_LEN = V2_HEADER_LEN + 5 + 3;
    static constexpr int V2_MAX_BODY_LEN = V2_MIN_BODY_LEN + CommFrame::FLEXRAY_MAX_PAYLOAD;
    static constexpr quint16 V2_LEN_FLAG = 0x8000;
    static constexpr quint8 V2_FLAG_FRAME_CRC_OK = 0x01;
    static constexpr quint8 V2_FLAG_HEADER_CRC_OK = 0x02;
    static constexpr qint64 REANCHOR_US = 1000000;

    //Adds bytes read from the device and appends the frames completed by them to frames. hostTimestamp is when
    //these bytes arrived: the timestamp of format 1 frames, and the reference for mapping format 2 device times.
    //Returns the number of frames appended.
    int feed(const char *data, int len, CommFrame::TimeStamp hostTimestamp, QList<CommFrame> &frames);
    void reset(); //new connection: drops buffered bytes and the device clock mapping

    quint64 framesParsed() const { return mFramesParsed; }
    quint64 nullFrames() const { return mNullFrames; }
    quint64 crcErrorFrames() const { return mCrcErrorFrames; }
    quint64 format2Records() const { return mFormat2Records; }
    quint64 bytesSkipped() const { return mBytesSkipped; }     //bytes dropped while resynchronising on a bad record

    //bus number for a record's source byte, -1 if it names neither FR1 nor FR2
    static int busFromSource(quint8 source);

private:
    qint64 unwrapDeviceTime(quint32 deviceUs);
    void updateOffset(qint64 latestDeviceUs, qint64 hostUs);

    QByteArray mBuffer;
    quint64 mFramesParsed = 0;
    quint64 mNullFrames = 0;
    quint64 mCrcErrorFrames = 0;
    quint64 mFormat2Records = 0;
    quint64 mBytesSkipped = 0;

    //device clock mapping (format 2)
    bool mHaveDeviceTime = false;
    quint32 mLastDeviceRaw = 0;
    qint64 mDevice64 = 0;  //unwrapped device time
    qint64 mOffsetUs = 0;  //host time = device time + offset
};

#endif // PICOFLEXRAYPARSER_H
