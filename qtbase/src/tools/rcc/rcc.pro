option(host_build)
CONFIG += force_bootstrap

DEFINES += QT_RCC QT_NO_CAST_FROM_ASCII

include(rcc.pri)
SOURCES += main.cpp

# Required for declarations of popen/pclose on Windows
windows: QMAKE_CXXFLAGS += -U__STRICT_ANSI__

load(qt_tool)
