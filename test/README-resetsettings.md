# Reset settings regression

Build `resetsettings.pro` out of tree with Qt5, then run `resetsettings_test`
(`QT_QPA_PLATFORM=offscreen` is supported). It uses the full production source set.

Three actual ConnectionWindow Reset paths start with a controlled backend in
running, stopped and unconfigured states. The replacement goes through the real
LAWICEL factory using a deliberately nonexistent serial device. Tests verify the
retained constructor/cache configuration, not a physical reconnection. Selection
notifications are blocked so unrelated live-settings reads do not query a stopped
worker while establishing the fixture. Startup is synchronized before cleanup.

Additional tests cover invalid snapshot indices, rejecting reads while a worker
is active, and applying complete non-default initial configurations before startup
on worker/owner-thread backends. Fake startup records the exact settings it sees.

`getStoppedBusSettings` requires serialized stop/reset/start ownership. The worker
must already be stopped/joined; it reads cached/constructor values without queuing
to that stopped thread. `setInitialBusSettings` only seeds a fresh connection
before its first start and does not mark hardware configured. Runtime settings
still use the backend API. Reset preserves the primary bus snapshot; other buses,
driver normalization after a successful physical start, reconnect success and
the separate replacement/reordering ownership issue1128 are outside this patch.
