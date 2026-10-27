# Build the real application sources with a QtTest entry point.
# qmake CONFIG+=release /path/to/SavvyCAN/test/framesender.pro && make
# QT_QPA_PLATFORM=offscreen ./framesender_test
SAVVYCAN_ROOT = $$clean_path($$PWD/..)
include($$SAVVYCAN_ROOT/SavvyCAN.pro)

for(entry, SOURCES): TEST_SOURCES += $$SAVVYCAN_ROOT/$$entry
for(entry, HEADERS): TEST_HEADERS += $$SAVVYCAN_ROOT/$$entry
for(entry, FORMS): TEST_FORMS += $$SAVVYCAN_ROOT/$$entry
for(entry, RESOURCES): TEST_RESOURCES += $$SAVVYCAN_ROOT/$$entry
SOURCES = $$TEST_SOURCES
HEADERS = $$TEST_HEADERS
FORMS = $$TEST_FORMS
RESOURCES = $$TEST_RESOURCES
SOURCES -= $$SAVVYCAN_ROOT/main.cpp
SOURCES += $$PWD/tst_framesender.cpp
INCLUDEPATH += $$SAVVYCAN_ROOT
QT += testlib
CONFIG += testcase console
CONFIG -= app_bundle
TARGET = framesender_test
