/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#ifndef QQUICKTASKBARBUTTON_P_H
#define QQUICKTASKBARBUTTON_P_H

#include <QQuickItem>
#include <QWinTaskbarButton>
#include <QWinTaskbarProgress>

#include "qquickiconloader_p.h"

QT_BEGIN_NAMESPACE

class QQuickTaskbarButtonPrivate;

class QQuickTaskbarOverlay : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl iconSource READ iconSource WRITE setIconSource NOTIFY iconSourceChanged)
    Q_PROPERTY(QString accessibleDescription READ accessibleDescription WRITE setAccessibleDescription NOTIFY accessibleDescriptionChanged)

public:
    explicit QQuickTaskbarOverlay(QWinTaskbarButton *button, QObject *parent = 0);

    QUrl iconSource() const;
    void setIconSource(const QUrl &iconSource);

    QString accessibleDescription() const;
    void setAccessibleDescription(const QString &description);

Q_SIGNALS:
    void iconSourceChanged();
    void accessibleDescriptionChanged();

private Q_SLOTS:
    void iconLoaded();

private:
    QUrl m_iconSource;
    QQuickIconLoader m_loader;
    QWinTaskbarButton *m_button;
};

class QQuickTaskbarButton : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QQuickTaskbarOverlay *overlay  READ overlay CONSTANT)
    Q_PROPERTY(QWinTaskbarProgress *progress READ progress CONSTANT)

public:
    explicit QQuickTaskbarButton(QQuickItem *parent = 0);
    ~QQuickTaskbarButton();

    QQuickTaskbarOverlay *overlay() const;
    QWinTaskbarProgress *progress() const;

protected:
    void itemChange(ItemChange, const ItemChangeData &) Q_DECL_OVERRIDE;

private:
    QWinTaskbarButton *m_button;
    QQuickTaskbarOverlay *m_overlay;
};

QT_END_NAMESPACE

#endif // QQUICKTASKBARBUTTON_P_H
