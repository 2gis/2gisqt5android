TARGET     = QtQuickTest

DEFINES += QT_NO_URL_CAST_FROM_STRING
QT = core
QT_PRIVATE = testlib-private quick qml-private  gui core-private

# Testlib is only a private dependency, which results in our users not
# inheriting CONFIG+=console transitively. Make it explicit.
MODULE_CONFIG = console

!contains(QT_CONFIG, no-widgets) {
    QT += widgets
    DEFINES += QT_QMLTEST_WITH_WIDGETS
}

load(qt_module)

# Install qmltestcase.prf into the Qt mkspecs so that "CONFIG += qmltestcase"
# can be used in customer applications to build against QtQuickTest.
feature.path = $$[QT_INSTALL_DATA]/mkspecs/features
feature.files = $$PWD/features/qmltestcase.prf
INSTALLS += feature

SOURCES += \
    $$PWD/quicktest.cpp \
    $$PWD/quicktestevent.cpp \
    $$PWD/quicktestresult.cpp
HEADERS += \
    $$PWD/quicktestglobal.h \
    $$PWD/quicktest.h \
    $$PWD/quicktestevent_p.h \
    $$PWD/quicktestresult_p.h \
    $$PWD/qtestoptions_p.h

DEFINES += QT_QML_DEBUG_NO_WARNING
