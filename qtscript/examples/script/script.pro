TEMPLATE = subdirs
SUBDIRS = \
    customclass \
    qsdbg

qtHaveModule(widgets) {
    SUBDIRS += \
        helloscript \
        context2d \
        defaultprototypes

    qtHaveModule(uitools) {
        SUBDIRS += \
            calculator \
            qstetrix
    }

    !wince {
        SUBDIRS += \
            qscript
    }
}

!wince {
    SUBDIRS += \
        marshal
}

maemo5: CONFIG += qt_example
