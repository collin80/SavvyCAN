#-------------------------------------------------
#
# Project created by QtCreator 2015-04-25T22:57:44
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets serialport

TARGET = SavvyCAN
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    canframemodel.cpp \
    utility.cpp

HEADERS  += mainwindow.h \
    can_structs.h \
    canframemodel.h \
    utility.h

FORMS    += mainwindow.ui
