# socketcand connection watchdog regressions

Build with the normal Qt 5 application dependencies and QtTest:

```sh
mkdir -p build-socketcand
cd build-socketcand
qmake CONFIG+=release ../test/socketcand.pro
make -j2
QT_QPA_PLATFORM=offscreen ./socketcand_test
```

The target links the full production source set with a QtTest entry point.
Four cases use real TCP sockets and a loopback server to check idle connections,
one/multiple bus recovery and status emission. A controlled socket subclass
holds ConnectingState for deterministic testing of an in-progress attempt.
Tests invoke the real watchdog directly on the main thread; they do not wait
for the ten-second timer or run the socketcand handshake/worker thread.
Status retains its existing meaning of a connection attempt, not completed
protocol negotiation. No physical CAN hardware is used.
