cmake_minimum_required(VERSION 3.21)
project(SavvyCAN_unit_tests LANGUAGES CXX)

set(QT_MIN_VERSION 6.4)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Find Qt6
find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Test SerialBus Widgets)

# Directories where SavvyCAN headers live
set(SAVVYCAN_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../bus_protocols
    ${CMAKE_CURRENT_SOURCE_DIR}/../connections
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

###### iso-tp handler test ######
set(TEST_SOURCES_ISOTP_HANDLER
    test_isotp_handler.cpp
    ../bus_protocols/isotp_handler.cpp  # the class under test
    ../canfilter.cpp
)

add_executable(TestIsoTpHandler ${TEST_SOURCES_ISOTP_HANDLER})

target_include_directories(TestIsoTpHandler PRIVATE ${SAVVYCAN_INCLUDE_DIRS})

target_link_libraries(TestIsoTpHandler
    Qt6::Core
    Qt6::Test
    Qt6::SerialBus
)

###### uds handler test ######
set(TEST_SOURCES_UDS_HANDLER
    test_uds_handler.cpp
    ../bus_protocols/uds_handler.cpp  # the class under test
    ../bus_protocols/isotp_handler.cpp

    ../canfilter.cpp
    ../utility.cpp
)

add_executable(TestUdsHandler ${TEST_SOURCES_UDS_HANDLER})

target_include_directories(TestUdsHandler PRIVATE ${SAVVYCAN_INCLUDE_DIRS})

target_link_libraries(TestUdsHandler
    Qt6::Core
    Qt6::Test
    Qt6::SerialBus
    Qt6::Widgets
)

###### OBD-II PID table and decoder ######
add_executable(TestObd2Pids
    test_obd2_pids.cpp
    ../bus_protocols/obd2_pids.cpp  # the code under test
)

target_include_directories(TestObd2Pids PRIVATE ${SAVVYCAN_INCLUDE_DIRS})

target_link_libraries(TestObd2Pids
    Qt6::Core
    Qt6::Test
)

###### OBD-II poll engine ######
add_executable(TestObd2Poller
    test_obd2_poller.cpp
    ../bus_protocols/obd2_poller.cpp  # the class under test
    ../bus_protocols/obd2_pids.cpp
    ../bus_protocols/isotp_handler.cpp
    ../canfilter.cpp
)

target_include_directories(TestObd2Poller PRIVATE ${SAVVYCAN_INCLUDE_DIRS})

target_link_libraries(TestObd2Poller
    Qt6::Core
    Qt6::Test
    Qt6::SerialBus
)

###### OBD-II window ######
add_executable(TestObd2Window
    test_obd2_window.cpp
    ../re/obd2window.cpp  # the class under test
    ../bus_protocols/obd2_poller.cpp
    ../bus_protocols/obd2_dbc.cpp
    ../bus_protocols/obd2_pids.cpp
    ../bus_protocols/isotp_handler.cpp
    ../canfilter.cpp
)

target_include_directories(TestObd2Window PRIVATE ${SAVVYCAN_INCLUDE_DIRS})

target_link_libraries(TestObd2Window
    Qt6::Core
    Qt6::Test
    Qt6::SerialBus
    Qt6::Widgets
)

###### OBD-II DBC ######
add_executable(TestObd2Dbc
    test_obd2_dbc.cpp
    ../bus_protocols/obd2_dbc.cpp  # the code under test
    ../bus_protocols/obd2_pids.cpp
    ../dbc/dbchandler.cpp
    ../dbc/dbc_classes.cpp
    ../utility.cpp
)

target_include_directories(TestObd2Dbc PRIVATE ${SAVVYCAN_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../dbc)

target_link_libraries(TestObd2Dbc
    Qt6::Core
    Qt6::Test
    Qt6::SerialBus
    Qt6::Widgets
)

###### log and DBC loading/saving without the file dialogs ######
set(TEST_SOURCES_FILE_DIALOG_SPLIT
    test_file_dialog_split.cpp
    ../framefileio.cpp  # the code under test
    ../lognotes.cpp
    ../dbc/dbchandler.cpp
    ../dbc/dbc_classes.cpp
    ../blfhandler.cpp
    ../pcaplite.cpp
    ../utility.cpp
)

add_executable(TestFileDialogSplit ${TEST_SOURCES_FILE_DIALOG_SPLIT})

target_include_directories(TestFileDialogSplit PRIVATE ${SAVVYCAN_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../dbc)

target_link_libraries(TestFileDialogSplit
    Qt6::Core
    Qt6::Test
    Qt6::SerialBus
    Qt6::Widgets
)

###### GitHub repos test ######
find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Network)

set(TEST_SOURCES_GITHUB
    test_github.cpp
    ../github/githubclient.cpp  # the code under test
    ../github/githubdeviceflow.cpp
    ../github/githubcache.cpp
    ../github/githubsettings.cpp
    ../github/githubbrowserdialog.cpp
    ../github/githubaccountdialog.cpp
    ../simplecrypt.cpp
)

add_executable(TestGitHub ${TEST_SOURCES_GITHUB})

target_include_directories(TestGitHub PRIVATE ${SAVVYCAN_INCLUDE_DIRS})

target_link_libraries(TestGitHub
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Widgets
)

###### log notes test ######
set(TEST_SOURCES_LOG_NOTES
    test_log_notes.cpp
    ../lognotes.cpp  # the code under test
    ../markerpresets.cpp
    ../shortcutcheck.cpp
    ../framefileio.cpp
    ../dbc/dbchandler.cpp
    ../dbc/dbc_classes.cpp
    ../blfhandler.cpp
    ../pcaplite.cpp
    ../utility.cpp
)

add_executable(TestLogNotes ${TEST_SOURCES_LOG_NOTES})

target_include_directories(TestLogNotes PRIVATE ${SAVVYCAN_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../dbc)

target_link_libraries(TestLogNotes
    Qt6::Core
    Qt6::Test
    Qt6::SerialBus
    Qt6::Widgets
)

###### log notes dialog and panel test ######
set(TEST_SOURCES_LOG_NOTES_UI
    test_log_notes_ui.cpp
    ../lognotesdialog.cpp  # the code under test
    ../lognotespanel.cpp
    ../markerpresetsdialog.cpp
    ../lognotes.cpp
    ../markerpresets.cpp
    ../shortcutcheck.cpp
)

add_executable(TestLogNotesUi ${TEST_SOURCES_LOG_NOTES_UI})

target_include_directories(TestLogNotesUi PRIVATE ${SAVVYCAN_INCLUDE_DIRS})

target_link_libraries(TestLogNotesUi
    Qt6::Core
    Qt6::Test
    Qt6::SerialBus
    Qt6::Widgets
)


# Enable ctest integration
enable_testing()
add_test(NAME TestIsoTpHandler COMMAND TestIsoTpHandler)
add_test(NAME TestUdsHandler COMMAND TestUdsHandler)
add_test(NAME TestObd2Pids COMMAND TestObd2Pids)
add_test(NAME TestObd2Poller COMMAND TestObd2Poller)
add_test(NAME TestObd2Window COMMAND TestObd2Window)
add_test(NAME TestObd2Dbc COMMAND TestObd2Dbc)
add_test(NAME TestFileDialogSplit COMMAND TestFileDialogSplit)
add_test(NAME TestGitHub COMMAND TestGitHub)
add_test(NAME TestLogNotes COMMAND TestLogNotes)
add_test(NAME TestLogNotesUi COMMAND TestLogNotesUi)
