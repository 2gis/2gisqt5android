/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQMLENGINE_H
#define QQMLENGINE_H

#include <QtCore/qurl.h>
#include <QtCore/qobject.h>
#include <QtCore/qmap.h>
#include <QtQml/qjsengine.h>
#include <QtQml/qqmlerror.h>
#include <QtQml/qqmldebug.h>

QT_BEGIN_NAMESPACE

class QQmlAbstractUrlInterceptor;

class Q_QML_EXPORT QQmlImageProviderBase
{
public:
    enum ImageType {
        Image,
        Pixmap,
        Texture,
        Invalid
    };

    enum Flag {
        ForceAsynchronousImageLoading  = 0x01
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    virtual ~QQmlImageProviderBase();

    virtual ImageType imageType() const = 0;
    virtual Flags flags() const = 0;

private:
    friend class QQuickImageProvider;
    QQmlImageProviderBase();
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QQmlImageProviderBase::Flags)

class QQmlComponent;
class QQmlEnginePrivate;
class QQmlImportsPrivate;
class QQmlExpression;
class QQmlContext;
class QQmlType;
class QUrl;
class QScriptContext;
class QNetworkAccessManager;
class QQmlNetworkAccessManagerFactory;
class QQmlIncubationController;
class Q_QML_EXPORT QQmlEngine : public QJSEngine
{
    Q_PROPERTY(QString offlineStoragePath READ offlineStoragePath WRITE setOfflineStoragePath)
    Q_OBJECT
public:
    QQmlEngine(QObject *p = 0);
    virtual ~QQmlEngine();

    QQmlContext *rootContext() const;

    void clearComponentCache();
    void trimComponentCache();

    QStringList importPathList() const;
    void setImportPathList(const QStringList &paths);
    void addImportPath(const QString& dir);

    QStringList pluginPathList() const;
    void setPluginPathList(const QStringList &paths);
    void addPluginPath(const QString& dir);

    bool addNamedBundle(const QString &name, const QString &fileName);

    bool importPlugin(const QString &filePath, const QString &uri, QList<QQmlError> *errors);

    void setNetworkAccessManagerFactory(QQmlNetworkAccessManagerFactory *);
    QQmlNetworkAccessManagerFactory *networkAccessManagerFactory() const;

    QNetworkAccessManager *networkAccessManager() const;

    void setUrlInterceptor(QQmlAbstractUrlInterceptor* urlInterceptor);
    QQmlAbstractUrlInterceptor* urlInterceptor() const;

    void addImageProvider(const QString &id, QQmlImageProviderBase *);
    QQmlImageProviderBase *imageProvider(const QString &id) const;
    void removeImageProvider(const QString &id);

    void setIncubationController(QQmlIncubationController *);
    QQmlIncubationController *incubationController() const;

    void setOfflineStoragePath(const QString& dir);
    QString offlineStoragePath() const;

    QUrl baseUrl() const;
    void setBaseUrl(const QUrl &);

    bool outputWarningsToStandardError() const;
    void setOutputWarningsToStandardError(bool);

    static QQmlContext *contextForObject(const QObject *);
    static void setContextForObject(QObject *, QQmlContext *);

    enum ObjectOwnership { CppOwnership, JavaScriptOwnership };
    static void setObjectOwnership(QObject *, ObjectOwnership);
    static ObjectOwnership objectOwnership(QObject *);
protected:
    QQmlEngine(QQmlEnginePrivate &dd, QObject *p);
    virtual bool event(QEvent *);

Q_SIGNALS:
    void quit();
    void warnings(const QList<QQmlError> &warnings);

private:
    Q_DISABLE_COPY(QQmlEngine)
    Q_DECLARE_PRIVATE(QQmlEngine)
};

QT_END_NAMESPACE

#endif // QQMLENGINE_H
