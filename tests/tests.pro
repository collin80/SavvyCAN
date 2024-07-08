QT += core gui serialbus widgets testlib serialbus

CONFIG += c++20

target.path= .

INCLUDEPATH += \
  $$PWD/../src/connections

SOURCES += \
    $$PWD/tst_lfqueue.cpp \
    $$PWD/main.cpp \
    $$PWD/tst_cancon.cpp \
    $$PWD/../src/connections/canconfactory.cpp \
    $$PWD/../src/connections/canconnection.cpp \
    $$PWD/../src/connections/gvretserial.cpp \
    $$PWD/../src/connections/socketcan.cpp \
    $$PWD/../src/connections/canbus.cpp

INSTALLS += target

HEADERS += \
    $$PWD/tst_lfqueue.h \
    $$PWD/tst_cancon.h \
    $$PWD/../src/connections/canconconst.h \
    $$PWD/../src/connections/canconfactory.h \
    $$PWD/../src/connections/canconnection.h \
    $$PWD/../src/connections/gvretserial.h \
    $$PWD/../src/connections/socketcan.h \
    $$PWD/../src/connections/canbus.h
