TARGET = assimpsceneparser
QT += core-private 3dcore 3dcore-private 3drenderer 3drenderer-private

PLUGIN_TYPE = sceneparsers
PLUGIN_CLASS_NAME = AssimpParser
load(qt_plugin)

include(assimp.pri)

LIBS += -lassimp

unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += assimp
}
