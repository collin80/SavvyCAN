# Capture-order timestamp ties

Build `captureorder.pro` out of tree with Qt5 and run `captureorder_test`
(use `QT_QPA_PLATFORM=offscreen` on a headless host). It links the repository's
production source set with a QtTest entry point.

The model assigns a fresh capture ordinal when appending/importing frames. Normal
timestamp sorts use it as an ascending tie-breaker in both timestamp directions.
They sort the current visible list rather than reconstructing it, preserving
membership across filters and existing buffer eviction. Overwrite mode still
sorts its interval column; other column sorting and wire/file formats are unchanged.

Tests cover ID-then-time sorting in both directions, ID and bus filtering,
append, clear/reload, original storage, distinct/legacy unsigned timestamp keys,
overwrite intervals, and actual bounded-buffer eviction. Payload ordinals provide
an independent expected arrival order.

The metadata adds one 64-bit value to CANFrame. Rebuild all source after this
internal layout change. In the tested x86_64 build sizeof(CANFrame) grows56 to64
bytes. At the default10million-frame reserve in both vectors this adds160MB of
frame storage/address-space reservation. The timestamp sort is in-place and
does not allocate another frame-sized sorting vector. This tradeoff needs review
before upstream acceptance. This patch does not add the optional index UI column.
