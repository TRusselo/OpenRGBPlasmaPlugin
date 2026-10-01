CONFIG += c++17 warn_on
INCLUDEPATH += $$PWD/src $$PWD/OpenRGB/dependencies/json

gcc:!clang:greaterThan(QT_GCC_MAJOR_VERSION, 15) {
    QMAKE_CXXFLAGS += -Wno-sfinae-incomplete
}
