#ifndef PICOFLEXRAY_H
#define PICOFLEXRAY_H

#include <QStringList>
#include <QList>
#include <QByteArray>
#include <QMetaType>
#include <QTimer>
#include "canconnection.h"
#include "picoflexrayparser.h"

struct libusb_context;
struct libusb_device_handle;

/*
 * FlexRay capture from a pico-flexray device (https://github.com/dynm/pico-flexray) over USB (libusb).
 * The device enumerates as a Panda (VID 0x3801, PID 0xddcc) with a serial number starting "picoflex" and streams
 * the FlexRay frames it captures on bulk endpoint 0x81: see PicoFlexRayParser for the record formats and how frames
 * are represented. On connecting it asks for record format 2 (device timestamps, CRC flags, CRC-failed frames);
 * stock firmware refuses and keeps sending format 1. Bus 0 is FR1, bus 1 is FR2. Sending: see piSendFrame().
 *
 * On Windows the device needs the WinUSB driver (install it once with Zadig). Built only when SavvyCAN is built
 * with libusb (HAVE_LIBUSB); otherwise the connection reports that it isn't available.
 *
 * The port is the device's USB serial number, or empty for the first pico-flexray device found.
 */
class PicoFlexRay : public CANConnection
{
    Q_OBJECT

public:
    static constexpr quint16 USB_VID = 0x3801;
    static constexpr quint16 USB_PID = 0xddcc;
    static constexpr const char *SERIAL_PREFIX = "picoflex";

    PicoFlexRay(QString serialNumber);
    virtual ~PicoFlexRay();

    static bool isAvailable(); //built with libusb
    static QStringList availableDevices(); //serial numbers of the connected pico-flexray devices

    //Vendor bulk-OUT ops the firmware understands (see pico-flexray src/panda_usb.c handle_vendor_out_payload).
    static constexpr unsigned char VENDOR_OUT_OVERRIDE = 0x90; //submit a replacement slice for an injection rule
    static constexpr unsigned char VENDOR_OUT_ENABLE = 0x91;   //turn the injector on or off globally
    //Host-configurable injection rules + generator schedule (frozen USB contract; the firmware builds to the
    //identical spec). All multi-byte fields are little-endian. These go to EP_BULK_OUT via vendorWrite().
    static constexpr unsigned char VENDOR_OUT_SET_INJECT_RULE    = 0x92; //define one injection rule
    static constexpr unsigned char VENDOR_OUT_CLEAR_INJECT_RULES = 0x93; //wipe all injection rules
    static constexpr unsigned char VENDOR_OUT_SET_SOURCE_SLOT    = 0x94; //define one generator (source) slot
    static constexpr unsigned char VENDOR_OUT_CLEAR_SOURCE       = 0x95; //wipe the generator schedule
    static constexpr unsigned char VENDOR_OUT_SET_SOURCE_PARAMS  = 0x96; //generator timing + run/stop
    //Vendor control-IN request (device-to-host), issued like PICOFLEX_GET_TS_INFO: a 16-byte status struct:
    //[0]='P'[1]='F'[2]=ver=1 [3]=injector_enabled [4]=inject_rule_count [5]=source_slot_count
    //[6]=source_running [7]=source_slot_drops [8..11]=u32 inject_matches [12..15]=u32 inject_drops.
    static constexpr unsigned char PICOFLEX_GET_INJECT_STATUS    = 0xa3;
    //Runtime mode selector (frozen contract, identical to the unified firmware). 0xa4 SET_MODE is a vendor control
    //OUT whose wValue carries the mode (0..3); 0xa5 GET_MODE is a vendor control IN returning one byte = active mode.
    static constexpr unsigned char PICOFLEX_SET_MODE            = 0xa4;
    static constexpr unsigned char PICOFLEX_GET_MODE            = 0xa5;

    //Device run mode (FROZEN enum, identical to the firmware). INJECT makes the injection rules relevant; SOURCE
    //makes the generator schedule relevant.
    enum Mode { MODE_SNIFF = 0, MODE_BRIDGE = 1, MODE_INJECT = 2, MODE_SOURCE = 3 };
    static bool isValidMode(int mode) { return mode >= MODE_SNIFF && mode <= MODE_SOURCE; }
    static QString modeName(int mode); //"Sniff"/"Bridge"/"Inject"/"Source", or "unknown"

    /*
     * One host-defined injection rule. The device matches a captured frame when its slot ID equals slotId and
     * (frame.cycle & cycleMask) == (cycleBase & cycleMask); cycleMask 0 means "any cycle". When a matching 0x90
     * override arrives, the firmware splices [winOffset, winOffset+winLength) of the override payload into the
     * slot's transmitted frame. SavvyCAN has no copy of the firmware's slot sizes, so it cannot validate these.
     */
    struct InjectRule {
        quint16 slotId = 0;    //FlexRay slot ID, 0..0x7FF
        quint8  cycleBase = 0; //cycle value to match
        quint8  cycleMask = 0; //0 = match any cycle
        quint8  winOffset = 0; //replace-window offset into the slot payload
        quint8  winLength = 0; //replace-window length
    };

    //One host-defined generator (source) slot the device transmits on its own schedule.
    struct SourceSlot {
        quint16    slotId = 0;              //FlexRay slot ID, 0..0x7FF
        quint8     chanMask = 0;            //bit0 = FR1 (bus A), bit1 = FR2 (bus B)
        bool       lastByteTracksCycle = false; //flags bit0: last payload byte follows the cycle counter
        QByteArray payload;                //0..254 payload bytes
    };

    //Generator timing + run/stop (op 0x96).
    struct SourceParams {
        quint32 periodUs = 0;   //frame period, microseconds
        quint32 cycleGapUs = 0; //inter-cycle gap, microseconds
        bool    enable = false; //run (true) / stop (false)
    };

    //Decoded reply of PICOFLEX_GET_INJECT_STATUS (op 0xa3).
    struct InjectStatus {
        bool    valid = false;          //header 'P''F' and version 1 present
        quint8  version = 0;
        bool    injectorEnabled = false;
        quint8  ruleCount = 0;          //rules the device currently holds
        quint8  sourceSlotCount = 0;    //generator slots the device currently holds
        bool    sourceRunning = false;
        quint8  sourceSlotDrops = 0;    //generator slots the device rejected (e.g. odd-length payload)
        quint32 totalMatches = 0;       //forwarded frames that matched an applied rule (NOT overrides applied -
                                        //a splice increments no counter; confirm a splice from the capture)
        quint32 totalDrops = 0;         //0x90 overrides dropped (no matching rule / bad length)
    };

    //--- pure byte-builders mirroring overridePayloadFor(): trivially checkable against the frozen contract ---
    //0x92 SET_INJECT_RULE  -> 7 bytes: [0x92][slot_lo][slot_hi][cycle_base][cycle_mask][win_off][win_len]
    static QByteArray setInjectRulePayload(const InjectRule &rule);
    //0x93 CLEAR_INJECT_RULES -> 1 byte: [0x93]
    static QByteArray clearInjectRulesPayload();
    //0x94 SET_SOURCE_SLOT  -> 6+N bytes: [0x94][slot_lo][slot_hi][chan_mask][len=N][flags][payload x N]
    static QByteArray setSourceSlotPayload(const SourceSlot &slot);
    //0x95 CLEAR_SOURCE_SCHEDULE -> 1 byte: [0x95]
    static QByteArray clearSourceSchedulePayload();
    //0x96 SET_SOURCE_PARAMS -> 10 bytes: [0x96][period_us u32 LE][cycle_gap_us u32 LE][enable]
    static QByteArray setSourceParamsPayload(const SourceParams &params);
    //Parse the 16-byte 0xa3 reply; returns false (and clears out) if it isn't a valid status struct.
    static bool parseInjectStatus(const QByteArray &reply, InjectStatus &out);

    //(De)serialise the config for QSettings so it survives reconnects and app restarts, and so the editor and
    //the connection read the same stored config. Pure, versioned, and independent of the wire format.
    static QByteArray serialiseInjectRules(const QList<InjectRule> &rules);
    static QList<InjectRule> deserialiseInjectRules(const QByteArray &blob);
    static QByteArray serialiseSourceSlots(const QList<SourceSlot> &slotList);
    static QList<SourceSlot> deserialiseSourceSlots(const QByteArray &blob);

    /*
     * Builds the op-0x90 bulk-OUT payload that submits frame as a replacement slice, or an empty array (with the
     * reason in error, if given) when the frame can't be injected. This is exactly what piSendFrame writes to the
     * device, so it is what the tests check.
     *
     * The frame's slot ID becomes the rule target_id, payload byte 0 (the cycle count on capture) becomes the
     * rule cycle_base the firmware matches on, and the rest of the payload (payload[1..]) is the replacement data:
     * it maps onto the slot's payload bytes, and the firmware splices the rule's replace window out of it into the
     * slot's cached frame. The firmware expects the slice to start with an AUTOSAR E2E CRC-8 (init 0xf1) over that
     * data as a transport check, which we prepend here (it is a separate byte, not a slot byte).
     *
     * SavvyCAN has no copy of the firmware's rule table, so it cannot validate the length: payload[1..] must be
     * replace_offset + replace_len bytes for the target rule, and only the replace_len bytes inside the window are
     * actually transmitted (the rest are positional padding the firmware ignores). A wrong length, slot ID or
     * cycle base is silently dropped by the device.
     */
    static QByteArray overridePayloadFor(const CommFrame &frame, QString *error = nullptr);

    //Builds the same op-0x90 bulk-OUT message as overridePayloadFor, straight from a slot ID, cycle base and the
    //slot payload (no CommFrame / isFlexRay() guard). This is what the config editor's "Send override" uses to fire
    //an injection directly, and it is exactly what overridePayloadFor produces for the same values. Returns an empty
    //array if slotId is out of range (>0x7FF) or payload is empty.
    static QByteArray overridePayload(quint16 slotId, quint8 cycleBase, const QByteArray &payload);

    //op-0x91 payload: [0x91][enabled]
    static QByteArray injectorEnablePayload(bool enabled);

    //AUTOSAR E2E Profile CRC-8 (polynomial 0x1D, MSB-first, no final XOR), matching the device firmware.
    static quint8 e2eCrc8(const unsigned char *data, int len, quint8 init);

    //QSettings group this connection stores its config under ("PicoFlex/<port>"). Only reads the immutable port,
    //so it's safe from any thread; the config editor loads the persisted config from here to avoid racing the
    //worker thread's copy.
    QString settingsGroup() const;

    //The device's last-known run mode (0..3), or -1 if never read / unsupported. Cached from the 0xa5 GET_MODE
    //reads (on connect and after setMode); safe to read from any thread. Used by the log notes.
    int lastKnownMode() const { return mLastKnownMode.loadRelaxed(); }

    //Human-readable lines describing this device's host-set config for the log notes: run mode, each injection
    //rule and each generator slot, every line prefixed with the device's label/serial. Reads the persisted
    //QSettings config (the same source the editor loads) plus lastKnownMode(); safe to call from the main thread.
    QStringList describeConfig() const;

signals:
    //emitted after a successful 0xa3 status read (the raw 16 bytes; parse with parseInjectStatus).
    void injectStatus(QByteArray reply);
    //emitted after a 0xa5 GET_MODE read (on connect and after a setMode); mode is 0..3, or -1 if the firmware
    //doesn't support the mode request. The editor shows this as the device's actual current mode.
    void deviceMode(int mode);

public slots:
    //Switch the device's run mode (0xa4 SET_MODE, wValue = mode). Only ever sent on the user's explicit choice,
    //never on connect. Reads the mode back afterwards and emits deviceMode. Safe to call from any thread.
    void setMode(int mode);
    //Request an immediate 0xa5 GET_MODE read (also done once on connect); emits deviceMode.
    void requestMode();
    //Replace the device's injection rules with these (clears, then sends each), store them for re-push on
    //reconnect, and persist them. Safe to call from any thread: marshalled to the connection's worker thread.
    void applyInjectRules(QList<PicoFlexRay::InjectRule> rules);
    void clearInjectRules();
    //Replace the device's generator schedule (slots + params) likewise.
    void applySourceSchedule(QList<PicoFlexRay::SourceSlot> slotList, PicoFlexRay::SourceParams params);
    void clearSourceSchedule();
    //Push just the generator timing / run-stop (op 0x96) without touching the slot table.
    void applySourceParams(PicoFlexRay::SourceParams params);
    //Fire one injection override (op 0x90) directly: builds [E2E-CRC8][payload] for slotId/cycleBase and writes it
    //to the bulk-OUT endpoint, bypassing the CAN-typed Send Frames path. It only reaches the bus if the firmware
    //has a matching injection rule and the injector is enabled; watch the 0xa3 match/drop counters for the result.
    void sendOverride(int slotId, int cycleBase, QByteArray payload);
    //Request an immediate 0xa3 status read (the connection also polls once a second while connected).
    void requestInjectStatus();

protected:
    virtual void piStarted();
    virtual void piStop();
    virtual void piSetBusSettings(int pBusIdx, CANBus pBus);
    virtual bool piGetBusSettings(int pBusIdx, CANBus& pBus);
    virtual void piSuspend(bool pSuspend);
    virtual bool piSendFrame(const CommFrame&);

private slots:
    void readDevice();
    void tryConnect();
    void pollInjectStatus(); //timer-driven 0xa3 read; runs in the worker thread
    void pollMode();         //0xa5 GET_MODE read; runs in the worker thread, emits deviceMode

private:
    bool openDevice();
    void closeDevice();
    void reportStatus();
    bool vendorWrite(const QByteArray &data, const char *what); //bulk-OUT write to the injector endpoint
    void applyInjectorEnable(); //sync the injector on/off state to the bus listen-only settings
    bool inWorkerThread(); //true when already running in the connection's worker thread
    void pushInjectRules();      //clear then send mInjectRules to the device (needs mHandle)
    void pushSourceSchedule();   //clear then send mSourceSlots + mSourceParams to the device (needs mHandle)
    void pushAllConfig();        //re-push everything configured (called on (re)connect, only if user-applied)
    void saveConfigToSettings();   //persist the members (so the editor restores them next session)

    PicoFlexRayParser mParser;
    QTimer *mReadTimer = nullptr;
    QTimer *mReconnectTimer = nullptr;
    QTimer *mStatusTimer = nullptr; //polls 0xa3 while connected
    libusb_context *mContext = nullptr;
    libusb_device_handle *mHandle = nullptr;
    QByteArray mReadBuffer;
    QList<CommFrame> mFrames;
    QString mLastError;
    int mRecordFormat = 1; //record format the firmware accepted (1 stock, 2 device us, 3 + fine clock/seq)
    bool mInjectorEnabled = false; //true once a bus is taken out of listen-only; sent to the device as op 0x91
    QAtomicInt mLastKnownMode { -1 }; //last mode the device reported (0xa5), cached for the log notes; -1 = unknown

    //host-configured injection rules + generator schedule (authoritative copy, re-pushed on reconnect)
    QList<InjectRule> mInjectRules;
    QList<SourceSlot> mSourceSlots;
    SourceParams mSourceParams;
    //true once the user has explicitly applied a config in this session. Gates the on-connect push so that
    //merely connecting never clears or overwrites the device's boot defaults / a free-running generator.
    bool mUserAppliedConfig = false;
    QString mLastStatusSummary; //throttles the status line in the debug log
};

Q_DECLARE_METATYPE(PicoFlexRay::InjectRule)
Q_DECLARE_METATYPE(PicoFlexRay::SourceSlot)
Q_DECLARE_METATYPE(PicoFlexRay::SourceParams)
Q_DECLARE_METATYPE(QList<PicoFlexRay::InjectRule>)
Q_DECLARE_METATYPE(QList<PicoFlexRay::SourceSlot>)

#endif // PICOFLEXRAY_H
