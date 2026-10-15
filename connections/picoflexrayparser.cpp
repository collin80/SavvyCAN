#include "picoflexrayparser.h"

int PicoFlexRayParser::busFromSource(quint8 source)
{
    //decimal digits, FR1/FR2 first: 1, 2, 13, 14, 23, 24 (12 and longer combinations aren't sent for captured frames)
    int firstDigit = source;
    while (firstDigit >= 10) firstDigit /= 10;
    if (firstDigit == 1) return 0;
    if (firstDigit == 2) return 1;
    return -1;
}

void PicoFlexRayParser::reset()
{
    mBuffer.clear();
}

int PicoFlexRayParser::feed(const char *data, int len, CommFrame::TimeStamp timestamp, QList<CommFrame> &frames)
{
    mBuffer.append(data, len);

    const unsigned char *buf = reinterpret_cast<const unsigned char *>(mBuffer.constData());
    const int bufLen = mBuffer.size();
    int pos = 0;
    int added = 0;

    while (pos + 2 <= bufLen)
    {
        const int bodyLen = buf[pos] | (buf[pos + 1] << 8);
        if (bodyLen < MIN_BODY_LEN || bodyLen > MAX_BODY_LEN)
        {
            //not a record start: skip a byte and look again, as pico-flexray's own recorder does
            pos++;
            mBytesSkipped++;
            continue;
        }
        //check the length against the header's payload length as soon as it's here, so a false start doesn't
        //wait for a whole record's worth of data
        if (pos + 6 > bufLen) break;
        const unsigned char *body = buf + pos + 2;
        const unsigned char *header = body + 1;
        const int payloadLen = ((header[2] >> 1) & 0x7F) * 2;
        if (MIN_BODY_LEN + payloadLen != bodyLen)
        {
            pos++;
            mBytesSkipped++;
            continue;
        }
        if (pos + 2 + bodyLen > bufLen) break; //record not complete yet

        const quint8 source = body[0];
        const quint8 indicators = header[0] >> 3; //reserved, payload preamble, null frame, sync, startup
        const bool nullFrame = !(indicators & 0x04);
        const quint32 slotId = ((header[0] & 0x07) << 8) | header[1];
        const quint8 cycle = header[4] & 0x3F;
        pos += 2 + bodyLen;

        if (nullFrame)
        {
            mNullFrames++;
            continue;
        }

        QByteArray payload;
        payload.reserve(1 + payloadLen);
        payload.append(static_cast<char>(cycle));
        payload.append(reinterpret_cast<const char *>(header + 5), payloadLen);

        CommFrame frame(CommFrame::FLEXRAYDataFrame);
        frame.setFrameId(slotId);
        frame.setExtendedFrameFormat(false);
        frame.setPayload(payload);
        frame.setBus(qMax(0, busFromSource(source)));
        frame.setReceived(true);
        frame.setTimeStamp(timestamp);
        frames.append(frame);
        mFramesParsed++;
        added++;
    }

    mBuffer.remove(0, pos);
    return added;
}
