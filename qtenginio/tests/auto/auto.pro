TEMPLATE = subdirs

SUBDIRS += \
    enginioclient \
    files \
    notifications \
    identity \

qtHaveModule(quick) {
    SUBDIRS += qmltests
}

qtHaveModule(widgets) {
    SUBDIRS += enginiomodel
}
