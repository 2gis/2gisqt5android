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
#include "qgeocameratiles_p.h"
#include "qgeocameradata_p.h"
#include "qgeotilespec_p.h"
#include "qgeomaptype_p.h"

#include <QtPositioning/private/qgeoprojection_p.h>
#include <QtPositioning/private/qdoublevector2d_p.h>
#include <QtPositioning/private/qdoublevector3d_p.h>

#include <QVector>
#include <QMap>
#include <QPair>

#include <QDebug>

#include <algorithm>
#include <cmath>

QT_BEGIN_NAMESPACE
#define ENABLE_PREFETCHING
// larger values fetch a bigger set when the camera stops
#define PREFETCH_FRUSTUM_SCALE 2.0
// #define PREFETCH_NEIGHBOUR_LAYER
#define PREFETCH_TWO_NEIGHBOUR_LAYERS

struct Frustum
{
    QDoubleVector3D topLeftNear;
    QDoubleVector3D topLeftFar;
    QDoubleVector3D topRightNear;
    QDoubleVector3D topRightFar;
    QDoubleVector3D bottomLeftNear;
    QDoubleVector3D bottomLeftFar;
    QDoubleVector3D bottomRightNear;
    QDoubleVector3D bottomRightFar;
};

typedef QVector<QDoubleVector3D> Polygon;

class QGeoCameraTilesPrivate
{
public:
    QGeoCameraTilesPrivate();
    ~QGeoCameraTilesPrivate();

    QString pluginString_;
    QGeoMapType mapType_;
    int mapVersion_;
    QGeoCameraData camera_;
    QSize screenSize_;
    int tileSize_;
    int maxZoom_;
    QSet<QGeoTileSpec> tiles_;

    int intZoomLevel_;
    int sideLength_;

    void updateMetadata();
    void updateGeometry(double viewExpansion = 1.0);

    Frustum frustum(double fieldOfViewGradient) const;

    class LengthSorter
    {
    public:
        QDoubleVector3D base;
        bool operator()(const QDoubleVector3D &lhs, const QDoubleVector3D &rhs)
        {
            return (lhs - base).lengthSquared() < (rhs - base).lengthSquared();
        }
    };

    void appendZIntersects(const QDoubleVector3D &start, const QDoubleVector3D &end, double z, QVector<QDoubleVector3D> &results) const;
    Polygon frustumFootprint(const Frustum &frustum) const;

    QPair<Polygon, Polygon> splitPolygonAtAxisValue(const Polygon &polygon, int axis, double value) const;
    QPair<Polygon, Polygon> clipFootprintToMap(const Polygon &footprint) const;

    QList<QPair<double, int> > tileIntersections(double p1, int t1, double p2, int t2) const;
    QSet<QGeoTileSpec> tilesFromPolygon(const Polygon &polygon) const;

    struct TileMap
    {
        TileMap();

        void add(int tileX, int tileY);

        QMap<int, QPair<int, int> > data;
    };
};

QGeoCameraTiles::QGeoCameraTiles()
    : d_ptr(new QGeoCameraTilesPrivate()) {}

QGeoCameraTiles::~QGeoCameraTiles()
{
    delete d_ptr;
}

void QGeoCameraTiles::findPrefetchTiles()
{
#if defined(ENABLE_PREFETCHING)
    Q_D(QGeoCameraTiles);

    d->tiles_.clear();

    // qDebug() << "prefetch called";
    int zoom = static_cast<int>(std::floor(d->camera_.zoomLevel()));
    d->intZoomLevel_ = zoom;
    d->sideLength_ = 1 << d->intZoomLevel_;
    d->updateGeometry(PREFETCH_FRUSTUM_SCALE);

#if defined(PREFETCH_NEIGHBOUR_LAYER)
    double zoomFraction = d->camera_.zoomLevel() - zoom;
    int nearestNeighbourLayer = zoomFraction > 0.5 ? zoom + 1 : zoom - 1;
    if (nearestNeighbourLayer <= d->maxZoom_ && nearestNeighbourLayer >= 0)
    {
        double oldZoom = d->camera_.zoomLevel();
        d->intZoomLevel_ = nearestNeighbourLayer;
        d->sideLength_ = 1 << d->intZoomLevel_;
        d->camera_.setZoomLevel(d->intZoomLevel_);

        // Approx heuristic, keeping total # prefetched tiles roughly independent of the
        // fractional zoom level.
        double neighbourScale = (1.0 + zoomFraction)/2.0;

        d->updateGeometry(PREFETCH_FRUSTUM_SCALE * neighbourScale);
        d->camera_.setZoomLevel(oldZoom);
    }
#elif defined(PREFETCH_TWO_NEIGHBOUR_LAYERS)
 //   int size1 = d->tiles_.size();

    // This is a simpler strategy, we just prefetch from layer above and below
    // for the layer below we only use half the size as this fills the screen
    double oldZoom = d->camera_.zoomLevel();
    if (zoom > 0)
    {
        d->intZoomLevel_ = zoom-1;
        d->sideLength_ = 1 << d->intZoomLevel_;
        d->camera_.setZoomLevel(d->intZoomLevel_);
        d->updateGeometry(0.5);
    }
 //   int size2 = d->tiles_.size();
    if (zoom < d->maxZoom_)
    {
        d->intZoomLevel_ = zoom+1;
        d->sideLength_ = 1 << d->intZoomLevel_;
        d->camera_.setZoomLevel(d->intZoomLevel_);
        d->updateGeometry(1.0);
    }
 //   qDebug() << "prefetched main tiles: " << size1 << " higher detail layer: " << d->tiles_.size() - size2 << " low detail layer: " << size2 - size1;
    d->intZoomLevel_ = zoom;
    d->sideLength_ = 1 << d->intZoomLevel_;
    d->camera_.setZoomLevel(oldZoom);
#endif
#endif
}


void QGeoCameraTiles::setCamera(const QGeoCameraData &camera)
{
    Q_D(QGeoCameraTiles);

    if (d->camera_ == camera)
        return;
    d->camera_ = camera;

    d->intZoomLevel_ = static_cast<int>(std::floor(d->camera_.zoomLevel()));
    d->sideLength_ = 1 << d->intZoomLevel_;

    d->tiles_.clear();
    d->updateGeometry();
}

void QGeoCameraTiles::setScreenSize(const QSize &size)
{
    Q_D(QGeoCameraTiles);

    if (d->screenSize_ == size)
        return;

    d->screenSize_ = size;
    d->tiles_.clear();
    d->updateGeometry();
}

void QGeoCameraTiles::setPluginString(const QString &pluginString)
{
    Q_D(QGeoCameraTiles);

    if (d->pluginString_ == pluginString)
        return;

    d->pluginString_ = pluginString;
    d->updateMetadata();
}

void QGeoCameraTiles::setMapType(const QGeoMapType &mapType)
{
    Q_D(QGeoCameraTiles);

    if (d->mapType_ == mapType)
        return;

    d->mapType_ = mapType;
    d->updateMetadata();
}

void QGeoCameraTiles::setMapVersion(const int mapVersion)
{
    Q_D(QGeoCameraTiles);

    if (d->mapVersion_ == mapVersion)
        return;

    d->mapVersion_ = mapVersion;
    d->updateMetadata();
}

void QGeoCameraTiles::setTileSize(int tileSize)
{
    Q_D(QGeoCameraTiles);

    if (d->tileSize_ == tileSize)
        return;

    d->tileSize_ = tileSize;
    d->tiles_.clear();
    d->updateGeometry();
}

int QGeoCameraTiles::tileSize() const
{
    Q_D(const QGeoCameraTiles);
    return d->tileSize_;
}

void QGeoCameraTiles::setMaximumZoomLevel(int maxZoom)
{
    Q_D(QGeoCameraTiles);

    if (d->maxZoom_ == maxZoom)
        return;

    d->maxZoom_ = maxZoom;
    d->tiles_.clear();
    d->updateGeometry();
}

QSet<QGeoTileSpec> QGeoCameraTiles::tiles() const
{
    Q_D(const QGeoCameraTiles);
    return d->tiles_;
}

QGeoCameraTilesPrivate::QGeoCameraTilesPrivate()
:   mapVersion_(-1), tileSize_(0), maxZoom_(0), intZoomLevel_(0), sideLength_(0)
{
}

QGeoCameraTilesPrivate::~QGeoCameraTilesPrivate() {}

void QGeoCameraTilesPrivate::updateMetadata()
{
    typedef QSet<QGeoTileSpec>::const_iterator iter;

    QSet<QGeoTileSpec> newTiles;

    iter i = tiles_.constBegin();
    iter end = tiles_.constEnd();

    for (; i != end; ++i) {
        QGeoTileSpec tile = *i;
        newTiles.insert(QGeoTileSpec(pluginString_, mapType_.mapId(), tile.zoom(), tile.x(), tile.y(), mapVersion_));
    }

    tiles_ = newTiles;
}

void QGeoCameraTilesPrivate::updateGeometry(double viewExpansion)
{
    // Find the frustum from the camera / screen / viewport information
    // The larger frustum when stationary is a form of prefetching
    Frustum f = frustum(viewExpansion);

    // Find the polygon where the frustum intersects the plane of the map
    Polygon footprint = frustumFootprint(f);

    // Clip the polygon to the map, split it up if it cross the dateline
    QPair<Polygon, Polygon> polygons = clipFootprintToMap(footprint);

    if (!polygons.first.isEmpty()) {
        QSet<QGeoTileSpec> tilesLeft = tilesFromPolygon(polygons.first);
        tiles_.unite(tilesLeft);
    }

    if (!polygons.second.isEmpty()) {
        QSet<QGeoTileSpec> tilesRight = tilesFromPolygon(polygons.second);
        tiles_.unite(tilesRight);
    }
}

Frustum QGeoCameraTilesPrivate::frustum(double fieldOfViewGradient) const
{
    QDoubleVector3D center = sideLength_ * QGeoProjection::coordToMercator(camera_.center());
    center.setZ(0.0);

    double f = qMin(screenSize_.width(), screenSize_.height()) / (1.0 * tileSize_);

    double z = std::pow(2.0, camera_.zoomLevel() - intZoomLevel_);

    double altitude = f / (2.0 * z);
    QDoubleVector3D eye = center;
    eye.setZ(altitude);

    QDoubleVector3D view = eye - center;
    QDoubleVector3D side = QDoubleVector3D::normal(view, QDoubleVector3D(0.0, 1.0, 0.0));
    QDoubleVector3D up = QDoubleVector3D::normal(side, view);

    double nearPlane = sideLength_ / (1.0 * tileSize_ * (1 << maxZoom_));
    double farPlane = 3.0;

    double aspectRatio = 1.0 * screenSize_.width() / screenSize_.height();

    double hn = 0.0;
    double wn = 0.0;
    double hf = 0.0;
    double wf = 0.0;

    // fixes field of view at 45 degrees
    // this assumes that viewSize = 2*nearPlane x 2*nearPlane

    if (aspectRatio > 1.0) {
        hn = 2 * fieldOfViewGradient * nearPlane;
        wn = hn * aspectRatio;

        hf = 2 * fieldOfViewGradient * farPlane;
        wf = hf * aspectRatio;
    } else {
        wn = 2 * fieldOfViewGradient * nearPlane;
        hn = wn / aspectRatio;

        wf = 2 * fieldOfViewGradient * farPlane;
        hf = wf / aspectRatio;
    }

    QDoubleVector3D d = center - eye;
    d.normalize();
    up.normalize();
    QDoubleVector3D right = QDoubleVector3D::normal(d, up);

    QDoubleVector3D cf = eye + d * farPlane;
    QDoubleVector3D cn = eye + d * nearPlane;

    Frustum frustum;

    frustum.topLeftFar = cf + (up * hf / 2) - (right * wf / 2);
    frustum.topRightFar = cf + (up * hf / 2) + (right * wf / 2);
    frustum.bottomLeftFar = cf - (up * hf / 2) - (right * wf / 2);
    frustum.bottomRightFar = cf - (up * hf / 2) + (right * wf / 2);

    frustum.topLeftNear = cn + (up * hn / 2) - (right * wn / 2);
    frustum.topRightNear = cn + (up * hn / 2) + (right * wn / 2);
    frustum.bottomLeftNear = cn - (up * hn / 2) - (right * wn / 2);
    frustum.bottomRightNear = cn - (up * hn / 2) + (right * wn / 2);

    return frustum;
}

void QGeoCameraTilesPrivate::appendZIntersects(const QDoubleVector3D &start,
                                               const QDoubleVector3D &end,
                                               double z,
                                               QVector<QDoubleVector3D> &results) const
{
    if (start.z() == end.z()) {
        if (start.z() == z) {
            results.append(start);
            results.append(end);
        }
    } else {
        double f = (start.z() - z) / (start.z() - end.z());
        if ((0 <= f) && (f <= 1.0)) {
            results.append((1 - f) * start + f * end);
        }
    }
}

/***************************************************/
/* Local copy of qSort & qSortHelper to suppress deprecation warnings
 * following the deprecation of QtAlgorithms. The comparison has subtle
 * differences which eluded detection so far. We just reuse old qSort for now.
 **/

template <typename RandomAccessIterator, typename LessThan>
inline void localqSort(RandomAccessIterator start, RandomAccessIterator end, LessThan lessThan)
{
    if (start != end)
        localqSortHelper(start, end, *start, lessThan);
}

template <typename RandomAccessIterator, typename T, typename LessThan>
void localqSortHelper(RandomAccessIterator start, RandomAccessIterator end, const T &t, LessThan lessThan)
{
top:
    int span = int(end - start);
    if (span < 2)
        return;

    --end;
    RandomAccessIterator low = start, high = end - 1;
    RandomAccessIterator pivot = start + span / 2;

    if (lessThan(*end, *start))
        qSwap(*end, *start);
    if (span == 2)
        return;

    if (lessThan(*pivot, *start))
        qSwap(*pivot, *start);
    if (lessThan(*end, *pivot))
        qSwap(*end, *pivot);
    if (span == 3)
        return;

    qSwap(*pivot, *end);

    while (low < high) {
        while (low < high && lessThan(*low, *end))
            ++low;

        while (high > low && lessThan(*end, *high))
            --high;

        if (low < high) {
            qSwap(*low, *high);
            ++low;
            --high;
        } else {
            break;
        }
    }

    if (lessThan(*low, *end))
        ++low;

    qSwap(*end, *low);
    localqSortHelper(start, low, t, lessThan);

    start = low + 1;
    ++end;
    goto top;
}
/***************************************************/


// Returns the intersection of the plane of the map and the camera frustum as a right handed polygon
Polygon QGeoCameraTilesPrivate::frustumFootprint(const Frustum &frustum) const
{
    Polygon points;
    points.reserve(24);

    appendZIntersects(frustum.topLeftNear, frustum.topLeftFar, 0.0, points);
    appendZIntersects(frustum.topRightNear, frustum.topRightFar, 0.0, points);
    appendZIntersects(frustum.bottomLeftNear, frustum.bottomLeftFar, 0.0, points);
    appendZIntersects(frustum.bottomRightNear, frustum.bottomRightFar, 0.0, points);

    appendZIntersects(frustum.topLeftNear, frustum.bottomLeftNear, 0.0, points);
    appendZIntersects(frustum.bottomLeftNear, frustum.bottomRightNear, 0.0, points);
    appendZIntersects(frustum.bottomRightNear, frustum.topRightNear, 0.0, points);
    appendZIntersects(frustum.topRightNear, frustum.topLeftNear, 0.0, points);

    appendZIntersects(frustum.topLeftFar, frustum.bottomLeftFar, 0.0, points);
    appendZIntersects(frustum.bottomLeftFar, frustum.bottomRightFar, 0.0, points);
    appendZIntersects(frustum.bottomRightFar, frustum.topRightFar, 0.0, points);
    appendZIntersects(frustum.topRightFar, frustum.topLeftFar, 0.0, points);

    if (points.isEmpty())
        return points;

    // sort points into a right handed polygon

    LengthSorter sorter;

    // - initial sort to remove duplicates
    sorter.base = points.first();
    localqSort(points.begin(), points.end(), sorter);
    //std::sort(points.begin(), points.end(), sorter);
    for (int i = points.size() - 1; i > 0; --i) {
        if (points.at(i) == points.at(i - 1))
            points.remove(i);
    }

    // - proper sort
    //   - start with the first point, put it in the sorted part of the list
    //   - add the nearest unsorted point to the last sorted point to the end
    //     of the sorted points
    Polygon::iterator i;
    for (i = points.begin(); i != points.end(); ++i) {
        sorter.base = *i;
        if (i + 1 != points.end())
            std::sort(i + 1, points.end(), sorter) ;
    }

    // - determine if what we have is right handed
    int size = points.size();
    if (size >= 3) {
        QDoubleVector3D normal = QDoubleVector3D::normal(points.at(1) - points.at(0),
                                                         points.at(2) - points.at(1));
        // - if not, reverse the list
        if (normal.z() < 0.0) {
            int halfSize = size / 2;
            for (int i = 0; i < halfSize; ++i) {
                QDoubleVector3D spare = points.at(i);
                points[i] = points[size - 1 - i];
                points[size - 1 - i] = spare;
            }
        }
    }

    return points;
}

QPair<Polygon, Polygon> QGeoCameraTilesPrivate::splitPolygonAtAxisValue(const Polygon &polygon, int axis, double value) const
{
    Polygon polygonBelow;
    Polygon polygonAbove;

    int size = polygon.size();

    if (size == 0) {
        return QPair<Polygon, Polygon>(polygonBelow, polygonAbove);
    }

    QVector<int> comparisons = QVector<int>(polygon.size());

    for (int i = 0; i < size; ++i) {
        double v = polygon.at(i).get(axis);
        if (qFuzzyCompare(v - value + 1.0, 1.0)) {
            comparisons[i] = 0;
        } else {
            if (v < value) {
                comparisons[i] = -1;
            } else if (value < v) {
                comparisons[i] = 1;
            }
        }
    }

    for (int index = 0; index < size; ++index) {
        int prevIndex = index - 1;
        if (prevIndex < 0)
            prevIndex += size;
        int nextIndex = (index + 1) % size;

        int prevComp = comparisons[prevIndex];
        int comp = comparisons[index];
        int nextComp = comparisons[nextIndex];

         if (comp == 0) {
            if (prevComp == -1) {
                polygonBelow.append(polygon.at(index));
                if (nextComp == 1) {
                    polygonAbove.append(polygon.at(index));
                }
            } else if (prevComp == 1) {
                polygonAbove.append(polygon.at(index));
                if (nextComp == -1) {
                    polygonBelow.append(polygon.at(index));
                }
            } else if (prevComp == 0) {
                if (nextComp == -1) {
                    polygonBelow.append(polygon.at(index));
                } else if (nextComp == 1) {
                    polygonAbove.append(polygon.at(index));
                } else if (nextComp == 0) {
                    // do nothing
                }
            }
        } else {
             if (comp == -1) {
                 polygonBelow.append(polygon.at(index));
             } else if (comp == 1) {
                 polygonAbove.append(polygon.at(index));
             }

             // there is a point between this and the next point
             // on the polygon that lies on the splitting line
             // and should be added to both the below and above
             // polygons
             if ((nextComp != 0) && (nextComp != comp)) {
                 QDoubleVector3D p1 = polygon.at(index);
                 QDoubleVector3D p2 = polygon.at(nextIndex);

                 double p1v = p1.get(axis);
                 double p2v = p2.get(axis);

                 double f = (p1v - value) / (p1v - p2v);

                 if (((0 <= f) && (f <= 1.0))
                         || qFuzzyCompare(f + 1.0, 1.0)
                         || qFuzzyCompare(f + 1.0, 2.0) ) {
                     QDoubleVector3D midPoint = (1.0 - f) * p1 + f * p2;
                     polygonBelow.append(midPoint);
                     polygonAbove.append(midPoint);
                 }
             }
        }
    }

    return QPair<Polygon, Polygon>(polygonBelow, polygonAbove);
}


QPair<Polygon, Polygon> QGeoCameraTilesPrivate::clipFootprintToMap(const Polygon &footprint) const
{
    bool clipX0 = false;
    bool clipX1 = false;
    bool clipY0 = false;
    bool clipY1 = false;

    double side = 1.0 * sideLength_;

    typedef Polygon::const_iterator const_iter;

    const_iter i = footprint.constBegin();
    const_iter end = footprint.constEnd();
    for (; i != end; ++i) {
        QDoubleVector3D p = *i;
        if ((p.x() < 0.0) || (qFuzzyIsNull(p.x())))
            clipX0 = true;
        if ((side < p.x()) || (qFuzzyCompare(side, p.x())))
            clipX1 = true;
        if (p.y() < 0.0)
            clipY0 = true;
        if (side < p.y())
            clipY1 = true;
    }

    Polygon results = footprint;

    if (clipY0) {
        results = splitPolygonAtAxisValue(results, 1, 0.0).second;
    }

    if (clipY1) {
        results = splitPolygonAtAxisValue(results, 1, side).first;
    }

    if (clipX0) {
        if (clipX1) {
            results = splitPolygonAtAxisValue(results, 0, 0.0).second;
            results = splitPolygonAtAxisValue(results, 0, side).first;
            return QPair<Polygon, Polygon>(results, Polygon());
        } else {
            QPair<Polygon, Polygon> pair = splitPolygonAtAxisValue(results, 0, 0.0);
            if (pair.first.isEmpty()) {
                // if we touched the line but didn't cross it...
                for (int i = 0; i < pair.second.size(); ++i) {
                    if (qFuzzyIsNull(pair.second.at(i).x()))
                        pair.first.append(pair.second.at(i));
                }
                if (pair.first.size() == 2) {
                    double y0 = pair.first[0].y();
                    double y1 = pair.first[1].y();
                    pair.first.clear();
                    pair.first.append(QDoubleVector3D(side, y0, 0.0));
                    pair.first.append(QDoubleVector3D(side - 0.001, y0, 0.0));
                    pair.first.append(QDoubleVector3D(side - 0.001, y1, 0.0));
                    pair.first.append(QDoubleVector3D(side, y1, 0.0));
                } else if (pair.first.size() == 1) {
                    // FIXME this is trickier
                    // - touching at one point on the tile boundary
                    // - probably need to build a triangular polygon across the edge
                    // - don't want to add another y tile if we can help it
                    //   - initial version doesn't care
                    double y = pair.first.at(0).y();
                    pair.first.clear();
                    pair.first.append(QDoubleVector3D(side - 0.001, y, 0.0));
                    pair.first.append(QDoubleVector3D(side, y + 0.001, 0.0));
                    pair.first.append(QDoubleVector3D(side, y - 0.001, 0.0));
                }
            } else {
                for (int i = 0; i < pair.first.size(); ++i) {
                    pair.first[i].setX(pair.first.at(i).x() + side);
                }
            }
            return pair;
        }
    } else {
        if (clipX1) {
            QPair<Polygon, Polygon> pair = splitPolygonAtAxisValue(results, 0, side);
            if (pair.second.isEmpty()) {
                // if we touched the line but didn't cross it...
                for (int i = 0; i < pair.first.size(); ++i) {
                    if (qFuzzyCompare(side, pair.first.at(i).x()))
                        pair.second.append(pair.first.at(i));
                }
                if (pair.second.size() == 2) {
                    double y0 = pair.second[0].y();
                    double y1 = pair.second[1].y();
                    pair.second.clear();
                    pair.second.append(QDoubleVector3D(0, y0, 0.0));
                    pair.second.append(QDoubleVector3D(0.001, y0, 0.0));
                    pair.second.append(QDoubleVector3D(0.001, y1, 0.0));
                    pair.second.append(QDoubleVector3D(0, y1, 0.0));
                } else if (pair.second.size() == 1) {
                    // FIXME this is trickier
                    // - touching at one point on the tile boundary
                    // - probably need to build a triangular polygon across the edge
                    // - don't want to add another y tile if we can help it
                    //   - initial version doesn't care
                    double y = pair.second.at(0).y();
                    pair.second.clear();
                    pair.second.append(QDoubleVector3D(0.001, y, 0.0));
                    pair.second.append(QDoubleVector3D(0.0, y - 0.001, 0.0));
                    pair.second.append(QDoubleVector3D(0.0, y + 0.001, 0.0));
                }
            } else {
                for (int i = 0; i < pair.second.size(); ++i) {
                    pair.second[i].setX(pair.second.at(i).x() - side);
                }
            }
            return pair;
        } else {
            return QPair<Polygon, Polygon>(results, Polygon());
        }
    }

}

QList<QPair<double, int> > QGeoCameraTilesPrivate::tileIntersections(double p1, int t1, double p2, int t2) const
{
    if (t1 == t2) {
        QList<QPair<double, int> > results = QList<QPair<double, int> >();
        results.append(QPair<double, int>(0.0, t1));
        return results;
    }

    int step = 1;
    if (t1 > t2) {
        step = -1;
    }

    int size = 1 + ((t2 - t1) / step);

    QList<QPair<double, int> > results = QList<QPair<double, int> >();

    results.append(QPair<double, int>(0.0, t1));

    if (step == 1) {
        for (int i = 1; i < size; ++i) {
            double f = (t1 + i - p1) / (p2 - p1);
            results.append(QPair<double, int>(f, t1 + i));
        }
    } else {
        for (int i = 1; i < size; ++i) {
            double f = (t1 - i + 1 - p1) / (p2 - p1);
            results.append(QPair<double, int>(f, t1 - i));
        }
    }

    return results;
}

QSet<QGeoTileSpec> QGeoCameraTilesPrivate::tilesFromPolygon(const Polygon &polygon) const
{
    int numPoints = polygon.size();

    if (numPoints == 0)
        return QSet<QGeoTileSpec>();

    QVector<int> tilesX(polygon.size());
    QVector<int> tilesY(polygon.size());

    // grab tiles at the corners of the polygon
    for (int i = 0; i < numPoints; ++i) {

        QDoubleVector2D p = polygon.at(i).toVector2D();

        int x = 0;
        int y = 0;

        if (qFuzzyCompare(p.x(), sideLength_ * 1.0))
            x = sideLength_ - 1;
        else {
            x = static_cast<int>(p.x()) % sideLength_;
            if ( !qFuzzyCompare(p.x(), 1.0 * x) && qFuzzyCompare(p.x(), 1.0 * (x + 1)) )
                x++;
        }

        if (qFuzzyCompare(p.y(), sideLength_ * 1.0))
            y = sideLength_ - 1;
        else {
            y = static_cast<int>(p.y()) % sideLength_;
            if ( !qFuzzyCompare(p.y(), 1.0 * y) && qFuzzyCompare(p.y(), 1.0 * (y + 1)) )
                y++;
        }

        tilesX[i] = x;
        tilesY[i] = y;
    }

    QGeoCameraTilesPrivate::TileMap map;

    // walk along the edges of the polygon and add all tiles covered by them
    for (int i1 = 0; i1 < numPoints; ++i1) {
        int i2 = (i1 + 1) % numPoints;

        double x1 = polygon.at(i1).get(0);
        double x2 = polygon.at(i2).get(0);

        bool xFixed = qFuzzyCompare(x1, x2);
        bool xIntegral = qFuzzyCompare(x1, std::floor(x1)) || qFuzzyCompare(x1 + 1.0, std::floor(x1 + 1.0));

        QList<QPair<double, int> > xIntersects
                = tileIntersections(x1,
                                    tilesX.at(i1),
                                    x2,
                                    tilesX.at(i2));

        double y1 = polygon.at(i1).get(1);
        double y2 = polygon.at(i2).get(1);

        bool yFixed = qFuzzyCompare(y1, y2);
        bool yIntegral = qFuzzyCompare(y1, std::floor(y1)) || qFuzzyCompare(y1 + 1.0, std::floor(y1 + 1.0));

        QList<QPair<double, int> > yIntersects
                = tileIntersections(y1,
                                    tilesY.at(i1),
                                    y2,
                                    tilesY.at(i2));

        int x = xIntersects.takeFirst().second;
        int y = yIntersects.takeFirst().second;


        /*
          If the polygon coincides with the tile edges we must be
          inclusive and grab all tiles on both sides. We also need
          to handle tiles with corners coindent with the
          corners of the polygon.
          e.g. all tiles marked with 'x' will be added

              "+" - tile boundaries
              "O" - polygon boundary

                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +   x   +   x   +   x   +       +
                +       +       +       +       +       +
                + + + + + + + + O O O O O + + + + + + + +
                +       +       O       0       +       +
                +       +   x   O   x   0   x   +       +
                +       +       O       0       +       +
                + + + + + + + + O 0 0 0 0 + + + + + + + +
                +       +       +       +       +       +
                +       +   x   +   x   +   x   +       +
                +       +       +       +       +       +
                + + + + + + + + + + + + + + + + + + + + +
        */


        int xOther = x;
        int yOther = y;

        if (xFixed && xIntegral) {
             if (y2 < y1) {
                 xOther = qMax(0, x - 1);
            }
        }

        if (yFixed && yIntegral) {
            if (x1 < x2) {
                yOther = qMax(0, y - 1);

            }
        }

        if (xIntegral) {
            map.add(xOther, y);
            if (yIntegral)
                map.add(xOther, yOther);

        }

        if (yIntegral)
            map.add(x, yOther);

        map.add(x,y);

        // top left corner
        int iPrev =  (i1 + numPoints - 1) % numPoints;
        double xPrevious = polygon.at(iPrev).get(0);
        double yPrevious = polygon.at(iPrev).get(1);
        bool xPreviousFixed = qFuzzyCompare(xPrevious, x1);
        if (xIntegral && xPreviousFixed && yIntegral && yFixed) {
            if ((x2 > x1) && (yPrevious > y1)) {
                if ((x - 1) > 0 && (y - 1) > 0)
                    map.add(x - 1, y - 1);
            } else if ((x2 < x1) && (yPrevious < y1)) {
                // what?
            }
        }

        // for the simple case where intersections do not coincide with
        // the boundaries, we move along the edge and add tiles until
        // the x and y intersection lists are exhausted

        while (!xIntersects.isEmpty() && !yIntersects.isEmpty()) {
            QPair<double, int> nextX = xIntersects.first();
            QPair<double, int> nextY = yIntersects.first();
            if (nextX.first < nextY.first) {
                x = nextX.second;
                map.add(x, y);
                xIntersects.removeFirst();

            } else if (nextX.first > nextY.first) {
                y = nextY.second;
                map.add(x, y);
                yIntersects.removeFirst();

            } else {
                map.add(x, nextY.second);
                map.add(nextX.second, y);
                x = nextX.second;
                y = nextY.second;
                map.add(x, y);
                xIntersects.removeFirst();
                yIntersects.removeFirst();
            }
        }

        while (!xIntersects.isEmpty()) {
            x = xIntersects.takeFirst().second;
            map.add(x, y);
            if (yIntegral && yFixed)
                map.add(x, yOther);

        }

        while (!yIntersects.isEmpty()) {
            y = yIntersects.takeFirst().second;
            map.add(x, y);
            if (xIntegral && xFixed)
                map.add(xOther, y);
        }
    }

    QSet<QGeoTileSpec> results;

    int z = intZoomLevel_;

    typedef QMap<int, QPair<int, int> >::const_iterator iter;
    iter i = map.data.constBegin();
    iter end = map.data.constEnd();

    for (; i != end; ++i) {
        int y = i.key();
        int minX = i->first;
        int maxX = i->second;
        for (int x = minX; x <= maxX; ++x) {
            results.insert(QGeoTileSpec(pluginString_, mapType_.mapId(), z, x, y, mapVersion_));
        }
    }

    return results;
}

QGeoCameraTilesPrivate::TileMap::TileMap() {}

void QGeoCameraTilesPrivate::TileMap::add(int tileX, int tileY)
{
    if (data.contains(tileY)) {
        int oldMinX = data.value(tileY).first;
        int oldMaxX = data.value(tileY).second;
        data.insert(tileY, QPair<int, int>(qMin(tileX, oldMinX), qMax(tileX, oldMaxX)));
    } else {
        data.insert(tileY, QPair<int, int>(tileX, tileX));
    }
}

QT_END_NAMESPACE
