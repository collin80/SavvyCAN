#-------------------------------------------------
#
# Project created by QtCreator 2015-04-25T22:57:44
#
#-------------------------------------------------

QT = core gui printsupport qml serialbus serialport widgets help network opengl

CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT

CONFIG += c++11
CONFIG += NO_UNIT_TESTS

DEFINES += QCUSTOMPLOT_USE_OPENGL

TARGET = SavvyCAN
TEMPLATE = app

QMAKE_INFO_PLIST = Info.plist.template
ICON = icons/SavvyIcon.icns

SOURCES += main.cpp\
    connections/mqtt_bus.cpp \
    mqtt/qmqtt_client.cpp \
    mqtt/qmqtt_client_p.cpp \
    mqtt/qmqtt_frame.cpp \
    mqtt/qmqtt_message.cpp \
    mqtt/qmqtt_network.cpp \
    mqtt/qmqtt_router.cpp \
    mqtt/qmqtt_routesubscription.cpp \
    mqtt/qmqtt_socket.cpp \
    mqtt/qmqtt_ssl_socket.cpp \
    mqtt/qmqtt_timer.cpp \
    mqtt/qmqtt_websocket.cpp \
    mqtt/qmqtt_websocketiodevice.cpp \
    qcpaxistickerhex.cpp \
    re/dbccomparatorwindow.cpp \
    mainwindow.cpp \
    canframemodel.cpp \
    simplecrypt.cpp \
    utility.cpp \
    qcustomplot.cpp \
    frameplaybackwindow.cpp \
    candatagrid.cpp \
    framesenderwindow.cpp \
    framefileio.cpp \
    mainsettingsdialog.cpp \
    firmwareuploaderwindow.cpp \
    scriptingwindow.cpp \
    scriptcontainer.cpp \
    canfilter.cpp \
    can_structs.cpp \
    motorcontrollerconfigwindow.cpp \
    connections/canconnection.cpp \
    connections/serialbusconnection.cpp \
    connections/canconfactory.cpp \
    connections/gvretserial.cpp \
    connections/socketcand.cpp \
    connections/canconmanager.cpp \
    re/sniffer/snifferitem.cpp \
    re/sniffer/sniffermodel.cpp \
    re/sniffer/snifferwindow.cpp \
    dbc/dbcmessageeditor.cpp \
    dbc/dbc_classes.cpp \
    dbc/dbchandler.cpp \
    dbc/dbcloadsavewindow.cpp \
    dbc/dbcmaineditor.cpp \
    dbc/dbcnodeeditor.cpp \
    dbc/dbcsignaleditor.cpp \
    re/discretestatewindow.cpp \
    re/filecomparatorwindow.cpp \
    re/flowviewwindow.cpp \
    re/frameinfowindow.cpp \
    re/fuzzingwindow.cpp \
    re/isotp_interpreterwindow.cpp \
    re/rangestatewindow.cpp \
    re/udsscanwindow.cpp \
    connections/canbus.cpp \
    connections/canconnectionmodel.cpp \
    connections/connectionwindow.cpp \
    re/graphingwindow.cpp \
    re/newgraphdialog.cpp \
    bisectwindow.cpp \
    signalviewerwindow.cpp \
    bus_protocols/isotp_handler.cpp \
    bus_protocols/j1939_handler.cpp \
    bus_protocols/uds_handler.cpp \
    jsedit.cpp \
    frameplaybackobject.cpp \
    helpwindow.cpp \
    blfhandler.cpp \
    re/sniffer/SnifferDelegate.cpp \
    connections/newconnectiondialog.cpp \
    re/temporalgraphwindow.cpp \
    filterutility.cpp \
    pcaplite.cpp

HEADERS  += mainwindow.h \
    can_structs.h \
    canframemodel.h \
    connections/socketcand.h \
    connections/mqtt_bus.h \
    mqtt/qmqtt.h \
    mqtt/qmqtt_client.h \
    mqtt/qmqtt_client_p.h \
    mqtt/qmqtt_frame.h \
    mqtt/qmqtt_global.h \
    mqtt/qmqtt_message.h \
    mqtt/qmqtt_message_p.h \
    mqtt/qmqtt_network_p.h \
    mqtt/qmqtt_networkinterface.h \
    mqtt/qmqtt_routedmessage.h \
    mqtt/qmqtt_router.h \
    mqtt/qmqtt_routesubscription.h \
    mqtt/qmqtt_socket_p.h \
    mqtt/qmqtt_socketinterface.h \
    mqtt/qmqtt_ssl_socket_p.h \
    mqtt/qmqtt_timer_p.h \
    mqtt/qmqtt_timerinterface.h \
    mqtt/qmqtt_websocket_p.h \
    mqtt/qmqtt_websocketiodevice_p.h \
    qcpaxistickerhex.h \
    re/dbccomparatorwindow.h \
    simplecrypt.h \
    utility.h \
    qcustomplot.h \
    frameplaybackwindow.h \
    candatagrid.h \
    framesenderwindow.h \
    can_trigger_structs.h \
    framefileio.h \
    config.h \
    mainsettingsdialog.h \
    firmwareuploaderwindow.h \
    scriptingwindow.h \
    scriptcontainer.h \
    canfilter.h \
    utils/lfqueue.h \
    motorcontrollerconfigwindow.h \
    connections/canconnection.h \
    connections/serialbusconnection.h \
    connections/canconconst.h \
    connections/canconfactory.h \
    connections/gvretserial.h \
    connections/canconmanager.h \
    re/sniffer/snifferitem.h \
    re/sniffer/sniffermodel.h \
    re/sniffer/snifferwindow.h \
    dbc/dbc_classes.h \
    dbc/dbchandler.h \
    dbc/dbcloadsavewindow.h \
    dbc/dbcmaineditor.h \
    dbc/dbcsignaleditor.h \
    dbc/dbcmessageeditor.h \
    dbc/dbcnodeeditor.h \
    re/discretestatewindow.h \
    re/filecomparatorwindow.h \
    re/flowviewwindow.h \
    re/frameinfowindow.h \
    re/fuzzingwindow.h \
    re/isotp_interpreterwindow.h \
    re/rangestatewindow.h \
    re/udsscanwindow.h \
    connections/canbus.h \
    connections/canconnectionmodel.h \
    connections/connectionwindow.h \
    re/graphingwindow.h \
    re/newgraphdialog.h \
    bisectwindow.h \
    signalviewerwindow.h \
    bus_protocols/isotp_handler.h \
    bus_protocols/j1939_handler.h \
    bus_protocols/uds_handler.h \
    bus_protocols/isotp_message.h \
    jsedit.h \
    frameplaybackobject.h \
    helpwindow.h \
    blfhandler.h \
    re/sniffer/SnifferDelegate.h \
    connections/newconnectiondialog.h \
    re/temporalgraphwindow.h \
    filterutility.h \
    pcaplite.h

FORMS    += ui/candatagrid.ui \
    ui/dbccomparatorwindow.ui \
    ui/dbcmessageeditor.ui \
    ui/connectionwindow.ui \
    ui/dbcloadsavewindow.ui \
    ui/dbcmaineditor.ui \
    ui/dbcsignaleditor.ui \
    ui/dbcnodeeditor.ui \
    ui/discretestatewindow.ui \
    ui/filecomparatorwindow.ui \
    ui/firmwareuploaderwindow.ui \
    ui/flowviewwindow.ui \
    ui/frameinfowindow.ui \
    ui/frameplaybackwindow.ui \
    ui/framesenderwindow.ui \
    ui/fuzzingwindow.ui \
    ui/graphingwindow.ui \
    ui/isotp_interpreterwindow.ui \
    ui/mainsettingsdialog.ui \
    ui/mainwindow.ui \
    ui/motorcontrollerconfigwindow.ui \
    ui/newgraphdialog.ui \
    ui/rangestatewindow.ui \
    ui/scriptingwindow.ui \
    ui/snifferwindow.ui \
    ui/udsscanwindow.ui \
    ui/bisectwindow.ui \
    ui/signalviewerwindow.ui \
    helpwindow.ui \
    ui/newconnectiondialog.ui \
    ui/temporalgraphwindow.ui
    
RESOURCES += \
    icons.qrc \
    images.qrc

win32-msvc* {
   LIBS += opengl32.lib
}

win32-g++ {
   LIBS += libopengl32
}

unix {
   isEmpty(PREFIX) {
      PREFIX=/usr/local
   }
   target.path = $$PREFIX/bin
   shortcutfiles.files=SavvyCAN.desktop
   shortcutfiles.path = $$PREFIX/share/applications
   INSTALLS += shortcutfiles
   DISTFILES += SavvyCAN.desktop
}

examplefiles.files=examples
examplefiles.path = $$PREFIX/share/savvycan/examples
INSTALLS += examplefiles

iconfiles.files=icons
iconfiles.path = $$PREFIX/share/icons
INSTALLS += iconfiles

helpfiles.files=help/*
helpfiles.path = $$PREFIX/bin/help
INSTALLS += helpfiles

INSTALLS += target

