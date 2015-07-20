/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qgraphicshelpergl2_p.h"
#ifndef QT_OPENGL_ES_2
#include <QOpenGLFunctions_2_0>
#include <private/attachmentpack_p.h>
#include <QtOpenGLExtensions/QOpenGLExtensions>
#include <private/qgraphicsutils_p.h>
#include <QDebug>

QT_BEGIN_NAMESPACE

namespace Qt3D {
namespace Render {

QGraphicsHelperGL2::QGraphicsHelperGL2()
    : m_funcs(Q_NULLPTR)
    , m_fboFuncs(Q_NULLPTR)
{

}

void QGraphicsHelperGL2::initializeHelper(QOpenGLContext *context,
                                          QAbstractOpenGLFunctions *functions)
{
    Q_UNUSED(context)
    m_funcs = static_cast<QOpenGLFunctions_2_0*>(functions);
    const bool ok = m_funcs->initializeOpenGLFunctions();
    Q_ASSERT(ok);
    Q_UNUSED(ok);
    // Check Vertex Array Object extension is present
    Q_ASSERT(context->hasExtension(QByteArrayLiteral("GL_ARB_vertex_array_object")));
    if (context->hasExtension(QByteArrayLiteral("GL_ARB_framebuffer_object"))) {
        m_fboFuncs = new QOpenGLExtension_ARB_framebuffer_object();
        const bool extensionOk = m_fboFuncs->initializeOpenGLFunctions();
        Q_ASSERT(extensionOk);
        Q_UNUSED(extensionOk);
    }
}

void QGraphicsHelperGL2::drawElementsInstanced(GLenum primitiveType,
                                               GLsizei primitiveCount,
                                               GLint indexType,
                                               void *indices,
                                               GLsizei instances)
{
    for (GLint i = 0; i < instances; i++)
        drawElements(primitiveType,
                     primitiveCount,
                     indexType,
                     indices);
}

void QGraphicsHelperGL2::drawArraysInstanced(GLenum primitiveType,
                                             GLint first,
                                             GLsizei count,
                                             GLsizei instances)
{
    for (GLint i = 0; i < instances; i++)
        drawArrays(primitiveType,
                   first,
                   count);
}

void QGraphicsHelperGL2::drawElements(GLenum primitiveType,
                                      GLsizei primitiveCount,
                                      GLint indexType,
                                      void *indices)
{
    m_funcs->glDrawElements(primitiveType,
                            primitiveCount,
                            indexType,
                            indices);
}

void QGraphicsHelperGL2::drawArrays(GLenum primitiveType,
                                    GLint first,
                                    GLsizei count)
{
    m_funcs->glDrawArrays(primitiveType,
                          first,
                          count);
}

void QGraphicsHelperGL2::setVerticesPerPatch(GLint verticesPerPatch)
{
    Q_UNUSED(verticesPerPatch);
    qWarning() << "Tessellation not supported with OpenGL 2";
}

void QGraphicsHelperGL2::useProgram(GLuint programId)
{
    m_funcs->glUseProgram(programId);
}

QVector<ShaderUniform> QGraphicsHelperGL2::programUniformsAndLocations(GLuint programId)
{
    QVector<ShaderUniform> uniforms;

    GLint nbrActiveUniforms = 0;
    m_funcs->glGetProgramiv(programId, GL_ACTIVE_UNIFORMS, &nbrActiveUniforms);
    uniforms.resize(nbrActiveUniforms);
    for (GLint i = 0; i < nbrActiveUniforms; i++) {
        ShaderUniform uniform;
        QByteArray uniformName(256, '\0');
        // Size is 1 for scalar and more for struct or arrays
        // Type is the GL Type
        m_funcs->glGetActiveUniform(programId, i, 256, NULL, &uniform.m_size, &uniform.m_type , uniformName.data());
        uniform.m_location = m_funcs->glGetUniformLocation(programId, uniformName.constData());
        uniform.m_name = QString::fromUtf8(uniformName);
        uniforms.append(uniform);
    }
    return uniforms;
}

QVector<ShaderAttribute> QGraphicsHelperGL2::programAttributesAndLocations(GLuint programId)
{
    QVector<ShaderAttribute> attributes;
    GLint nbrActiveAttributes = 0;
    m_funcs->glGetProgramiv(programId, GL_ACTIVE_ATTRIBUTES, &nbrActiveAttributes);
    for (GLint i = 0; i < nbrActiveAttributes; i++) {
        ShaderAttribute attribute;
        QByteArray attributeName(256, '\0');
        // Size is 1 for scalar and more for struct or arrays
        // Type is the GL Type
        m_funcs->glGetActiveAttrib(programId, i, 256, NULL, &attribute.m_size, &attribute.m_type , attributeName.data());
        attribute.m_location = m_funcs->glGetAttribLocation(programId, attributeName.constData());
        attribute.m_name = QString::fromUtf8(attributeName);
        attributes.append(attribute);
    }
    return attributes;
}

QVector<ShaderUniformBlock> QGraphicsHelperGL2::programUniformBlocks(GLuint programId)
{
    Q_UNUSED(programId)
    QVector<ShaderUniformBlock> blocks;
    qWarning() << "UBO are not supported by OpenGL 2.0 (since OpenGL 3.1)";
    return blocks;
}

void QGraphicsHelperGL2::vertexAttribDivisor(GLuint index,
                                             GLuint divisor)
{
    Q_UNUSED(index)
    Q_UNUSED(divisor)
}

void QGraphicsHelperGL2::blendEquation(GLenum mode)
{
    m_funcs->glBlendEquation(mode);
}

void QGraphicsHelperGL2::alphaTest(GLenum mode1, GLenum mode2)
{
    m_funcs->glEnable(GL_ALPHA_TEST);
    m_funcs->glAlphaFunc(mode1, mode2);
}

void QGraphicsHelperGL2::depthTest(GLenum mode)
{
    m_funcs->glEnable(GL_DEPTH_TEST);
    m_funcs->glDepthFunc(mode);
}

void QGraphicsHelperGL2::depthMask(GLenum mode)
{
    m_funcs->glDepthMask(mode);
}

void QGraphicsHelperGL2::cullFace(GLenum mode)
{
    m_funcs->glEnable(GL_CULL_FACE);
    m_funcs->glCullFace(mode);
}

void QGraphicsHelperGL2::frontFace(GLenum mode)
{
    m_funcs->glFrontFace(mode);
}

void QGraphicsHelperGL2::enableAlphaCoverage()
{
    m_funcs->glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
}

void QGraphicsHelperGL2::disableAlphaCoverage()
{
    m_funcs->glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
}

GLuint QGraphicsHelperGL2::createFrameBufferObject()
{
    if (m_fboFuncs != Q_NULLPTR) {
        GLuint id;
        m_fboFuncs->glGenFramebuffers(1, &id);
        return id;
    }
    qWarning() << "FBO not supported by your OpenGL hardware";
    return 0;
}

void QGraphicsHelperGL2::releaseFrameBufferObject(GLuint frameBufferId)
{
    if (m_fboFuncs != Q_NULLPTR)
        m_fboFuncs->glDeleteFramebuffers(1, &frameBufferId);
    else
        qWarning() << "FBO not supported by your OpenGL hardware";
}

bool QGraphicsHelperGL2::checkFrameBufferComplete()
{
    if (m_fboFuncs != Q_NULLPTR)
        return (m_fboFuncs->glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    return false;
}

void QGraphicsHelperGL2::bindFrameBufferAttachment(QOpenGLTexture *texture, const Attachment &attachment)
{
    if (m_fboFuncs != Q_NULLPTR) {
        GLenum attr = GL_DEPTH_STENCIL_ATTACHMENT;

        if (attachment.m_type <= QRenderAttachment::ColorAttachment15)
            attr = GL_COLOR_ATTACHMENT0 + attachment.m_type;
        else if (attachment.m_type == QRenderAttachment::DepthAttachment)
            attr = GL_DEPTH_ATTACHMENT;
        else if (attachment.m_type == QRenderAttachment::StencilAttachment)
            attr = GL_STENCIL_ATTACHMENT;
        else
            qCritical() << "DepthStencil Attachment not supported on OpenGL 2.0";

        texture->bind();
        QOpenGLTexture::Target target = texture->target();
        if (target == QOpenGLTexture::Target3D)
            m_fboFuncs->glFramebufferTexture3D(GL_DRAW_FRAMEBUFFER, attr, target, texture->textureId(), attachment.m_mipLevel, attachment.m_layer);
        else if (target == QOpenGLTexture::TargetCubeMap)
            m_fboFuncs->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attr, attachment.m_face, texture->textureId(), attachment.m_mipLevel);
        else if (target == QOpenGLTexture::Target1D)
            m_fboFuncs->glFramebufferTexture1D(GL_DRAW_FRAMEBUFFER, attr, target, texture->textureId(), attachment.m_mipLevel);
        else if (target == QOpenGLTexture::Target2D || target == QOpenGLTexture::TargetRectangle)
            m_fboFuncs->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attr, target, texture->textureId(), attachment.m_mipLevel);
        else
            qCritical() << "Texture format not supported for Attachment on OpenGL 2.0";
        texture->release();
    }
}

bool QGraphicsHelperGL2::supportsFeature(QGraphicsHelperInterface::Feature feature) const
{
    switch (feature) {
    case MRT:
        return (m_fboFuncs != Q_NULLPTR);
    case Tessellation:
        return false;
    case UniformBufferObject:
        return false;
    default:
        return false;
    }
}

void QGraphicsHelperGL2::drawBuffers(GLsizei n, const int *bufs)
{
    QVarLengthArray<GLenum, 16> drawBufs(n);

    for (int i = 0; i < n; i++)
        drawBufs[i] = GL_COLOR_ATTACHMENT0 + bufs[i];
    m_funcs->glDrawBuffers(n, drawBufs.constData());
}

void QGraphicsHelperGL2::bindFragDataLocation(GLuint, const QHash<QString, int> &)
{
    qCritical() << "bindFragDataLocation is not supported by GL 2.0";
}

void QGraphicsHelperGL2::bindUniform(const QVariant &v, const ShaderUniform &description)
{
    switch (description.m_type) {

    case GL_FLOAT:
        m_funcs->glUniform1fv(description.m_location, description.m_size,
                              QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 1));
        break;

    case GL_FLOAT_VEC2:
        m_funcs->glUniform2fv(description.m_location, description.m_size,
                              QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 2));
        break;

    case GL_FLOAT_VEC3:
        m_funcs->glUniform3fv(description.m_location, description.m_size,
                              QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 3));
        break;

    case GL_FLOAT_VEC4:
        m_funcs->glUniform4fv(description.m_location, description.m_size,
                              QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 4));
        break;

    case GL_FLOAT_MAT2:
        m_funcs->glUniformMatrix2fv(description.m_location, description.m_size, GL_FALSE,
                                    QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 4));
        break;

    case GL_FLOAT_MAT3:
        m_funcs->glUniformMatrix3fv(description.m_location, description.m_size, GL_FALSE,
                                      QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 9));
        break;

    case GL_FLOAT_MAT4:
        m_funcs->glUniformMatrix4fv(description.m_location, description.m_size, GL_FALSE,
                                    QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 16));
        break;

    case GL_INT:
        m_funcs->glUniform1iv(description.m_location, description.m_size,
                              QGraphicsUtils::valueArrayFromVariant<GLint>(v, description.m_size, 1));
        break;

    case GL_INT_VEC2:
        m_funcs->glUniform2iv(description.m_location, description.m_size,
                              QGraphicsUtils::valueArrayFromVariant<GLint>(v, description.m_size, 2));
        break;

    case GL_INT_VEC3:
        m_funcs->glUniform3iv(description.m_location, description.m_size,
                              QGraphicsUtils::valueArrayFromVariant<GLint>(v, description.m_size, 3));
        break;

    case GL_INT_VEC4:
        m_funcs->glUniform4iv(description.m_location, description.m_size,
                              QGraphicsUtils::valueArrayFromVariant<GLint>(v, description.m_size, 4));
        break;

    case GL_BOOL:
        m_funcs->glUniform1iv(description.m_location, description.m_size,
                              QGraphicsUtils::valueArrayFromVariant<GLint>(v, description.m_size, 1));
        break;

    case GL_BOOL_VEC2:
        m_funcs->glUniform2iv(description.m_location, description.m_size,
                              QGraphicsUtils::valueArrayFromVariant<GLint>(v, description.m_size, 2));
        break;

    case GL_BOOL_VEC3:
        m_funcs->glUniform3iv(description.m_location, description.m_size,
                              QGraphicsUtils::valueArrayFromVariant<GLint>(v, description.m_size, 3));
        break;

    case GL_BOOL_VEC4:
        m_funcs->glUniform4iv(description.m_location, description.m_size,
                              QGraphicsUtils::valueArrayFromVariant<GLint>(v, description.m_size, 4));
        break;

    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE: {
        Q_ASSERT(description.m_size == 1);
        m_funcs->glUniform1i(description.m_location, v.toInt());
        break;
    }

    default:
        qWarning() << Q_FUNC_INFO << "unsupported uniform type:" << description.m_type << "for " << description.m_name;
        break;
    }
}

void QGraphicsHelperGL2::bindFrameBufferObject(GLuint frameBufferId)
{
    if (m_fboFuncs != Q_NULLPTR)
        m_fboFuncs->glBindFramebuffer(GL_READ_FRAMEBUFFER, frameBufferId);
    else
        qWarning() << "FBO not supported by your OpenGL hardware";
}

GLuint QGraphicsHelperGL2::boundFrameBufferObject()
{
    GLint id = 0;
    m_funcs->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &id);
    return id;
}

void QGraphicsHelperGL2::bindUniformBlock(GLuint programId, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    Q_UNUSED(programId)
    Q_UNUSED(uniformBlockIndex)
    Q_UNUSED(uniformBlockBinding)
    qWarning() << "UBO are not supported by OpenGL 2.0 (since OpenGL 3.1)";
}

void QGraphicsHelperGL2::bindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    Q_UNUSED(target);
    Q_UNUSED(index);
    Q_UNUSED(buffer);
    qWarning() << "bindBufferBase is not supported by OpenGL 2.0 (since OpenGL 3.0)";
}

void QGraphicsHelperGL2::buildUniformBuffer(const QVariant &v, const ShaderUniform &description, QByteArray &buffer)
{
    Q_UNUSED(v)
    Q_UNUSED(description)
    Q_UNUSED(buffer)
    qWarning() << "UBO are not supported by OpenGL 2.0 (since OpenGL 3.1)";
}

uint QGraphicsHelperGL2::uniformByteSize(const ShaderUniform &description)
{
    uint rawByteSize = 0;
    int arrayStride = qMax(description.m_arrayStride, 0);
    int matrixStride = qMax(description.m_matrixStride, 0);

    switch (description.m_type) {

    case GL_FLOAT_VEC2:
    case GL_INT_VEC2:
        rawByteSize = 8;
        break;

    case GL_FLOAT_VEC3:
    case GL_INT_VEC3:
        rawByteSize = 12;
        break;

    case GL_FLOAT_VEC4:
    case GL_INT_VEC4:
        rawByteSize = 16;
        break;

    case GL_FLOAT_MAT2:
        rawByteSize = matrixStride ? 2 * matrixStride : 16;
        break;

    case GL_FLOAT_MAT3:
        rawByteSize = matrixStride ? 3 * matrixStride : 36;
        break;

    case GL_FLOAT_MAT4:
        rawByteSize = matrixStride ? 4 * matrixStride : 64;
        break;

    case GL_BOOL:
        rawByteSize = 1;
        break;

    case GL_BOOL_VEC2:
        rawByteSize = 2;
        break;

    case GL_BOOL_VEC3:
        rawByteSize = 3;
        break;

    case GL_BOOL_VEC4:
        rawByteSize = 4;
        break;

    case GL_INT:
    case GL_FLOAT:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
        rawByteSize = 4;
        break;
    }

    return arrayStride ? rawByteSize * arrayStride : rawByteSize;
}

} // Render
} // Qt3D

QT_END_NAMESPACE

#endif // !QT_OPENGL_ES_2
