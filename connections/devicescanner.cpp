#include "connections/devicescanner.h"
#include "connections/picoflexray.h"

#include <QSerialPortInfo>
#include <QSerialPort>
#include <QCanBus>
#include <QCanBusDeviceInfo>
#include <QSettings>
#include <QElapsedTimer>
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
    //matches GVRET/LAWICEL getLabelKey(): USB serial when reported, else the port name
    d.labelKey = serialNumber.isEmpty() ? portName : serialNumber;

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
            d.labelKey = serial; //getLabelKey() for pico-flexray is the USB serial (its port)
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
            //matches SerialBus getLabelKey(): USB serial when the plugin reports one, else the channel name
            d.labelKey = info.serialNumber().isEmpty() ? info.name() : info.serialNumber();
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
        d.labelKey = ip; //getLabelKey() for a network connection is its host/port string
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
        d.labelKey = host;
        d.confidence = DetectedDevice::Certain;
        out.append(d);
    }

    // Show the user's own saved label (set in the Connected Devices table, persisted per device by
    // CANConnection::getLabelKey()) for any detected device that has one - so a device the user named
    // once shows that name here. The technical name moves into the tooltip.
    {
        QSettings settings;
        for (DetectedDevice &d : out) {
            if (d.labelKey.isEmpty()) continue;
            const QString lbl = settings.value("DeviceLabels/" + d.labelKey).toString();
            if (lbl.isEmpty()) continue;
            d.detail = d.detail.isEmpty() ? d.displayName : (d.displayName + " — " + d.detail);
            d.displayName = lbl;
            d.hasUserLabel = true;
        }
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

bool DeviceScanner::probeGvret(const QString &portName, QString &details, bool &anyFd)
{
    details.clear();
    anyFd = false;

    QSerialPort port;
    port.setPortName(portName);
    port.setBaudRate(1000000); // GVRET default; over USB-CDC the rate is nominal (ignored)
    port.setFlowControl(QSerialPort::NoFlowControl);
    if (!port.open(QIODevice::ReadWrite)) return false; // busy (already connected) or not openable
    // ESP32 boards use DTR/RTS for reset + bootloader selection; keep them LOW so the probe never
    // resets the board or drops it into the ROM download mode. Matches GVRetSerial's own port setup.
    port.setDataTerminalReady(false);
    port.setRequestToSend(false);

    // 0xE7 switches an ESP32RET/GVRET board into binary mode (no reply); harmless on a non-GVRET
    // device. Then the binary commands (0xF1 + opcode): device info (0x07) and FD settings (0x16);
    // a GVRET device echoes the opcode back with its reply payload. Note: this leaves a GVRET board
    // in binary mode, which is fine for SavvyCAN (it sends 0xE7 on connect too).
    port.write(QByteArray::fromHex("E7F107F116"));
    port.flush();

    QByteArray r;
    QElapsedTimer t; t.start();
    while (t.elapsed() < 600 && r.size() < 512) {
        if (port.waitForReadyRead(120)) r.append(port.readAll());
        else if (!r.isEmpty()) break; // got something then a quiet gap - reply is complete
    }
    port.close();

    bool ok = false;
    int buildNum = -1, numBuses = -1;
    QStringList busBits;

    // A GVRET board whose buses are enabled (the T2 saves bus enable in NVS, so this is the normal
    // case) streams CAN frames alongside the replies once it's in binary mode - and an 0xF1 0x07 or
    // 0xF1 0x16 can appear inside frame data. So a match must be a *well-formed* reply of the exact
    // expected length AND be followed by end-of-buffer or another 0xF1 (the next framed message).
    // (Hardening suggested by the T2 firmware session; reasonable, not yet bench-proven on a live bus.)
    auto framedAt = [&r](int i, int len) -> bool {
        if (i + len > r.size()) return false;                                   // reply not fully present
        if (i + len < r.size() && (quint8)r[i + len] != 0xF1) return false;     // not followed by a framed msg
        return true;
    };

    for (int i = 0; i + 1 < r.size(); i++) {
        if ((quint8)r[i] != 0xF1) continue;
        const quint8 op = (quint8)r[i + 1];
        if (op == 0x07 && buildNum < 0 && framedAt(i, 8)) {
            // GET_DEVICE_INFO reply: F1 07 build_lo build_hi [eeprom][filetype][autolog][singlewire]
            buildNum = (quint8)r[i + 2] | ((quint8)r[i + 3] << 8);
            ok = true;
        } else if (op == 0x16 && numBuses < 0 && i + 2 < r.size()) {
            // GET_FD reply: F1 16 nbuses, then per bus: flags(1, bit0 = FD capable) + data rate(u32 LE)
            const int nb = (quint8)r[i + 2];
            if (nb < 1 || nb > 16 || !framedAt(i, 3 + 5 * nb)) continue;
            numBuses = nb;
            int p = i + 3;
            for (int b = 0; b < nb; b++, p += 5) {
                const quint8 flags = (quint8)r[p];
                const quint32 rate = (quint8)r[p + 1] | ((quint8)r[p + 2] << 8)
                                   | ((quint8)r[p + 3] << 16) | ((quint32)(quint8)r[p + 4] << 24);
                const bool fdCap = flags & 0x01;
                if (fdCap) anyFd = true;
                QString bb = QStringLiteral("bus%1 %2").arg(b).arg(fdCap ? "CAN FD" : "classic");
                if (fdCap && rate > 0) bb += QStringLiteral(" @ %1k").arg(rate / 1000);
                busBits << bb;
            }
            ok = true;
        }
    }

    if (!ok) return false;
    QString head = QStringLiteral("GVRET");
    if (buildNum >= 0) head += QStringLiteral(" build %1").arg(buildNum);
    if (numBuses >= 0) head += QStringLiteral(", %1 bus%2").arg(numBuses).arg(numBuses == 1 ? "" : "es");
    details = busBits.isEmpty() ? head : (head + " (" + busBits.join(", ") + ")");
    return true;
}
