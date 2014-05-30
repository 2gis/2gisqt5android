# QML tests in this directory depend on a Qt platform plugin that supports OpenGL.
# QML tests that do not require an OpenGL context should go in ../declarative_core.

TEMPLATE = app
TARGET = tst_declarative_ui
!no_ui_tests:CONFIG += qmltestcase
SOURCES += main.cpp

QT += location quick

OTHER_FILES = *.qml
TESTDATA = $$OTHER_FILES

win32|linux:CONFIG+=insignificant_test # QTBUG-31797
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

# Import path used by 'make check' since CI doesn't install test imports
IMPORTPATH = $$OUT_PWD/../../../qml
