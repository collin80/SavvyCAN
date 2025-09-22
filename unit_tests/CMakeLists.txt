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
find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Test SerialBus)

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

# Enable ctest integration
enable_testing()
add_test(NAME TestIsoTpHandler COMMAND TestIsoTpHandler)
