QT = core gui printsupport qml serialbus serialport widgets help network opengl

TEMPLATE = app
QMAKE_INFO_PLIST = Info.plist.template
ICON = ../desktop/media/icons/SavvyIcon.icns

include( ../common.pri )

SOURCES += \
    bus_protocols/isotp_handler.cpp \
    bus_protocols/j1939_handler.cpp \
    bus_protocols/uds_handler.cpp \
    connections/canbus.cpp \
    connections/canconfactory.cpp \
    connections/canconmanager.cpp \
    connections/canconnection.cpp \
    connections/canconnectionmodel.cpp \
    connections/canlogserver.cpp \
    connections/canserver.cpp \
    connections/connectionwindow.cpp \
    connections/gvretserial.cpp \
    connections/lawicel_serial.cpp \
    connections/mqtt_bus.cpp \
    connections/newconnectiondialog.cpp \
    connections/serialbusconnection.cpp \
    connections/socketcand.cpp \
    dbc/dbc_classes.cpp \
    dbc/dbchandler.cpp \
    dbc/dbcloadsavewindow.cpp \
    dbc/dbcmaineditor.cpp \
    dbc/dbcmessageeditor.cpp \
    dbc/dbcnodeduplicateeditor.cpp \
    dbc/dbcnodeeditor.cpp \
    dbc/dbcnoderebaseeditor.cpp \
    dbc/dbcsignaleditor.cpp \
    mqtt/qmqtt_client_p.cpp \
    mqtt/qmqtt_client.cpp \
    mqtt/qmqtt_network.cpp \
    mqtt/qmqtt_frame.cpp \
    mqtt/qmqtt_routesubscription.cpp \
    mqtt/qmqtt_ssl_socket.cpp \
    mqtt/qmqtt_message.cpp \
    mqtt/qmqtt_socket.cpp \
    mqtt/qmqtt_websocket.cpp \
    mqtt/qmqtt_router.cpp \
    mqtt/qmqtt_timer.cpp \
    mqtt/qmqtt_websocketiodevice.cpp \
    re/sniffer/SnifferDelegate.cpp \
    re/sniffer/snifferitem.cpp \
    re/sniffer/snifferwindow.cpp \
    re/sniffer/sniffermodel.cpp \
    re/discretestatewindow.cpp \
    re/filecomparatorwindow.cpp \
    re/flowviewwindow.cpp \
    re/frameinfowindow.cpp \
    re/fuzzingwindow.cpp \
    re/graphingwindow.cpp \
    re/isotp_interpreterwindow.cpp \
    re/newgraphdialog.cpp \
    re/rangestatewindow.cpp \
    re/temporalgraphwindow.cpp \
    re/udsscanwindow.cpp \
    re/dbccomparatorwindow.cpp \
    main/bisectwindow.cpp \
    main/blfhandler.cpp \
    main/canbridgewindow.cpp \
    main/candatagrid.cpp \
    main/canfilter.cpp \
    main/canframemodel.cpp \
    main/can_structs.cpp \
    main/filterutility.cpp \
    main/firmwareuploaderwindow.cpp \
    main/framefileio.cpp \
    main/frameplaybackobject.cpp \
    main/frameplaybackwindow.cpp \
    main/framesenderobject.cpp \
    main/framesenderwindow.cpp \
    main/helpwindow.cpp \
    main/jsedit.cpp \
    main/main.cpp \
    main/mainsettingsdialog.cpp \
    main/mainwindow.cpp \
    main/motorcontrollerconfigwindow.cpp \
    main/pcaplite.cpp \
    main/qcpaxistickerhex.cpp \
    main/qcustomplot.cpp \
    main/scriptcontainer.cpp \
    main/scriptingwindow.cpp \
    main/signalviewerwindow.cpp \
    main/simplecrypt.cpp \
    main/triggerdialog.cpp \
    main/utility.cpp

HEADERS += \
    bus_protocols/isotp_handler.h \
    bus_protocols/isotp_message.h \
    bus_protocols/j1939_handler.h \
    bus_protocols/uds_handler.h \
    connections/canbus.h \
    connections/canconconst.h \
    connections/canconfactory.h \
    connections/canconmanager.h \
    connections/canconnection.h \
    connections/canconnectionmodel.h \
    connections/canlogserver.h \
    connections/canserver.h \
    connections/connectionwindow.h \
    connections/gvretserial.h \
    connections/lawicel_serial.h \
    connections/mqtt_bus.h \
    connections/newconnectiondialog.h \
    connections/serialbusconnection.h \
    connections/socketcand.h \
    dbc/dbc_classes.h \
    dbc/dbchandler.h \
    dbc/dbcloadsavewindow.h \
    dbc/dbcmaineditor.h \
    dbc/dbcmessageeditor.h \
    dbc/dbcnodeduplicateeditor.h \
    dbc/dbcnodeeditor.h \
    dbc/dbcnoderebaseeditor.h \
    dbc/dbcsignaleditor.h \
    mqtt/qmqtt_client.h \
    mqtt/qmqtt_client_p.h \
    mqtt/qmqtt_frame.h \
    mqtt/qmqtt_network_p.h \
    mqtt/qmqtt_networkinterface.h \
    mqtt/qmqtt_websocket_p.h \
    mqtt/qmqtt_socketinterface.h \
    mqtt/qmqtt_message.h \
    mqtt/qmqtt_ssl_socket_p.h \
    mqtt/qmqtt_message_p.h \
    mqtt/qmqtt_websocketiodevice_p.h \
    mqtt/qmqtt_routesubscription.h \
    mqtt/qmqtt_socket_p.h \
    mqtt/qmqtt_routedmessage.h \
    mqtt/qmqtt_timerinterface.h \
    mqtt/qmqtt_timer_p.h \
    mqtt/qmqtt_router.h \
    mqtt/qmqtt_global.h \
    mqtt/qmqtt.h \
    re/sniffer/SnifferDelegate.h \
    re/sniffer/snifferitem.h \
    re/sniffer/snifferwindow.h \
    re/sniffer/sniffermodel.h \
    re/dbccomparatorwindow.h \
    re/discretestatewindow.h \
    re/filecomparatorwindow.h \
    re/flowviewwindow.h \
    re/frameinfowindow.h \
    re/fuzzingwindow.h \
    re/graphingwindow.h \
    re/isotp_interpreterwindow.h \
    re/newgraphdialog.h \
    re/rangestatewindow.h \
    re/temporalgraphwindow.h \
    re/udsscanwindow.h \
    utils/lfqueue.h \
    main/bisectwindow.h \
    main/blfhandler.h \
    main/canbridgewindow.h \
    main/candatagrid.h \
    main/canfilter.h \
    main/canframemodel.h \
    main/can_structs.h \
    main/can_trigger_structs.h \
    main/config.h \
    main/filterutility.h \
    main/firmwareuploaderwindow.h \
    main/framefileio.h \
    main/frameplaybackobject.h \
    main/frameplaybackwindow.h \
    main/framesenderobject.h \
    main/framesenderwindow.h \
    main/helpwindow.h \
    main/jsedit.h \
    main/mainsettingsdialog.h \
    main/mainwindow.h \
    main/motorcontrollerconfigwindow.h \
    main/pcaplite.h \
    main/qcpaxistickerhex.h \
    main/qcustomplot.h \
    main/scriptcontainer.h \
    main/scriptingwindow.h \
    main/signalviewerwindow.h \
    main/simplecrypt.h \
    main/triggerdialog.h \
    main/utility.h

FORMS += \
    ui/bisectwindow.ui \
    ui/canbridgewindow.ui \
    ui/candatagrid.ui \
    ui/connectionwindow.ui \
    ui/dbccomparatorwindow.ui \
    ui/dbcloadsavewindow.ui \
    ui/dbcmaineditor.ui \
    ui/dbcmessageeditor.ui \
    ui/dbcnodeduplicateeditor.ui \
    ui/dbcnodeeditor.ui \
    ui/dbcnoderebaseeditor.ui \
    ui/dbcsignaleditor.ui \
    ui/discretestatewindow.ui \
    ui/filecomparatorwindow.ui \
    ui/firmwareuploaderwindow.ui \
    ui/flowviewwindow.ui \
    ui/frameinfowindow.ui \
    ui/frameplaybackwindow.ui \
    ui/framesenderwindow.ui \
    ui/fuzzingwindow.ui \
    ui/graphingwindow.ui \
    ui/helpwindow.ui \
    ui/isotp_interpreterwindow.ui \
    ui/mainsettingsdialog.ui \
    ui/mainwindow.ui \
    ui/motorcontrollerconfigwindow.ui \
    ui/newconnectiondialog.ui \
    ui/newgraphdialog.ui \
    ui/rangestatewindow.ui \
    ui/scriptingwindow.ui \
    ui/signalviewerwindow.ui \
    ui/snifferwindow.ui \
    ui/temporalgraphwindow.ui \
    ui/triggerdialog.ui \
    ui/udsscanwindow.ui

RESOURCES += \
    ../desktop/media/icons/icons.qrc \
    ../desktop/media/images/images.qrc

INCLUDEPATH += \
  bus_protocols \
  connections \
  dbc \
  main \
  mqtt \
  re \
  re/sniffer \
  ui \
  utils

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
   shortcutfiles.files = ../desktop/SavvyCAN.desktop
   shortcutfiles.path = $$PREFIX/share/applications
   INSTALLS += shortcutfiles
   DISTFILES += ../desktop/SavvyCAN.desktop
}

examplefiles.files = ../examples
examplefiles.path = $$PREFIX/share/savvycan/examples
INSTALLS += examplefiles

iconfiles.files = ../desktop/media/icons
iconfiles.path = $$PREFIX/share/icons
INSTALLS += iconfiles

helpfiles.files = ../help
helpfiles.path = $$PREFIX/bin/help
INSTALLS += helpfiles

INSTALLS += target
