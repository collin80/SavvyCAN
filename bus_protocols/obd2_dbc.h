#ifndef OBD2_DBC_H
#define OBD2_DBC_H

#include <QList>
#include <QString>

#include "obd2_pids.h"

/*
 * A DBC file for OBD-II Mode 01 replies, made from the PID table, so the graph window, the signal viewer and the
 * frame view can decode the poller's answers (and a saved log can be decoded again later).
 *
 * One message per ECU reply ID. A single-frame reply is: byte 0 the ISO-TP length, byte 1 the service (0x41),
 * byte 2 the PID, then the PID's data bytes. So the DBC multiplexes three levels deep:
 *   Service (M, byte 1) -> PID (m65M, byte 2) -> PID_0C_EngineSpeed (m12) and so on,
 * linked with SG_MUL_VAL_ lines. Every value is big-endian (Motorola) as J1979 sends it.
 */
namespace OBD2
{
    //"PID_0C_EngineSpeed": unique in a message, since field names are unique across the table
    QString dbcSignalName(quint8 pid, const Field &field);
    //Motorola start bit (the DBC number of the field's most significant bit) in the reply frame
    int dbcStartBit(const Field &field);
    QString dbcMessageName(quint32 ecuReplyId);
    //The whole DBC file, with a message for each ECU (29-bit IDs are marked extended)
    QString makeDbc(const QList<quint32> &ecuReplyIds);
}

#endif // OBD2_DBC_H
