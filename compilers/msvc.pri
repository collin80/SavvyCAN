QMAKE_CXXFLAGS += -external:I "$$[QT_INSTALL_HEADERS]"
for (inc, QT) {
    QMAKE_CXXFLAGS += -external:I \"$$[QT_INSTALL_HEADERS]/Qt$$system("echo $$inc | sed 's/.*/\u&/'")\"
}

QMAKE_CXXFLAGS += \
    -W4 \
    -w14242 \
    -w14254 \
    -w14263 \
    -w14165 \
    -w14287 \
    -we4289 \
    -w14296 \
    -w14311 \
    -w14545 \
    -w14546 \
    -w14547 \
    -w14549 \
    -w14555 \
    -w14619 \
    -w14640 \
    -w14826 \
    -w14905 \
    -w14906 \
    -w14928 \
    -permissive    - \
    -volatile:iso \
    -Zc:inline \
    -Zc:preprocessor \
    -Zc:enumTypes \
    -Zc:lambda \
    -Zc:__cplusplus \
    -Zc:externConstexpr \
    -Zc:throwingNew \
    -sdl \
    -guard:cf \
    -guard:ehcont \
    -GF \
    -Gy \
    -MP12 \
    -EHsc \
    -GR \
    -utf-8

QMAKE_CXXFLAGS_DEBUG += \
    -D_DEBUG \
    -DDEBUG \
    -Od \
    -Zi

QMAKE_CXXFLAGS_RELEASE += \
    -D_RELEASE \
    -DNDEBUG \
    -Ox

QMAKE_CXXFLAGS_RELEASE_WITH_DEBUGINFO += \
    -D_PROFILE \
    -DNDEBUG \
    -Ox \
    -Oy- \
    -Zi