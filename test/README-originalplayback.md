# Original-timing playback regression

From an external build directory, with the Qt5 development modules listed in
SavvyCAN's README installed:

```
qmake /path/to/SavvyCAN/test/originalplayback.pro
make -j4
QT_QPA_PLATFORM=offscreen ./originalplayback_test
```

This target compiles SavvyCAN's production source list with a QtTest entry point.
A private test friend supplies literal elapsed microseconds to the production
original-timing scheduler, with real SequenceItem data, updatePosition, filters,
bus expansion, sending buffer and end-of-cache signals. It avoids wall-clock
sleeps and CAN hardware. It does not validate physical CAN transmission or the
GUI/worker thread handoff.

The 20 behavior cases cover first/last/future rows in both directions, multiple
due rows per tick, zero/equal timestamps, finite repeats, one-row files, filtered
IDs, file/override/all bus selection, paused/empty caches, and a synchronous end callback replacing the old cache. CurrentPosition is
the next unsent row; a complete pass ends this tick even when another repeat is
enabled, so zero-duration caches cannot spin. The next repeat starts at the
wrapped row's timestamp on the following tick; leftover elapsed time is not
carried across passes. The normal UI requires at least one repeat.

Five safe cases were run against the unchanged original scheduling algorithm
through a mechanically extracted elapsed-time seam: three failures and two
passing controls. The raw baseline seam patch/test/log are in the private work
record. Zero-duration/repeat cases were not executed on the old unbounded loop.
Fixed-burst playback, missing-ID filter lookup, cross-thread setter ownership,
empty-input start slots, and physical transport behavior are separate scope.
