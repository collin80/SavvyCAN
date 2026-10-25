# SavvyCAN OBD-II poller: logging vehicle parameters next to another bus

Scope: sending OBD-II requests from SavvyCAN on one bus to read vehicle operating values (speed, RPM, coolant
temperature, battery voltage and so on) while another bus is logged, so the two can be graphed and compared on one
timeline (Tom-evnut fork). Written against `fork-ci` (2026-10-25). Each phase branches from and merges into `fork-ci`.

---

## 1. Where it stands (2026-10-25)

| Phase | State |
|---|---|
| 0: ISO-TP and UDS unit tests building again | Done (`obd2-phase0-tests`): both pass, also under ASan/UBSan; UDS payload rule written down; UDS Scanner negative reply code fixed |
| 1: PID table and decoder | Done (`obd2-phase1-pids`): 101 Mode 01 PIDs (8 of them supported-PID bitmaps), supported-PID bitmaps, replies with several PIDs, Mode 09 VIN and ECU name; `TestObd2Pids` |
| 2: poll engine | Next |
| 3: OBD-II window | Not started |
| 4: generated OBD DBC, graphing and comparison | Not started |
| 5: scripting, Mode 22 DID lists, docs | Not started |

---

## 2. Decisions

| Question | Decision |
|---|---|
| New feature or an extension | An extension of the existing ISO-TP/UDS stack, with a new poll engine and window. No new connection type, frame type or graph source |
| How replies are decoded for graphs | A DBC that SavvyCAN generates, attached to the OBD bus. Graphs, the signal viewer and saved logs then work unchanged, and a saved log can be decoded again later |
| What gets sent | Read-only services only: Mode 01, Mode 09 and (phase 5) Mode 22. Mode 04 (clear DTCs) and every writing or control service are never sent by the poller |
| One PID per request | Yes by default, so every Mode 01 reply fits in one frame and the DBC can decode it. Several PIDs per request (allowed up to 6) is an option for later |
| Open (needed from the user, section 8) | One adapter with two buses or two adapters; 11-bit or 29-bit OBD on the target cars; whether manufacturer Mode 22 DIDs are needed; the polling rate wanted |

---

## 3. What SavvyCAN already has

| Need | What exists | Where |
|---|---|---|
| Send a request, get multi-frame replies | `ISOTP_HANDLER`: single and multi-frame, 11 and 29-bit IDs (`ID > 0x7FF` sets extended), flow control frames, pad byte | `bus_protocols/isotp_handler.cpp`, `HandlerFactory::createISOTPHandler()` |
| Service names | `UDS_HANDLER` already lists the OBD-II services 01 to 0A next to the UDS ones | `bus_protocols/uds_handler.h` |
| Send, wait, time out | The UDS Scanner sends a request, waits for the reply on request ID + offset (8 by default) and moves on | `re/udsscanwindow.cpp`, **Send Frames > UDS Scanner** |
| Polling without new code | The Frame Sender sends fixed frames on a timer (`100ms` trigger); scripts have `isotp` and `uds` objects and a tick timer | `framesenderobject.cpp`, `scriptcontainer.h` |
| Requests and replies in the log | Frames SavvyCAN sends are echoed into the capture (`setLocalEcho`) with their bus number, so one log holds the OBD traffic and the other bus | `connections/canconmanager.cpp` |
| Decoding | DBC multiplexing with several levels (`SG_MUL_VAL_`); the parser's own example line is an OBD PID (`S1_PID_0D_VehicleSpeed` on 2024 = 0x7E8) | `dbc/dbchandler.cpp`, `DBC_SIGNAL::isSignalInMessage` |
| Graphs | The graph window checks `isSignalInMessage` for each frame, so multiplexed signals plot correctly | `re/graphingwindow.cpp` |
| Building a DBC in code | `DBCHandler::createBlankFile()`, `DBCFile::addMessage()`, `DBCFile::setAssocBus()` | `dbc/dbchandler.h` |
| One clock for all buses | The "system clock" timestamp setting (`Main/TimeClock`) | `mainsettingsdialog.cpp` |

So this can be done today with a script polling 0x7DF and a hand-written OBD DBC. That's clumsy: the PID
formulas are typed by hand, nothing finds which PIDs the car supports, and the timing isn't managed.

**Missing:** a table of standard PIDs with their formulas, a poll scheduler, discovery of the supported PIDs, a
window to drive it, and a generated OBD DBC.

---

## 4. Design

### 4.1 PID table and decoder (`bus_protocols/obd2_pids.{h,cpp}`)

- A table of SAE J1979 Mode 01 PIDs: number, name, unit, data bytes, and how to decode them. Most are a
  big-endian 1 or 2 byte value with a scale and offset, which maps straight onto a DBC signal, for example:

  | PID | Name | Decoding |
  |---|---|---|
  | 0x04 | Calculated engine load | A × 100 / 255 % |
  | 0x05 | Coolant temperature | A − 40 °C |
  | 0x0C | Engine speed | (256A + B) / 4 rpm |
  | 0x0D | Vehicle speed | A km/h |
  | 0x10 | MAF air flow | (256A + B) / 100 g/s |
  | 0x11 | Throttle position | A × 100 / 255 % |
  | 0x2F | Fuel level | A × 100 / 255 % |
  | 0x42 | Control module voltage | (256A + B) / 1000 V |

- PIDs with more than one value (O2 sensors: voltage and fuel trim) become several signals; bit fields (0x01
  monitor status, 0x03 fuel system status) become 1-bit or small signals with value tables.
- Each entry has a `decode(QByteArray) -> QList<value>` used by the window's live table, and the DBC signal
  definitions used by phase 4, both from the same table so they can't disagree.
- Supported-PID bitmaps: PIDs 0x00, 0x20, 0x40 … 0xC0 each return 4 bytes, one bit per following PID; the last
  bit says whether the next bitmap PID is supported.
- Mode 09: PID 0x02 (VIN, 17 characters, several frames) and 0x0A (ECU name), decoded as text.
- The table is data in C++ to start with. A file format users can add to (needed for Mode 22) comes in phase 5.
- **As built (phase 1):** `OBD2::Field` holds name, unit, start bit, bit length, scale, offset, signed and value
  names; bit 0 is the top bit of data byte A, so `(256A + B) / 4` is bits 0–15 with scale 0.25. Field names are
  unique across all PIDs and usable as DBC signal names. `parseMode01Reply` also splits replies holding several
  PIDs, ready for phase 5. PIDs whose data needs more than a scale and offset (the monitor test bits after byte A
  of 0x01 and 0x41, the multi-sensor PIDs from 0x65 on, maximum values in 0x4F/0x50) are left out or only partly
  decoded for now.

### 4.2 Poll engine (`bus_protocols/obd2_poller.{h,cpp}`)

A received `UDS_MESSAGE` keeps the whole ISO-TP payload, service byte first (`41 0C 1A F8` for an engine speed
reply), as written in `uds_handler.h` since phase 0. The poll engine reads replies that way.

A `QObject` that owns an `ISOTP_HANDLER` (flow control on, so Mode 09 and Mode 22 replies of several frames
arrive whole) and runs the polling. No UI in it, so the window and scripts can share it.

- **Addressing**

  | | Request | Reply |
  |---|---|---|
  | 11-bit to all ECUs | 0x7DF | 0x7E8 – 0x7EF |
  | 11-bit to one ECU | 0x7E0 – 0x7E7 | request + 8 |
  | 29-bit to all ECUs | 0x18DB33F1 | 0x18DAF1xx |
  | 29-bit to one ECU | 0x18DAxxF1 | 0x18DAF1xx |

- **Discovery:** asks for PID 0x00 to all ECUs, keeps every ECU that answers, then follows each ECU's bitmaps
  (0x20, 0x40 …) to list its supported PIDs. After discovery, requests go to one ECU at a time so the other ECUs
  aren't also asked.
- **Scheduling:** a list of (ECU, PID, rate). One request is outstanding at a time: the next one goes out when the
  reply arrives or the wait times out. PIDs are taken in turn, weighted by their rate. An overall limit on requests
  a second keeps the bus load and the gateway in check.
- **Timing:** wait 50 ms for a reply (P2 of ISO 15765-4), or the full 50 ms after a request to all ECUs so every ECU
  can answer. A negative reply 0x7F with code 0x78 (response pending) extends the wait to 5 s. Other negative replies
  mark that PID unsupported for that ECU.
- **Safety:** a fixed list of services the engine will send (01, 09, later 22). Anything else is refused in code,
  not only hidden in the UI.
- **Signals out:** `valueUpdated(ecu, pid, values, timestamp)`, `pidUnsupported(ecu, pid)`, `statsUpdated(...)`
  (achieved rate, timeouts, negative replies).

### 4.3 OBD-II window (`re/obd2window.{h,cpp}`, `ui/obd2window.ui`)

Under **Send Frames**, next to the UDS Scanner.

- Bus, addressing (11/29-bit), **Discover** button, list of ECUs found.
- PID list with tick boxes, filtered to what discovery found (all PIDs shown before discovery), and a rate for each
  (or one rate for all).
- **Start / Stop**, a live table (PID, name, value, unit, last update, answers, timeouts) and the overall rate
  achieved.
- **Read VIN** (Mode 09) once, shown in the window.
- Save and load poll sets (`.obd`, JSON: bus, addressing, ECUs, PIDs, rates).
- A warning when the bus's connection is listen-only, since nothing can be sent then.
- Help page `help/obd2.md`.

### 4.4 Generated OBD DBC (phase 4)

**Make DBC** builds a DBC file in memory (`createBlankFile()`), attaches it to the OBD bus (`setAssocBus()`) and
lists it in the DBC File Manager, where it can also be saved as a `.dbc` file:

- One message for each ECU that answered (0x7E8 …, or the 29-bit IDs).
- Byte 0 is the ISO-TP length and isn't a signal. Byte 1 is the root multiplexor (service reply: 0x41 for Mode
  01, 0x62 for Mode 22). Byte 2 (the PID; for Mode 22, bytes 2–3, the DID) is multiplexed under 0x41.
- Under each PID, its signals as in the PID table, big-endian, with scale, offset, unit and value tables.
- Signal names are `PID_0C_EngineSpeed` and so on, like the parser's example, so they read well in the graph
  setup.
- **Graph selected PIDs** opens the graph window with one graph for each ticked PID.
- Replies longer than one frame (VIN, long Mode 22 values) can't be decoded by a DBC. They show in the window's
  table; graphing them would need a separate source of values and is not planned.

### 4.5 Timing across buses

- Each adapter timestamps frames with its own clock. With **two adapters** (OBD on one, the logged bus on the other)
  the "system clock" setting (`Main/TimeClock`) has to be on, so both buses share one time base. The window warns
  when it's off and more than one connection is open.
- On **one adapter with two buses** (for example a dual-bus GVRET device) both buses already share the device's
  clock.
- A reply arrives a few milliseconds after the value was measured; for most comparisons that's small next to the
  poll interval, and the help page says so.

---

## 5. Phases

| Phase | Content | Branch |
|---|---|---|
| 0 (done) | Get `TestIsoTpHandler` and `TestUdsHandler` building and passing on `fork-ci` again, since everything below uses those handlers | `obd2-phase0-tests` |
| 1 (done) | `obd2_pids`: Mode 01 table, decoding, supported-PID bitmaps, Mode 09 VIN; `TestObd2Pids` | `obd2-phase1-pids` |
| 2 | `Obd2Poller`: addressing, discovery, scheduling, timeouts and response pending, the service safety list, stats; `TestObd2Poller` | `obd2-phase2-poller` |
| 3 | OBD-II window, poll sets, VIN, help page, menu entry | `obd2-phase3-window` |
| 4 | Generated OBD DBC, attached to the OBD bus; Graph selected PIDs; clock warning | `obd2-phase4-dbc` |
| 5 | `obd` scripting object; Mode 22 DIDs from a user file per car (JSON, same fields as the PID table); several PIDs per request; FORK-CHANGES entry | `obd2-phase5-extras` |

---

## 6. Testing

- `unit_tests/test_obd2_pids.cpp` (`TestObd2Pids`): every table entry decodes known replies to known values
  (worked examples from J1979), bitmap parsing across 0x00 to 0xC0, VIN from a reply of several frames.
- `unit_tests/test_obd2_poller.cpp` (`TestObd2Poller`): a fake send callback and fake replies, as in
  `TestIsoTpHandler`. Covers discovery with one and with several ECUs, 11 and 29-bit IDs, one request outstanding at
  a time, timeouts, 0x7F 0x78 then a real reply, negative replies marking a PID unsupported, the rate limit, and that
  the engine refuses Mode 04 and other services outside the safety list.
- Phase 4: the generated DBC saved and loaded again gives the same messages and signals, and decoding a recorded
  reply through the DBC gives the same value as the PID table.
- Builds and tests in the `savvycan-test:qt6.10` Docker image, as the other fork work.
- On a car or a bench ECU: discovery, a poll set at 10 PIDs, a log saved with the other bus, graphs of an OBD value
  and a signal from the other bus side by side. A recorded log of real OBD traffic goes into the test data so phase
  4 can be checked without a car.

---

## 7. Risks

- **Gateways and bus load:** many cars answer only through a gateway that limits replies (often 10–30 a second in
  all). The rate limit and the achieved-rate display make this visible; the default rate is kept low.
- **Car behaviour:** Mode 01 and 09 only read values and are normal for scan tools while driving, but some ECUs log
  a fault or slow down when polled hard. The safety list keeps out anything that changes the car.
- **Clock alignment:** two adapters without the system clock setting give timelines that don't line up (section
  4.5).
- **Mode 22:** DIDs, their scaling and the session they need differ for each make. Without a DID list for the car,
  Mode 22 isn't useful; some DIDs need an extended session, which the poller won't open in the first version.
- **Electric vehicles:** many report little over Mode 01 (often only 0x42, 0x46, 0x5B); most battery and motor
  values are Mode 22. If the target cars are EVs, phase 5 matters more.

---

## 8. Needed from the user

- Whether the OBD bus and the logged bus will be on one adapter with two buses or on two adapters.
- The target cars, and whether they use 11-bit or 29-bit OBD.
- Whether manufacturer Mode 22 DIDs are needed, and for which cars (and any DID lists already known).
- The polling rate wanted for the values that matter most.
- A car or bench ECU for phase 2 onwards, and a short recorded log of OBD traffic for the test data.
