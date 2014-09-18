TEMPLATE = subdirs

SUBDIRS += \
#     cmake \
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
