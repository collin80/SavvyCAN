QMAKE_MAKEFLAGS += -j12

QMAKE_CXXFLAGS += -isystem "$$[QT_INSTALL_HEADERS]"
for (inc, QT) {
    QMAKE_CXXFLAGS += -isystem \"$$[QT_INSTALL_HEADERS]/Qt$$system("echo $$inc | sed 's/.*/\u&/'")\"
}

# Add stricter compiler warnings
QMAKE_CXXFLAGS += \
    -pedantic \
    -Wall \
    -Wextra \
    -Wshadow \
    -Wnon-virtual-dtor \
    -Wold-style-cast \
    -Wcast-align \
    -Wunused \
    -Woverloaded-virtual \
    -Wpedantic \
    -Wconversion \
    -Wsign-conversion \
    -Wmisleading-indentation \
    -Wnull-dereference \
    -Wdouble-promotion \
    -Wformat=2 \
    -Wimplicit-fallthrough

# For smaller binaries:
QMAKE_CXXFLAGS += \
    -fdata-sections \
    -ffunction-sections

# Position independent code:
QMAKE_CXXFLAGS += \
    -fPIC

# Add security flags
QMAKE_CXXFLAGS += \
    -D_FORTIFY_SOURCE=2 \
    -fstrict-aliasing \
    -Wstrict-aliasing \
    -fstack-protector-strong \
    -Wstack-protector \
    -fcf-protection=full \
    -Wdisabled-optimization

# Hide ELF symbols:
QMAKE_CXXFLAGS += \
    -fvisibility=hidden \
    -fvisibility-inlines-hidden

# Debug:
QMAKE_CXXFLAGS_DEBUG += \
    -D_DEBUG \
    -DDEBUG \
    -O0 \ # Disable optimizations
    -g # Enable debugging

# Profile:
QMAKE_CXXFLAGS_RELEASE_WITH_DEBUGINFO += \
    -D_PROFILE \
    -DNDEBUG \
    -O2 \ # Enable level 2 optimizations
    -fno-omit-frame-pointer \ # Do not omit frame pointers
    -g # Enable debugging

# Release:
QMAKE_CXXFLAGS_RELEASE += \
    -D_RELEASE \
    -DNDEBUG \
    -O2 # Enable level 2 optimizations

# Linker flags:
QMAKE_LFLAGS += \
    -Wl,--gc-sections \ # Remove unused code
    -Wl,-z,now \ # Enable immediate relocation table
    -Wl,-z,relro \ # Enable read-only relocation table
    -Wl,-z,defs \ # Detect under-linking
