# DBC multiplex round-trip regression

Use Qt5.14 or newer with the normal SavvyCAN build dependencies. Build this
full-production-source QtTest target in a fresh external directory:

```sh
mkdir /tmp/savvycan-mux-build
cd /tmp/savvycan-mux-build
qmake /absolute/path/to/SavvyCAN/test/multiplexroundtrip.pro
make -j4
QT_QPA_PLATFORM=offscreen ./multiplexroundtrip_test
```

The actual DBC loader/writer runs on minimal DBC inputs modeled on #1064.
Independent range tables check selector membership, including excluded values,
simple and ordinary signals, disjoint/singleton ranges, duplicate/split entries
and the supported signed-int boundary. Each valid input completes three
save/reload cycles. Malformed extended entries must warn without damaging the
simple fallback. A timer records and dismisses actual loader error dialogs;
loadFile returning true alone is not treated as error-free loading.

To retain generated input/output files, set SAVVYCAN_MUX_ARTIFACT_DIR to an
external directory. No hardware is needed.
