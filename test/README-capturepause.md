# Capture pause regressions

Build with the normal Qt 5 application dependencies and QtTest:

```sh
mkdir -p build-capturepause
cd build-capturepause
qmake CONFIG+=release ../test/capturepause.pro
make -j2
QT_QPA_PLATFORM=offscreen ./capturepause_test
```

The target links the full production source set with a QtTest entry point.
It uses factory-created socketcand connections and their worker-thread parser
to check paused creation, reset, resume, and flushing. Frame text is injected
directly into the real parser; no adapter or full network handshake is tested.
Settings use an isolated temporary directory. A small connection subclass also
attempts reception inside startup to check initial pause for worker and inline
connections.
