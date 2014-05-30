CXX_MODULE = sensors
TARGET  = declarative_sensors
TARGETPATH = QtSensors
IMPORT_VERSION = 5.1 # Doesn't matter, as long as it's a valid version?!

QT += qml sensors sensors-private

HEADERS += \
    qmlsensor.h \
    qmlsensorrange.h \
    qmlaccelerometer.h \
    qmlaltimeter.h \
    qmlambientlightsensor.h \
    qmlambienttemperaturesensor.h \
    qmlcompass.h \
    qmlgyroscope.h \
    qmlholstersensor.h \
    qmlirproximitysensor.h \
    qmllightsensor.h \
    qmlmagnetometer.h \
    qmlorientationsensor.h \
    qmlpressuresensor.h\
    qmlproximitysensor.h \
    qmltapsensor.h \
    qmlrotationsensor.h \
    qmlsensorglobal.h \
    qmltiltsensor.h \
    qmlsensorgesture.h

SOURCES += sensors.cpp \
    qmlsensor.cpp \
    qmlsensorrange.cpp \
    qmlaccelerometer.cpp \
    qmlaltimeter.cpp \
    qmlambientlightsensor.cpp \
    qmlambienttemperaturesensor.cpp \
    qmlcompass.cpp \
    qmlgyroscope.cpp \
    qmlholstersensor.cpp \
    qmlirproximitysensor.cpp \
    qmllightsensor.cpp \
    qmlmagnetometer.cpp \
    qmlorientationsensor.cpp \
    qmlpressuresensor.cpp\
    qmlproximitysensor.cpp \
    qmltapsensor.cpp \
    qmlrotationsensor.cpp \
    qmlsensorglobal.cpp \
    qmltiltsensor.cpp \
    qmlsensorgesture.cpp

load(qml_plugin)

OTHER_FILES += \
    plugin.json qmldir plugins.qmltypes
