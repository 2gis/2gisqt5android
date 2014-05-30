/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mediasamplevideobuffer.h"

MediaSampleVideoBuffer::MediaSampleVideoBuffer(IMediaSample *sample, int bytesPerLine)
    : QAbstractVideoBuffer(NoHandle)
    , m_sample(sample)
    , m_bytesPerLine(bytesPerLine)
    , m_mapMode(NotMapped)
{
    m_sample->AddRef();
}

MediaSampleVideoBuffer::~MediaSampleVideoBuffer()
{
    m_sample->Release();
}

uchar *MediaSampleVideoBuffer::map(MapMode mode, int *numBytes, int *bytesPerLine)
{
    if (m_mapMode == NotMapped && mode != NotMapped) {
        if (numBytes)
            *numBytes = m_sample->GetActualDataLength();

        if (bytesPerLine)
            *bytesPerLine = m_bytesPerLine;

        BYTE *bytes = 0;

        if (m_sample->GetPointer(&bytes) == S_OK) {
            m_mapMode = mode;

            return reinterpret_cast<uchar *>(bytes);
        }
    }
    return 0;
}

void MediaSampleVideoBuffer::unmap()
{
    m_mapMode = NotMapped;
}

QAbstractVideoBuffer::MapMode MediaSampleVideoBuffer::mapMode() const
{
    return m_mapMode;
}
