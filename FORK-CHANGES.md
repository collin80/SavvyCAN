# Changes in this fork (`fork-ci`)

`fork-ci` follows [collin80/SavvyCAN](https://github.com/collin80/SavvyCAN) `QT6WIP` (synced at `3288f1f`,
2026-10-14). Since 2026-10-13, upstream includes this fork's CAN FD series (collin80/SavvyCAN#1134 to
collin80/SavvyCAN#1141) and the other open pull requests `fork-ci` carried.

`fork-ci` only adds what isn't upstream yet:

| Change | Branch | Upstream |
|---|---|---|
| Playback: "Send all frames as CAN FD" (with or without BRS); the last frames of a burst are no longer dropped | `fd-playback` | merged: [#1149](https://github.com/collin80/SavvyCAN/pull/1149) |
| ASC: CAN FD frames written as `d` lines (CANalyzer 8.x); CRTD format check no longer reads out of bounds | `fd-logs` (`ba3d4ee`) | merged: [#1150](https://github.com/collin80/SavvyCAN/pull/1150) |
| Fork CI (Linux AppImage and Windows package on every push), this file, the README notice | `fork-ci` only | n/a |

**Builds:** every push to `fork-ci` builds a Linux x64 AppImage and a Windows x64 package. Download them from the
*Fork Build (Linux + Windows)* runs under **Actions**.

## How this file is kept

Every fix or new feature that lands on `fork-ci` gets:

- an entry in the [change log](#change-log), newest first, giving the date, the branch, what changed and why;
- an update to the tables below if it changes what's supported or merged;
- its upstream pull request link in the change log once one is opened.

---

## CAN FD support

Everything here is in upstream `QT6WIP` except the rows marked "fork only".

| Interface or file | Receive FD | Send FD | FD setup (data rate, on/off) | Branch |
|---|---|---|---|---|
| GVRET (serial, TCP), e.g. ESP32RET on a LilyGO T-2CAN-FD | ✅ | ✅ opcode 20 | ✅ `GET_FD`/`SETUP_FD` from the bus settings panel, firmware permitting | `fix-gvret-qt6`, `fd-send`, `fd-gvret` |
| QtSerialBus (SocketCAN, PEAK, virtualcan, ...) | ✅ FD/BRS/ESI | ✅ | ✅ FD and data rate in the connection dialog | `fd-serialbus` |
| LAWICEL / slcan | ✅ `d/D/b/B` | ✅ | ✅ `Y1/Y2/Y4/Y5` | `fd-lawicel`, `fd-send` |
| MQTT | ✅ FD/BRS/ESI/remote flags | ✅ | n/a | `fd-network` |
| CANlogserver | ✅ `id##<flags><data>` | n/a | n/a | `fd-network` |
| socketcand | ❌ the socketcand protocol has no CAN FD commands | | | |
| CANserver | ❌ fixed 16-byte Panda records, classic only | | | |
| Native CSV | ✅ | ✅ (save) | | `fd-logs` |
| candump | ✅ `##<flags>` | ✅ | | `fd-logs` |
| Vector ASC | ✅ `CANFD` lines, and `d` lines over 8 bytes (CANalyzer 8.x, fork only) | ✅ | | `fd-logs` |
| Vector BLF | ✅ objects 100 and 101 | (no BLF saver) | | `fd-logs` |
| PCAN TRC 2.0/2.1 | ✅ `FD/FB/FE/BI` | (no TRC saver) | | `fd-logs` |
| Wireshark SocketCAN pcap | ✅ | (no pcap saver) | | `fd-logs` |
| DBC files | ✅ `VFrameFormat`, `CANFD_BRS`, signals past byte 8 | ✅ saved back | FD and BRS check boxes in the message editor | `fd-dbc` |
| Send Frames window and scripts | n/a | ✅ FD/BRS columns, `can.sendFDFrame()` | DBC CAN FD messages tick FD/BRS | `fd-send`, `fd-dbc` |
| Playback window | n/a | ✅ as loaded, or **Send all frames as CAN FD** (with or without BRS, fork only) | n/a | `fd-playback` |

**Frame list:** the frame type column shows `D` for CAN FD frames, with `B` for BRS and `E` for ESI.

**Frame model:** a frame's type and its FD flag always agree (`fd-core`).

## Branches

| Branch | Based on | Contents | Upstream PR |
|---|---|---|---|
| `fix-gvret-qt6` | `QT6WIP` | GVRET crash when saving bus settings (collin80/SavvyCAN#910); zero-length and CAN FD frame parsing (collin80/SavvyCAN#1121) | merged in [#1135](https://github.com/collin80/SavvyCAN/pull/1135) |
| `fd-send` | `fix-gvret-qt6` | Sending CAN FD: GVRET opcode 20, LAWICEL DLC and IDs, Send Frames FD/BRS columns, `can.sendFDFrame()` | merged: [#1135](https://github.com/collin80/SavvyCAN/pull/1135) |
| `fd-core` | `QT6WIP` | One CAN FD representation: frame type and FD flag kept in step; `isDataFrame()` | merged: [#1134](https://github.com/collin80/SavvyCAN/pull/1134) |
| `fd-serialbus` | `fd-core` | QtSerialBus CAN FD | merged: [#1136](https://github.com/collin80/SavvyCAN/pull/1136) |
| `fd-lawicel` | `fd-core` | LAWICEL CAN FD receive fixes and data rate | merged: [#1137](https://github.com/collin80/SavvyCAN/pull/1137) |
| `fd-logs` | `fd-core` | CAN FD in log files | merged: [#1138](https://github.com/collin80/SavvyCAN/pull/1138); `ba3d4ee` (ASC `d` lines, CRTD check) merged: [#1150](https://github.com/collin80/SavvyCAN/pull/1150) |
| `fd-network` | `fd-core` | MQTT and CANlogserver CAN FD | merged: [#1139](https://github.com/collin80/SavvyCAN/pull/1139) |
| `fd-gvret` | `fd-send` + `fd-serialbus` | GVRET `GET_FD`/`SETUP_FD` | [#1140](https://github.com/collin80/SavvyCAN/pull/1140) (draft: protocol proposal) |
| `fd-dbc` | `fd-send` + `fd-core` | DBC CAN FD messages | merged: [#1141](https://github.com/collin80/SavvyCAN/pull/1141) |
| `fd-playback` | `fd-core` | Playback: send all frames as CAN FD; last frames of a burst no longer dropped | merged: [#1149](https://github.com/collin80/SavvyCAN/pull/1149) |
| `fork-ci` | upstream `QT6WIP` | Upstream plus the changes that aren't merged yet and the fork's CI | - |

## Other pull requests `fork-ci` carried

Until they were merged upstream (all of them are now in `QT6WIP`), `fork-ci` also carried these open pull requests:
collin80/SavvyCAN#1081, collin80/SavvyCAN#1082, collin80/SavvyCAN#1083, collin80/SavvyCAN#1085,
collin80/SavvyCAN#1086, collin80/SavvyCAN#1087, collin80/SavvyCAN#1093, collin80/SavvyCAN#1094,
collin80/SavvyCAN#1095, collin80/SavvyCAN#1099 and collin80/SavvyCAN#1101.

## Known issues

- **GVRET FD layout:** the `GET_FD`/`SETUP_FD` byte layout proposed in
  [#1140](https://github.com/collin80/SavvyCAN/pull/1140) was merged upstream as proposed. ESP32RET's `t2canfd-board`
  branch in `Tom-evnut/ESP32RET` implements it; upstream ESP32RET doesn't yet.
- **socketcand and CANserver:** classic CAN only (see the CAN FD table).

## Testing

Automated tests cover every CAN FD change above:

- QtTest suites built from this code: mock GVRET and LAWICEL devices, Qt `virtualcan`, a real MQTT broker, the
  Send Frames window;
- cross-checks of the log and DBC files with python-can and cantools;
- AddressSanitizer and UndefinedBehaviorSanitizer on every run.

They run in Docker from a separate test bench repository (private for now). The GVRET paths were also tested on
LilyGO T-2CAN-FD hardware.

---

## Change log

### 2026-10-15: BLF CAN FD regression upstream (collin80/SavvyCAN#1151)

- **Merged upstream:** collin80/SavvyCAN#1149 and collin80/SavvyCAN#1150, on 2026-10-14.
- **New regression upstream:** `QT6WIP` 1321d96 has a new streaming BLF parser (collin80/SavvyCAN#1116, large-file
  loading) that dropped the CAN FD handling from collin80/SavvyCAN#1138. CAN FD BLF objects (100 and 101) load as
  classic frames, without BRS or ESI, and remote frames lose their DLC.
- **Fix:** collin80/SavvyCAN#1151 (branch `fix-blf-canfd`).
  - It restores CAN FD handling inside the new parser.
  - It makes upstream's BLF test build on Qt 6 again.
  - It adds CAN FD checks to that test.
- **`fork-ci` not affected:** it predates the new parser. It will be synced with `QT6WIP` once collin80/SavvyCAN#1151
  is merged, so the regression doesn't come in.

### 2026-10-14: remaining changes offered upstream

- **Playback:** `fd-playback` (send all frames as CAN FD; the burst fix) is open as collin80/SavvyCAN#1149.
- **ASC and CRTD:** the newer `fd-logs` fix (`ba3d4ee`: ASC `d` lines, CRTD check crash) is open as
  collin80/SavvyCAN#1150.
- **Once both are merged:** `fork-ci` will differ from upstream only by the fork's CI, README notice and this file.

### 2026-10-14: synced with upstream `QT6WIP`

- **Merged upstream:** collin80 merged the CAN FD series (collin80/SavvyCAN#1134 to collin80/SavvyCAN#1141) on
  2026-10-13, along with the other pull requests `fork-ci` carried.
- **New baseline:** `fork-ci` now follows `QT6WIP` (`3288f1f`) and keeps only `fd-playback`, the newer `fd-logs`
  fix and the fork's CI.
- **Checked:** the automated tests pass on upstream `QT6WIP` itself, apart from the two not-yet-merged `fd-logs`
  fixes. None of the merged changes was lost in collin80's merges with `master`.
- **New from upstream:** `fork-ci` picks up collin80's own work, including:
  - a search window and a UDS firmware uploader;
  - translations;
  - PCAN TRC export (collin80/SavvyCAN#1142);
  - graphing fixes (collin80/SavvyCAN#1143);
  - QtSerialBus on macOS (collin80/SavvyCAN#1145).
- **Fork CI:** `fork-build.yml` has upstream's current Linux and Windows jobs.

### 2026-10-14: ASC files from CANalyzer 8.x; crash in the CRTD check (`fd-logs`)

- **ASC from CANalyzer 8.x:** CANalyzer 8.x (and older `log2asc`) write CAN FD frames as ordinary `d` lines with
  their full length (`d 32 ...`). These now load as CAN FD, without BRS, since the line doesn't record it. Any such
  line used to fail the whole file.
- **CRTD crash (already in SavvyCAN):** the CRTD format check read past the end of an empty token on lines such as
  ASC's `0.000 1 c0 Rx ...`. Auto-detection runs that check after a failed ASC load, so opening such a file could
  crash SavvyCAN.
- **How it was found:** loading a VW ID.4 drive unit log in the automated tests. It now loads all 2,166 frames,
  1,951 of them CAN FD.
- **Upstream:** not yet in collin80/SavvyCAN#1138.

### 2026-10-12: Playback can send everything as CAN FD (`fd-playback`)

- **New option:** "Send all frames as CAN FD" in the Playback window, with "with bit rate switch (BRS)" (on by default).
  - **Why:** for logs that don't record which frames were CAN FD. In candump's screen format, for example, CAN FD
    frames of 8 bytes or less load as classic.
  - **What it changes:** every data frame is sent as CAN FD. Remote frames are sent as they are, since CAN FD has
    none, and the loaded frames aren't changed.
  - **Saved:** both settings are remembered.
- **Fixed (already in SavvyCAN):** with a burst setting over 1, playback dropped up to burst - 1 frames at the end of
  every log. The tick that reached the end returned before sending what it had queued.

### 2026-10-11: CAN FD series offered upstream

- **Pull requests:** the eight `fd-*` branches are open against `collin80/SavvyCAN` `QT6WIP` as collin80/SavvyCAN#1134 to collin80/SavvyCAN#1141,
  listed in [Branches](#branches). collin80/SavvyCAN#1135 includes the GVRET fixes from `fix-gvret-qt6`.
- **Draft:** collin80/SavvyCAN#1140 (GVRET `GET_FD`/`SETUP_FD`) is a draft until collin80 agrees the byte layout.
- **Before opening:** each branch was checked to build on its own.

### 2026-10-11: QtSerialBus timestamp code cleaned up (`fd-serialbus`)

- **Dead code removed:** `framesReceived()` built a timestamp with the static `TimeStamp::fromMicroSeconds()`,
  threw it away, and set the frame's timestamp to zero.
- **Timestamps were already right:** the frame's real timestamp is set later in the same loop. The dead code only
  made it look as if every frame had a zero timestamp (it was listed as a known issue here by mistake).
- **New test:** received frames have non-zero timestamps in arrival order.

### 2026-10-11: fixes found by the automated tests

All 11 bugs below were found on the tests' first run.

**`fd-core`: frame model**

- Asking for CAN FD on a remote or error frame no longer sets the FD flag (or BRS/ESI) without the frame type.

**`fd-lawicel`: LAWICEL**

- The CAN FD data rate (`Y<n>`) is sent when CAN FD is on. It depended on uninitialised members, so it was
  usually missing.

**`fd-serialbus`: QtSerialBus**

- Remote frames are sent as remote frames, not as zero-filled data frames.

**`fd-logs`: log files**

- **ASC:** SavvyCAN's own ASC files no longer lose their first frame.
- **ASC:** remote frame lines no longer fail the whole load.
- **ASC:** channels are read as 1-based, matching the saver, so buses no longer come back one higher.
- **ASC:** remote frames are saved without data bytes.
- **candump:** frames longer than 64 bytes (FD) or 8 bytes (classic) are skipped.
- **candump:** a blank line no longer repeats the previous frame.
- **PCAN TRC and BLF:** remote frames keep their DLC as their length.

**`fd-dbc`: DBC**

- ENUM attributes no longer collect the values of earlier ENUM attributes. This was corrupting `CANFD_BRS` on
  save.

### 2026-10-11: DBC CAN FD messages (`fd-dbc`)

- **Load and save:** `VFrameFormat` (StandardCAN_FD/ExtendedCAN_FD) and `CANFD_BRS` are read and written back.
  Messages longer than 8 bytes count as CAN FD.
- **Message editor:** "CAN FD frame" and "Bit rate switch" check boxes.
- **Send Frames:** ticks FD/BRS for CAN FD messages from the DBC file and sizes the payload.

### 2026-10-11: GVRET CAN FD setup (`fd-gvret`)

- **On connect:** SavvyCAN asks the device for its FD settings (`GET_FD`, 22).
- **Bus settings panel:** FD-capable buses get the FD controls, and changes are sent with `SETUP_FD` (21).
- **Older firmware:** devices that don't answer `GET_FD` are never sent `SETUP_FD`.
- **Bug fixed:** the `GET_FD` reply handler never returned to idle.

### 2026-10-11: network interfaces (`fd-network`)

- **MQTT:** receive honours the remote/FD/error flags; BRS (0x10) and ESI (0x20) are added in both directions.
- **CANlogserver:** reads `id##<flags><data>` CAN FD lines.

### 2026-10-11: log files (`fd-logs`)

CAN FD in six formats:

- **Native CSV:** optional flags column; old files load unchanged.
- **candump:** `##<flags>`.
- **Vector ASC:** `CANFD` lines, read and written.
- **Vector BLF:** objects 100 and 101.
- **PCAN TRC 2.x:** `FD/FB/FE/BI` types.
- **Wireshark SocketCAN pcap.**

### 2026-10-11: LAWICEL (`fd-lawicel`)

- `D` frames no longer set BRS.
- Hex DLC in the timestamp pre-parser.
- CAN FD frame type.
- A warning for unsupported data rates.

### 2026-10-11: QtSerialBus (`fd-serialbus`)

- **Receive:** FD/BRS/ESI are copied on receive.
- **Data rate:** `DataBitRateKey` is set from the data rate.
- **FD stays on:** `piStarted()` no longer turns FD off.
- **Dialog:** the connection dialog shows FD and the data rate.

### 2026-10-11: one CAN FD representation (`fd-core`)

- The frame type and the FD flag are kept in step.
- New `isDataFrame()` check.
- The frame list shows BRS and ESI.

### 2026-10-11: upstream pull requests

- Merged the 11 pull requests listed above into `fork-ci`.
- Fork CI: Linux AppImage and Windows package builds.

### 2026-10-10: sending CAN FD (`fd-send`)

- **GVRET:** opcode 20 transmit.
- **LAWICEL:** hex DLC and ID masking.
- **Send Frames:** FD/BRS columns.
- **Scripts:** `can.sendFDFrame()`.

### 2026-10-10: GVRET fixes (`fix-gvret-qt6`)

- **collin80/SavvyCAN#910:** fixed the crash when saving bus settings under Qt 6.
- **collin80/SavvyCAN#1121:** fixed zero-length and CAN FD frame parsing.
