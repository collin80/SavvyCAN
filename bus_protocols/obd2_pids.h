#ifndef OBD2_PIDS_H
#define OBD2_PIDS_H

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QVector>

/*
 * OBD-II (SAE J1979) parameter IDs: what each Mode 01 PID holds and how to turn its data bytes into values.
 *
 * Each PID is a list of fields. A field is an unsigned (or two's complement) big-endian bit range of the data
 * bytes after the PID byte, and value = raw * scale + offset, which is also how a DBC signal decodes. The poller
 * decodes replies with these fields and the generated OBD DBC is built from them, so the two can't disagree.
 *
 * Bit positions count from the most significant bit of data byte A: bit 0 is A7, bit 7 is A0, bit 8 is B7 and
 * so on. "(256A + B) / 4" is startBit 0, bitLength 16, scale 0.25.
 */
namespace OBD2
{
    enum Service
    {
        SHOW_CURRENT_DATA = 0x01,
        VEHICLE_INFO = 0x09,
        POSITIVE_REPLY_OFFSET = 0x40
    };

    enum VehicleInfoPid
    {
        VIN_PID = 0x02,
        ECU_NAME_PID = 0x0A
    };

    struct Field
    {
        QString name;           //for signal names: letters and digits only, e.g. "EngineSpeed"
        QString unit;
        int startBit;
        int bitLength;
        double scale;
        double offset;
        bool isSigned;
        QList<QPair<int, QString>> valueNames; //names for raw values (states, flags), empty for plain numbers

        double minimum() const;
        double maximum() const;
    };

    struct PidInfo
    {
        quint8 pid;
        QString name;           //for display, e.g. "Engine speed"
        int dataBytes;
        QVector<Field> fields;  //empty for the supported-PID bitmaps
    };

    struct Value
    {
        const Field *field;
        qint64 raw;
        double value;
        QString text;           //the value name when the field has one, else empty
    };

    struct Reply
    {
        quint8 pid;
        QByteArray data;        //the bytes after the PID
    };

    //Every Mode 01 PID known here, in PID order
    const QVector<PidInfo> &mode01Pids();
    //nullptr for a PID not in the table
    const PidInfo *findMode01Pid(quint8 pid);

    //PIDs 0x00, 0x20, 0x40 ... 0xE0 answer with a bitmap of the 32 PIDs after them
    bool isSupportBitmapPid(quint8 pid);
    //The PIDs a bitmap reply says are supported, in order. The last one is the next bitmap PID when its bit is set.
    QList<quint8> supportedPids(quint8 bitmapPid, const QByteArray &data);

    //Raw value of a big-endian bit range (see the bit numbering above); false if data is too short
    bool extractBits(const QByteArray &data, int startBit, int bitLength, bool isSigned, qint64 &raw);
    //Every field of the PID; false if data is shorter than the PID's data bytes
    bool decode(const PidInfo &info, const QByteArray &data, QList<Value> &values);

    //The ISO-TP payload of a request for one Mode 01 PID: 01 <pid>
    QByteArray mode01Request(quint8 pid);
    //Splits a Mode 01 reply payload (41 <pid> <data> [<pid> <data> ...]) into its PIDs. A reply to a request for
    //several PIDs holds them one after another, so each PID's length has to be known: parsing stops at a PID that
    //isn't in the table. A reply that starts with an unknown PID is taken as that one PID with the rest as its
    //data. Returns false if nothing could be read.
    bool parseMode01Reply(const QByteArray &payload, QList<Reply> &replies);

    //The ISO-TP payload of a Mode 09 request: 09 <pid>
    QByteArray mode09Request(quint8 pid);
    //Text of a Mode 09 VIN (PID 02) or ECU name (PID 0A) reply payload: 49 <pid> <count> <text>. Padding NULs are
    //removed. False if it isn't such a reply.
    bool decodeMode09Text(const QByteArray &payload, quint8 pid, QString &text);
}

#endif // OBD2_PIDS_H
