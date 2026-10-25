# Changes in this fork (`fork-ci`)

`fork-ci` follows [collin80/SavvyCAN](https://github.com/collin80/SavvyCAN) `QT6WIP` (synced at `3288f1f`,
2026-10-14). Since 2026-10-13, upstream includes this fork's CAN FD series (collin80/SavvyCAN#1134 to
collin80/SavvyCAN#1141) and the other open pull requests `fork-ci` carried.

`fork-ci` only adds what isn't upstream yet:

| Change | Branch | Upstream |
|---|---|---|
| Playback: "Send all frames as CAN FD" (with or without BRS); the last frames of a burst are no longer dropped | `fd-playback` | merged: [#1149](https://github.com/collin80/SavvyCAN/pull/1149) |
| ASC: CAN FD frames written as `d` lines (CANalyzer 8.x); CRTD format check no longer reads out of bounds | `fd-logs` (`ba3d4ee`) | merged: [#1150](https://github.com/collin80/SavvyCAN/pull/1150) |
| GitHub repos: load log files and DBC files from GitHub (File > Load Log File from GitHub, DBC File Manager > Load from GitHub), signed in with GitHub or a token | `github-phase1-load` | fork only for now |
| Log and DBC files loadable and savable without the file dialogs; Candump saves no longer reported as failed; a failed DBC load no longer shows another file | `github-phase0-refactor`, `github-phase1-load` | not offered yet |
| Log savers: CAN-DO saves work on Qt 6, Generic ID/Data CSV keeps frames shorter than 8 bytes, Vehicle Spy (never written) no longer offered for saving | `fix-log-savers` | not offered yet |
| Fork CI (Linux AppImage and Windows package on every push, with Qt's TLS plugins), this file, the README notice | `fork-ci` only | n/a |

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
| `github-phase0-refactor` | `fork-ci` | File dialogs split from loading and saving logs and DBC files; Candump save result | - |
| `github-phase1-load` | `fork-ci` | GitHub repos: loading logs and DBC files, sign-in, download cache | - |
| `github-shared-repos` | `fork-ci` | One GitHub repo list for logs and DBC files | - |
| `github-polish` | `fork-ci` | SavvyCAN GitHub App built in, Choose Repos..., EV-CANlogs preloaded | - |
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
- **Vehicle Spy:** logs load but can't be saved; the saver was never written (see `fix-log-savers`).
- **Short frames in other logs** (found by the `fix-log-savers` tests; upstream too, not fixed yet):
  - Vector trace: loading a frame with no data crashes (a list index out of range).
  - IXXAT MiniLog: a frame with no data loads back with one byte.
  - Cabana: frames shorter than 8 bytes load back padded to 8 bytes.
  - PCAN Viewer: a frame with no data isn't loaded.
- **Old unit tests:** `unit_tests/test_isotp_handler.cpp` and `test_uds_handler.cpp` predate `CommFrame` and don't
  compile; build the other tests by name.
- **GitHub sign-in:** the token is stored scrambled, not encrypted. Private repos need the SavvyCAN GitHub App
  installed on them (Choose Repos... in File > GitHub Account...).

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

### 2026-10-25: GitHub sign-in for everyone (`github-polish`)

- **SavvyCAN GitHub App built in:** "Sign in with GitHub" uses [github.com/apps/savvycan](https://github.com/apps/savvycan),
  so users register nothing and enter no client ID. Clicking it copies the code and opens GitHub's page; the user
  pastes it and approves.
- **Private repos:** Choose Repos... in the GitHub Account dialog installs the app on the user's (or their
  organisations') repos. A repo GitHub won't show gets a hint in the browser: sign in, or install the app on it.
- **Own app:** "Use my own GitHub App or OAuth App" keeps the client ID field for forks and organisations; the
  private-repos choice only shows for OAuth Apps.
- **Starting repo:** new users' repo list starts with the public `Tom-evnut/EV-CANlogs`.
- **Confirmed** by the user on Windows before this change: signing in with the app and loading logs and DBC files
  from `Tom-evnut/EV-CANlogs`.

### 2026-10-25: CAN-DO, Vehicle Spy and Generic CSV savers (`fix-log-savers`)

Three savers found broken by the phase 0 tests (upstream too).

- **CAN-DO:** saving wrote into an empty `QByteArray`, which Qt 6 doesn't allow (an assertion in debug builds,
  undefined behaviour in release builds). Records are now 12 bytes from the start, and an empty log no longer crashes.
  - The milliseconds were written wrong.
  - The loader and the format check now skip the record a CAN-DO file starts with, instead of failing on it.
  - They no longer read past a short last record or misread bytes of 0x80 and above.
  - A frame stamped just after the minute wraps now gets the right time.
- **Vehicle Spy:** saving reported success without writing anything. The layout of a real Vehicle Spy export isn't
  known well enough to write one, so it's no longer offered for saving (save type indexes after CAN-DO move down by
  one); loading is unchanged.
- **Generic ID/Data CSV:** frames shorter than 8 bytes loaded back with an extra zero byte (the loader counted the
  line end as a byte).
- **Tests:** `TestFileDialogSplit` round-trips CAN-DO and frames of 7, 3, 1 and 0 bytes, and checks the CAN-DO
  details. The short frames found the four other formats listed under [Known issues](#known-issues).

### 2026-10-25: one GitHub repo list for logs and DBC files (`github-shared-repos`)

- **Change:** the repos added in the GitHub browser are one list, shown when loading logs and DBC files alike; most
  repos hold both. Before, each repo was listed for logs, DBC files or both.
- **Removing** a repo now asks first, since it leaves both lists. Repos saved before are all in the one list.

### 2026-10-25: load logs and DBC files from GitHub (`github-phase1-load`)

Phase 1 of the [GitHub repos roadmap](docs/github-repos-roadmap.md):
logs and DBC files kept in GitHub repos load straight into SavvyCAN.

- **Where:** File > Load Log File from GitHub..., and Load from GitHub in the DBC File Manager. File > GitHub
  Account... signs in.
- **Sign-in:** either "Sign in with GitHub" (OAuth device flow: SavvyCAN shows a code to enter on github.com; needs an
  OAuth App's client ID, entered in the GitHub Account dialog) or a pasted personal access token. Public repos work
  without signing in.
- **Browsing:** saved repos (one list for logs and DBC files), branches, the file tree filtered by file type and by
  name.
- **How files load:** the file is downloaded into a cache folder, keeping its name, with a `.origin.json` beside it
  recording the repo, branch, path and blob SHA (which saving back will need). The existing loaders then load it, so
  every log format and autodetect work. DBC files loaded from GitHub reload from the cache at the next start.
- **Limits:** files up to 100 MB (GitHub's API); Git LFS files are recognised but not loaded yet.
- **Fixes on the way:** a DBC file that fails to load no longer shows the previously loaded file a second time (or,
  at startup, gives it the failed file's bus and settings).
- **CI:** the Windows package now includes Qt's `tls` plugins and the Linux AppImage the `tls` plugins, without which
  no HTTPS request works.
- **Tests:** `TestGitHub` runs the API client and the device flow against a fake GitHub on a local port (paging,
  errors, rate limit, cancelled sign-ins, a client deleted mid-request), plus the cache, settings, and both dialogs.

### 2026-10-25: log and DBC loading/saving without the file dialogs (`github-phase0-refactor`)

Phase 0 of the GitHub repos roadmap: groundwork, no visible change apart from the Candump fix.

- **Split:** `FrameFileIO::loadFilters()`/`saveFilters()` with `loadFrameFileAs()`/`saveFrameFileAs()`, and
  `DBCHandler::loadFilters()`/`saveFilters()` with `loadFileAs()`/`saveDBCFileAs()`, load or save by file type
  without a file dialog. The dialogs use them; the DBC window's file types keep their translations.
- **Fix:** saving as Candump/Kayak reported failure even when it worked, so the main window kept the old file name.
- **Tests:** `TestFileDialogSplit` saves and reloads each log type and a DBC file. It found the broken CAN-DO and
  Vehicle Spy savers listed under [Known issues](#known-issues).

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
