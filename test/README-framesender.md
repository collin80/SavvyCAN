# Custom frame sender regressions

From the repository root, with the normal SavvyCAN Qt 5 build dependencies and
QtTest installed:

```sh
mkdir -p build-framesender
cd build-framesender
qmake CONFIG+=release ../test/framesender.pro
make -j2
QT_QPA_PLATFORM=offscreen ./framesender_test
```

This target compiles the application's production sources with a QtTest entry
point. The tests use the real sender table, its cell-change signal, file loader,
modifier parser and modifier execution. They cover the payload and trigger from
issues #1152/#1153, intended byte modifications, and removing modifications.

The sender timer is stopped, and no CAN connection is created. These tests inspect
the resulting payload before backend transmission; they do not verify an adapter
or traffic on a physical CAN bus.
