# Cabana CSV import regressions

```sh
mkdir -p build-cabana
cd build-cabana
qmake CONFIG+=release ../test/cabana.pro
make -j2
QT_QPA_PLATFORM=offscreen ./cabana_test
```

The target links the production source set with a QtTest entry point. Temporary
files exercise both format recognition and loading, decimal/0x identifiers,
leading-zero decimal compatibility, prefixed payloads and the reported log
through automatic format detection. Malformed identifier/payload and missing
field cases must fail rather than create corrupted frames.

This reader supports classical CAN payloads of zero through eight bytes.
Oversized payloads are rejected; these tests do not establish CAN FD support.
Existing timestamp normalization and bus conversion remain outside this change.
