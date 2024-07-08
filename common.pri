CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT

CONFIG += c++20
CONFIG += NO_UNIT_TESTS

DEFINES += QCUSTOMPLOT_USE_OPENGL

# Store intermedia stuff somewhere else
isEmpty(GENERATED_DIR){
 GENERATED_DIR = generated
}
 # Store intermedia stuff somewhere else
OBJECTS_DIR = $${GENERATED_DIR}/obj
MOC_DIR = $${GENERATED_DIR}/moc
RCC_DIR = $${GENERATED_DIR}/rcc
UI_DIR = $${GENERATED_DIR}/ui
UI_HEADERS_DIR = $${GENERATED_DIR}/ui
UI_SOURCES_DIR = $${GENERATED_DIR}/ui

win32-msvc* {
    include( ./compilers/msvc.pri )
}
else {
    include( ./compilers/gcc.pri )
}
