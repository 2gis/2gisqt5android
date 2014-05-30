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

#include <QtCore/qstring.h>
#include <QtCore/qdebug.h>
#include <QtCore/QDir>
#include <QtCore/QDebug>

#include "qgstreamercaptureserviceplugin.h"

//#define QT_SUPPORTEDMIMETYPES_DEBUG

#include "qgstreamercaptureservice.h"
#include <private/qgstutils_p.h>

#include <private/qcore_unix_p.h>
#include <linux/videodev2.h>

QMediaService* QGstreamerCaptureServicePlugin::create(const QString &key)
{
    QGstUtils::initializeGst();

    if (key == QLatin1String(Q_MEDIASERVICE_AUDIOSOURCE))
        return new QGstreamerCaptureService(key);

#if defined(USE_GSTREAMER_CAMERA)
    if (key == QLatin1String(Q_MEDIASERVICE_CAMERA))
        return new QGstreamerCaptureService(key);
#endif

    qWarning() << "Gstreamer capture service plugin: unsupported key:" << key;
    return 0;
}

void QGstreamerCaptureServicePlugin::release(QMediaService *service)
{
    delete service;
}

#if defined(USE_GSTREAMER_CAMERA)
QMediaServiceProviderHint::Features QGstreamerCaptureServicePlugin::supportedFeatures(
        const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_CAMERA)
        return QMediaServiceProviderHint::VideoSurface;

    return QMediaServiceProviderHint::Features();
}

QByteArray QGstreamerCaptureServicePlugin::defaultDevice(const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_CAMERA) {
        if (m_cameraDevices.isEmpty())
            updateDevices();

        return m_defaultCameraDevice;
    }

    return QByteArray();
}

QList<QByteArray> QGstreamerCaptureServicePlugin::devices(const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_CAMERA) {
        if (m_cameraDevices.isEmpty())
            updateDevices();

        return m_cameraDevices;
    }

    return QList<QByteArray>();
}

QString QGstreamerCaptureServicePlugin::deviceDescription(const QByteArray &service, const QByteArray &device)
{
    if (service == Q_MEDIASERVICE_CAMERA) {
        if (m_cameraDevices.isEmpty())
            updateDevices();

        for (int i=0; i<m_cameraDevices.count(); i++)
            if (m_cameraDevices[i] == device)
                return m_cameraDescriptions[i];
    }

    return QString();
}

QVariant QGstreamerCaptureServicePlugin::deviceProperty(const QByteArray &service, const QByteArray &device, const QByteArray &property)
{
    Q_UNUSED(service);
    Q_UNUSED(device);
    Q_UNUSED(property);
    return QVariant();
}

void QGstreamerCaptureServicePlugin::updateDevices() const
{
    m_defaultCameraDevice.clear();
    m_cameraDevices.clear();
    m_cameraDescriptions.clear();

    QDir devDir("/dev");
    devDir.setFilter(QDir::System);

    QFileInfoList entries = devDir.entryInfoList(QStringList() << "video*");

    foreach( const QFileInfo &entryInfo, entries ) {
        //qDebug() << "Try" << entryInfo.filePath();

        int fd = qt_safe_open(entryInfo.filePath().toLatin1().constData(), O_RDWR );
        if (fd == -1)
            continue;

        bool isCamera = false;

        v4l2_input input;
        memset(&input, 0, sizeof(input));
        for (; ::ioctl(fd, VIDIOC_ENUMINPUT, &input) >= 0; ++input.index) {
            if(input.type == V4L2_INPUT_TYPE_CAMERA || input.type == 0) {
                isCamera = ::ioctl(fd, VIDIOC_S_INPUT, input.index) != 0;
                break;
            }
        }

        if (isCamera) {
            // find out its driver "name"
            QString name;
            struct v4l2_capability vcap;
            memset(&vcap, 0, sizeof(struct v4l2_capability));

            if (ioctl(fd, VIDIOC_QUERYCAP, &vcap) != 0)
                name = entryInfo.fileName();
            else
                name = QString((const char*)vcap.card);
            //qDebug() << "found camera: " << name;

            m_cameraDevices.append(entryInfo.filePath().toLocal8Bit());
            m_cameraDescriptions.append(name);
        }
        qt_safe_close(fd);
    }

    if (!m_cameraDevices.isEmpty())
        m_defaultCameraDevice = m_cameraDevices.first();
}
#endif

QMultimedia::SupportEstimate QGstreamerCaptureServicePlugin::hasSupport(const QString &mimeType,
                                                                     const QStringList& codecs) const
{
    if (m_supportedMimeTypeSet.isEmpty())
        updateSupportedMimeTypes();

    return QGstUtils::hasSupport(mimeType, codecs, m_supportedMimeTypeSet);
}

void QGstreamerCaptureServicePlugin::updateSupportedMimeTypes() const
{
    //enumerate supported mime types
    gst_init(NULL, NULL);

    GList *plugins, *orig_plugins;
    orig_plugins = plugins = gst_default_registry_get_plugin_list ();

    while (plugins) {
        GList *features, *orig_features;

        GstPlugin *plugin = (GstPlugin *) (plugins->data);
        plugins = g_list_next (plugins);

        if (plugin->flags & (1<<1)) //GST_PLUGIN_FLAG_BLACKLISTED
            continue;

        orig_features = features = gst_registry_get_feature_list_by_plugin(gst_registry_get_default (),
                                                                        plugin->desc.name);
        while (features) {
            if (!G_UNLIKELY(features->data == NULL)) {
                GstPluginFeature *feature = GST_PLUGIN_FEATURE(features->data);
                if (GST_IS_ELEMENT_FACTORY (feature)) {
                    GstElementFactory *factory = GST_ELEMENT_FACTORY(gst_plugin_feature_load(feature));
                    if (factory
                       && factory->numpadtemplates > 0
                       && (qstrcmp(factory->details.klass, "Codec/Decoder/Audio") == 0
                          || qstrcmp(factory->details.klass, "Codec/Decoder/Video") == 0
                          || qstrcmp(factory->details.klass, "Codec/Demux") == 0 )) {
                        const GList *pads = factory->staticpadtemplates;
                        while (pads) {
                            GstStaticPadTemplate *padtemplate = (GstStaticPadTemplate*)(pads->data);
                            pads = g_list_next (pads);
                            if (padtemplate->direction != GST_PAD_SINK)
                                continue;
                            if (padtemplate->static_caps.string) {
                                GstCaps *caps = gst_static_caps_get(&padtemplate->static_caps);
                                if (!gst_caps_is_any (caps) && ! gst_caps_is_empty (caps)) {
                                    for (guint i = 0; i < gst_caps_get_size(caps); i++) {
                                        GstStructure *structure = gst_caps_get_structure(caps, i);
                                        QString nameLowcase = QString(gst_structure_get_name (structure)).toLower();

                                        m_supportedMimeTypeSet.insert(nameLowcase);
                                        if (nameLowcase.contains("mpeg")) {
                                            //Because mpeg version number is only included in the detail
                                            //description,  it is necessary to manually extract this information
                                            //in order to match the mime type of mpeg4.
                                            const GValue *value = gst_structure_get_value(structure, "mpegversion");
                                            if (value) {
                                                gchar *str = gst_value_serialize (value);
                                                QString versions(str);
                                                QStringList elements = versions.split(QRegExp("\\D+"), QString::SkipEmptyParts);
                                                foreach (const QString &e, elements)
                                                    m_supportedMimeTypeSet.insert(nameLowcase + e);
                                                g_free (str);
                                            }
                                        }
                                    }
                                }
                                gst_caps_unref(caps);
                            }
                        }
                        gst_object_unref (factory);
                    }
                } else if (GST_IS_TYPE_FIND_FACTORY(feature)) {
                    QString name(gst_plugin_feature_get_name(feature));
                    if (name.contains('/')) //filter out any string without '/' which is obviously not a mime type
                        m_supportedMimeTypeSet.insert(name.toLower());
                }
            }
            features = g_list_next (features);
        }
        gst_plugin_feature_list_free (orig_features);
    }
    gst_plugin_list_free (orig_plugins);

#if defined QT_SUPPORTEDMIMETYPES_DEBUG
    QStringList list = m_supportedMimeTypeSet.toList();
    list.sort();
    if (qgetenv("QT_DEBUG_PLUGINS").toInt() > 0) {
        foreach (const QString &type, list)
            qDebug() << type;
    }
#endif
}

QStringList QGstreamerCaptureServicePlugin::supportedMimeTypes() const
{
    return QStringList();
}

