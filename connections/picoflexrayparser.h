#ifndef PICOFLEXRAYPARSER_H
#define PICOFLEXRAYPARSER_H

#include <QByteArray>
#include <QList>
#include "can_structs.h"

/*
 * Parser for the frame stream a pico-flexray device (https://github.com/dynm/pico-flexray) sends on its bulk IN
 * endpoint. The stream is a sequence of variable-length records, which can be split across USB transfers:
 *
 *   [u16 body length, little endian][u8 source][5-byte FlexRay header][payload][3-byte frame CRC, big endian]
 *
 * The source is the capture channel as decimal digits: 1 = FR1, 2 = FR2, and an FR3/FR4 tag can follow
 * (13 = seen on FR1, tagged FR3). Frames are captured on FR1 (bus 0) or FR2 (bus 1); the FR3/FR4 tag isn't kept.
 *
 * Each record becomes a CommFrame::FLEXRAYDataFrame (see CommFrame::isFlexRay()): ID = slot ID,
 * payload = cycle count followed by the FlexRay payload. Null frames (null frame indicator 0, no valid data) are
 * counted and dropped.
 */
class PicoFlexRayParser
{
public:
    static constexpr int MIN_BODY_LEN = 1 + 5 + 3;                                 //source + header + CRC, no payload
    static constexpr int MAX_BODY_LEN = MIN_BODY_LEN + CommFrame::FLEXRAY_MAX_PAYLOAD;

    //Adds bytes read from the device and appends the frames completed by them to frames, all with the given
    //timestamp (the device sends none). Returns the number of frames appended.
    int feed(const char *data, int len, CommFrame::TimeStamp timestamp, QList<CommFrame> &frames);
    void reset();

    quint64 framesParsed() const { return mFramesParsed; }
    quint64 nullFrames() const { return mNullFrames; }
    quint64 bytesSkipped() const { return mBytesSkipped; } //bytes dropped while resynchronising on a bad record

    //bus number for a record's source byte, -1 if it names neither FR1 nor FR2
    static int busFromSource(quint8 source);

private:
    QByteArray mBuffer;
    quint64 mFramesParsed = 0;
    quint64 mNullFrames = 0;
    quint64 mBytesSkipped = 0;
};

#endif // PICOFLEXRAYPARSER_H
