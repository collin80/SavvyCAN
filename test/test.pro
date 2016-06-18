QT += widgets testlib
QT -= gui

CONFIG += c++11

INCLUDEPATH += ../utils

SOURCES += \
    tst_lfqueue.cpp


#HEADERS += \
#    ../utils/lfqueue.h

target.path= .
INSTALLS += target
