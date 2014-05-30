requires(qtHaveModule(network))

TEMPLATE = subdirs

SUBDIRS += enginio_client

qtHaveModule(quick) {
    SUBDIRS += enginio_plugin
    enginio_plugin.depends = enginio_client
}
