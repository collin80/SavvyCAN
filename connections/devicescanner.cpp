#include "connections/devicescanner.h"
#include "connections/picoflexray.h"

#include <QSerialPortInfo>
#include <QCanBus>
#include <QCanBusDeviceInfo>
#include <algorithm>

// --- serial adapter identification -------------------------------------------------------------
// VID/PID and descriptor strings only narrow things down - many different CAN boards share the same
// generic USB-serial chip (CH340 / FTDI / CP210x), so for those we return Unknown and let the user
// confirm the protocol. We only claim Likely when a USB vendor or a descriptor string actually hints.

DetectedDevice DeviceScanner::identifySerial(quint16 vid, quint16 pid,
                                             const QString &portName,
                                             const QString &description,
                                             const QString &manufacturer,
                                             const QString &serialNumber)
{
    DetectedDevice d;
    d.location = portName;
    d.portArg = portName;

    QString hay = (description + " " + manufacturer).toLower();
    auto has = [&hay](const char *s){ return hay.contains(QLatin1String(s)); };

    QString vidpid = QString("%1:%2").arg(vid, 4, 16, QLatin1Char('0')).arg(pid, 4, 16, QLatin1Char('0'));
    QStringList bits;
    if (vid || pid) bits << ("USB " + vidpid);
    if (!manufacturer.isEmpty()) bits << manufacturer;
    if (!serialNumber.isEmpty()) bits << ("serial " + serialNumber);
    d.detail = bits.join(", ");

    // descriptor hints first (strongest signal when present)
    if (has("gvret") || has("esp32ret") || has("macchina") || has("canbed") || has("a0")) {
        d.displayName = description.isEmpty() ? QStringLiteral("GVRET device") : description;
        d.type = CANCon::GVRET_SERIAL; d.confidence = DetectedDevice::Likely; return d;
    }
    if (has("canable") || has("slcan") || has("lawicel")) {
        d.displayName = description.isEmpty() ? QStringLiteral("LAWICEL/SLCAN device") : description;
        d.type = CANCon::LAWICEL; d.confidence = DetectedDevice::Likely; return d;
    }

    // USB vendor hints
    switch (vid) {
    case 0x303A: // Espressif (ESP32-S2/S3 native USB) - almost always an ESP32RET/GVRET build here
        d.displayName = description.isEmpty() ? QStringLiteral("ESP32 serial (GVRET?)") : description;
        d.type = CANCon::GVRET_SERIAL; d.confidence = DetectedDevice::Likely; return d;
    default: break;
    }

    // generic USB-serial chip, or no VID/PID: can't tell the protocol apart. Default-guess GVRET
    // (the most common serial CAN type here) but flag it Unknown so the user confirms.
    QString chip;
    switch (vid) {
    case 0x1A86: chip = QStringLiteral("CH340"); break;      // QinHeng
    case 0x10C4: chip = QStringLiteral("CP210x"); break;     // Silicon Labs
    case 0x0403: chip = QStringLiteral("FTDI"); break;       // FTDI
    default: break;
    }
    if (!description.isEmpty()) d.displayName = description;
    else if (!chip.isEmpty())  d.displayName = chip + QStringLiteral(" serial device");
    else                       d.displayName = QStringLiteral("Serial device");
    d.type = CANCon::GVRET_SERIAL; // default-guess only
    d.confidence = DetectedDevice::Unknown;
    return d;
}

// --- full scan ---------------------------------------------------------------------------------

QList<DetectedDevice> DeviceScanner::scan(const QVector<QString> &gvretIps,
                                          const QVector<QString> &kayakHosts)
{
    QList<DetectedDevice> out;

    // 1) USB FlexRay (libusb) - unambiguous
    if (PicoFlexRay::isAvailable()) {
        for (const QString &serial : PicoFlexRay::availableDevices()) {
            DetectedDevice d;
            const QString shortId = serial.right(8);
            d.displayName = QStringLiteral("pico-flexray %1").arg(shortId.isEmpty() ? serial : shortId);
            d.location = serial;
            d.detail = QStringLiteral("FlexRay over USB");
            d.type = CANCon::PICOFLEXRAY;
            d.portArg = serial;
            d.confidence = DetectedDevice::Certain;
            out.append(d);
        }
    }

    // 2) QtSerialBus hardware, per installed plugin - the plugin names the device.
    // Skip plugins/devices that are virtual or mere driver placeholders (they aren't attached
    // hardware and otherwise flood the list): the virtualcan plugin, and any device the plugin
    // itself flags virtual.
    for (const QString &plugin : QCanBus::instance()->plugins()) {
        if (plugin.compare(QLatin1String("virtualcan"), Qt::CaseInsensitive) == 0) continue;
        QString err;
        const QList<QCanBusDeviceInfo> devs = QCanBus::instance()->availableDevices(plugin, &err);
        for (const QCanBusDeviceInfo &info : devs) {
            if (info.isVirtual()) continue; //drop virtual/non-hardware channels
            DetectedDevice d;
            d.displayName = info.description().isEmpty()
                              ? QStringLiteral("%1: %2").arg(plugin, info.name())
                              : info.description();
            d.location = info.name();
            QStringList bits; bits << plugin;
            if (!info.serialNumber().isEmpty()) bits << ("serial " + info.serialNumber());
            d.detail = bits.join(", ");
            d.type = CANCon::SERIALBUS;
            d.driver = plugin;
            d.portArg = info.name();
            d.confidence = DetectedDevice::Certain;
            out.append(d);
        }
    }

    // 3) Serial ports - identified by USB metadata (best guess for generic chips)
    for (const QSerialPortInfo &p : QSerialPortInfo::availablePorts()) {
        const quint16 vid = p.hasVendorIdentifier() ? p.vendorIdentifier() : 0;
        const quint16 pid = p.hasProductIdentifier() ? p.productIdentifier() : 0;
        out.append(identifySerial(vid, pid, p.portName(), p.description(),
                                  p.manufacturer(), p.serialNumber()));
    }

    // 4) Network devices already discovered via UDP broadcast
    for (const QString &ip : gvretIps) {
        DetectedDevice d;
        d.displayName = QStringLiteral("GVRET @ %1").arg(ip);
        d.location = ip;
        d.detail = QStringLiteral("network GVRET (UDP broadcast)");
        d.type = CANCon::REMOTE;
        d.portArg = ip;
        d.confidence = DetectedDevice::Certain;
        out.append(d);
    }
    for (const QString &host : kayakHosts) {
        DetectedDevice d;
        d.displayName = QStringLiteral("socketcand @ %1").arg(host);
        d.location = host;
        d.detail = QStringLiteral("network socketcand (UDP broadcast)");
        d.type = CANCon::KAYAK;
        d.portArg = host;
        d.confidence = DetectedDevice::Certain;
        out.append(d);
    }

    // Order: physical USB hardware we enumerate directly (pico-flexray + USB-serial adapters) always
    // first, then driver-mediated QtSerialBus devices, then network devices. Stable, so insertion
    // order is kept within each tier.
    auto rankOf = [](const DetectedDevice &d) -> int {
        switch (d.type) {
        case CANCon::PICOFLEXRAY: return 0; // USB (FlexRay)
        case CANCon::GVRET_SERIAL:
        case CANCon::LAWICEL:     return 0; // USB-serial adapters
        case CANCon::SERIALBUS:   return 1; // driver-mediated hardware / plugins
        case CANCon::REMOTE:
        case CANCon::KAYAK:       return 2; // network (broadcast-discovered)
        default:                  return 3;
        }
    };
    std::stable_sort(out.begin(), out.end(), [&rankOf](const DetectedDevice &a, const DetectedDevice &b){
        return rankOf(a) < rankOf(b);
    });

    return out;
}
