# Connection ownership regression

Build outside the source tree, with Qt 5.14 or newer and the normal SavvyCAN
dependencies. This target compiles the full production source set with a QtTest
entry point:

```sh
mkdir /tmp/savvycan-ownership-build
cd /tmp/savvycan-ownership-build
qmake /absolute/path/to/SavvyCAN/test/connectionownership.pro
make -j4
QT_QPA_PLATFORM=offscreen ./connectionownership_test
```

The tests exercise real manager/model/window operations with controlled connection
objects and independent destructor counters. QPointer prevents the original
reorder failure from causing a second deletion during fixture cleanup. Tests cover
caller-owned replacement, self replacement, both reorder directions and repeated
swaps, boundary no-ops, Reset success/factory failure, and explicit removal.
The Reset success control uses the real LAWICEL factory with a nonexistent serial
path. It does not validate a hardware reconnect or change Reset's settings logic.
