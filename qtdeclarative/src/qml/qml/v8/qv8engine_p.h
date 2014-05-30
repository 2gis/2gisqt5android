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

#ifndef QQMLV8ENGINE_P_H
#define QQMLV8ENGINE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qglobal.h>
#include <QtCore/qvariant.h>
#include <QtCore/qset.h>
#include <QtCore/qmutex.h>
#include <QtCore/qstack.h>
#include <QtCore/qstringlist.h>
#include <QtCore/QElapsedTimer>
#include <QtCore/QThreadStorage>

#include <qjsengine.h>
#include "private/qintrusivelist_p.h"

#include <private/qqmlpropertycache_p.h>

#include <private/qv4qobjectwrapper_p.h>
#include <private/qv4value_inl_p.h>
#include <private/qv4object_p.h>
#include <private/qv4identifier_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {
    struct ArrayObject;
    struct ExecutionEngine;
}

// Uncomment the following line to enable global handle debugging.  When enabled, all the persistent
// handles allocated using qPersistentNew() (or registered with qPersistentRegsiter()) and disposed
// with qPersistentDispose() are tracked.  If you try and do something illegal, like double disposing
// a handle, qFatal() is called.
// #define QML_GLOBAL_HANDLE_DEBUGGING

#define V4THROW_ERROR(string) \
    return ctx->throwError(QString::fromUtf8(string));

#define V4THROW_TYPE(string) \
    return ctx->throwTypeError(QStringLiteral(string));

#define V8_DEFINE_EXTENSION(dataclass, datafunction) \
    static inline dataclass *datafunction(QV8Engine *engine) \
    { \
        static int extensionId = -1; \
        if (extensionId == -1) { \
            QV8Engine::registrationMutex()->lock(); \
            if (extensionId == -1) \
                extensionId = QV8Engine::registerExtension(); \
            QV8Engine::registrationMutex()->unlock(); \
        } \
        dataclass *rv = (dataclass *)engine->extensionData(extensionId); \
        if (!rv) { \
            rv = new dataclass(engine); \
            engine->setExtensionData(extensionId, rv); \
        } \
        return rv; \
    } \

// Used to allow a QObject method take and return raw V8 handles without having to expose
// v8 in the public API.
// Use like this:
//     class MyClass : public QObject {
//         Q_OBJECT
//         ...
//         Q_INVOKABLE void myMethod(QQmlV8Function*);
//     };
// The QQmlV8Function - and consequently the arguments and return value - only remains
// valid during the call.  If the return value isn't set within myMethod(), the will return
// undefined.
class QV8Engine;
// ### GC
class QQmlV4Function
{
public:
    int length() const { return callData->argc; }
    QV4::ReturnedValue operator[](int idx) { return (idx < callData->argc ? callData->args[idx].asReturnedValue() : QV4::Encode::undefined()); }
    QQmlContextData *context() { return ctx; }
    QV4::ReturnedValue qmlGlobal() { return callData->thisObject.asReturnedValue(); }
    void setReturnValue(QV4::ReturnedValue rv) { retVal = rv; }
    QV8Engine *engine() const { return e; }
    QV4::ExecutionEngine *v4engine() const;
private:
    friend struct QV4::QObjectMethod;
    QQmlV4Function();
    QQmlV4Function(const QQmlV4Function &);
    QQmlV4Function &operator=(const QQmlV4Function &);

    QQmlV4Function(QV4::CallData *callData, QV4::ValueRef retVal,
                   const QV4::ValueRef global, QQmlContextData *c, QV8Engine *e)
        : callData(callData), retVal(retVal), ctx(c), e(e)
    {
        callData->thisObject.val = global.asReturnedValue();
    }

    QV4::CallData *callData;
    QV4::ValueRef retVal;
    QQmlContextData *ctx;
    QV8Engine *e;
};

class Q_QML_PRIVATE_EXPORT QQmlV4Handle
{
public:
    QQmlV4Handle() : d(QV4::Encode::undefined()) {}
    explicit QQmlV4Handle(QV4::ValueRef v) : d(v.asReturnedValue()) {}
    explicit QQmlV4Handle(QV4::ReturnedValue v) : d(v) {}

    operator QV4::ReturnedValue() const { return d; }

private:
    quint64 d;
};

class QObject;
class QQmlEngine;
class QQmlValueType;
class QNetworkAccessManager;
class QQmlContextData;

class Q_QML_PRIVATE_EXPORT QV8Engine
{
    friend class QJSEngine;
    typedef QSet<QV4::Object *> V8ObjectSet;
public:
    static QV8Engine* get(QJSEngine* q) { Q_ASSERT(q); return q->handle(); }
//    static QJSEngine* get(QV8Engine* d) { Q_ASSERT(d); return d->q; }
    static QV4::ExecutionEngine *getV4(QJSEngine *q) { return q->handle()->m_v4Engine; }
    static QV4::ExecutionEngine *getV4(QV8Engine *d) { return d->m_v4Engine; }

    QV8Engine(QJSEngine* qq);
    virtual ~QV8Engine();

    // This enum should be in sync with QQmlEngine::ObjectOwnership
    enum ObjectOwnership { CppOwnership, JavaScriptOwnership };

    struct Deletable {
        virtual ~Deletable() {}
    };

    void initQmlGlobalObject();
    void setEngine(QQmlEngine *engine);
    QQmlEngine *engine() { return m_engine; }
    QJSEngine *publicEngine() { return q; }
    QV4::ReturnedValue global();

    void *xmlHttpRequestData() { return m_xmlHttpRequestData; }

    Deletable *listModelData() { return m_listModelData; }
    void setListModelData(Deletable *d) { if (m_listModelData) delete m_listModelData; m_listModelData = d; }

    QQmlContextData *callingContext();

    void freezeObject(const QV4::ValueRef value);

    QVariant toVariant(const QV4::ValueRef value, int typeHint);
    QV4::ReturnedValue fromVariant(const QVariant &);

    // Return a JS string for the given QString \a string
    QV4::ReturnedValue toString(const QString &string);

    // Return the network access manager for this engine.  By default this returns the network
    // access manager of the QQmlEngine.  It is overridden in the case of a threaded v8
    // instance (like in WorkerScript).
    virtual QNetworkAccessManager *networkAccessManager();

    // Return the list of illegal id names (the names of the properties on the global object)
    const QSet<QString> &illegalNames() const;

    void gc();

    static QMutex *registrationMutex();
    static int registerExtension();

    inline Deletable *extensionData(int) const;
    void setExtensionData(int, Deletable *);

    QV4::ReturnedValue variantListToJS(const QVariantList &lst);
    inline QVariantList variantListFromJS(QV4::ArrayObjectRef array)
    { V8ObjectSet visitedObjects; return variantListFromJS(array, visitedObjects); }

    QV4::ReturnedValue variantMapToJS(const QVariantMap &vmap);
    inline QVariantMap variantMapFromJS(QV4::ObjectRef object)
    { V8ObjectSet visitedObjects; return variantMapFromJS(object, visitedObjects); }

    QV4::ReturnedValue variantToJS(const QVariant &value);
    inline QVariant variantFromJS(const QV4::ValueRef value)
    { V8ObjectSet visitedObjects; return variantFromJS(value, visitedObjects); }

    QV4::ReturnedValue metaTypeToJS(int type, const void *data);
    bool metaTypeFromJS(const QV4::ValueRef value, int type, void *data);

    bool convertToNativeQObject(const QV4::ValueRef value,
                                const QByteArray &targetType,
                                void **result);

    // used for console.time(), console.timeEnd()
    void startTimer(const QString &timerName);
    qint64 stopTimer(const QString &timerName, bool *wasRunning);

    // used for console.count()
    int consoleCountHelper(const QString &file, quint16 line, quint16 column);

    QObject *qtObjectFromJS(const QV4::ValueRef value);

protected:
    QJSEngine* q;
    QQmlEngine *m_engine;

    QV4::ExecutionEngine *m_v4Engine;

    QV4::PersistentValue m_freezeObject;

    void *m_xmlHttpRequestData;

    QVector<Deletable *> m_extensionData;
    Deletable *m_listModelData;

    QSet<QString> m_illegalNames;

    QElapsedTimer m_time;
    QHash<QString, qint64> m_startedTimers;

    QHash<QString, quint32> m_consoleCount;

    QVariant toBasicVariant(const QV4::ValueRef);

    void initializeGlobal();

private:
    QVariantList variantListFromJS(QV4::ArrayObjectRef array, V8ObjectSet &visitedObjects);
    QVariantMap variantMapFromJS(QV4::ObjectRef object, V8ObjectSet &visitedObjects);
    QVariant variantFromJS(const QV4::ValueRef value, V8ObjectSet &visitedObjects);

    Q_DISABLE_COPY(QV8Engine)
};

inline QV8Engine::Deletable *QV8Engine::extensionData(int index) const
{
    if (index < m_extensionData.count())
        return m_extensionData[index];
    else
        return 0;
}

inline QV4::ExecutionEngine *QQmlV4Function::v4engine() const
{
    return QV8Engine::getV4(e);
}

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QQmlV4Handle)

#endif // QQMLV8ENGINE_P_H
