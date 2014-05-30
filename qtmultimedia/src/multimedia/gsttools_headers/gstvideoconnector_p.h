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

#ifndef QGSTVIDEOCONNECTOR_H
#define QGSTVIDEOCONNECTOR_H

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_VIDEO_CONNECTOR \
  (gst_video_connector_get_type())
#define GST_VIDEO_CONNECTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_VIDEO_CONNECTOR, GstVideoConnector))
#define GST_VIDEO_CONNECTOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_VIDEO_CONNECTOR, GstVideoConnectorClass))
#define GST_IS_VIDEO_CONNECTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_VIDEO_CONNECTOR))
#define GST_IS_VIDEO_CONNECTOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_VIDEO_CONNECTOR))

typedef struct _GstVideoConnector GstVideoConnector;
typedef struct _GstVideoConnectorClass GstVideoConnectorClass;

struct _GstVideoConnector {
  GstElement element;

  GstPad *srcpad;
  GstPad *sinkpad;

  gboolean relinked;
  gboolean failedSignalEmited;
  GstSegment segment;
  GstBuffer *latest_buffer;
};

struct _GstVideoConnectorClass {
  GstElementClass parent_class;

  /* action signal to resend new segment */
  void (*resend_new_segment) (GstElement * element, gboolean emitFailedSignal);
};

GType gst_video_connector_get_type (void);

G_END_DECLS

#endif

