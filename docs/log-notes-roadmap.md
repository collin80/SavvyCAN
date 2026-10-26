# SavvyCAN log notes: setup, scenario and markers saved with each log

Scope: when a log is saved, SavvyCAN asks what the log is for: how the logger was set up, the vehicle or system under
test, the scenario or flows it should capture, and what was seen. The notes travel with the log, so the log file alone
is enough, with no separate engineering notes to find. When a log is opened, its notes are shown.
Written against `fork-ci` (2026-10-25). Each phase branches from and merges into `fork-ci`.

---

## 1. Where it stands (2026-10-25)

| Phase | State |
|---|---|
| 1: notes format, native CSV block, sidecar file | Done, merged into `fork-ci` (`d7049fa`); older loader checked |
| 2: notes dialog on save, Log Notes panel | Done, merged into `fork-ci` (`bb54d73`); tried by the user on Windows (open a log, edit its notes, save, reopen) |
| 3: notes when capture starts, continuous logging, markers | Done, merged into `fork-ci`. Tried by the user on Windows: the capture-start prompt, markers on a loaded log (kept through save and reload), marker lines in graphs. Continuous logging's notes not tried by hand yet |
| 4: notes with GitHub load and save | Done on `log-notes-github`, not merged; `TestGitHub` covers it against its fake GitHub, not tried against GitHub by hand yet |

---

## 2. Decisions

| Question | Decision |
|---|---|
| Where the notes go | Inside the log for SavvyCAN's native (GVRET) CSV; a `<log file>.notes.md` file next to the log for every other format |
| Notes format | Markdown with a small `key: value` header: readable on GitHub and in any editor, and parsed back by SavvyCAN |
| When to ask | On every log save, and when capture starts (continuous logging, first bus connected). Both can be turned off |
| First version also has | Timeline markers, fields filled in from SavvyCAN's setup, templates and the last notes reused, notes loaded and saved with GitHub logs |

---

## 3. How it fits the current code

- Every log save goes through `FrameFileIO::saveFrameFileAs(path, filterIndex, frames)` (the file dialog,
  and GitHub's save in its phase 2), and every load through `loadFrameFileAs` or `autoDetectLoadFile`. The notes are
  written **after** the frames are saved and read **after** they're loaded, so no saver or loader changes, apart from
  the native CSV loader skipping notes lines.
- Native CSV save index 0 is the only format that embeds notes. Others get the sidecar file.
- `LogNotes` (`lognotes.{h,cpp}`) holds the notes, turns them to and from Markdown, and reads and writes the native
  CSV block and the sidecar file. It has no GUI, so it's tested headless.

---

## 4. The notes

### 4.1 Fields

| Field | Filled by | Markdown |
|---|---|---|
| Title | user | `title:` header |
| Author | user (remembered) | `author:` header |
| Tags | user | `tags:` header, comma separated |
| Saved | SavvyCAN | `saved:` header, ISO 8601 local time |
| SavvyCAN version | SavvyCAN | `savvycan:` header (`VERSION` in `config.h`) |
| Log file | SavvyCAN | `log:` header, the file name the notes were saved with |
| Frames, time span | SavvyCAN | `frames:` and `span:` headers; `filtered:` when a filtered list was saved |
| Vehicle or system under test | user | `## System` section |
| Logging setup (tap point, wiring, termination, tools) | user | `## Setup` section |
| Scenario: steps or flows the log should capture | user | `## Scenario` section |
| Observations, known issues | user | `## Observations` section |
| Buses: interface, port, bitrate, FD, listen-only | SavvyCAN (editable) | `## Buses` section, one `- ` line per bus |
| DBC and LDF files, with their bus | SavvyCAN (editable) | `## DBC files` section, one `- ` line per file |
| Timeline markers | user, during capture | `## Markers` section, `- <seconds> s: <text>` |

Unknown header keys and sections are kept as they are when the notes are saved again, so newer versions can add
fields without older ones dropping them.

### 4.2 Markdown

```markdown
# SavvyCAN log notes v1
title: E90 HVAC flaps: AC on/off cycle
author: tomdebree
tags: hvac, lin, bench
saved: 2026-10-25T14:03:11
savvycan: 242
log: e90-hvac-ac-cycle.csv
frames: 48213
span: 0.000000 s to 182.400000 s

## System
BMW E90 HVAC module, flap motors on the bench

## Setup
Tapped at the HVAC module connector, 120 ohm termination on the bench harness.

## Scenario
1. Ignition on
2. AC on, wait for the flaps to settle
3. AC off

## Observations
Motor 3 answers late after wake-up.

## Buses
- 0: PCAN-USB Pro FD (PCAN_USBBUS1), 500 kbit/s
- 1: APGDT001 LIN, 9600 bit/s

## DBC files
- bmw_e90.dbc (bus 0)

## Markers
- 12.400000 s: AC on
- 95.020000 s: AC off
```

- The first line marks the format and its version. Header lines (`key: value`) follow until the first `## `
  heading; each `## ` heading starts a section that runs to the next one. Sub-headings in a section use `###`.
- Marker and span times are in seconds with six decimals (microseconds), on the same clock as the frames'
  time stamps.

### 4.3 Inside a native CSV log

The same Markdown, after the last frame, each line starting with `# `:

```
39747828,000005EB,false,Rx,0,8,E8,45,85,4B,4A,28,36,69,
# # SavvyCAN log notes v1
# title: E90 HVAC flaps: AC on/off cycle
# tags: hvac%2C lin%2C bench
# ...
```

- **At the end, not the top:** format detection (`isNativeCSVFile`) reads the header and the first frame line, and
  continuous logging writes frames as they come in, so the block is added when the file is closed.
- **Commas are written as `%2C`** (and `%` as `%25`) inside the block, so no notes line ever has the five
  comma-separated fields of a frame line. The sidecar file keeps plain commas.
- **New builds** skip `#` lines when loading frames, and read the notes from the end of the file without reading it
  all again.
- **Older builds** (upstream SavvyCAN and this fork before the change) count each notes line as a bad line. Checked
  against the `fork-ci` loader before this change (2026-10-25, section 9), with 100 frames and notes full of commas,
  one of them a copy of a frame line:
  - autodetect (the default, and dropped files) says "Could not autodetect the file type", then asks "Do you want
    to salvage what could be loaded?";
  - GVRET picked by hand says "File load completed with errors", then asks the same;
  - answering Yes loads every frame, all of them right, and no extra frames from the notes.
- Other tools reading GVRET CSV may need to skip `#` lines too.

### 4.4 Sidecar file

- `<log file name>.notes.md` next to the log, keeping the log's extension (`run1.blf.notes.md`), so two logs with
  the same base name don't share notes.
- Written for every format except native CSV. On load, any format looks for it; for native CSV the block inside the
  log wins and the sidecar is only a fallback (for example notes written by hand).

---

## 5. Asking for and showing notes (phase 2, 3)

### 5.1 Notes dialog (phase 2)

- Opens after the file name is picked in **Save Log File** and **Save Filtered Log File**, and from the GitHub save
  (GitHub phase 2).
- Filled in with the current notes (those of the loaded log, or typed for the capture), or else the typed fields of
  the last notes saved; the author defaults to the computer's user name. Frames, span, version and date are filled in
  fresh each time. Buses and DBC files come from the current connections and DBC File Manager, except for notes
  loaded from a log: those keep the buses and DBC files of that log's capture (filled in only where it had none).
  All of them can be edited.
- **Templates:** a picker with **Save as Template...** and **Delete**, stored in `QSettings` (`LogNotes/Templates`).
  A template holds the user's fields only, not markers or the filled-in ones. One per test bench is the idea.
- **Save with Notes** writes the log and the notes; **Save without Notes** writes the log only; **Cancel** saves
  nothing.
- An "Ask for notes when saving logs" box, also in Preferences (`LogNotes/AskOnSave`). Without asking, notes are
  saved only when the user has typed some (from File > Log Notes..., or a capture-start prompt in phase 3).
- The typed fields of the notes saved are kept (`LogNotes/Last`) to start the next ones from.
- **An old sidecar file** with the same name as a log saved without notes is removed, after asking, so it can't
  describe the wrong log.

### 5.2 Log Notes panel (phase 2)

- A dock panel in the main window shows the notes of the loaded log: title and header, the sections, buses, DBC files,
  and markers. It opens by itself when a loaded log has notes, and is empty otherwise.
- **File > Log Notes...** opens the notes dialog for the loaded log. Saving there rewrites the notes only: the
  block at the end of a native CSV log, or the sidecar file. With no log file open (a capture), or a log opened from
  GitHub, the notes are only kept for the next save.
- **File > Show Log Notes Panel** shows or hides the panel; whether it's shown is remembered.
- **Clear frames** keeps the typed fields for the next capture and drops what described the old log.
- The file status line shows that the log has notes.

### 5.3 When capture starts (phase 3)

- The notes dialog (**Keep Notes** or **Later**) opens when **Start Continuous Logging** has opened its file, and
  when the first bus connects with an empty frame list (also when connections are restored at startup). The notes
  kept become the capture's notes and fill in the save dialog later; they start from the last notes saved, with the
  buses and DBC files of the current setup.
- Continuous logging writes the block at the end of its file when it stops (**Cease Continuous Logging**). The
  dialog is shown first (**Save with Notes** or **Save without Notes**; the log is already written, so no Cancel) so
  the observations can be added. The frame count and time span are those of the frames the continuous log got.
- Picking Start Continuous Logging and then cancelling its file dialog no longer leaves the menu saying "Cease
  Continuous Logging" with no file open.
- Turned off with "Ask for notes when capture starts" in the dialog or Preferences (`LogNotes/AskOnCapture`).
- Clearing the frame list keeps the typed fields for the next capture, without asking, and drops the markers and
  what described the old log.

---

## 6. Timeline markers (phase 3)

- **Ctrl+M** (File > Add Marker...), the **Add Marker** button under Clear Frames, or the one in the Log Notes panel
  adds a marker, with a short text box (empty text gives "Marker 3"). While capturing (a bus connected and capturing
  not suspended) it marks the newest frame; otherwise the frame clicked in the frame list, or the newest one. (The
  frame list selects single cells, so it's the current cell's row; the first build used whole selected rows and
  always marked the newest frame, found by the user on Windows.)
- Markers are kept in the current notes, in time order, and saved with them. On a loaded log whose notes can be
  written back, a marker is saved into the log (or its `.notes.md`) at once, as File > Log Notes... does.
- **Log Notes panel:** a markers list under the notes; double-clicking one selects the nearest frame in the frame
  list (and turns off auto scroll so it stays there).
- **Graphs:** each marker is a labelled vertical dashed line (a `QCPItemStraightLine` and a `QCPItemText`) at the x
  the graphs give a frame at that time, in every time display mode, with its text at the bottom of the plot (at the
  top it ran into the X/Y readout; the user asked for it below). View > Show Log Markers turns them off
  (`Graphing/ShowLogMarkers`). Remove All Graphs keeps them. Tried by the user on Windows.
- **Frame list:** marker times are shown in the panel only; the frame list itself doesn't get extra rows.

---

## 7. GitHub (phase 4, and GitHub phase 2)

The GitHub window knows of "companion files" (`GitHubBrowserDialog::setCompanionSuffixes`): files named
`<file><suffix>` that go with a file. The log windows set `.notes.md`; the GitHub code itself knows nothing of notes.

- **Load:** opening a log from GitHub also downloads `<file>.notes.md` from the same commit, if there is one, into the
  cache next to the log (with its own `.origin.json`), so the normal sidecar lookup finds it. A notes file cached
  earlier that GitHub no longer has is removed, so it can't be shown as the log's. Notes inside a GVRET log need
  nothing extra. If the notes can't be downloaded, the log still opens and the user is told.
- **Browser:** a log with a notes file has a different icon and the tooltip "With run.csv.notes.md"; the notes file
  isn't listed on its own. A notes file whose log isn't there is listed as a file.
- **Save:** pressing Save in the GitHub window opens the notes dialog (the file type is known then): Save with Notes,
  Save without Notes, or Cancel (back to the GitHub window, nothing saved). Notes go inside a GVRET log; for other
  types the writer puts `<file>.notes.md` next to the log, and the window commits it after the log, as a second
  commit ("<message> (bench.csv.notes.md)") on the same branch, before any pull request. It replaces the notes file
  there, whatever its version: the log's own conflict check has already been answered. Saving again after a conflict
  starts from what was typed.
- **Saved without notes:** a notes file already on GitHub next to the log is left alone, and the result message says
  it's from an earlier save. (Deleting files on GitHub isn't in `GitHubClient` yet.)
- **Not done:** the notes' title and tags in the browser (they'd need every notes file downloaded), the notes' title
  as the commit message, one commit for both files (Git Data API, GitHub phase 4).

---

## 8. Phases

| Phase | Content | Branch |
|---|---|---|
| 1 | `LogNotes` with Markdown read/write, native CSV block (write after the frames, read from the end), sidecar file, native CSV loader skipping `#` lines, `FrameFileIO::saveLogNotes`/`loadLogNotes`; `TestLogNotes`; older loader checked | `log-notes` |
| 2 | Notes dialog on save (filled-in fields, templates, last notes), Log Notes panel on load, File > Log Notes..., preferences | `log-notes-ui` |
| 3 | Notes when capture starts, continuous logging block, timeline markers (shortcut, panel, graphs) | `log-notes-capture` |
| 4 | GitHub: download the notes with a log, notes in the browser, commit them with GitHub saves (with GitHub phase 2) | `log-notes-github` |

---

## 9. Testing

- `unit_tests/test_log_notes.cpp` (`TestLogNotes`), phase 1:
  - Markdown round trip with every field, empty notes, unknown keys and sections kept, multi-line sections,
    marker times parsed to the microsecond;
  - the native CSV block: frames load the same with and without notes, commas and `%` survive, notes longer than the
    tail read size, a log with notes and no frames still detected as native CSV, CRLF line ends;
  - the sidecar file for another format (candump), the block preferred over a sidecar, no notes found, `#` lines
    that aren't SavvyCAN's notes kept.
- Once, by hand: the `fork-ci` native CSV loader from before this change, built on its own, on a log with notes
  written by the new code (section 4.3).
- `unit_tests/test_log_notes_ui.cpp` (`TestLogNotesUi`), phase 2: the dialog shows and returns every field
  (including the ones it doesn't show), typed fields and markers, Save with/without Notes and Cancel, the "ask" box,
  OK/Cancel when editing, templates (save, list, use, delete), the last notes, and the panel (escaping, sections,
  Edit...). With `SAVVYCAN_SCREENSHOTS` set it saves pictures of the dialog and panel.
- Phase 3: `TestLogNotesUi` adds the capture-start dialog (Keep Notes, Later, its own "ask" setting), the dialog when
  continuous logging stops (no Cancel; closing it means no notes) and the panel's markers list; `TestLogNotes` adds
  marker ordering and the frame span. The main window parts (when the prompts open, the marker time, going to a
  marker, the graph lines) are built and the app starts, but are checked by hand.
- Phase 4: `TestGitHub` adds `testCompanionOpen` (the log listed with its notes, a lone notes file listed on its own,
  the notes downloaded into the cache with their origin, an old cached notes file dropped) and `testCompanionSave`
  (log and notes committed, saved again over both, saved without notes leaving GitHub's alone with a message, a
  writer that cancels saving nothing and saying nothing). The notes dialog in the GitHub save is checked by hand.
- By hand: a capture on the bench with markers, saved as native CSV and BLF, reopened in a new SavvyCAN, opened in an
  older build, and opened in a text editor and on GitHub.

---

## 10. Risks and open points

- **Older builds and other tools** see notes lines in native CSV as bad lines (section 4.3): two messages before the
  frames load. If that bothers people who share logs with others on older builds, a setting can put the notes in the
  sidecar file for native CSV too (phase 2 could add it).
- **Sidecar files get left behind** when only the log is copied. The dialog's help text says so; native CSV (notes inside
  the log) and GitHub saves (both files in one commit) keep them together.
- **Markers on a loaded log** use the log's own time stamps; logs whose time stamps were rebased (playback, crop) keep
  the markers' original times. Crop could later shift or drop markers.
- **Notes text with `## ` lines** starts a new section when read back; sub-headings should use `###`.
