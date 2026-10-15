# SavvyCAN FlexRay reverse-engineering: shortcomings and improvement plan

Scope: improving FlexRay protocol reverse engineering in SavvyCAN (Tom-evnut fork), using the
pico-flexray capture device. Based on the `flexray-pico` (capture) and `flexray-pico-inject`
(send) branches.

The current branches make FlexRay *usable*: slots arrive, decode against a DBC, and a slot's
payload can be overridden. The gaps below are about turning that into a strong RE workflow.

---

## 1. Shortcomings

### Capture / parser (`connections/picoflexrayparser.cpp`)

| # | Shortcoming | Why it hurts RE |
|---|---|---|
| C1 | **Null frames are dropped.** Any frame with the null-frame indicator clear is discarded. | A slot toggling between null and populated is itself a signal (gating / feature-active). You can't see which slots exist but are idle, nor when a slot goes quiet. |
| C2 | **Cycle count is stuffed into payload byte 0.** Works with the pico DBC convention, but overloads real data. | Conflates protocol metadata with payload. Bit-level RE tools count byte 0 as data; a slot whose real byte 0 matters is shifted. Multiplexing only works if every DBC follows the convention. |
| C3 | **Header metadata is discarded**: indicators (sync, startup, payload preamble), payload length (words), header CRC (11-bit), frame CRC (24-bit). | No CRC-valid flag, no payload-preamble (NM/message-ID present) visibility, no way to spot malformed frames — all useful RE cues. |
| C4 | **No device timestamps.** Frames are host-stamped when the USB batch arrives (~1 ms jitter, batched). | FlexRay is TDMA with a 64-cycle schedule (~5 ms/cycle). Without per-frame device time you can't reconstruct the slot schedule, measure jitter, or align cycles reliably. **Biggest RE limitation.** |
| C5 | **Source bitmask is flattened to a bus number.** The firmware's multi-source info (a frame seen on FR1+FR2, etc.) collapses to one bus. | Loses "seen on both sides" information that distinguishes genuine ECU traffic from forwarded echoes. |

### Model / UI (`can_structs.h`, `canframemodel.cpp`, DBC, RE tools)

| # | Shortcoming | Why it hurts RE |
|---|---|---|
| M1 | **No dedicated Slot / Cycle columns.** Slot shows in the ID column; cycle is hidden in payload byte 0. | Can't sort/group/filter by cycle; the schedule view is invisible. |
| M2 | **DBC multiplexing relies on the byte-0 cycle convention.** | Fragile; a standard FlexRay DBC (cycle as a real multiplexor on a header field) won't line up. |
| M3 | **No static/dynamic segment or slot-timeline view.** Each slot is treated like a CAN message ID. | No picture of the TDMA schedule, which is how FlexRay RE usually starts. |
| M4 | **RE analysis tools don't know about cycle multiplexing.** Frequency-of-change, bitflip, bisect and graphing treat the payload flat (incl. the cycle byte). | Multiplexed signals across cycles look like noise; false positives from the cycle byte. |
| M5 | **No AutoSAR E2E / alive-counter awareness.** CRC8 param discovery is an offline python script (`crc8_init_searcher.py`). | E2E CRC8 + alive counter is central to ADAS FlexRay RE and to building valid injections; doing it offline is slow and error-prone. |

### Playback / injection

| # | Shortcoming | Why it hurts RE |
|---|---|---|
| P1 | **Playback sends captured FlexRay as CAN/CAN FD, not FlexRay.** | Can't replay a FlexRay log back onto the bus for A/B testing. |
| P2 | **Injection is override-only and firmware-compiled.** Only slots with a rule in `flexray_injector_rules.h` can be written, and the lowest-ID slot can't be injected. | Adding/trying a new target slot means editing + reflashing firmware; no interactive "try this slot" from SavvyCAN. |

---

## 2. Proposed plan (phased)

Ordered by RE value per unit of effort. Phases 1–2 are the high-value core; 3–4 are accelerators.

### Phase 1 — Richer capture: timestamps + full header (firmware + parser)

The highest-value change, and it needs a small firmware fork plus a parser update together.

- **Firmware (pico-flexray fork):** append a `u32` microsecond device timestamp to each USB record,
  and stop dropping null frames (send them with a flag). Keep the record backward-detectable
  (bump a version byte or widen the length field) so the parser can tell old vs new.
- **Parser:** read the device timestamp instead of host-stamping; keep null frames (flagged);
  carry indicators, payload length, header CRC and frame CRC out of the record.
- **Frame model:** this needs somewhere to *put* that metadata — see Phase 2's `CommFrame`
  extension. Until then, stash it minimally (e.g. a flags byte) so nothing is lost.
- **Done when:** captured frames carry device time (sub-cycle resolution), null frames are visible,
  and CRC/indicator fields survive into the model.

### Phase 2 — FlexRay-aware frame model and UI

- **`CommFrame` FlexRay metadata:** add optional fields for cycle, indicators, null-frame flag,
  header-CRC-valid and frame-CRC-valid — instead of stuffing cycle into payload byte 0. Keep
  payload as the true FlexRay payload. (This ripples through load/save and the model, so do it as
  one deliberate change; see risks.)
- **Frame list columns:** add **Slot** and **Cycle** columns; mark null frames; optional CRC-valid flag.
- **DBC multiplexing:** multiplex on the real cycle field, not payload byte 0. Keep a compatibility
  path for the existing pico DBCs (e.g. `dbc/lateral.dbc`).
- **Native CSV / log formats:** round-trip the new FlexRay fields.
- **Done when:** slot and cycle are first-class, payload is unpolluted, and both a standard FlexRay
  DBC and the pico DBCs decode correctly.

### Phase 3 — RE accelerators

- **E2E / CRC helper dialog:** port `crc8_init_searcher.py` into an in-app tool — feed captured
  frames of a slot, get polynomial/init/coverage, and an alive-counter detector. Feeds both
  analysis and injection-rule authoring.
- **Cycle-aware analysis:** teach frequency-of-change, bitflip, bisect and graphing to filter/group
  by cycle so multiplexed signals resolve.
- **Slot schedule / timeline view:** a view of slots against cycle/time (needs Phase 1 timestamps)
  to show the TDMA layout and spot static vs dynamic slots.
- **Done when:** a new slot's E2E params and alive counter can be found in-app, and multiplexed
  signals show up cleanly in the analysis tools.

### Phase 4 — Playback and interactive injection

- **FlexRay playback:** replay a captured FlexRay log as FlexRay overrides (bounded by what the
  firmware's rules allow).
- **Interactive injection rules:** let SavvyCAN push a slot's trigger/cycle/E2E/replace config to
  the firmware at runtime, so trying a new target slot doesn't require editing + reflashing
  `flexray_injector_rules.h`. (Needs a matching firmware protocol addition.)
- **Done when:** a target slot can be chosen and exercised from SavvyCAN without a firmware rebuild.

---

## 3. Risks / notes

- **`CommFrame` changes ripple widely** (every file format, the model, the RE tools). Phase 2's
  metadata change is the one to design carefully and land as a single reviewed step.
- **Firmware changes mean forking pico-flexray.** Timestamps (C4/Phase 1) and runtime injection
  rules (Phase 4) both require device-side changes, kept behind a record-version bump so SavvyCAN
  still reads stock-firmware streams.
- **Keep backward compatibility** with the current record format and the pico DBCs throughout, so
  existing captures and `dbc/lateral.dbc` keep working.
- **Hardware verification is still pending** for the existing capture and inject branches — validate
  those on a real board before building Phase 1 on top.

---

## 4. Suggested sequencing

1. Bench-verify the existing `flexray-pico` / `flexray-pico-inject` branches on real hardware.
2. Phase 1 (timestamps + null + header) — firmware fork + parser, one branch.
3. Phase 2 (model + UI + DBC) — the `CommFrame` metadata change, one reviewed branch.
4. Phase 3 accelerators — independent, can be parallel once Phase 2 lands.
5. Phase 4 — after the rest is proven.
