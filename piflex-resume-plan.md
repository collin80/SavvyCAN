# PiFlex — resume plan

_Updated 2026-11-15. Single pick-up point. Repos: PiFlex-FW (firmware, `main`) and
SavvyCAN (`fork-ci`, host). Sessions: `piflex-hostcfg-fw` (firmware), `savvycan-hostcfg`
(host/Qt), `pico-flexray-injection-f` (the physical two-board bench rig)._

## TOP OF BACKLOG (user will bench later): P3 bench-verify the 160 MHz fix

P3 is **root-caused and fixed in firmware PR #7 (PiFlex-FW), NOT a hardware/termination
issue** — the ~15% bridged-frame corruption was compounding sample-timing jitter from
**fractional PIO clock dividers** (at 150 MHz: RX ÷1.875, emit ÷7.5, ±7 ns dither stacking
with the bridge mirror's edge quantization → bits flip next to transitions). Fix: run
`clk_sys` at **160 MHz** so both dividers are integer (RX 2.0, emit 8.0). Builds clean,
8 host tests + a new clock_test pass. **Awaiting bench verification (deferred by user):**
1. Flash the 160 MHz build from PR #7 onto both boards.
2. Re-run the bridge test (A=Source, B=Bridge, 500 µs), export, I analyze → expect far-side
   corruption **~15% → ~0**.
3. Ideally add a **3rd PiFlex (pure sniffer) or scope** on the bridged bus to separate
   Board B bridge-TX from Board A RX-while-TX.
4. If clean → merge PR #7. If residual → firmware session has a decode-and-re-encode
   regenerator fallback queued (don't build it until the 160 MHz result is in).
Full bench run-by-run + the corrected diagnosis: `piflex-p3-fidelity-runsheet.md`.
Firmware root-cause note: `piflex-fw-bridge-timing-p3-rootcause.md` (in the PR #7 worktree).

Other backlog: runtime-check the two Qt6 ports now on fork-ci (#1168 connection reorder,
#1164 timestamp sort — compile-validated only); UI items in [[savvycan-ux-backlog]],
[[savvycan-device-labels]], and the log-notes PiFlex-config expansion.

## State snapshot — P1 + P2 DONE and merged; P3 fixed-pending-bench; 5 upstream/port fixes merged (PR #4)

- **PiFlex-FW `main`:** phases 1-5 + host-config (ops 0x92-0x96, status 0xA3) +
  **unified runtime-mode** (PR #6, squash `059d627`) — one firmware switches
  SNIFF/BRIDGE/INJECT/SOURCE at runtime via `0xA4 SET_MODE` / `0xA5 GET_MODE`,
  bus-safe transitions, boot default SNIFF.
- **SavvyCAN `fork-ci`:** host-config + format-3 (`efb4955`) + **GUI override-send +
  F1 help** (PR #3, squash `b49365d`) + **runtime mode selector** (PR #2, squash
  `85d793e`). CI green on fork-ci.
- **Open PRs:** none. All P1/P2 PRs merged, branches deleted.
- **Bench-verified on the two-board rig (unified firmware `0b7904c`):** all four modes
  via the selector — Source (generator runs), Bridge (mirror), Inject (override
  splices), Sniff (passive); device-mode readout tracks `0xA5`; tab gating correct
  (Injection tab in Inject, Generator tab in Source); clean Source→Bridge→Inject→
  Sniff transitions with **0 drops / no runaway**. GUI injection splice confirmed in
  two captures (dedicated build + unified): `AA BB` lands in slot `0x001` payload
  bytes [0:2] on the injector-output buses.

## Upstream alignment — collin80/SavvyCAN `QT6WIP` (as of 2026-11-15)

`fork-ci` tracks upstream **`QT6WIP`** (the Qt6 line), not `master`. Only **2 commits behind**
(upstream #1173, intentionally skipped); well ahead with our own work. **Every beneficial
QT6WIP PR is now in `fork-ci`.** No open SavvyCAN PRs.

- **In fork-ci (done):**
  - #1181 timestamp-wrap, #1177 + #1179 SocketCANd — clean cherry-picks (PR #4).
  - #1168 connection-ownership use-after-free + #1164 sort tie-order — **ported** to our
    diverged Qt6 `CANConManager`/`CommFrameModel` (PR #4).
  - #1144 Frame ID highlight + per-ID count + overview bar — ported, hand-merged the
    scroll-area `.ui` (PR #5).
  - #1113 real-time search — our fork already had the ID search bar, so only the **payload/data
    search** was added (PR #6).
  - #708 changed-byte highlight (overwrite mode) — **lean** port: a new `CanDataItemDelegate`
    highlights changed bytes without touching `CommFrameModel::data()` (no FlexRay/LIN
    regression); automatic in overwrite+hex mode, no toggle. **Visually verified** (PR #7).
- **Already in our Qt6 base (no action):** #1161 (via #1035), #1170 (via Tom's #1138), #1160.
- **Skipped by user:** #1173 (macOS serial-port fix — the 2 commits we're behind; clean to
  cherry-pick anytime for byte parity), #1174 (translations), #1105 (filter group selectors),
  #1126 (Chinese UI + Windows installer).
- **Remaining candidate — parked:** #835 gs_usb (CANable / CANtact / CAN Bus Debugger on
  Windows). The big one — external **LGPL `candle_dll`** build integration + new connection
  backend + needs the hardware to verify. Waiting on a ~£12 CANable-compatible CAN-FD adapter
  to arrive and confirming it runs **gs_usb / candleLight** firmware (some cheap ones are
  slcan-only, which #835 doesn't use). Only worth the port once that's confirmed.
- **Runtime-check pending (compile-validated by CI only):** #1168 (reorder connections),
  #1164 (sort by timestamp with equal-time rows), #1144 (toggle frame-ID highlight). #1113 and
  #708 are effectively verified in use.
- SavvyCAN `fix(...)` PRs on `master` (#1161/#1164/#1168/#1170) are all accounted for.

## Key behavioral findings (carry forward — already corrected in help text)

- **Override is cycle-targeted.** The `0x90` override carries ONE `cycle_base`;
  firmware `fr_inject_match` does an exact cycle match, so the splice fires only on
  the cycle equal to the **"Cycle base"** field (not every cycle). The rule's
  **"Cycle mask" is counter-only** (governs rule-hit counting), it does NOT widen the
  override across cycles. To override every cycle today you'd send 64 overrides, or
  add a firmware "any-cycle" override sentinel (noted as a PiFlex-FW follow-up).
- **"Rule matches" (0xA3) counts rule-hits, not applied splices.** It climbs on
  matching traffic even with no `0x90` sent. The ONLY proof a splice landed is the
  capture (bytes on the wire). (Label + help fixed in PR #3; counter byte[8..11].)
- Injector is OFF under listen-only; the host warns the override will be dropped
  until listen-only is un-ticked. Correct safety behavior.

## P3 — the only remaining item: high-rate bridge-cascade fidelity

Across captures the **generator output is byte-perfect at source**, but the two-board
back-to-back **bridge/inject cascade at a very fast rate** degrades fidelity:
- `flexray testing.csv`: corruption grows per hop (~7% one hop → ~37% two hops).
- `unified test.csv`: slot-1 **54.5% CRCERR** (a MIX of legitimate Bridge-mode
  cross-channel CRC mismatches — a channel-A CRC on an FR2 frame reads as error, which
  is correct spec behavior — AND real cascade corruption) + a trickle of **spurious
  slot `0x000`** (~6,800).
- Even on the targeted cycle, only **~38% of cycle-0 frames spliced** (store-and-forward
  can't catch every frame at this rate).
**Do:** re-run the generator at **slower periods (500 µs, 1 ms)** on the two-board rig,
re-export, compare corruption vs rate. Rate-dependent ⇒ a fidelity/timing limit (have
the firmware session look at the bridge/inject regeneration timing margin at high fps);
rate-independent ⇒ suspect rig **termination** on the ring segments. To separate "real
corruption" from "legit cross-channel CRC," isolate a single mode (pure Source, or a
one-way bridge) when measuring.

## P4 — DONE

fork-ci Fork Build for the format-3 merge (`efb4955`, run 42490414761) confirmed green
(Windows + Linux). PR #3 (run 43481369284) and PR #2 (run 43484672377) also green.

## Artifacts on disk

- Unified UF2 (flashed on BOTH bench boards): `C:\Users\tomde\bench-staging\two-board\piflex_0b7904c_unified.uf2`
- Dedicated UF2s: `..._BRIDGE_bidir.uf2`, `..._hostcfg_INJECT_oneway.uf2`, `..._hostcfg_SOURCE.uf2` (same folder)
- Unified-mode exe: `C:\Users\tomde\devtools\SavvyCAN-unified-pr2\SavvyCAN.exe` (= merged fork-ci)
- Bench boards: `picoflexEB103F0B22C94A07`, `picoflexC6D0C862AC3322B9`; wiring A FR1→B FR2, B FR1→A FR2.
- Captures: `C:\Users\tomde\Downloads\{flexray testing, test overrride, test 2 override, unified test}.csv`

## Frozen contracts

- Host-config ops `0x92`-`0x96`, status `0xA3` (16 B, byte[7]=source_slot_drops, byte[8..11]=rule-match count).
- Mode enum **0=SNIFF 1=BRIDGE 2=INJECT 3=SOURCE**; `0xA4 SET_MODE` (OUT, wValue=mode), `0xA5 GET_MODE` (IN, 1 byte).
- Override `0x90` = `[id_lo][id_hi][cycle_base][sliceLen_lo][sliceLen_hi][E2E-CRC8 init 0xF1][payload]`; rule `0x92` = `[slot_lo][slot_hi][cyc_base][cyc_mask][win_off][win_len]`.
- Clean-room: never read dynm/pico-flexray or Damien's source; FlexRay payloads are even-length.

## Attribution (every PR)

Commit: `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`
PR: `🤖 Generated with [Claude Code](https://claude.com/claude-code)`
