# GVRET/native CSV timestamp regressions

Build with the normal Qt 5 application dependencies and QtTest:

```sh
mkdir -p build-nativecsv
cd build-nativecsv
qmake CONFIG+=release ../test/nativecsv.pro
make -j2
QT_QPA_PLATFORM=offscreen ./nativecsv_test
```

The target links the full production source set with a QtTest entry point.
It loads temporary CSV files through the real `FrameFileIO` importer, covering
both header versions, timestamps of zero through the one-millisecond boundary,
longer timestamps, the reported mixed log, repeated imports, metadata/payload
controls and existing missing-timestamp spacing. A long signed timestamp is
retained as a compatibility control. No CAN hardware or live capture is used.

Only valid short unsigned timestamps change behavior. The existing long-field
conversion and fallback for missing/unrecognized short fields remain unchanged.
