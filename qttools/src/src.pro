TEMPLATE = subdirs

qtHaveModule(widgets) {
    no-png {
        message("Some graphics-related tools are unavailable without PNG support")
    } else {
        SUBDIRS = assistant \
                  pixeltool \
                  qtestlib \
                  designer

        linguist.depends = designer
    }
}

SUBDIRS += linguist \


if(!android|android_app):!ios: SUBDIRS += qtpaths

mac {
    SUBDIRS += macdeployqt
}

android {
    SUBDIRS += androiddeployqt
}


win32|winrt:SUBDIRS += windeployqt
winrt:SUBDIRS += winrtrunner
!android:!ios:!qnx:!wince*:!winrt* {
    qtHaveModule(gui):SUBDIRS += qtdiag
    qtHaveModule(dbus): SUBDIRS += qdbus
    SUBDIRS += qtplugininfo \
		qdoc
}

qtNomakeTools( \
    pixeltool \
    macdeployqt \
)
