# ISO-TP export regression

Build with the repository's Qt5 production source set and a QtTest entry point:

```sh
mkdir -p /tmp/savvycan-isotpexport
cd /tmp/savvycan-isotpexport
qmake /absolute/path/to/SavvyCAN/test/isotpexport.pro
make -j4
QT_QPA_PLATFORM=offscreen ./isotpexport_test
```

The test creates the actual main window/model and ISO-TP window with isolated
settings, injects messages through its real slot, chooses output files in its
Save List dialog and checks the saved records. Empty/single lists, two/three
messages, an incomplete record and repeated saves are covered. Dialog timers
drive Qt's event loop; the watchdog is a failure guard, not the timing oracle.

This is a Qt5 backport regression. Current QT6WIP already scopes its accumulator
per record. Hardware, native dialogs and packaging are outside this test.
