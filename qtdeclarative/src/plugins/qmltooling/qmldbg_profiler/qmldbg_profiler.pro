TARGET = qmldbg_profiler
QT = qml-private core-private

PLUGIN_TYPE = qmltooling
PLUGIN_CLASS_NAME = QQmlProfilerServiceFactory
load(qt_plugin)

SOURCES += \
    $$PWD/qqmlenginecontrolservice.cpp \
    $$PWD/qqmlprofileradapter.cpp \
    $$PWD/qqmlprofilerservice.cpp \
    $$PWD/qqmlprofilerservicefactory.cpp \
    $$PWD/qv4profileradapter.cpp

HEADERS += \
    $$PWD/../shared/qqmlconfigurabledebugservice.h \
    $$PWD/qqmlenginecontrolservice.h \
    $$PWD/qqmlprofileradapter.h \
    $$PWD/qqmlprofilerservice.h \
    $$PWD/qqmlprofilerservicefactory.h \
    $$PWD/qv4profileradapter.h

INCLUDEPATH += $$PWD \
    $$PWD/../shared

OTHER_FILES += \
    $$PWD/qqmlprofilerservice.json

