cmake_minimum_required(VERSION 3.21)
project(SavvyCAN VERSION 1.0 LANGUAGES C CXX)

set(QT_MIN_VERSION 6.4)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

option(USE_DBUS "Enable DBus support" OFF)
option(BUILD_TESTS "Build test target" OFF)
option(USE_SANITIZER "Enable sanitizers for Debug builds" OFF)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui PrintSupport SerialBus SerialPort Widgets Help Network OpenGL Qml)
find_package(OpenGL REQUIRED)

if(Qt6_VERSION VERSION_LESS ${QT_MIN_VERSION})
    message(FATAL_ERROR "Qt version ${Qt6_VERSION} found, but ${QT_MIN_VERSION} or higher is required")
endif()

# Function for setting debug and sanitizer flags
function(set_debug_sanitizer_options target_name)
    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -g -O0)
        if(USE_SANITIZER)
            target_compile_options(${target_name} PRIVATE -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer)
            target_link_options(${target_name} PRIVATE -fsanitize=address -fsanitize=undefined)
        endif()
    endif()
endfunction()

if(USE_DBUS AND UNIX AND NOT APPLE)
    find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS DBus)
endif()

qt_standard_project_setup()

qt_add_executable(SavvyCAN WIN32 MACOSX_BUNDLE
    bisectwindow.cpp bisectwindow.h
    blfhandler.cpp blfhandler.h
    bus_protocols/isotp_handler.cpp bus_protocols/isotp_handler.h
    bus_protocols/handler_factory.cpp bus_protocols/handler_factory.h
    bus_protocols/isotp_message.h
    bus_protocols/j1939_handler.cpp bus_protocols/j1939_handler.h
    bus_protocols/uds_handler.cpp bus_protocols/uds_handler.h
    can_structs.cpp can_structs.h
    can_trigger_structs.h
    canbridgewindow.cpp canbridgewindow.h
    candatagrid.cpp candatagrid.h
    canfilter.cpp canfilter.h
    canframemodel.cpp canframemodel.h
    config.h
    connections/canbus.cpp connections/canbus.h
    connections/canconconst.h
    connections/canconfactory.cpp connections/canconfactory.h
    connections/canconmanager.cpp connections/canconmanager.h
    connections/canconnection.cpp connections/canconnection.h
    connections/canconnectionmodel.cpp connections/canconnectionmodel.h
    connections/canlogserver.cpp connections/canlogserver.h
    connections/canserver.cpp connections/canserver.h
    connections/connectionwindow.cpp connections/connectionwindow.h
    connections/gvretserial.cpp connections/gvretserial.h
    connections/lawicel_serial.cpp connections/lawicel_serial.h
    connections/mqtt_bus.cpp connections/mqtt_bus.h
    connections/newconnectiondialog.cpp connections/newconnectiondialog.h
    connections/serialbusconnection.cpp connections/serialbusconnection.h
    connections/socketcand.cpp connections/socketcand.h
    dbc/dbc_classes.cpp dbc/dbc_classes.h
    dbc/dbchandler.cpp dbc/dbchandler.h
    dbc/dbcloadsavewindow.cpp dbc/dbcloadsavewindow.h
    dbc/dbcmaineditor.cpp dbc/dbcmaineditor.h
    dbc/dbcmessageeditor.cpp dbc/dbcmessageeditor.h
    dbc/dbcnodeduplicateeditor.cpp dbc/dbcnodeduplicateeditor.h
    dbc/dbcnodeeditor.cpp dbc/dbcnodeeditor.h
    dbc/dbcnoderebaseeditor.cpp dbc/dbcnoderebaseeditor.h
    dbc/dbcsignaleditor.cpp dbc/dbcsignaleditor.h
    filterutility.cpp filterutility.h
    firmwareuploaderwindow.cpp firmwareuploaderwindow.h
    framefileio.cpp framefileio.h
    frameplaybackobject.cpp frameplaybackobject.h
    frameplaybackwindow.cpp frameplaybackwindow.h
    framesenderobject.cpp framesenderobject.h
    framesenderwindow.cpp framesenderwindow.h
    helpwindow.cpp helpwindow.h
    jsedit.cpp jsedit.h
    main.cpp
    mainsettingsdialog.cpp mainsettingsdialog.h
    mainwindow.cpp mainwindow.h
    motorcontrollerconfigwindow.cpp motorcontrollerconfigwindow.h
    mqtt/qmqtt.h
    mqtt/qmqtt_client.cpp mqtt/qmqtt_client.h mqtt/qmqtt_client_p.cpp mqtt/qmqtt_client_p.h
    mqtt/qmqtt_frame.cpp mqtt/qmqtt_frame.h
    mqtt/qmqtt_global.h
    mqtt/qmqtt_message.cpp mqtt/qmqtt_message.h mqtt/qmqtt_message_p.h
    mqtt/qmqtt_network.cpp mqtt/qmqtt_network_p.h
    mqtt/qmqtt_networkinterface.h
    mqtt/qmqtt_routedmessage.h
    mqtt/qmqtt_router.cpp mqtt/qmqtt_router.h
    mqtt/qmqtt_routesubscription.cpp mqtt/qmqtt_routesubscription.h
    mqtt/qmqtt_socket.cpp mqtt/qmqtt_socket_p.h
    mqtt/qmqtt_socketinterface.h
    mqtt/qmqtt_ssl_socket.cpp mqtt/qmqtt_ssl_socket_p.h
    mqtt/qmqtt_timer.cpp mqtt/qmqtt_timer_p.h
    mqtt/qmqtt_timerinterface.h
    mqtt/qmqtt_websocket.cpp mqtt/qmqtt_websocket_p.h
    mqtt/qmqtt_websocketiodevice.cpp mqtt/qmqtt_websocketiodevice_p.h
    pcaplite.cpp pcaplite.h
    qcpaxistickerhex.cpp qcpaxistickerhex.h
    qcustomplot.cpp qcustomplot.h
    re/dbccomparatorwindow.cpp re/dbccomparatorwindow.h
    re/discretestatewindow.cpp re/discretestatewindow.h
    re/filecomparatorwindow.cpp re/filecomparatorwindow.h
    re/flowviewwindow.cpp re/flowviewwindow.h
    re/frameinfowindow.cpp re/frameinfowindow.h
    re/fuzzingwindow.cpp re/fuzzingwindow.h
    re/graphingwindow.cpp re/graphingwindow.h
    re/isotp_interpreterwindow.cpp re/isotp_interpreterwindow.h
    re/newgraphdialog.cpp re/newgraphdialog.h
    re/rangestatewindow.cpp re/rangestatewindow.h
    re/sniffer/SnifferDelegate.cpp re/sniffer/SnifferDelegate.h
    re/sniffer/snifferitem.cpp re/sniffer/snifferitem.h
    re/sniffer/sniffermodel.cpp re/sniffer/sniffermodel.h
    re/sniffer/snifferwindow.cpp re/sniffer/snifferwindow.h
    re/temporalgraphwindow.cpp re/temporalgraphwindow.h
    re/udsscanwindow.cpp re/udsscanwindow.h
    scriptcontainer.cpp scriptcontainer.h
    scriptingwindow.cpp scriptingwindow.h
    signalviewerwindow.cpp signalviewerwindow.h
    simplecrypt.cpp simplecrypt.h
    triggerdialog.cpp triggerdialog.h triggerdialog.ui
    utility.cpp utility.h
    utils/HexSpinBox.h
    utils/lfqueue.h
    images.qrc icons.qrc
)

if(APPLE)
    set_target_properties(SavvyCAN PROPERTIES
        MACOSX_BUNDLE_ICON_FILE SavvyIcon.icns
        RESOURCE "icons/SavvyIcon.icns"
    )
endif()

target_include_directories(SavvyCAN PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mqtt
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/bus_protocols
    ${CMAKE_CURRENT_SOURCE_DIR}/connections
)

target_compile_definitions(SavvyCAN PRIVATE
    QCUSTOMPLOT_USE_OPENGL
)

if(CMAKE_BUILD_TYPE STREQUAL Release)
    target_compile_definitions(SavvyCAN PRIVATE QT_NO_DEBUG_OUTPUT)
endif()

set_debug_sanitizer_options(SavvyCAN)

target_link_libraries(SavvyCAN PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::PrintSupport
    Qt6::SerialBus
    Qt6::SerialPort
    Qt6::Widgets
    Qt6::Help
    Qt6::Network
    Qt6::OpenGL
    Qt6::Qml # for QJSEngine
    OpenGL::GL
)

if(USE_DBUS AND UNIX AND NOT APPLE)
    target_link_libraries(SavvyCAN PRIVATE Qt6::DBus)
endif()

if(WIN32)
    set_target_properties(SavvyCAN PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(SavvyCAN PRIVATE icons/SavvyIcon.rc)
endif()

if(UNIX AND NOT APPLE)
    if(NOT DEFINED PREFIX)
        set(PREFIX "/usr/local")
    endif()
    install(TARGETS SavvyCAN RUNTIME DESTINATION ${PREFIX}/bin)
    install(FILES SavvyCAN.desktop DESTINATION ${PREFIX}/share/applications)
    install(DIRECTORY icons/hicolor DESTINATION ${PREFIX}/share/icons)
    install(DIRECTORY examples DESTINATION ${PREFIX}/share/savvycan)
    install(DIRECTORY help DESTINATION ${PREFIX}/share/savvycan)
endif()

install(TARGETS SavvyCAN
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(BUILD_TESTS)
    find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Test)
    qt_add_executable(SavvyCAN_tests
        test/main.cpp
        test/tst_lfqueue.cpp test/tst_lfqueue.h
        test/tst_cancon.cpp test/tst_cancon.h
        connections/canconfactory.cpp connections/canconfactory.h
        connections/canconnection.cpp connections/canconnection.h
        connections/gvretserial.cpp connections/gvretserial.h
        connections/socketcand.cpp connections/socketcand.h
        connections/canconconst.h
        connections/canbus.cpp connections/canbus.h
        utils/lfqueue.h
    )
    target_include_directories(SavvyCAN_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/mqtt
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/bus_protocols
        ${CMAKE_CURRENT_SOURCE_DIR}/connections
    )
    target_link_libraries(SavvyCAN_tests PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::SerialBus
        Qt6::Widgets
        Qt6::Test
    )
    set_debug_sanitizer_options(SavvyCAN_tests)
    install(TARGETS SavvyCAN_tests
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
