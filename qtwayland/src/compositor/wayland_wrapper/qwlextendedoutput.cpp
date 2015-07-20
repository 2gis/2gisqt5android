/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwlextendedoutput_p.h"

#include "qwlcompositor_p.h"
#include "qwlsurface_p.h"
#include "qwloutput_p.h"

QT_BEGIN_NAMESPACE

namespace QtWayland {

OutputExtensionGlobal::OutputExtensionGlobal(Compositor *compositor)
    : QtWaylandServer::qt_output_extension(compositor->wl_display(), 1)
    , m_compositor(compositor)
{
}

void OutputExtensionGlobal::output_extension_get_extended_output(qt_output_extension::Resource *resource, uint32_t id, wl_resource *output_resource)
{
    OutputResource *output = static_cast<OutputResource *>(Output::Resource::fromResource(output_resource));
    Q_ASSERT(output->extendedOutput == 0);

    ExtendedOutput *extendedOutput = static_cast<ExtendedOutput *>(qt_extended_output::add(resource->client(), id));

    Q_ASSERT(!output->extendedOutput);
    output->extendedOutput = extendedOutput;
    extendedOutput->output = output;
}

}

QT_END_NAMESPACE
