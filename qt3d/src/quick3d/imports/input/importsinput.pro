CXX_MODULE = qml
TARGET = quick3dinputplugin
TARGETPATH = Qt3D/Input

QT += core-private qml 3dcore 3dinput 3dinput-private 3dquickinput-private

OTHER_FILES += qmldir

load(qml_plugin)

HEADERS += \
    qt3dquick3dinputplugin.h

SOURCES += \
    qt3dquick3dinputplugin.cpp
