QT += core gui serialbus widgets testlib serialbus


CONFIG += c++11

INCLUDEPATH += ../ ../connections

SOURCES += \
    tst_lfqueue.cpp \
    tst_blf.cpp \
    main.cpp \
    tst_cancon.cpp \
    ../blfhandler.cpp \
    ../can_structs.cpp \
    ../connections/canconfactory.cpp \
    ../connections/canconnection.cpp \
    ../connections/gvretserial.cpp \
    ../connections/socketcan.cpp \
    ../canbus.cpp


#HEADERS += \
#    ../utils/lfqueue.h

target.path= .
INSTALLS += target

HEADERS += \
    tst_lfqueue.h \
    tst_blf.h \
    tst_cancon.h \
    ../blfhandler.h \
    ../can_structs.h \
    ../connections/canconconst.h \
    ../connections/canconfactory.h \
    ../connections/canconnection.h \
    ../connections/gvretserial.h \
    ../connections/socketcan.h \
    ../canbus.h
