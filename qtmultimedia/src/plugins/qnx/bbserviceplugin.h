/****************************************************************************
**
** Copyright (C) 2012 Research In Motion
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
#ifndef BBRSERVICEPLUGIN_H
#define BBRSERVICEPLUGIN_H

#include <qmediaserviceproviderplugin.h>

QT_BEGIN_NAMESPACE

class BbServicePlugin
    : public QMediaServiceProviderPlugin,
      public QMediaServiceSupportedDevicesInterface,
      public QMediaServiceDefaultDeviceInterface,
      public QMediaServiceCameraInfoInterface,
      public QMediaServiceFeaturesInterface
{
    Q_OBJECT
    Q_INTERFACES(QMediaServiceSupportedDevicesInterface)
    Q_INTERFACES(QMediaServiceDefaultDeviceInterface)
    Q_INTERFACES(QMediaServiceCameraInfoInterface)
    Q_INTERFACES(QMediaServiceFeaturesInterface)
    Q_PLUGIN_METADATA(IID "org.qt-project.qt.mediaserviceproviderfactory/5.0" FILE "blackberry_mediaservice.json")
public:
    BbServicePlugin();

    QMediaService *create(const QString &key) Q_DECL_OVERRIDE;
    void release(QMediaService *service) Q_DECL_OVERRIDE;
    QMediaServiceProviderHint::Features supportedFeatures(const QByteArray &service) const Q_DECL_OVERRIDE;

    QByteArray defaultDevice(const QByteArray &service) const Q_DECL_OVERRIDE;
    QList<QByteArray> devices(const QByteArray &service) const Q_DECL_OVERRIDE;
    QString deviceDescription(const QByteArray &service, const QByteArray &device) Q_DECL_OVERRIDE;
    QVariant deviceProperty(const QByteArray &service, const QByteArray &device, const QByteArray &property) Q_DECL_OVERRIDE;

    QCamera::Position cameraPosition(const QByteArray &device) const Q_DECL_OVERRIDE;
    int cameraOrientation(const QByteArray &device) const Q_DECL_OVERRIDE;

private:
    void updateDevices() const;

    mutable QByteArray m_defaultCameraDevice;
    mutable QList<QByteArray> m_cameraDevices;
    mutable QStringList m_cameraDescriptions;
};

QT_END_NAMESPACE

#endif
