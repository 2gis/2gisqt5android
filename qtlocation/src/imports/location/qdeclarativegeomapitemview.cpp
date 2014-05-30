/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#include "qdeclarativegeomapitemview_p.h"
#include "qdeclarativegeomap_p.h"
#include "qdeclarativegeomapitembase_p.h"

#include <QtCore/QAbstractItemModel>
#include <QtQml/QQmlParserStatus>
#include <QtQml/QQmlContext>
#include <QtQml/private/qqmlopenmetaobject_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype MapItemView
    \instantiates QDeclarativeGeoMapItemView
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-maps
    \since Qt Location 5.0
    \inherits QQuickItem

    \brief The MapItemView is used to populate Map from a model.

    The MapItemView is used to populate Map with MapItems from a model.
    The MapItemView type only makes sense when contained in a Map,
    meaning that it has no standalone presentation.

    \section2 Example Usage

    This example demonstrates how to use the MapViewItem object to display
    a \l{Route}{route} on a \l{Map}{map}:

    \snippet declarative/maps.qml QtQuick import
    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/maps.qml MapRoute
*/

QDeclarativeGeoMapItemView::QDeclarativeGeoMapItemView(QQuickItem *parent)
    : QObject(parent), componentCompleted_(false), delegate_(0),
      itemModel_(0), map_(0), fitViewport_(false)
{
}

QDeclarativeGeoMapItemView::~QDeclarativeGeoMapItemView()
{
    if (map_)
        removeInstantiatedItems();
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemView::componentComplete()
{
    componentCompleted_ = true;
}

/*!
    \qmlproperty model QtLocation::MapItemView::model

    This property holds the model that provides data used for
    creating the map item defined by the delegate.
*/
QVariant QDeclarativeGeoMapItemView::model() const
{
    return modelVariant_;
}

void QDeclarativeGeoMapItemView::setModel(const QVariant &model)
{
    if (!model.isValid() || model == modelVariant_)
        return;

    QAbstractItemModel *itemModel = 0;
    if (QObject *object = qvariant_cast<QObject *>(model))
        itemModel = qobject_cast<QAbstractItemModel *>(object);
    if (!itemModel)
        return;

    modelVariant_ = model;
    itemModel_ = itemModel;
    QObject::connect(itemModel_, SIGNAL(modelReset()),
                     this, SLOT(itemModelReset()));
    QObject::connect(itemModel_, SIGNAL(rowsRemoved(QModelIndex,int,int)),
                     this, SLOT(itemModelRowsRemoved(QModelIndex,int,int)));
    QObject::connect(itemModel_, SIGNAL(rowsInserted(QModelIndex,int,int)),
                     this, SLOT(itemModelRowsInserted(QModelIndex,int,int)));
    repopulate();
    emit modelChanged();
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemView::itemModelReset()
{
    repopulate();
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemView::itemModelRowsInserted(QModelIndex, int start, int end)
{
    if (!componentCompleted_ || !map_ || !delegate_ || !itemModel_)
        return;

    QDeclarativeGeoMapItemBase *mapItem;
    for (int i = start; i <= end; ++i) {
        mapItem = createItem(i);
        if (!mapItem) {
            break;
        }
        mapItemList_.append(mapItem);
        map_->addMapItem(mapItem);
    }
    if (fitViewport_)
        fitViewport();
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemView::itemModelRowsRemoved(QModelIndex, int start, int end)
{
    if (!componentCompleted_ || !map_ || !delegate_ || !itemModel_)
        return;

    for (int i = end; i >= start; --i) {
        QDeclarativeGeoMapItemBase *mapItem = mapItemList_.takeAt(i);
        Q_ASSERT(mapItem);
        if (!mapItem) // bad
            break;
        map_->removeMapItem(mapItem);
        mapItem->deleteLater();
    }
    if (fitViewport_)
        fitViewport();
}

/*!
    \qmlproperty Component QtLocation::MapItemView::delegate

    This property holds the delegate which defines how each item in the
    model should be displayed. The Component must contain exactly one
    MapItem -derived object as the root object.

*/
QQmlComponent *QDeclarativeGeoMapItemView::delegate() const
{
    return delegate_;
}

void QDeclarativeGeoMapItemView::setDelegate(QQmlComponent *delegate)
{
    if (!delegate)
        return;
    delegate_ = delegate;

    repopulate();
    emit delegateChanged();
}

/*!
    \qmlproperty Component QtLocation::MapItemView::autoFitViewport

    This property controls whether to automatically pan and zoom the viewport
    to display all map items when items are added or removed.

    Defaults to false.

*/
bool QDeclarativeGeoMapItemView::autoFitViewport() const
{
    return fitViewport_;
}

void QDeclarativeGeoMapItemView::setAutoFitViewport(const bool &fitViewport)
{
    if (fitViewport == fitViewport_)
        return;
    fitViewport_ = fitViewport;
    emit autoFitViewportChanged();
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemView::fitViewport()
{
    if (!map_ || !fitViewport_)
        return;

    if (map_->mapItems().size() > 0)
        map_->fitViewportToMapItems();
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemView::setMapData(QDeclarativeGeoMap *map)
{
    if (!map || map_) // changing map on the fly not supported
        return;
    map_ = map;
}

/*!
    \internal
*/
void QDeclarativeGeoMapItemView::removeInstantiatedItems()
{
    if (!map_)
        return;
    foreach (QDeclarativeGeoMapItemBase *mapItem, mapItemList_) {
        mapItem->deleteLater();
        map_->removeMapItem(mapItem);
    }
    mapItemList_.clear();
}

/*!
    \internal
    Removes and repopulates all items.
*/
void QDeclarativeGeoMapItemView::repopulate()
{
    if (!componentCompleted_ || !map_ || !delegate_ || !itemModel_)
        return;
    // Free any earlier instances
    removeInstantiatedItems();

    // Iterate model data and instantiate delegates.
    QDeclarativeGeoMapItemBase *mapItem;
    for (int i = 0; i < itemModel_->rowCount(); ++i) {
        mapItem = createItem(i);
        Q_ASSERT(mapItem);
        if (!mapItem) // bad
            break;
        mapItemList_.append(mapItem);
        map_->addMapItem(mapItem);
    }
    if (fitViewport_)
        fitViewport();
}

QDeclarativeGeoMapItemBase *QDeclarativeGeoMapItemView::createItem(int modelRow)
{
    if (itemModel_)
        return createItemFromItemModel(modelRow);
    return 0;
}


/*!
    \internal
*/
QDeclarativeGeoMapItemBase *QDeclarativeGeoMapItemView::createItemFromItemModel(int modelRow)
{
    if (!delegate_ || !itemModel_)
        return 0;

    QModelIndex index = itemModel_->index(modelRow, 0); // column 0
    if (!index.isValid()) {
        qWarning() << "QDeclarativeGeoMapItemView Index is not valid: " << modelRow;
        return 0;
    }

    QObject *model = new QObject(this);
    QQmlOpenMetaObject *modelMetaObject = new QQmlOpenMetaObject(model);

    QHashIterator<int, QByteArray> iterator(itemModel_->roleNames());
    QQmlContext *itemContext = new QQmlContext(qmlContext(this));
    while (iterator.hasNext()) {
        iterator.next();
        QVariant modelData = itemModel_->data(index, iterator.key());
        if (!modelData.isValid())
            continue;

        itemContext->setContextProperty(QString::fromLatin1(iterator.value().constData()),
                                        modelData);

        modelMetaObject->setValue(iterator.value(), modelData);
    }
    itemContext->setContextProperty(QLatin1String("model"), model);
    itemContext->setContextProperty(QLatin1String("index"), modelRow);

    QObject *obj = delegate_->create(itemContext);

    if (!obj) {
        qWarning() << "QDeclarativeGeoMapItemView map item creation failed.";
        delete itemContext;
        return 0;
    }
    QDeclarativeGeoMapItemBase *declMapObj = qobject_cast<QDeclarativeGeoMapItemBase *>(obj);
    if (!declMapObj) {
        qWarning() << "QDeclarativeGeoMapItemView map item delegate is of unsupported type.";
        delete itemContext;
        return 0;
    }
    itemContext->setParent(declMapObj);
    model->setParent(declMapObj);
    return declMapObj;
}

#include "moc_qdeclarativegeomapitemview_p.cpp"

QT_END_NAMESPACE
