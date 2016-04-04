TARGET   = Qt3DRender

QT      += core-private gui-private 3dcore 3dcore-private openglextensions concurrent

DEFINES += QT3DRENDER_LIBRARY

MODULE   = 3drender

MODULE_PLUGIN_TYPES = \
    sceneparsers

load(qt_module)

include (backend/render-backend.pri)
include (geometry/geometry.pri)
include (graphicshelpers/graphicshelpers.pri)
include (framegraph/framegraph.pri)
include (frontend/render-frontend.pri)
include (jobs/jobs.pri)
include (lights/lights.pri)
include (materialsystem/materialsystem.pri)
include (renderstates/renderstates.pri)
include (io/io.pri)
include (defaults/defaults.pri)
include (picking/picking.pri)
include (raycasting/raycasting.pri)
include (services/services.pri)
include (texture/texture.pri)

RESOURCES += $$PWD/render.qrc

OTHER_FILES += \
    $$PWD/shaders/* \
    $$PWD/shaders/gl3/* \
    $$PWD/shaders/es2/*

gcov {
    CONFIG += static
    QMAKE_CXXFLAGS += -fprofile-arcs -ftest-coverage
    QMAKE_LFLAGS += -fprofile-arcs -ftest-coverage
}

HEADERS += \
    renderlogging_p.h \
    qt3drender_global.h \
    qt3drender_global_p.h

!contains(QT_CONFIG, egl):DEFINES += QT_NO_EGL

# otherwise mingw headers do not declare common functions like ::strcasecmp
win32-g++*:QMAKE_CXXFLAGS_CXX11 = -std=gnu++0x

SOURCES += \
    renderlogging.cpp
