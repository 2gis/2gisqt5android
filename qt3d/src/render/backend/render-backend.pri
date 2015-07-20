INCLUDEPATH += $$PWD

include("framegraph/framegraph.pri")
include("jobs/render-jobs.pri")

HEADERS += \
    $$PWD/qrenderaspect.h \
    $$PWD/qrenderaspect_p.h \
    $$PWD/renderthread_p.h \
    $$PWD/renderconfiguration_p.h \
    $$PWD/renderer_p.h \
    $$PWD/rendermaterial_p.h \
    $$PWD/rendermesh_p.h \
    $$PWD/qgraphicscontext_p.h \
    $$PWD/rendershader_p.h \
    $$PWD/rendertechnique_p.h \
    $$PWD/rendercameralens_p.h \
    $$PWD/quniformvalue_p.h \
    $$PWD/rendertexture_p.h \
    $$PWD/rendertextureprovider_p.h \
    $$PWD/meshdatamanager_p.h \
    $$PWD/states/blendstate_p.h \
    $$PWD/genericstate_p.h \
    $$PWD/qgraphicshelperinterface_p.h \
    $$PWD/qgraphicshelpergl3_p.h \
    $$PWD/qgraphicshelperes2_p.h \
    $$PWD/qgraphicshelpergl2_p.h \
    $$PWD/renderview_p.h \
    $$PWD/rendercommand_p.h \
    $$PWD/renderqueues_p.h \
    $$PWD/rendereffect_p.h \
    $$PWD/renderrenderpass_p.h \
    $$PWD/parameterpack_p.h \
    $$PWD/texturedatamanager_p.h \
    $$PWD/renderlayer_p.h \
    $$PWD/renderentity_p.h \
    $$PWD/rendertransform_p.h \
    $$PWD/renderstate_p.h \
    $$PWD/rendertarget_p.h \
    $$PWD/renderattachment_p.h \
    $$PWD/renderscene_p.h \
    $$PWD/scenemanager_p.h \
    $$PWD/attachmentpack_p.h \
    $$PWD/shadervariables_p.h \
    $$PWD/qgraphicsutils_p.h \
    $$PWD/rendernodefunctor_p.h \
    $$PWD/managers_p.h \
    $$PWD/handle_types_p.h \
    $$PWD/renderannotation_p.h \
    $$PWD/renderparameter_p.h \
    $$PWD/platformsurfacefilter_p.h \
    $$PWD/rendershaderdata_p.h \
    $$PWD/renderparametermapping_p.h \
    $$PWD/rendertextureimage_p.h

SOURCES += \
    $$PWD/qrenderaspect.cpp \
    $$PWD/renderthread.cpp \
    $$PWD/renderconfiguration.cpp \
    $$PWD/renderer.cpp \
    $$PWD/rendermaterial.cpp \
    $$PWD/rendermesh.cpp \
    $$PWD/qgraphicscontext.cpp \
    $$PWD/rendershader.cpp \
    $$PWD/rendertechnique.cpp \
    $$PWD/rendercameralens.cpp \
    $$PWD/quniformvalue.cpp \
    $$PWD/rendertexture.cpp \
    $$PWD/rendertextureprovider.cpp \
    $$PWD/meshdatamanager.cpp \
    $$PWD/states/blendstate.cpp \
    $$PWD/qgraphicshelpergl3.cpp \
    $$PWD/qgraphicshelperes2.cpp \
    $$PWD/qgraphicshelpergl2.cpp \
    $$PWD/renderview.cpp \
    $$PWD/rendercommand.cpp \
    $$PWD/renderqueues.cpp \
    $$PWD/rendereffect.cpp \
    $$PWD/renderrenderpass.cpp \
    $$PWD/parameterpack.cpp \
    $$PWD/texturedatamanager.cpp \
    $$PWD/renderlayer.cpp \
    $$PWD/renderentity.cpp \
    $$PWD/rendertransform.cpp \
    $$PWD/renderstate.cpp \
    $$PWD/rendertarget.cpp \
    $$PWD/renderattachment.cpp \
    $$PWD/renderscene.cpp \
    $$PWD/scenemanager.cpp \
    $$PWD/attachmentpack.cpp \
    $$PWD/renderannotation.cpp \
    $$PWD/renderparameter.cpp \
    $$PWD/platformsurfacefilter.cpp \
    $$PWD/rendershaderdata.cpp \
    $$PWD/renderparametermapping.cpp \
    $$PWD/rendertextureimage.cpp
