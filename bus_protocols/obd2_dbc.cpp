#include "obd2_dbc.h"

#include <QStringList>

namespace OBD2
{

namespace
{
    const int REPLY_DATA_OFFSET = 3; //ISO-TP length, service, PID
    const char *NODE = "OBD2_ECU";

    QString number(double value)
    {
        return QString::number(value, 'g', 15);
    }

    QString quoted(QString text)
    {
        return "\"" + text.replace('"', '\'') + "\"";
    }

    quint32 dbcId(quint32 ecuReplyId)
    {
        return ecuReplyId > 0x7FF ? (ecuReplyId | 0x80000000u) : ecuReplyId; //bit 31 marks an extended ID
    }
}

QString dbcSignalName(quint8 pid, const Field &field)
{
    return QString("PID_%1_").arg(pid, 2, 16, QChar('0')).toUpper() + field.name;
}

int dbcStartBit(const Field &field)
{
    int msb = REPLY_DATA_OFFSET * 8 + field.startBit; //counting from the top bit of byte 0
    return (msb / 8) * 8 + (7 - msb % 8);
}

QString dbcMessageName(quint32 ecuReplyId)
{
    return "OBD2_ECU_" + QString("%1").arg(ecuReplyId, ecuReplyId > 0x7FF ? 8 : 3, 16, QChar('0')).toUpper();
}

QString makeDbc(const QList<quint32> &ecuReplyIds)
{
    QStringList lines;
    QStringList comments;
    QStringList values;
    QStringList muxValues;

    lines << "VERSION \"\"" << "" << "NS_ :" << "" << "BS_:" << "" << QString("BU_: %1").arg(NODE) << "";

    for (quint32 ecu : ecuReplyIds)
    {
        quint32 id = dbcId(ecu);
        lines << QString("BO_ %1 %2: 8 %3").arg(id).arg(dbcMessageName(ecu)).arg(NODE);
        lines << " SG_ Service M : 15|8@0+ (1,0) [0|255] \"\" Vector__XXX";
        lines << " SG_ PID m65M : 23|8@0+ (1,0) [0|255] \"\" Vector__XXX";
        muxValues << QString("SG_MUL_VAL_ %1 PID Service 65-65;").arg(id);
        comments << QString("CM_ BO_ %1 \"OBD-II Mode 01 replies from ECU 0x%2, made by SavvyCAN's OBD-II Poller\";")
                        .arg(id).arg(ecu, 0, 16);

        for (const PidInfo &info : mode01Pids())
        {
            for (const Field &field : info.fields)
            {
                QString name = dbcSignalName(info.pid, field);
                lines << QString(" SG_ %1 m%2 : %3|%4@0%5 (%6,%7) [%8|%9] %10 Vector__XXX")
                             .arg(name).arg(info.pid).arg(dbcStartBit(field)).arg(field.bitLength)
                             .arg(field.isSigned ? "-" : "+").arg(number(field.scale)).arg(number(field.offset))
                             .arg(number(field.minimum())).arg(number(field.maximum())).arg(quoted(field.unit));
                muxValues << QString("SG_MUL_VAL_ %1 %2 PID %3-%3;").arg(id).arg(name).arg(info.pid);
                comments << QString("CM_ SG_ %1 %2 %3;").arg(id).arg(name).arg(quoted(info.name));
                if (!field.valueNames.isEmpty())
                {
                    QString line = QString("VAL_ %1 %2").arg(id).arg(name);
                    for (const auto &valueName : field.valueNames)
                        line += QString(" %1 %2").arg(valueName.first).arg(quoted(valueName.second));
                    values << line + " ;";
                }
            }
        }
        lines << "";
    }

    return (lines + comments + QStringList{""} + values + QStringList{""} + muxValues).join('\n') + '\n';
}

}
