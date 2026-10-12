# Changes in this fork (`fork-ci`)

`fork-ci` is [collin80/SavvyCAN](https://github.com/collin80/SavvyCAN) `QT6WIP` (at `8675061`) with:

- CAN FD support across the interfaces and log formats;
- bug fixes;
- 11 open upstream pull requests merged in.

Each change is also kept on its own branch, offered upstream as a separate pull request (see [Branches](#branches)).

**Builds:** every push to `fork-ci` builds a Linux x64 AppImage and a Windows x64 package. Download them from the
*Fork Build (Linux + Windows)* runs under **Actions**.

## How this file is kept

Every fix or new feature that lands on `fork-ci` gets:

- an entry in the [change log](#change-log), newest first, giving the date, the branch, what changed and why;
- an update to the tables below if it changes what's supported or merged;
- its upstream pull request link in the change log once one is opened.

---

## CAN FD support

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
| Vector ASC | ✅ `CANFD` lines | ✅ | | `fd-logs` |
| Vector BLF | ✅ objects 100 and 101 | (no BLF saver) | | `fd-logs` |
| PCAN TRC 2.0/2.1 | ✅ `FD/FB/FE/BI` | (no TRC saver) | | `fd-logs` |
| Wireshark SocketCAN pcap | ✅ | (no pcap saver) | | `fd-logs` |
| DBC files | ✅ `VFrameFormat`, `CANFD_BRS`, signals past byte 8 | ✅ saved back | FD and BRS check boxes in the message editor | `fd-dbc` |
| Send Frames window and scripts | n/a | ✅ FD/BRS columns, `can.sendFDFrame()` | DBC CAN FD messages tick FD/BRS | `fd-send`, `fd-dbc` |
| Playback window | n/a | ✅ as loaded, or **Send all frames as CAN FD** (with or without BRS) | n/a | `fd-playback` |

**Frame list:** the frame type column shows `D` for CAN FD frames, with `B` for BRS and `E` for ESI.

**Frame model:** a frame's type and its FD flag always agree (`fd-core`).

## Branches

| Branch | Based on | Contents | Upstream PR |
|---|---|---|---|
| `fix-gvret-qt6` | `QT6WIP` | GVRET crash when saving bus settings (collin80/SavvyCAN#910); zero-length and CAN FD frame parsing (collin80/SavvyCAN#1121) | in [#1135](https://github.com/collin80/SavvyCAN/pull/1135) |
| `fd-send` | `fix-gvret-qt6` | Sending CAN FD: GVRET opcode 20, LAWICEL DLC and IDs, Send Frames FD/BRS columns, `can.sendFDFrame()` | [#1135](https://github.com/collin80/SavvyCAN/pull/1135) |
| `fd-core` | `QT6WIP` | One CAN FD representation: frame type and FD flag kept in step; `isDataFrame()` | [#1134](https://github.com/collin80/SavvyCAN/pull/1134) |
| `fd-serialbus` | `fd-core` | QtSerialBus CAN FD | [#1136](https://github.com/collin80/SavvyCAN/pull/1136) |
| `fd-lawicel` | `fd-core` | LAWICEL CAN FD receive fixes and data rate | [#1137](https://github.com/collin80/SavvyCAN/pull/1137) |
| `fd-logs` | `fd-core` | CAN FD in log files | [#1138](https://github.com/collin80/SavvyCAN/pull/1138) |
| `fd-network` | `fd-core` | MQTT and CANlogserver CAN FD | [#1139](https://github.com/collin80/SavvyCAN/pull/1139) |
| `fd-gvret` | `fd-send` + `fd-serialbus` | GVRET `GET_FD`/`SETUP_FD` | [#1140](https://github.com/collin80/SavvyCAN/pull/1140) (draft: protocol proposal) |
| `fd-dbc` | `fd-send` + `fd-core` | DBC CAN FD messages | [#1141](https://github.com/collin80/SavvyCAN/pull/1141) |
| `fd-playback` | `fd-core` | Playback: send all frames as CAN FD; last frames of a burst no longer dropped | not yet |
| `fork-ci` | all of the above | Everything, plus the upstream PRs below and the fork's CI | - |

## Upstream pull requests merged into `fork-ci`

| PR | Author | Title |
|---|---|---|
| [#1081](https://github.com/collin80/SavvyCAN/pull/1081) | @minoseigenheer | Scripting window: resizable panes, crash fix on delete, system font |
| [#1082](https://github.com/collin80/SavvyCAN/pull/1082) | @minoseigenheer | Fix missing and mislabeled received CAN frames |
| [#1083](https://github.com/collin80/SavvyCAN/pull/1083) | @minoseigenheer | Save/restore main window UI state across sessions |
| [#1085](https://github.com/collin80/SavvyCAN/pull/1085) | @minoseigenheer | Fix app icon not showing on macOS and Windows |
| [#1086](https://github.com/collin80/SavvyCAN/pull/1086) | @minoseigenheer | Fix Cmd+W closing the active window on macOS |
| [#1087](https://github.com/collin80/SavvyCAN/pull/1087) | @minoseigenheer | Correct PCAN timestamps (uptime-based backends) |
| [#1093](https://github.com/collin80/SavvyCAN/pull/1093) | @minoseigenheer | Hex/decimal display toggle for the signal value table |
| [#1094](https://github.com/collin80/SavvyCAN/pull/1094) | @minoseigenheer | Explicit extended frame flag in the DBC message editor |
| [#1095](https://github.com/collin80/SavvyCAN/pull/1095) | @minoseigenheer | Fix duplicate matchingcriteria/filterlabeling `BA_DEF_DEF_` output |
| [#1099](https://github.com/collin80/SavvyCAN/pull/1099) | @dry-75 | serialbus: create the connection timer in the worker thread |
| [#1101](https://github.com/collin80/SavvyCAN/pull/1101) | @dry-75 | serialbusconnection.cpp: restore `setLocalEcho()` |

collin80/SavvyCAN#1087 brought in a call to a `deviceStateChanged()` that doesn't exist on `QT6WIP`; `fork-ci` drops it (`2a3a57b`).

## Known issues

- **GVRET FD layout:** `GET_FD`/`SETUP_FD` use a byte layout this fork proposed. It still needs agreeing with
  collin80, since it's his protocol. It's proposed in the draft PR [#1140](https://github.com/collin80/SavvyCAN/pull/1140). The matching firmware is ESP32RET branch `t2canfd-board` in
  `Tom-evnut/ESP32RET`.
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
