/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
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

#ifndef QQUICKWEBENGINEVIEW_P_H
#define QQUICKWEBENGINEVIEW_P_H

#include <private/qtwebengineglobal_p.h>
#include "qquickwebenginescript_p.h"
#include <QQuickItem>

QT_BEGIN_NAMESPACE

class QQmlWebChannel;
class QQuickWebEngineCertificateError;
class QQuickWebEngineHistory;
class QQuickWebEngineLoadRequest;
class QQuickWebEngineNavigationRequest;
class QQuickWebEngineNewViewRequest;
class QQuickWebEngineProfile;
class QQuickWebEngineSettings;
class QQuickWebEngineViewExperimental;
class QQuickWebEngineViewPrivate;

#ifdef ENABLE_QML_TESTSUPPORT_API
class QQuickWebEngineTestSupport;
#endif

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineFullScreenRequest {
    Q_GADGET
    Q_PROPERTY(bool toggleOn READ toggleOn)
public:
    QQuickWebEngineFullScreenRequest();
    QQuickWebEngineFullScreenRequest(QQuickWebEngineViewPrivate *viewPrivate, bool toggleOn);

    Q_INVOKABLE void accept();
    bool toggleOn() { return m_toggleOn; }

private:
    QQuickWebEngineViewPrivate *viewPrivate;
    bool m_toggleOn;
};

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineView : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QUrl icon READ icon NOTIFY iconChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(int loadProgress READ loadProgress NOTIFY loadProgressChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY urlChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY urlChanged)
    Q_PROPERTY(bool isFullScreen READ isFullScreen NOTIFY isFullScreenChanged REVISION 1)
    Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY zoomFactorChanged REVISION 1)
    Q_PROPERTY(QQuickWebEngineProfile *profile READ profile WRITE setProfile NOTIFY profileChanged FINAL REVISION 1)
    Q_PROPERTY(QQuickWebEngineSettings *settings READ settings REVISION 1)
    Q_PROPERTY(QQuickWebEngineHistory *navigationHistory READ navigationHistory CONSTANT FINAL REVISION 1)
    Q_PROPERTY(QQmlWebChannel *webChannel READ webChannel WRITE setWebChannel NOTIFY webChannelChanged REVISION 1)
    Q_PROPERTY(QQmlListProperty<QQuickWebEngineScript> userScripts READ userScripts FINAL)

#ifdef ENABLE_QML_TESTSUPPORT_API
    Q_PROPERTY(QQuickWebEngineTestSupport *testSupport READ testSupport WRITE setTestSupport FINAL)
#endif

    Q_ENUMS(NavigationRequestAction);
    Q_ENUMS(NavigationType);
    Q_ENUMS(LoadStatus);
    Q_ENUMS(ErrorDomain);
    Q_ENUMS(NewViewDestination);
    Q_ENUMS(Feature);
    Q_ENUMS(JavaScriptConsoleMessageLevel);
    Q_FLAGS(FindFlags);

public:
    QQuickWebEngineView(QQuickItem *parent = 0);
    ~QQuickWebEngineView();

    QUrl url() const;
    void setUrl(const QUrl&);
    QUrl icon() const;
    bool isLoading() const;
    int loadProgress() const;
    QString title() const;
    bool canGoBack() const;
    bool canGoForward() const;
    bool isFullScreen() const;
    qreal zoomFactor() const;
    void setZoomFactor(qreal arg);

    QQuickWebEngineViewExperimental *experimental() const;

    // must match WebContentsAdapterClient::NavigationRequestAction
    enum NavigationRequestAction {
        AcceptRequest,
        // Make room in the valid range of the enum so
        // we can expose extra actions in experimental.
        IgnoreRequest = 0xFF
    };

    // must match WebContentsAdapterClient::NavigationType
    enum NavigationType {
        LinkClickedNavigation,
        TypedNavigation,
        FormSubmittedNavigation,
        BackForwardNavigation,
        ReloadNavigation,
        OtherNavigation
    };

    enum LoadStatus {
        LoadStartedStatus,
        LoadStoppedStatus,
        LoadSucceededStatus,
        LoadFailedStatus
    };

    enum ErrorDomain {
         NoErrorDomain,
         InternalErrorDomain,
         ConnectionErrorDomain,
         CertificateErrorDomain,
         HttpErrorDomain,
         FtpErrorDomain,
         DnsErrorDomain
    };

    enum NewViewDestination {
        NewViewInWindow,
        NewViewInTab,
        NewViewInDialog,
        NewViewInBackgroundTab
    };

    enum Feature {
        MediaAudioCapture,
        MediaVideoCapture,
        MediaAudioVideoCapture,
        Geolocation
    };

    // must match WebContentsAdapterClient::JavaScriptConsoleMessageLevel
    enum JavaScriptConsoleMessageLevel {
        InfoMessageLevel = 0,
        WarningMessageLevel,
        ErrorMessageLevel
    };

    enum FindFlag {
        FindBackward = 1,
        FindCaseSensitively = 2,
    };
    Q_DECLARE_FLAGS(FindFlags, FindFlag)

    // QmlParserStatus
    virtual void componentComplete() Q_DECL_OVERRIDE;

    QQuickWebEngineProfile *profile() const;
    void setProfile(QQuickWebEngineProfile *);
    QQmlListProperty<QQuickWebEngineScript> userScripts();

    QQuickWebEngineSettings *settings() const;
    QQmlWebChannel *webChannel();
    void setWebChannel(QQmlWebChannel *);
    QQuickWebEngineHistory *navigationHistory() const;

#ifdef ENABLE_QML_TESTSUPPORT_API
    QQuickWebEngineTestSupport *testSupport() const;
    void setTestSupport(QQuickWebEngineTestSupport *testSupport);
#endif

public Q_SLOTS:
    void runJavaScript(const QString&, const QJSValue & = QJSValue());
    void loadHtml(const QString &html, const QUrl &baseUrl = QUrl());
    void goBack();
    void goForward();
    void goBackOrForward(int index);
    void reload();
    void reloadAndBypassCache();
    void stop();
    Q_REVISION(1) void findText(const QString &subString, FindFlags options = 0, const QJSValue &callback = QJSValue());
    Q_REVISION(1) void fullScreenCancelled();
    Q_REVISION(1) void grantFeaturePermission(const QUrl &securityOrigin, Feature, bool granted);

Q_SIGNALS:
    void titleChanged();
    void urlChanged();
    void iconChanged();
    void loadingChanged(QQuickWebEngineLoadRequest *loadRequest);
    void loadProgressChanged();
    void linkHovered(const QUrl &hoveredUrl);
    void navigationRequested(QQuickWebEngineNavigationRequest *request);
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message, int lineNumber, const QString &sourceID);
    Q_REVISION(1) void certificateError(QQuickWebEngineCertificateError *error);
    Q_REVISION(1) void fullScreenRequested(const QQuickWebEngineFullScreenRequest &request);
    Q_REVISION(1) void isFullScreenChanged();
    Q_REVISION(1) void featurePermissionRequested(const QUrl &securityOrigin, Feature feature);
    Q_REVISION(1) void newViewRequested(QQuickWebEngineNewViewRequest *request);
    Q_REVISION(1) void zoomFactorChanged(qreal arg);
    Q_REVISION(1) void profileChanged();
    Q_REVISION(1) void webChannelChanged();


protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry);
    void itemChange(ItemChange, const ItemChangeData &);

private:
    Q_DECLARE_PRIVATE(QQuickWebEngineView)
    QScopedPointer<QQuickWebEngineViewPrivate> d_ptr;

    friend class QQuickWebEngineViewExperimental;
    friend class QQuickWebEngineViewExperimentalExtension;
    friend class QQuickWebEngineNewViewRequest;
#ifndef QT_NO_ACCESSIBILITY
    friend class QQuickWebEngineViewAccessible;
#endif // QT_NO_ACCESSIBILITY
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickWebEngineView)
Q_DECLARE_METATYPE(QQuickWebEngineFullScreenRequest)

#endif // QQUICKWEBENGINEVIEW_P_H
