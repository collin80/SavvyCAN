#include "obd2_pids.h"

#include <algorithm>
#include <cmath>

namespace OBD2
{

namespace
{
    const double PERCENT = 100.0 / 255.0;   //A * 100 / 255
    const double TRIM = 100.0 / 128.0;      //A * 100 / 128 - 100

    Field number(const QString &name, const QString &unit, int startBit, int bitLength,
                 double scale = 1.0, double offset = 0.0, bool isSigned = false)
    {
        return Field{name, unit, startBit, bitLength, scale, offset, isSigned, {}};
    }

    Field state(const QString &name, int startBit, int bitLength, const QList<QPair<int, QString>> &valueNames)
    {
        return Field{name, QString(), startBit, bitLength, 1.0, 0.0, false, valueNames};
    }

    const QList<QPair<int, QString>> OFF_ON = {{0, "Off"}, {1, "On"}};

    const QList<QPair<int, QString>> FUEL_SYSTEM_STATUS = {
        {0, "Off"},
        {1, "Open loop, engine not warm enough"},
        {2, "Closed loop, using oxygen sensor feedback"},
        {4, "Open loop, engine load or fuel cut"},
        {8, "Open loop, system failure"},
        {16, "Closed loop, fault in an oxygen sensor"}
    };

    const QList<QPair<int, QString>> SECONDARY_AIR_STATUS = {
        {1, "Upstream"},
        {2, "Downstream of catalytic converter"},
        {4, "From the outside atmosphere or off"},
        {8, "Pump commanded on for diagnostics"}
    };

    const QList<QPair<int, QString>> OBD_STANDARDS = {
        {1, "OBD-II (CARB)"}, {2, "OBD (EPA)"}, {3, "OBD and OBD-II"}, {4, "OBD-I"}, {5, "Not OBD compliant"},
        {6, "EOBD"}, {7, "EOBD and OBD-II"}, {8, "EOBD and OBD"}, {9, "EOBD, OBD and OBD-II"}, {10, "JOBD"},
        {11, "JOBD and OBD-II"}, {12, "JOBD and EOBD"}, {13, "JOBD, EOBD and OBD-II"}
    };

    const QList<QPair<int, QString>> FUEL_TYPES = {
        {0, "Not available"}, {1, "Gasoline"}, {2, "Methanol"}, {3, "Ethanol"}, {4, "Diesel"}, {5, "LPG"},
        {6, "CNG"}, {7, "Propane"}, {8, "Electric"}, {9, "Bifuel running gasoline"},
        {10, "Bifuel running methanol"}, {11, "Bifuel running ethanol"}, {12, "Bifuel running LPG"},
        {13, "Bifuel running CNG"}, {14, "Bifuel running propane"}, {15, "Bifuel running electricity"},
        {16, "Bifuel running electric and combustion engine"}, {17, "Hybrid gasoline"}, {18, "Hybrid ethanol"},
        {19, "Hybrid diesel"}, {20, "Hybrid electric"}, {21, "Hybrid running electric and combustion engine"},
        {22, "Hybrid regenerative"}, {23, "Bifuel running diesel"}
    };

    //Oxygen sensors 1 to 8 come in three PID ranges with the same layout per sensor
    void addOxygenSensors(QVector<PidInfo> &pids)
    {
        for (int s = 0; s < 8; s++)
        {
            const QString n = QString::number(s + 1);
            pids.append({quint8(0x14 + s), "Oxygen sensor " + n + " voltage and fuel trim", 2, {
                number("O2S" + n + "_Voltage", "V", 0, 8, 0.005),
                number("O2S" + n + "_ShortTermFuelTrim", "%", 8, 8, TRIM, -100.0)}});
        }
        for (int s = 0; s < 8; s++)
        {
            const QString n = QString::number(s + 1);
            pids.append({quint8(0x24 + s), "Oxygen sensor " + n + " air-fuel ratio and voltage", 4, {
                number("O2S" + n + "_Lambda", "", 0, 16, 2.0 / 65536.0),
                number("O2S" + n + "_WideVoltage", "V", 16, 16, 8.0 / 65536.0)}});
        }
        for (int s = 0; s < 8; s++)
        {
            const QString n = QString::number(s + 1);
            pids.append({quint8(0x34 + s), "Oxygen sensor " + n + " air-fuel ratio and current", 4, {
                number("O2S" + n + "_CurrentLambda", "", 0, 16, 2.0 / 65536.0), //names unique across all PIDs
                number("O2S" + n + "_Current", "mA", 16, 16, 1.0 / 256.0, -128.0)}});
        }
    }

    QVector<PidInfo> buildMode01Pids()
    {
        QVector<PidInfo> pids;

        for (int base = 0x00; base <= 0xE0; base += 0x20)
        {
            QString range = QString("%1-%2").arg(base + 1, 2, 16, QChar('0')).arg(base + 0x20, 2, 16, QChar('0'))
                                .toUpper();
            pids.append({quint8(base), "PIDs supported [" + range + "]", 4, {}});
        }

        pids.append({0x01, "Monitor status since DTCs cleared", 4, {
            state("MIL", 0, 1, OFF_ON),
            number("DTC_Count", "", 1, 7),
            state("IgnitionType", 12, 1, {{0, "Spark"}, {1, "Compression"}})}});
        pids.append({0x03, "Fuel system status", 2, {
            state("FuelSystem1_Status", 0, 8, FUEL_SYSTEM_STATUS),
            state("FuelSystem2_Status", 8, 8, FUEL_SYSTEM_STATUS)}});
        pids.append({0x04, "Calculated engine load", 1, {number("EngineLoad", "%", 0, 8, PERCENT)}});
        pids.append({0x05, "Engine coolant temperature", 1, {number("CoolantTemp", "degC", 0, 8, 1.0, -40.0)}});
        pids.append({0x06, "Short term fuel trim, bank 1", 1, {number("ShortTermFuelTrim_B1", "%", 0, 8, TRIM, -100.0)}});
        pids.append({0x07, "Long term fuel trim, bank 1", 1, {number("LongTermFuelTrim_B1", "%", 0, 8, TRIM, -100.0)}});
        pids.append({0x08, "Short term fuel trim, bank 2", 1, {number("ShortTermFuelTrim_B2", "%", 0, 8, TRIM, -100.0)}});
        pids.append({0x09, "Long term fuel trim, bank 2", 1, {number("LongTermFuelTrim_B2", "%", 0, 8, TRIM, -100.0)}});
        pids.append({0x0A, "Fuel pressure (gauge)", 1, {number("FuelPressure", "kPa", 0, 8, 3.0)}});
        pids.append({0x0B, "Intake manifold absolute pressure", 1, {number("IntakeManifoldPressure", "kPa", 0, 8)}});
        pids.append({0x0C, "Engine speed", 2, {number("EngineSpeed", "rpm", 0, 16, 0.25)}});
        pids.append({0x0D, "Vehicle speed", 1, {number("VehicleSpeed", "km/h", 0, 8)}});
        pids.append({0x0E, "Timing advance", 1, {number("TimingAdvance", "deg", 0, 8, 0.5, -64.0)}});
        pids.append({0x0F, "Intake air temperature", 1, {number("IntakeAirTemp", "degC", 0, 8, 1.0, -40.0)}});
        pids.append({0x10, "Mass air flow rate", 2, {number("MAF", "g/s", 0, 16, 0.01)}});
        pids.append({0x11, "Throttle position", 1, {number("ThrottlePosition", "%", 0, 8, PERCENT)}});
        pids.append({0x12, "Commanded secondary air status", 1, {
            state("SecondaryAirStatus", 0, 8, SECONDARY_AIR_STATUS)}});
        pids.append({0x13, "Oxygen sensors present (2 banks)", 1, {number("O2SensorsPresent2Banks", "", 0, 8)}});
        pids.append({0x1C, "OBD standards this vehicle conforms to", 1, {state("OBDStandard", 0, 8, OBD_STANDARDS)}});
        pids.append({0x1D, "Oxygen sensors present (4 banks)", 1, {number("O2SensorsPresent4Banks", "", 0, 8)}});
        pids.append({0x1E, "Auxiliary input status", 1, {state("PowerTakeOff", 7, 1, OFF_ON)}});
        pids.append({0x1F, "Run time since engine start", 2, {number("RunTime", "s", 0, 16)}});
        pids.append({0x21, "Distance travelled with MIL on", 2, {number("DistanceWithMIL", "km", 0, 16)}});
        pids.append({0x22, "Fuel rail pressure (relative to manifold vacuum)", 2, {
            number("FuelRailPressureRelative", "kPa", 0, 16, 0.079)}});
        pids.append({0x23, "Fuel rail gauge pressure", 2, {number("FuelRailGaugePressure", "kPa", 0, 16, 10.0)}});
        pids.append({0x2C, "Commanded EGR", 1, {number("CommandedEGR", "%", 0, 8, PERCENT)}});
        pids.append({0x2D, "EGR error", 1, {number("EGRError", "%", 0, 8, TRIM, -100.0)}});
        pids.append({0x2E, "Commanded evaporative purge", 1, {number("CommandedEvapPurge", "%", 0, 8, PERCENT)}});
        pids.append({0x2F, "Fuel tank level", 1, {number("FuelLevel", "%", 0, 8, PERCENT)}});
        pids.append({0x30, "Warm-ups since codes cleared", 1, {number("WarmUpsSinceClear", "", 0, 8)}});
        pids.append({0x31, "Distance travelled since codes cleared", 2, {number("DistanceSinceClear", "km", 0, 16)}});
        pids.append({0x32, "Evap system vapour pressure", 2, {number("EvapVapourPressure", "Pa", 0, 16, 0.25, 0.0, true)}});
        pids.append({0x33, "Absolute barometric pressure", 1, {number("BarometricPressure", "kPa", 0, 8)}});
        pids.append({0x3C, "Catalyst temperature, bank 1 sensor 1", 2, {number("CatalystTemp_B1S1", "degC", 0, 16, 0.1, -40.0)}});
        pids.append({0x3D, "Catalyst temperature, bank 2 sensor 1", 2, {number("CatalystTemp_B2S1", "degC", 0, 16, 0.1, -40.0)}});
        pids.append({0x3E, "Catalyst temperature, bank 1 sensor 2", 2, {number("CatalystTemp_B1S2", "degC", 0, 16, 0.1, -40.0)}});
        pids.append({0x3F, "Catalyst temperature, bank 2 sensor 2", 2, {number("CatalystTemp_B2S2", "degC", 0, 16, 0.1, -40.0)}});
        pids.append({0x42, "Control module voltage", 2, {number("ControlModuleVoltage", "V", 0, 16, 0.001)}});
        pids.append({0x43, "Absolute load value", 2, {number("AbsoluteLoad", "%", 0, 16, PERCENT)}});
        pids.append({0x44, "Commanded air-fuel equivalence ratio", 2, {number("CommandedLambda", "", 0, 16, 2.0 / 65536.0)}});
        pids.append({0x45, "Relative throttle position", 1, {number("RelativeThrottlePosition", "%", 0, 8, PERCENT)}});
        pids.append({0x46, "Ambient air temperature", 1, {number("AmbientAirTemp", "degC", 0, 8, 1.0, -40.0)}});
        pids.append({0x47, "Absolute throttle position B", 1, {number("ThrottlePositionB", "%", 0, 8, PERCENT)}});
        pids.append({0x48, "Absolute throttle position C", 1, {number("ThrottlePositionC", "%", 0, 8, PERCENT)}});
        pids.append({0x49, "Accelerator pedal position D", 1, {number("AcceleratorPedalD", "%", 0, 8, PERCENT)}});
        pids.append({0x4A, "Accelerator pedal position E", 1, {number("AcceleratorPedalE", "%", 0, 8, PERCENT)}});
        pids.append({0x4B, "Accelerator pedal position F", 1, {number("AcceleratorPedalF", "%", 0, 8, PERCENT)}});
        pids.append({0x4C, "Commanded throttle actuator", 1, {number("CommandedThrottle", "%", 0, 8, PERCENT)}});
        pids.append({0x4D, "Time run with MIL on", 2, {number("TimeWithMIL", "min", 0, 16)}});
        pids.append({0x4E, "Time since trouble codes cleared", 2, {number("TimeSinceClear", "min", 0, 16)}});
        pids.append({0x51, "Fuel type", 1, {state("FuelType", 0, 8, FUEL_TYPES)}});
        pids.append({0x52, "Ethanol fuel percentage", 1, {number("EthanolPercent", "%", 0, 8, PERCENT)}});
        pids.append({0x53, "Absolute evap system vapour pressure", 2, {number("AbsEvapVapourPressure", "kPa", 0, 16, 0.005)}});
        pids.append({0x55, "Short term secondary oxygen sensor trim, banks 1 and 3", 2, {
            number("ShortTermSecondaryO2Trim_B1", "%", 0, 8, TRIM, -100.0),
            number("ShortTermSecondaryO2Trim_B3", "%", 8, 8, TRIM, -100.0)}});
        pids.append({0x56, "Long term secondary oxygen sensor trim, banks 1 and 3", 2, {
            number("LongTermSecondaryO2Trim_B1", "%", 0, 8, TRIM, -100.0),
            number("LongTermSecondaryO2Trim_B3", "%", 8, 8, TRIM, -100.0)}});
        pids.append({0x57, "Short term secondary oxygen sensor trim, banks 2 and 4", 2, {
            number("ShortTermSecondaryO2Trim_B2", "%", 0, 8, TRIM, -100.0),
            number("ShortTermSecondaryO2Trim_B4", "%", 8, 8, TRIM, -100.0)}});
        pids.append({0x58, "Long term secondary oxygen sensor trim, banks 2 and 4", 2, {
            number("LongTermSecondaryO2Trim_B2", "%", 0, 8, TRIM, -100.0),
            number("LongTermSecondaryO2Trim_B4", "%", 8, 8, TRIM, -100.0)}});
        pids.append({0x59, "Fuel rail absolute pressure", 2, {number("FuelRailAbsPressure", "kPa", 0, 16, 10.0)}});
        pids.append({0x5A, "Relative accelerator pedal position", 1, {number("RelativeAcceleratorPedal", "%", 0, 8, PERCENT)}});
        pids.append({0x5B, "Hybrid battery pack remaining life", 1, {number("HybridBatteryRemaining", "%", 0, 8, PERCENT)}});
        pids.append({0x5C, "Engine oil temperature", 1, {number("OilTemp", "degC", 0, 8, 1.0, -40.0)}});
        pids.append({0x5D, "Fuel injection timing", 2, {number("FuelInjectionTiming", "deg", 0, 16, 1.0 / 128.0, -210.0)}});
        pids.append({0x5E, "Engine fuel rate", 2, {number("FuelRate", "L/h", 0, 16, 0.05)}});
        pids.append({0x61, "Driver's demand engine percent torque", 1, {number("DemandTorque", "%", 0, 8, 1.0, -125.0)}});
        pids.append({0x62, "Actual engine percent torque", 1, {number("ActualTorque", "%", 0, 8, 1.0, -125.0)}});
        pids.append({0x63, "Engine reference torque", 2, {number("ReferenceTorque", "Nm", 0, 16)}});
        pids.append({0x64, "Engine percent torque data", 5, {
            number("TorqueIdle", "%", 0, 8, 1.0, -125.0),
            number("TorquePoint1", "%", 8, 8, 1.0, -125.0),
            number("TorquePoint2", "%", 16, 8, 1.0, -125.0),
            number("TorquePoint3", "%", 24, 8, 1.0, -125.0),
            number("TorquePoint4", "%", 32, 8, 1.0, -125.0)}});
        pids.append({0x8E, "Engine friction percent torque", 1, {number("FrictionTorque", "%", 0, 8, 1.0, -125.0)}});
        pids.append({0xA6, "Odometer", 4, {number("Odometer", "km", 0, 32, 0.1)}});

        addOxygenSensors(pids);

        std::sort(pids.begin(), pids.end(), [](const PidInfo &a, const PidInfo &b) { return a.pid < b.pid; });
        return pids;
    }
}

double Field::minimum() const
{
    double rawMin = isSigned ? -std::ldexp(1.0, bitLength - 1) : 0.0;
    double rawMax = isSigned ? std::ldexp(1.0, bitLength - 1) - 1.0 : std::ldexp(1.0, bitLength) - 1.0;
    return std::min(rawMin * scale + offset, rawMax * scale + offset);
}

double Field::maximum() const
{
    double rawMin = isSigned ? -std::ldexp(1.0, bitLength - 1) : 0.0;
    double rawMax = isSigned ? std::ldexp(1.0, bitLength - 1) - 1.0 : std::ldexp(1.0, bitLength) - 1.0;
    return std::max(rawMin * scale + offset, rawMax * scale + offset);
}

const QVector<PidInfo> &mode01Pids()
{
    static const QVector<PidInfo> pids = buildMode01Pids();
    return pids;
}

const PidInfo *findMode01Pid(quint8 pid)
{
    for (const PidInfo &info : mode01Pids())
    {
        if (info.pid == pid) return &info;
    }
    return nullptr;
}

bool isSupportBitmapPid(quint8 pid)
{
    return (pid % 0x20) == 0;
}

QList<quint8> supportedPids(quint8 bitmapPid, const QByteArray &data)
{
    QList<quint8> pids;
    if (!isSupportBitmapPid(bitmapPid) || data.size() < 4) return pids;

    for (int i = 0; i < 32; i++)
    {
        int pid = bitmapPid + i + 1;
        if (pid > 0xFF) break; //the last bit of the 0xE0 bitmap has no PID after it
        if (static_cast<quint8>(data[i / 8]) & (0x80 >> (i % 8))) pids.append(static_cast<quint8>(pid));
    }
    return pids;
}

bool extractBits(const QByteArray &data, int startBit, int bitLength, bool isSigned, qint64 &raw)
{
    if (startBit < 0 || bitLength < 1 || bitLength > 32) return false;
    if (startBit + bitLength > data.size() * 8) return false;

    quint64 value = 0;
    for (int i = startBit; i < startBit + bitLength; i++)
    {
        int bit = (static_cast<quint8>(data[i / 8]) >> (7 - (i % 8))) & 1;
        value = (value << 1) | static_cast<quint64>(bit);
    }

    raw = static_cast<qint64>(value);
    if (isSigned && (value & (1ull << (bitLength - 1)))) raw -= static_cast<qint64>(1ull << bitLength);
    return true;
}

bool decode(const PidInfo &info, const QByteArray &data, QList<Value> &values)
{
    values.clear();
    if (data.size() < info.dataBytes) return false;

    for (const Field &field : info.fields)
    {
        qint64 raw;
        if (!extractBits(data, field.startBit, field.bitLength, field.isSigned, raw)) return false;

        Value v{&field, raw, raw * field.scale + field.offset, QString()};
        for (const auto &valueName : field.valueNames)
        {
            if (valueName.first == raw)
            {
                v.text = valueName.second;
                break;
            }
        }
        values.append(v);
    }
    return true;
}

QByteArray mode01Request(quint8 pid)
{
    QByteArray request;
    request.append(static_cast<char>(SHOW_CURRENT_DATA));
    request.append(static_cast<char>(pid));
    return request;
}

bool parseMode01Reply(const QByteArray &payload, QList<Reply> &replies)
{
    replies.clear();
    if (payload.size() < 2) return false;
    if (static_cast<quint8>(payload[0]) != SHOW_CURRENT_DATA + POSITIVE_REPLY_OFFSET) return false;

    int pos = 1;
    while (pos < payload.size())
    {
        quint8 pid = static_cast<quint8>(payload[pos]);
        const PidInfo *info = findMode01Pid(pid);
        if (!info)
        {
            //A single PID we don't know: everything after it is its data
            if (pos == 1) replies.append({pid, payload.mid(pos + 1)});
            break;
        }
        if (pos + 1 + info->dataBytes > payload.size()) break;
        replies.append({pid, payload.mid(pos + 1, info->dataBytes)});
        pos += 1 + info->dataBytes;
    }
    return !replies.isEmpty();
}

QByteArray mode09Request(quint8 pid)
{
    QByteArray request;
    request.append(static_cast<char>(VEHICLE_INFO));
    request.append(static_cast<char>(pid));
    return request;
}

bool decodeMode09Text(const QByteArray &payload, quint8 pid, QString &text)
{
    text.clear();
    if (payload.size() < 4) return false;
    if (static_cast<quint8>(payload[0]) != VEHICLE_INFO + POSITIVE_REPLY_OFFSET) return false;
    if (static_cast<quint8>(payload[1]) != pid) return false;

    QByteArray chars = payload.mid(3); //byte 2 is the number of data items, always 1 on CAN
    chars.replace('\0', QByteArray()); //VINs and ECU names are padded with NULs; ECU names also have one inside
    text = QString::fromLatin1(chars).trimmed();
    return true;
}

}
