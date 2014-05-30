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


#ifndef QGSTREAMERMEDIACONTAINERCONTROL_H
#define QGSTREAMERMEDIACONTAINERCONTROL_H

#include <qmediacontainercontrol.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qset.h>

#include <gst/gst.h>

QT_BEGIN_NAMESPACE

class QGstreamerMediaContainerControl : public QMediaContainerControl
{
Q_OBJECT
public:
    QGstreamerMediaContainerControl(QObject *parent);
    virtual ~QGstreamerMediaContainerControl() {};

    virtual QStringList supportedContainers() const { return m_supportedContainers; }
    virtual QString containerFormat() const { return m_format; }
    virtual void setContainerFormat(const QString &formatMimeType) { m_format = formatMimeType; }

    virtual QString containerDescription(const QString &formatMimeType) const { return m_containerDescriptions.value(formatMimeType); }

    QByteArray formatElementName() const { return m_elementNames.value(containerFormat()); }

    QSet<QString> supportedStreamTypes(const QString &container) const;

    static QSet<QString> supportedStreamTypes(GstElementFactory *factory, GstPadDirection direction);

    QString containerExtension() const;

private:
    QString m_format;
    QStringList m_supportedContainers;
    QMap<QString,QByteArray> m_elementNames;
    QMap<QString, QString> m_containerDescriptions;
    QMap<QString, QString> m_containerExtensions;
    QMap<QString, QSet<QString> > m_streamTypes;
};

QT_END_NAMESPACE

#endif // QGSTREAMERMEDIACONTAINERCONTROL_H
