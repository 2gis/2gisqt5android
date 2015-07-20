/*
 * Copyright 2008 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkStrokerPriv.h"
#include "SkGeometry.h"
#include "SkPath.h"

#if QUAD_STROKE_APPROXIMATION

    enum {
        kTangent_RecursiveLimit,
        kCubic_RecursiveLimit,
        kQuad_RecursiveLimit
    };

    // quads with extreme widths (e.g. (0,1) (1,6) (0,3) width=5e7) recurse to point of failure
    // largest seen for normal cubics : 5, 26
    // largest seen for normal quads : 11
    static const int kRecursiveLimits[] = { 5*3, 26*3, 11*3 };  // 3x limits seen in practical tests

    SK_COMPILE_ASSERT(SK_ARRAY_COUNT(kRecursiveLimits) == kQuad_RecursiveLimit + 1,
            recursive_limits_mismatch);

    #ifdef SK_DEBUG  // enables tweaking these values at runtime from SampleApp
        bool gDebugStrokerErrorSet = false;
        SkScalar gDebugStrokerError;

        int gMaxRecursion[SK_ARRAY_COUNT(kRecursiveLimits)] = { 0 };
    #endif
    #ifndef DEBUG_QUAD_STROKER
        #define DEBUG_QUAD_STROKER 0
    #endif

    #if DEBUG_QUAD_STROKER
        /* Enable to show the decisions made in subdividing the curve -- helpful when the resulting
           stroke has more than the optimal number of quadratics and lines */
        #define STROKER_RESULT(resultType, depth, quadPts, format, ...) \
                SkDebugf("[%d] %s " format "\n", depth, __FUNCTION__, __VA_ARGS__), \
                SkDebugf("  " #resultType " t=(%g,%g)\n", quadPts->fStartT, quadPts->fEndT), \
                resultType
        #define STROKER_DEBUG_PARAMS(...) , __VA_ARGS__
    #else
        #define STROKER_RESULT(resultType, depth, quadPts, format, ...) \
                resultType
        #define STROKER_DEBUG_PARAMS(...)
    #endif

#endif

#define kMaxQuadSubdivide   5
#define kMaxCubicSubdivide  7

static inline bool degenerate_vector(const SkVector& v) {
    return !SkPoint::CanNormalize(v.fX, v.fY);
}

#if !QUAD_STROKE_APPROXIMATION
static inline bool normals_too_curvy(const SkVector& norm0, SkVector& norm1) {
    /*  root2/2 is a 45-degree angle
        make this constant bigger for more subdivisions (but not >= 1)
    */
    static const SkScalar kFlatEnoughNormalDotProd =
                                            SK_ScalarSqrt2/2 + SK_Scalar1/10;

    SkASSERT(kFlatEnoughNormalDotProd > 0 &&
             kFlatEnoughNormalDotProd < SK_Scalar1);

    return SkPoint::DotProduct(norm0, norm1) <= kFlatEnoughNormalDotProd;
}

static inline bool normals_too_pinchy(const SkVector& norm0, SkVector& norm1) {
    // if the dot-product is -1, then we are definitely too pinchy. We tweak
    // that by an epsilon to ensure we have significant bits in our test
    static const int kMinSigBitsForDot = 8;
    static const SkScalar kDotEpsilon = FLT_EPSILON * (1 << kMinSigBitsForDot);
    static const SkScalar kTooPinchyNormalDotProd = kDotEpsilon - 1;

    // just some sanity asserts to help document the expected range
    SkASSERT(kTooPinchyNormalDotProd >= -1);
    SkASSERT(kTooPinchyNormalDotProd < SkDoubleToScalar(-0.999));

    SkScalar dot = SkPoint::DotProduct(norm0, norm1);
    return dot <= kTooPinchyNormalDotProd;
}
#endif

static bool set_normal_unitnormal(const SkPoint& before, const SkPoint& after,
                                  SkScalar radius,
                                  SkVector* normal, SkVector* unitNormal) {
    if (!unitNormal->setNormalize(after.fX - before.fX, after.fY - before.fY)) {
        return false;
    }
    unitNormal->rotateCCW();
    unitNormal->scale(radius, normal);
    return true;
}

static bool set_normal_unitnormal(const SkVector& vec,
                                  SkScalar radius,
                                  SkVector* normal, SkVector* unitNormal) {
    if (!unitNormal->setNormalize(vec.fX, vec.fY)) {
        return false;
    }
    unitNormal->rotateCCW();
    unitNormal->scale(radius, normal);
    return true;
}

///////////////////////////////////////////////////////////////////////////////
#if QUAD_STROKE_APPROXIMATION

struct SkQuadConstruct {    // The state of the quad stroke under construction.
    SkPoint fQuad[3];       // the stroked quad parallel to the original curve
    SkPoint fTangentStart;  // a point tangent to fQuad[0]
    SkPoint fTangentEnd;    // a point tangent to fQuad[2]
    SkScalar fStartT;       // a segment of the original curve
    SkScalar fMidT;         //              "
    SkScalar fEndT;         //              "
    bool fStartSet;         // state to share common points across structs
    bool fEndSet;           //                     "

    // return false if start and end are too close to have a unique middle
    bool init(SkScalar start, SkScalar end) {
        fStartT = start;
        fMidT = (start + end) * SK_ScalarHalf;
        fEndT = end;
        fStartSet = fEndSet = false;
        return fStartT < fMidT && fMidT < fEndT;
    }

    bool initWithStart(SkQuadConstruct* parent) {
        if (!init(parent->fStartT, parent->fMidT)) {
            return false;
        }
        fQuad[0] = parent->fQuad[0];
        fTangentStart = parent->fTangentStart;
        fStartSet = true;
        return true;
    }

    bool initWithEnd(SkQuadConstruct* parent) {
        if (!init(parent->fMidT, parent->fEndT)) {
            return false;
        }
        fQuad[2] = parent->fQuad[2];
        fTangentEnd = parent->fTangentEnd;
        fEndSet = true;
        return true;
   }
};
#endif

class SkPathStroker {
public:
#if QUAD_STROKE_APPROXIMATION
    SkPathStroker(const SkPath& src,
                  SkScalar radius, SkScalar miterLimit, SkScalar error, SkPaint::Cap cap,
                  SkPaint::Join join);
#else
    SkPathStroker(const SkPath& src,
                  SkScalar radius, SkScalar miterLimit, SkPaint::Cap cap,
                  SkPaint::Join join);
#endif

    void moveTo(const SkPoint&);
    void lineTo(const SkPoint&);
    void quadTo(const SkPoint&, const SkPoint&);
    void cubicTo(const SkPoint&, const SkPoint&, const SkPoint&);
    void close(bool isLine) { this->finishContour(true, isLine); }

    void done(SkPath* dst, bool isLine) {
        this->finishContour(false, isLine);
        fOuter.addPath(fExtra);
        dst->swap(fOuter);
    }

private:
#if QUAD_STROKE_APPROXIMATION
    SkScalar    fError;
#endif
    SkScalar    fRadius;
    SkScalar    fInvMiterLimit;

    SkVector    fFirstNormal, fPrevNormal, fFirstUnitNormal, fPrevUnitNormal;
    SkPoint     fFirstPt, fPrevPt;  // on original path
    SkPoint     fFirstOuterPt;
    int         fSegmentCount;
    bool        fPrevIsLine;

    SkStrokerPriv::CapProc  fCapper;
    SkStrokerPriv::JoinProc fJoiner;

    SkPath  fInner, fOuter; // outer is our working answer, inner is temp
    SkPath  fExtra;         // added as extra complete contours

#if QUAD_STROKE_APPROXIMATION
    enum StrokeType {
        kOuter_StrokeType = 1,      // use sign-opposite values later to flip perpendicular axis
        kInner_StrokeType = -1
    } fStrokeType;

    enum ResultType {
        kSplit_ResultType,          // the caller should split the quad stroke in two
        kDegenerate_ResultType,     // the caller should add a line
        kQuad_ResultType,           // the caller should (continue to try to) add a quad stroke
        kNormalError_ResultType,    // the cubic's normal couldn't be computed -- abort
    };

    enum ReductionType {
        kPoint_ReductionType,       // all curve points are practically identical
        kLine_ReductionType,        // the control point is on the line between the ends
        kQuad_ReductionType,        // the control point is outside the line between the ends
        kDegenerate_ReductionType,  // the control point is on the line but outside the ends
        kDegenerate2_ReductionType, // two control points are on the line but outside ends (cubic)
        kDegenerate3_ReductionType, // three areas of max curvature found (for cubic)
    };

    enum IntersectRayType {
        kCtrlPt_RayType,
        kResultType_RayType,
    };

    int fRecursionDepth;            // track stack depth to abort if numerics run amok
    bool fFoundTangents;            // do less work until tangents meet (cubic)

    void addDegenerateLine(const SkQuadConstruct* );
    ReductionType CheckCubicLinear(const SkPoint cubic[4], SkPoint reduction[3],
                                   const SkPoint** tanPtPtr);
    ReductionType CheckQuadLinear(const SkPoint quad[3], SkPoint* reduction);
    ResultType compareQuadCubic(const SkPoint cubic[4], SkQuadConstruct* );
    ResultType compareQuadQuad(const SkPoint quad[3], SkQuadConstruct* );
    bool cubicMidOnLine(const SkPoint cubic[4], const SkQuadConstruct* ) const;
    bool cubicPerpRay(const SkPoint cubic[4], SkScalar t, SkPoint* tPt, SkPoint* onPt,
                      SkPoint* tangent) const;
    bool cubicQuadEnds(const SkPoint cubic[4], SkQuadConstruct* );
    bool cubicQuadMid(const SkPoint cubic[4], const SkQuadConstruct* , SkPoint* mid) const;
    bool cubicStroke(const SkPoint cubic[4], SkQuadConstruct* );
    void init(StrokeType strokeType, SkQuadConstruct* , SkScalar tStart, SkScalar tEnd);
    ResultType intersectRay(SkQuadConstruct* , IntersectRayType  STROKER_DEBUG_PARAMS(int) ) const;
    bool ptInQuadBounds(const SkPoint quad[3], const SkPoint& pt) const;
    void quadPerpRay(const SkPoint quad[3], SkScalar t, SkPoint* tPt, SkPoint* onPt,
                     SkPoint* tangent) const;
    bool quadStroke(const SkPoint quad[3], SkQuadConstruct* );
    void setCubicEndNormal(const SkPoint cubic[4],
                           const SkVector& normalAB, const SkVector& unitNormalAB,
                           SkVector* normalCD, SkVector* unitNormalCD);
    void setQuadEndNormal(const SkPoint quad[3],
                          const SkVector& normalAB, const SkVector& unitNormalAB,
                          SkVector* normalBC, SkVector* unitNormalBC);
    void setRayPts(const SkPoint& tPt, SkVector* dxy, SkPoint* onPt, SkPoint* tangent) const;
    static bool SlightAngle(SkQuadConstruct* );
    ResultType strokeCloseEnough(const SkPoint stroke[3], const SkPoint ray[2],
                                 SkQuadConstruct*  STROKER_DEBUG_PARAMS(int depth) ) const;
    ResultType tangentsMeet(const SkPoint cubic[4], SkQuadConstruct* );
#endif

    void    finishContour(bool close, bool isLine);
    void    preJoinTo(const SkPoint&, SkVector* normal, SkVector* unitNormal,
                      bool isLine);
    void    postJoinTo(const SkPoint&, const SkVector& normal,
                       const SkVector& unitNormal);

    void    line_to(const SkPoint& currPt, const SkVector& normal);
#if !QUAD_STROKE_APPROXIMATION
    void    quad_to(const SkPoint pts[3],
                    const SkVector& normalAB, const SkVector& unitNormalAB,
                    SkVector* normalBC, SkVector* unitNormalBC,
                    int subDivide);
    void    cubic_to(const SkPoint pts[4],
                    const SkVector& normalAB, const SkVector& unitNormalAB,
                    SkVector* normalCD, SkVector* unitNormalCD,
                    int subDivide);
#endif
};

///////////////////////////////////////////////////////////////////////////////

void SkPathStroker::preJoinTo(const SkPoint& currPt, SkVector* normal,
                              SkVector* unitNormal, bool currIsLine) {
    SkASSERT(fSegmentCount >= 0);

    SkScalar    prevX = fPrevPt.fX;
    SkScalar    prevY = fPrevPt.fY;

    SkAssertResult(set_normal_unitnormal(fPrevPt, currPt, fRadius, normal,
                                         unitNormal));

    if (fSegmentCount == 0) {
        fFirstNormal = *normal;
        fFirstUnitNormal = *unitNormal;
        fFirstOuterPt.set(prevX + normal->fX, prevY + normal->fY);

        fOuter.moveTo(fFirstOuterPt.fX, fFirstOuterPt.fY);
        fInner.moveTo(prevX - normal->fX, prevY - normal->fY);
    } else {    // we have a previous segment
        fJoiner(&fOuter, &fInner, fPrevUnitNormal, fPrevPt, *unitNormal,
                fRadius, fInvMiterLimit, fPrevIsLine, currIsLine);
    }
    fPrevIsLine = currIsLine;
}

void SkPathStroker::postJoinTo(const SkPoint& currPt, const SkVector& normal,
                               const SkVector& unitNormal) {
    fPrevPt = currPt;
    fPrevUnitNormal = unitNormal;
    fPrevNormal = normal;
    fSegmentCount += 1;
}

void SkPathStroker::finishContour(bool close, bool currIsLine) {
    if (fSegmentCount > 0) {
        SkPoint pt;

        if (close) {
            fJoiner(&fOuter, &fInner, fPrevUnitNormal, fPrevPt,
                    fFirstUnitNormal, fRadius, fInvMiterLimit,
                    fPrevIsLine, currIsLine);
            fOuter.close();
            // now add fInner as its own contour
            fInner.getLastPt(&pt);
            fOuter.moveTo(pt.fX, pt.fY);
            fOuter.reversePathTo(fInner);
            fOuter.close();
        } else {    // add caps to start and end
            // cap the end
            fInner.getLastPt(&pt);
            fCapper(&fOuter, fPrevPt, fPrevNormal, pt,
                    currIsLine ? &fInner : NULL);
            fOuter.reversePathTo(fInner);
            // cap the start
            fCapper(&fOuter, fFirstPt, -fFirstNormal, fFirstOuterPt,
                    fPrevIsLine ? &fInner : NULL);
            fOuter.close();
        }
    }
    // since we may re-use fInner, we rewind instead of reset, to save on
    // reallocating its internal storage.
    fInner.rewind();
    fSegmentCount = -1;
}

///////////////////////////////////////////////////////////////////////////////

#if QUAD_STROKE_APPROXIMATION
SkPathStroker::SkPathStroker(const SkPath& src,
                             SkScalar radius, SkScalar miterLimit, SkScalar error,
                             SkPaint::Cap cap, SkPaint::Join join)
#else
SkPathStroker::SkPathStroker(const SkPath& src,
                             SkScalar radius, SkScalar miterLimit,
                             SkPaint::Cap cap, SkPaint::Join join)
#endif
        : fRadius(radius) {

    /*  This is only used when join is miter_join, but we initialize it here
        so that it is always defined, to fis valgrind warnings.
    */
    fInvMiterLimit = 0;

    if (join == SkPaint::kMiter_Join) {
        if (miterLimit <= SK_Scalar1) {
            join = SkPaint::kBevel_Join;
        } else {
            fInvMiterLimit = SkScalarInvert(miterLimit);
        }
    }
    fCapper = SkStrokerPriv::CapFactory(cap);
    fJoiner = SkStrokerPriv::JoinFactory(join);
    fSegmentCount = -1;
    fPrevIsLine = false;

    // Need some estimate of how large our final result (fOuter)
    // and our per-contour temp (fInner) will be, so we don't spend
    // extra time repeatedly growing these arrays.
    //
    // 3x for result == inner + outer + join (swag)
    // 1x for inner == 'wag' (worst contour length would be better guess)
    fOuter.incReserve(src.countPoints() * 3);
    fOuter.setIsVolatile(true);
    fInner.incReserve(src.countPoints());
    fInner.setIsVolatile(true);
#if QUAD_STROKE_APPROXIMATION
#ifdef SK_DEBUG
    if (!gDebugStrokerErrorSet) {
        gDebugStrokerError = error;
    }
    fError = gDebugStrokerError;
#else
    fError = error;
#endif
    fRecursionDepth = 0;
#endif
}

void SkPathStroker::moveTo(const SkPoint& pt) {
    if (fSegmentCount > 0) {
        this->finishContour(false, false);
    }
    fSegmentCount = 0;
    fFirstPt = fPrevPt = pt;
}

void SkPathStroker::line_to(const SkPoint& currPt, const SkVector& normal) {
    fOuter.lineTo(currPt.fX + normal.fX, currPt.fY + normal.fY);
    fInner.lineTo(currPt.fX - normal.fX, currPt.fY - normal.fY);
}

void SkPathStroker::lineTo(const SkPoint& currPt) {
    if (SkPath::IsLineDegenerate(fPrevPt, currPt)) {
        return;
    }
    SkVector    normal, unitNormal;

    this->preJoinTo(currPt, &normal, &unitNormal, true);
    this->line_to(currPt, normal);
    this->postJoinTo(currPt, normal, unitNormal);
}

#if !QUAD_STROKE_APPROXIMATION
void SkPathStroker::quad_to(const SkPoint pts[3],
                      const SkVector& normalAB, const SkVector& unitNormalAB,
                      SkVector* normalBC, SkVector* unitNormalBC,
                      int subDivide) {
    if (!set_normal_unitnormal(pts[1], pts[2], fRadius,
                               normalBC, unitNormalBC)) {
        // pts[1] nearly equals pts[2], so just draw a line to pts[2]
        this->line_to(pts[2], normalAB);
        *normalBC = normalAB;
        *unitNormalBC = unitNormalAB;
        return;
    }

    if (--subDivide >= 0 && normals_too_curvy(unitNormalAB, *unitNormalBC)) {
        SkPoint     tmp[5];
        SkVector    norm, unit;

        SkChopQuadAtHalf(pts, tmp);
        this->quad_to(&tmp[0], normalAB, unitNormalAB, &norm, &unit, subDivide);
        this->quad_to(&tmp[2], norm, unit, normalBC, unitNormalBC, subDivide);
    } else {
        SkVector    normalB;

        normalB = pts[2] - pts[0];
        normalB.rotateCCW();
        SkScalar dot = SkPoint::DotProduct(unitNormalAB, *unitNormalBC);
        SkAssertResult(normalB.setLength(SkScalarDiv(fRadius,
                                     SkScalarSqrt((SK_Scalar1 + dot)/2))));

        fOuter.quadTo(  pts[1].fX + normalB.fX, pts[1].fY + normalB.fY,
                        pts[2].fX + normalBC->fX, pts[2].fY + normalBC->fY);
        fInner.quadTo(  pts[1].fX - normalB.fX, pts[1].fY - normalB.fY,
                        pts[2].fX - normalBC->fX, pts[2].fY - normalBC->fY);
    }
}
#endif

#if QUAD_STROKE_APPROXIMATION
void SkPathStroker::setQuadEndNormal(const SkPoint quad[3], const SkVector& normalAB,
        const SkVector& unitNormalAB, SkVector* normalBC, SkVector* unitNormalBC) {
    if (!set_normal_unitnormal(quad[1], quad[2], fRadius, normalBC, unitNormalBC)) {
        *normalBC = normalAB;
        *unitNormalBC = unitNormalAB;
    }
}

void SkPathStroker::setCubicEndNormal(const SkPoint cubic[4], const SkVector& normalAB,
        const SkVector& unitNormalAB, SkVector* normalCD, SkVector* unitNormalCD) {
    SkVector    ab = cubic[1] - cubic[0];
    SkVector    cd = cubic[3] - cubic[2];

    bool    degenerateAB = degenerate_vector(ab);
    bool    degenerateCD = degenerate_vector(cd);

    if (degenerateAB && degenerateCD) {
        goto DEGENERATE_NORMAL;
    }

    if (degenerateAB) {
        ab = cubic[2] - cubic[0];
        degenerateAB = degenerate_vector(ab);
    }
    if (degenerateCD) {
        cd = cubic[3] - cubic[1];
        degenerateCD = degenerate_vector(cd);
    }
    if (degenerateAB || degenerateCD) {
DEGENERATE_NORMAL:
        *normalCD = normalAB;
        *unitNormalCD = unitNormalAB;
        return;
    }
    SkAssertResult(set_normal_unitnormal(cd, fRadius, normalCD, unitNormalCD));
}

void SkPathStroker::init(StrokeType strokeType, SkQuadConstruct* quadPts, SkScalar tStart,
        SkScalar tEnd) {
    fStrokeType = strokeType;
    fFoundTangents = false;
    quadPts->init(tStart, tEnd);
}

// returns the distance squared from the point to the line
static SkScalar pt_to_line(const SkPoint& pt, const SkPoint& lineStart, const SkPoint& lineEnd) {
    SkVector dxy = lineEnd - lineStart;
    if (degenerate_vector(dxy)) {
        return pt.distanceToSqd(lineStart);
    }
    SkVector ab0 = pt - lineStart;
    SkScalar numer = dxy.dot(ab0);
    SkScalar denom = dxy.dot(dxy);
    SkScalar t = numer / denom;
    SkPoint hit;
    hit.fX = lineStart.fX * (1 - t) + lineEnd.fX * t;
    hit.fY = lineStart.fY * (1 - t) + lineEnd.fY * t;
    return hit.distanceToSqd(pt);
}

/*  Given a cubic, determine if all four points are in a line.
    Return true if the inner points is close to a line connecting the outermost points.

    Find the outermost point by looking for the largest difference in X or Y.
    Given the indices of the outermost points, and that outer_1 is greater than outer_2,
    this table shows the index of the smaller of the remaining points:

                      outer_2
                  0    1    2    3
      outer_1     ----------------
         0     |  -    2    1    1
         1     |  -    -    0    0
         2     |  -    -    -    0
         3     |  -    -    -    -

    If outer_1 == 0 and outer_2 == 1, the smaller of the remaining indices (2 and 3) is 2.

    This table can be collapsed to: (1 + (2 >> outer_2)) >> outer_1

    Given three indices (outer_1 outer_2 mid_1) from 0..3, the remaining index is:

               mid_2 == (outer_1 ^ outer_2 ^ mid_1)
 */
static bool cubic_in_line(const SkPoint cubic[4]) {
    SkScalar ptMax = -1;
    int outer1, outer2;
    for (int index = 0; index < 3; ++index) {
        for (int inner = index + 1; inner < 4; ++inner) {
            SkVector testDiff = cubic[inner] - cubic[index];
            SkScalar testMax = SkTMax(SkScalarAbs(testDiff.fX), SkScalarAbs(testDiff.fY));
            if (ptMax < testMax) {
                outer1 = index;
                outer2 = inner;
                ptMax = testMax;
            }
        }
    }
    SkASSERT(outer1 >= 0 && outer1 <= 2);
    SkASSERT(outer2 >= 1 && outer2 <= 3);
    SkASSERT(outer1 < outer2);
    int mid1 = (1 + (2 >> outer2)) >> outer1;
    SkASSERT(mid1 >= 0 && mid1 <= 2);
    SkASSERT(outer1 != mid1 && outer2 != mid1);
    int mid2 = outer1 ^ outer2 ^ mid1;
    SkASSERT(mid2 >= 1 && mid2 <= 3);
    SkASSERT(mid2 != outer1 && mid2 != outer2 && mid2 != mid1);
    SkASSERT(((1 << outer1) | (1 << outer2) | (1 << mid1) | (1 << mid2)) == 0x0f);
    SkScalar lineSlop = ptMax * ptMax * 0.00001f;  // this multiplier is pulled out of the air
    return pt_to_line(cubic[mid1], cubic[outer1], cubic[outer2]) <= lineSlop
            && pt_to_line(cubic[mid2], cubic[outer1], cubic[outer2]) <= lineSlop;
}

/* Given quad, see if all there points are in a line.
   Return true if the inside point is close to a line connecting the outermost points.

   Find the outermost point by looking for the largest difference in X or Y.
   Since the XOR of the indices is 3  (0 ^ 1 ^ 2)
   the missing index equals: outer_1 ^ outer_2 ^ 3
 */
static bool quad_in_line(const SkPoint quad[3]) {
    SkScalar ptMax = -1;
    int outer1, outer2;
    for (int index = 0; index < 2; ++index) {
        for (int inner = index + 1; inner < 3; ++inner) {
            SkVector testDiff = quad[inner] - quad[index];
            SkScalar testMax = SkTMax(SkScalarAbs(testDiff.fX), SkScalarAbs(testDiff.fY));
            if (ptMax < testMax) {
                outer1 = index;
                outer2 = inner;
                ptMax = testMax;
            }
        }
    }
    SkASSERT(outer1 >= 0 && outer1 <= 1);
    SkASSERT(outer2 >= 1 && outer2 <= 2);
    SkASSERT(outer1 < outer2);
    int mid = outer1 ^ outer2 ^ 3;
    SkScalar lineSlop =  ptMax * ptMax * 0.00001f;  // this multiplier is pulled out of the air
    return pt_to_line(quad[mid], quad[outer1], quad[outer2]) <= lineSlop;
}

SkPathStroker::ReductionType SkPathStroker::CheckCubicLinear(const SkPoint cubic[4],
        SkPoint reduction[3], const SkPoint** tangentPtPtr) {
    bool degenerateAB = degenerate_vector(cubic[1] - cubic[0]);
    bool degenerateBC = degenerate_vector(cubic[2] - cubic[1]);
    bool degenerateCD = degenerate_vector(cubic[3] - cubic[2]);
    if (degenerateAB & degenerateBC & degenerateCD) {
        return kPoint_ReductionType;
    }
    if (degenerateAB + degenerateBC + degenerateCD == 2) {
        return kLine_ReductionType;
    }
    if (!cubic_in_line(cubic)) {
        *tangentPtPtr = degenerateAB ? &cubic[2] : &cubic[1];
        return kQuad_ReductionType;
    }
    SkScalar tValues[3];
    int count = SkFindCubicMaxCurvature(cubic, tValues);
    if (count == 0) {
        return kLine_ReductionType;
    }
    for (int index = 0; index < count; ++index) {
        SkScalar t = tValues[index];
        SkEvalCubicAt(cubic, t, &reduction[index], NULL, NULL);
    }
    SK_COMPILE_ASSERT(kQuad_ReductionType + 1 == kDegenerate_ReductionType, enum_out_of_whack);
    SK_COMPILE_ASSERT(kQuad_ReductionType + 2 == kDegenerate2_ReductionType, enum_out_of_whack);
    SK_COMPILE_ASSERT(kQuad_ReductionType + 3 == kDegenerate3_ReductionType, enum_out_of_whack);

    return (ReductionType) (kQuad_ReductionType + count);
}

SkPathStroker::ReductionType SkPathStroker::CheckQuadLinear(const SkPoint quad[3],
        SkPoint* reduction) {
    bool degenerateAB = degenerate_vector(quad[1] - quad[0]);
    bool degenerateBC = degenerate_vector(quad[2] - quad[1]);
    if (degenerateAB & degenerateBC) {
        return kPoint_ReductionType;
    }
    if (degenerateAB | degenerateBC) {
        return kLine_ReductionType;
    }
    if (!quad_in_line(quad)) {
        return kQuad_ReductionType;
    }
    SkScalar t = SkFindQuadMaxCurvature(quad);
    if (0 == t) {
        return kLine_ReductionType;
    }
    SkEvalQuadAt(quad, t, reduction, NULL);
    return kDegenerate_ReductionType;
}

#else

void SkPathStroker::cubic_to(const SkPoint pts[4],
                      const SkVector& normalAB, const SkVector& unitNormalAB,
                      SkVector* normalCD, SkVector* unitNormalCD,
                      int subDivide) {
    SkVector    ab = pts[1] - pts[0];
    SkVector    cd = pts[3] - pts[2];
    SkVector    normalBC, unitNormalBC;

    bool    degenerateAB = degenerate_vector(ab);
    bool    degenerateCD = degenerate_vector(cd);

    if (degenerateAB && degenerateCD) {
DRAW_LINE:
        this->line_to(pts[3], normalAB);
        *normalCD = normalAB;
        *unitNormalCD = unitNormalAB;
        return;
    }

    if (degenerateAB) {
        ab = pts[2] - pts[0];
        degenerateAB = degenerate_vector(ab);
    }
    if (degenerateCD) {
        cd = pts[3] - pts[1];
        degenerateCD = degenerate_vector(cd);
    }
    if (degenerateAB || degenerateCD) {
        goto DRAW_LINE;
    }
    SkAssertResult(set_normal_unitnormal(cd, fRadius, normalCD, unitNormalCD));
    bool degenerateBC = !set_normal_unitnormal(pts[1], pts[2], fRadius,
                                               &normalBC, &unitNormalBC);
#ifndef SK_IGNORE_CUBIC_STROKE_FIX
    if (--subDivide < 0) {
        goto DRAW_LINE;
    }
#endif
    if (degenerateBC || normals_too_curvy(unitNormalAB, unitNormalBC) ||
             normals_too_curvy(unitNormalBC, *unitNormalCD)) {
#ifdef SK_IGNORE_CUBIC_STROKE_FIX
        // subdivide if we can
        if (--subDivide < 0) {
            goto DRAW_LINE;
        }
#endif
        SkPoint     tmp[7];
        SkVector    norm, unit, dummy, unitDummy;

        SkChopCubicAtHalf(pts, tmp);
        this->cubic_to(&tmp[0], normalAB, unitNormalAB, &norm, &unit,
                       subDivide);
        // we use dummys since we already have a valid (and more accurate)
        // normals for CD
        this->cubic_to(&tmp[3], norm, unit, &dummy, &unitDummy, subDivide);
    } else {
        SkVector    normalB, normalC;

        // need normals to inset/outset the off-curve pts B and C

        SkVector    unitBC = pts[2] - pts[1];
        unitBC.normalize();
        unitBC.rotateCCW();

        normalB = unitNormalAB + unitBC;
        normalC = *unitNormalCD + unitBC;

        SkScalar dot = SkPoint::DotProduct(unitNormalAB, unitBC);
        SkAssertResult(normalB.setLength(SkScalarDiv(fRadius,
                                    SkScalarSqrt((SK_Scalar1 + dot)/2))));
        dot = SkPoint::DotProduct(*unitNormalCD, unitBC);
        SkAssertResult(normalC.setLength(SkScalarDiv(fRadius,
                                    SkScalarSqrt((SK_Scalar1 + dot)/2))));

        fOuter.cubicTo( pts[1].fX + normalB.fX, pts[1].fY + normalB.fY,
                        pts[2].fX + normalC.fX, pts[2].fY + normalC.fY,
                        pts[3].fX + normalCD->fX, pts[3].fY + normalCD->fY);

        fInner.cubicTo( pts[1].fX - normalB.fX, pts[1].fY - normalB.fY,
                        pts[2].fX - normalC.fX, pts[2].fY - normalC.fY,
                        pts[3].fX - normalCD->fX, pts[3].fY - normalCD->fY);
    }
}
#endif

void SkPathStroker::quadTo(const SkPoint& pt1, const SkPoint& pt2) {
#if QUAD_STROKE_APPROXIMATION
    const SkPoint quad[3] = { fPrevPt, pt1, pt2 };
    SkPoint reduction;
    ReductionType reductionType = CheckQuadLinear(quad, &reduction);
    if (kPoint_ReductionType == reductionType) {
        return;
    }
    if (kLine_ReductionType == reductionType) {
        this->lineTo(pt2);
        return;
    }
    if (kDegenerate_ReductionType == reductionType) {
        this->lineTo(reduction);
        SkStrokerPriv::JoinProc saveJoiner = fJoiner;
        fJoiner = SkStrokerPriv::JoinFactory(SkPaint::kRound_Join);
        this->lineTo(pt2);
        fJoiner = saveJoiner;
        return;
    }
    SkASSERT(kQuad_ReductionType == reductionType);
    SkVector normalAB, unitAB, normalBC, unitBC;
    this->preJoinTo(pt1, &normalAB, &unitAB, false);
    SkQuadConstruct quadPts;
    this->init(kOuter_StrokeType, &quadPts, 0, 1);
    if (!this->quadStroke(quad, &quadPts)) {
        return;
    }
    this->init(kInner_StrokeType, &quadPts, 0, 1);
    if (!this->quadStroke(quad, &quadPts)) {
        return;
    }
    this->setQuadEndNormal(quad, normalAB, unitAB, &normalBC, &unitBC);
#else
    bool    degenerateAB = SkPath::IsLineDegenerate(fPrevPt, pt1);
    bool    degenerateBC = SkPath::IsLineDegenerate(pt1, pt2);

    if (degenerateAB | degenerateBC) {
        if (degenerateAB ^ degenerateBC) {
            this->lineTo(pt2);
        }
        return;
    }

    SkVector    normalAB, unitAB, normalBC, unitBC;

    this->preJoinTo(pt1, &normalAB, &unitAB, false);

    {
        SkPoint pts[3], tmp[5];
        pts[0] = fPrevPt;
        pts[1] = pt1;
        pts[2] = pt2;

        if (SkChopQuadAtMaxCurvature(pts, tmp) == 2) {
            unitBC.setNormalize(pts[2].fX - pts[1].fX, pts[2].fY - pts[1].fY);
            unitBC.rotateCCW();
            if (normals_too_pinchy(unitAB, unitBC)) {
                normalBC = unitBC;
                normalBC.scale(fRadius);

                fOuter.lineTo(tmp[2].fX + normalAB.fX, tmp[2].fY + normalAB.fY);
                fOuter.lineTo(tmp[2].fX + normalBC.fX, tmp[2].fY + normalBC.fY);
                fOuter.lineTo(tmp[4].fX + normalBC.fX, tmp[4].fY + normalBC.fY);

                fInner.lineTo(tmp[2].fX - normalAB.fX, tmp[2].fY - normalAB.fY);
                fInner.lineTo(tmp[2].fX - normalBC.fX, tmp[2].fY - normalBC.fY);
                fInner.lineTo(tmp[4].fX - normalBC.fX, tmp[4].fY - normalBC.fY);

                fExtra.addCircle(tmp[2].fX, tmp[2].fY, fRadius,
                                 SkPath::kCW_Direction);
            } else {
                this->quad_to(&tmp[0], normalAB, unitAB, &normalBC, &unitBC,
                              kMaxQuadSubdivide);
                SkVector n = normalBC;
                SkVector u = unitBC;
                this->quad_to(&tmp[2], n, u, &normalBC, &unitBC,
                              kMaxQuadSubdivide);
            }
        } else {
            this->quad_to(pts, normalAB, unitAB, &normalBC, &unitBC,
                          kMaxQuadSubdivide);
        }
    }
#endif

    this->postJoinTo(pt2, normalBC, unitBC);
}

#if QUAD_STROKE_APPROXIMATION
// Given a point on the curve and its derivative, scale the derivative by the radius, and
// compute the perpendicular point and its tangent.
void SkPathStroker::setRayPts(const SkPoint& tPt, SkVector* dxy, SkPoint* onPt,
        SkPoint* tangent) const {
    if (!dxy->setLength(fRadius)) {  // consider moving double logic into SkPoint::setLength
        double xx = dxy->fX;
        double yy = dxy->fY;
        double dscale = fRadius / sqrt(xx * xx + yy * yy);
        dxy->fX = SkDoubleToScalar(xx * dscale);
        dxy->fY = SkDoubleToScalar(yy * dscale);
    }
    SkScalar axisFlip = SkIntToScalar(fStrokeType);  // go opposite ways for outer, inner
    onPt->fX = tPt.fX + axisFlip * dxy->fY;
    onPt->fY = tPt.fY - axisFlip * dxy->fX;
    if (tangent) {
        tangent->fX = onPt->fX + dxy->fX;
        tangent->fY = onPt->fY + dxy->fY;
    }
}

// Given a cubic and t, return the point on curve, its perpendicular, and the perpendicular tangent.
// Returns false if the perpendicular could not be computed (because the derivative collapsed to 0)
bool SkPathStroker::cubicPerpRay(const SkPoint cubic[4], SkScalar t, SkPoint* tPt, SkPoint* onPt,
        SkPoint* tangent) const {
    SkVector dxy;
    SkEvalCubicAt(cubic, t, tPt, &dxy, NULL);
    if (dxy.fX == 0 && dxy.fY == 0) {
        if (SkScalarNearlyZero(t)) {
            dxy = cubic[2] - cubic[0];
        } else if (SkScalarNearlyZero(1 - t)) {
            dxy = cubic[3] - cubic[1];
        } else {
            return false;
        }
        if (dxy.fX == 0 && dxy.fY == 0) {
            dxy = cubic[3] - cubic[0];
        }
    }
    setRayPts(*tPt, &dxy, onPt, tangent);
    return true;
}

// Given a cubic and a t range, find the start and end if they haven't been found already.
bool SkPathStroker::cubicQuadEnds(const SkPoint cubic[4], SkQuadConstruct* quadPts) {
    if (!quadPts->fStartSet) {
        SkPoint cubicStartPt;
        if (!this->cubicPerpRay(cubic, quadPts->fStartT, &cubicStartPt, &quadPts->fQuad[0],
                &quadPts->fTangentStart)) {
            return false;
        }
        quadPts->fStartSet = true;
    }
    if (!quadPts->fEndSet) {
        SkPoint cubicEndPt;
        if (!this->cubicPerpRay(cubic, quadPts->fEndT, &cubicEndPt, &quadPts->fQuad[2],
                &quadPts->fTangentEnd)) {
            return false;
        }
        quadPts->fEndSet = true;
    }
    return true;
}

bool SkPathStroker::cubicQuadMid(const SkPoint cubic[4], const SkQuadConstruct* quadPts,
        SkPoint* mid) const {
    SkPoint cubicMidPt;
    return this->cubicPerpRay(cubic, quadPts->fMidT, &cubicMidPt, mid, NULL);
}

// Given a quad and t, return the point on curve, its perpendicular, and the perpendicular tangent.
void SkPathStroker::quadPerpRay(const SkPoint quad[3], SkScalar t, SkPoint* tPt, SkPoint* onPt,
        SkPoint* tangent) const {
    SkVector dxy;
    SkEvalQuadAt(quad, t, tPt, &dxy);
    if (dxy.fX == 0 && dxy.fY == 0) {
        dxy = quad[2] - quad[0];
    }
    setRayPts(*tPt, &dxy, onPt, tangent);
}

// Find the intersection of the stroke tangents to construct a stroke quad.
// Return whether the stroke is a degenerate (a line), a quad, or must be split.
// Optionally compute the quad's control point.
SkPathStroker::ResultType SkPathStroker::intersectRay(SkQuadConstruct* quadPts,
        IntersectRayType intersectRayType  STROKER_DEBUG_PARAMS(int depth)) const {
    const SkPoint& start = quadPts->fQuad[0];
    const SkPoint& end = quadPts->fQuad[2];
    SkVector aLen = quadPts->fTangentStart - start;
    SkVector bLen = quadPts->fTangentEnd - end;
    SkScalar denom = aLen.cross(bLen);
    SkVector ab0 = start - end;
    SkScalar numerA = bLen.cross(ab0);
    SkScalar numerB = aLen.cross(ab0);
    if (!SkScalarNearlyZero(denom)) {
        // if the perpendicular distances from the quad points to the opposite tangent line
        // are small, a straight line is good enough
        SkScalar dist1 = pt_to_line(start, end, quadPts->fTangentEnd);
        SkScalar dist2 = pt_to_line(end, start, quadPts->fTangentStart);
        if (SkTMax(dist1, dist2) <= fError * fError) {
            return STROKER_RESULT(kDegenerate_ResultType, depth, quadPts,
                    "SkTMax(dist1=%g, dist2=%g) <= fError * fError", dist1, dist2);
        }
        if ((numerA >= 0) != (numerB >= 0)) {
            if (kCtrlPt_RayType == intersectRayType) {
                numerA /= denom;
                SkPoint* ctrlPt = &quadPts->fQuad[1];
                ctrlPt->fX = start.fX * (1 - numerA) + quadPts->fTangentStart.fX * numerA;
                ctrlPt->fY = start.fY * (1 - numerA) + quadPts->fTangentStart.fY * numerA;
            }
            return STROKER_RESULT(kQuad_ResultType, depth, quadPts,
                    "(numerA=%g >= 0) != (numerB=%g >= 0)", numerA, numerB);
        }
        return STROKER_RESULT(kSplit_ResultType, depth, quadPts,
                "(numerA=%g >= 0) == (numerB=%g >= 0)", numerA, numerB);
    } else { // if the lines are parallel, straight line is good enough
        return STROKER_RESULT(kDegenerate_ResultType, depth, quadPts,
                "SkScalarNearlyZero(denom=%g)", denom);
    }
}

// Given a cubic and a t-range, determine if the stroke can be described by a quadratic.
SkPathStroker::ResultType SkPathStroker::tangentsMeet(const SkPoint cubic[4],
        SkQuadConstruct* quadPts) {
    if (!this->cubicQuadEnds(cubic, quadPts)) {
        return kNormalError_ResultType;
    }
    return intersectRay(quadPts, kResultType_RayType  STROKER_DEBUG_PARAMS(fRecursionDepth));
}

// Intersect the line with the quad and return the t values on the quad where the line crosses.
static int intersect_quad_ray(const SkPoint line[2], const SkPoint quad[3], SkScalar roots[2]) {
    SkVector vec = line[1] - line[0];
    SkScalar r[3];
    for (int n = 0; n < 3; ++n) {
        r[n] = (quad[n].fY - line[0].fY) * vec.fX - (quad[n].fX - line[0].fX) * vec.fY;
    }
    SkScalar A = r[2];
    SkScalar B = r[1];
    SkScalar C = r[0];
    A += C - 2 * B;  // A = a - 2*b + c
    B -= C;  // B = -(b - c)
    return SkFindUnitQuadRoots(A, 2 * B, C, roots);
}

// Return true if the point is close to the bounds of the quad. This is used as a quick reject.
bool SkPathStroker::ptInQuadBounds(const SkPoint quad[3], const SkPoint& pt) const {
    SkScalar xMin = SkTMin(SkTMin(quad[0].fX, quad[1].fX), quad[2].fX);
    if (pt.fX + fError < xMin) {
        return false;
    }
    SkScalar xMax = SkTMax(SkTMax(quad[0].fX, quad[1].fX), quad[2].fX);
    if (pt.fX - fError > xMax) {
        return false;
    }
    SkScalar yMin = SkTMin(SkTMin(quad[0].fY, quad[1].fY), quad[2].fY);
    if (pt.fY + fError < yMin) {
        return false;
    }
    SkScalar yMax = SkTMax(SkTMax(quad[0].fY, quad[1].fY), quad[2].fY);
    if (pt.fY - fError > yMax) {
        return false;
    }
    return true;
}

static bool points_within_dist(const SkPoint& nearPt, const SkPoint& farPt, SkScalar limit) {
    return nearPt.distanceToSqd(farPt) <= limit * limit;
}

static bool sharp_angle(const SkPoint quad[3]) {
    SkVector smaller = quad[1] - quad[0];
    SkVector larger = quad[1] - quad[2];
    SkScalar smallerLen = smaller.lengthSqd();
    SkScalar largerLen = larger.lengthSqd();
    if (smallerLen > largerLen) {
        SkTSwap(smaller, larger);
        largerLen = smallerLen;
    }
    if (!smaller.setLength(largerLen)) {
        return false;
    }
    SkScalar dot = smaller.dot(larger);
    return dot > 0;
}

SkPathStroker::ResultType SkPathStroker::strokeCloseEnough(const SkPoint stroke[3],
        const SkPoint ray[2], SkQuadConstruct* quadPts  STROKER_DEBUG_PARAMS(int depth)) const {
    SkPoint strokeMid;
    SkEvalQuadAt(stroke, SK_ScalarHalf, &strokeMid);
    // measure the distance from the curve to the quad-stroke midpoint, compare to radius
    if (points_within_dist(ray[0], strokeMid, fError)) {  // if the difference is small
        if (sharp_angle(quadPts->fQuad)) {
            return STROKER_RESULT(kSplit_ResultType, depth, quadPts,
                    "sharp_angle (1) =%g,%g, %g,%g, %g,%g",
                    quadPts->fQuad[0].fX, quadPts->fQuad[0].fY,
                    quadPts->fQuad[1].fX, quadPts->fQuad[1].fY,
                    quadPts->fQuad[2].fX, quadPts->fQuad[2].fY);
        }
        return STROKER_RESULT(kQuad_ResultType, depth, quadPts,
                "points_within_dist(ray[0]=%g,%g, strokeMid=%g,%g, fError)",
                ray[0].fX, ray[0].fY, strokeMid.fX, strokeMid.fY);
    }
    // measure the distance to quad's bounds (quick reject)
        // an alternative : look for point in triangle
    if (!ptInQuadBounds(stroke, ray[0])) {  // if far, subdivide
        return STROKER_RESULT(kSplit_ResultType, depth, quadPts,
                "!pt_in_quad_bounds(stroke=(%g,%g %g,%g %g,%g), ray[0]=%g,%g)",
                stroke[0].fX, stroke[0].fY, stroke[1].fX, stroke[1].fY, stroke[2].fX, stroke[2].fY,
                ray[0].fX, ray[0].fY);
    }
    // measure the curve ray distance to the quad-stroke
    SkScalar roots[2];
    int rootCount = intersect_quad_ray(ray, stroke, roots);
    if (rootCount != 1) {
        return STROKER_RESULT(kSplit_ResultType, depth, quadPts,
                "rootCount=%d != 1", rootCount);
    }
    SkPoint quadPt;
    SkEvalQuadAt(stroke, roots[0], &quadPt);
    SkScalar error = fError * (SK_Scalar1 - SkScalarAbs(roots[0] - 0.5f) * 2);
    if (points_within_dist(ray[0], quadPt, error)) {  // if the difference is small, we're done
        if (sharp_angle(quadPts->fQuad)) {
            return STROKER_RESULT(kSplit_ResultType, depth, quadPts,
                    "sharp_angle (2) =%g,%g, %g,%g, %g,%g",
                    quadPts->fQuad[0].fX, quadPts->fQuad[0].fY,
                    quadPts->fQuad[1].fX, quadPts->fQuad[1].fY,
                    quadPts->fQuad[2].fX, quadPts->fQuad[2].fY);
        }
        return STROKER_RESULT(kQuad_ResultType, depth, quadPts,
                "points_within_dist(ray[0]=%g,%g, quadPt=%g,%g, error=%g)",
                ray[0].fX, ray[0].fY, quadPt.fX, quadPt.fY, error);
    }
    // otherwise, subdivide
    return STROKER_RESULT(kSplit_ResultType, depth, quadPts, "%s", "fall through");
}

SkPathStroker::ResultType SkPathStroker::compareQuadCubic(const SkPoint cubic[4],
        SkQuadConstruct* quadPts) {
    // get the quadratic approximation of the stroke
    if (!this->cubicQuadEnds(cubic, quadPts)) {
        return kNormalError_ResultType;
    }
    ResultType resultType = intersectRay(quadPts, kCtrlPt_RayType
            STROKER_DEBUG_PARAMS(fRecursionDepth) );
    if (resultType != kQuad_ResultType) {
        return resultType;
    }
    // project a ray from the curve to the stroke
    SkPoint ray[2];  // point near midpoint on quad, midpoint on cubic
    if (!this->cubicPerpRay(cubic, quadPts->fMidT, &ray[1], &ray[0], NULL)) {
        return kNormalError_ResultType;
    }
    return strokeCloseEnough(quadPts->fQuad, ray, quadPts  STROKER_DEBUG_PARAMS(fRecursionDepth));
}

// if false is returned, caller splits quadratic approximation
SkPathStroker::ResultType SkPathStroker::compareQuadQuad(const SkPoint quad[3],
        SkQuadConstruct* quadPts) {
    // get the quadratic approximation of the stroke
    if (!quadPts->fStartSet) {
        SkPoint quadStartPt;
        this->quadPerpRay(quad, quadPts->fStartT, &quadStartPt, &quadPts->fQuad[0],
                &quadPts->fTangentStart);
        quadPts->fStartSet = true;
    }
    if (!quadPts->fEndSet) {
        SkPoint quadEndPt;
        this->quadPerpRay(quad, quadPts->fEndT, &quadEndPt, &quadPts->fQuad[2],
                &quadPts->fTangentEnd);
        quadPts->fEndSet = true;
    }
    ResultType resultType = intersectRay(quadPts, kCtrlPt_RayType
            STROKER_DEBUG_PARAMS(fRecursionDepth));
    if (resultType != kQuad_ResultType) {
        return resultType;
    }
    // project a ray from the curve to the stroke
    SkPoint ray[2];
    this->quadPerpRay(quad, quadPts->fMidT, &ray[1], &ray[0], NULL);
    return strokeCloseEnough(quadPts->fQuad, ray, quadPts  STROKER_DEBUG_PARAMS(fRecursionDepth));
}

void SkPathStroker::addDegenerateLine(const SkQuadConstruct* quadPts) {
    const SkPoint* quad = quadPts->fQuad;
    SkPath* path = fStrokeType == kOuter_StrokeType ? &fOuter : &fInner;
    path->lineTo(quad[2].fX, quad[2].fY);
}

bool SkPathStroker::cubicMidOnLine(const SkPoint cubic[4], const SkQuadConstruct* quadPts) const {
    SkPoint strokeMid;
    if (!cubicQuadMid(cubic, quadPts, &strokeMid)) {
        return false;
    }
    SkScalar dist = pt_to_line(strokeMid, quadPts->fQuad[0], quadPts->fQuad[2]);
    return dist < fError * fError;
}

bool SkPathStroker::cubicStroke(const SkPoint cubic[4], SkQuadConstruct* quadPts) {
    if (!fFoundTangents) {
        ResultType resultType = this->tangentsMeet(cubic, quadPts);
        if (kQuad_ResultType != resultType) {
            if (kNormalError_ResultType == resultType) {
                return false;
            }
            if ((kDegenerate_ResultType == resultType
                    || points_within_dist(quadPts->fQuad[0], quadPts->fQuad[2], fError))
                    && cubicMidOnLine(cubic, quadPts)) {
                addDegenerateLine(quadPts);
                return true;
            }
        } else {
            fFoundTangents = true;
        }
    }
    if (fFoundTangents) {
        ResultType resultType = this->compareQuadCubic(cubic, quadPts);
        if (kQuad_ResultType == resultType) {
            SkPath* path = fStrokeType == kOuter_StrokeType ? &fOuter : &fInner;
            const SkPoint* stroke = quadPts->fQuad;
            path->quadTo(stroke[1].fX, stroke[1].fY, stroke[2].fX, stroke[2].fY);
            return true;
        }
        if (kDegenerate_ResultType == resultType) {
            addDegenerateLine(quadPts);
            return true;
        }
        if (kNormalError_ResultType == resultType) {
            return false;
        }
    }
    if (!SkScalarIsFinite(quadPts->fQuad[2].fX) || !SkScalarIsFinite(quadPts->fQuad[2].fY)) {
        return false;  // just abort if projected quad isn't representable
    }
    SkDEBUGCODE(gMaxRecursion[fFoundTangents] = SkTMax(gMaxRecursion[fFoundTangents],
            fRecursionDepth + 1));
    if (++fRecursionDepth > kRecursiveLimits[fFoundTangents]) {
        return false;  // just abort if projected quad isn't representable
    }
    SkQuadConstruct half;
    if (!half.initWithStart(quadPts)) {
        addDegenerateLine(quadPts);
        return true;
    }
    if (!this->cubicStroke(cubic, &half)) {
        return false;
    }
    if (!half.initWithEnd(quadPts)) {
        addDegenerateLine(quadPts);
        return true;
    }
    if (!this->cubicStroke(cubic, &half)) {
        return false;
    }
    --fRecursionDepth;
    return true;
}

bool SkPathStroker::quadStroke(const SkPoint quad[3], SkQuadConstruct* quadPts) {
    ResultType resultType = this->compareQuadQuad(quad, quadPts);
    if (kQuad_ResultType == resultType) {
        const SkPoint* stroke = quadPts->fQuad;
        SkPath* path = fStrokeType == kOuter_StrokeType ? &fOuter : &fInner;
        path->quadTo(stroke[1].fX, stroke[1].fY, stroke[2].fX, stroke[2].fY);
        return true;
    }
    if (kDegenerate_ResultType == resultType) {
        addDegenerateLine(quadPts);
        return true;
    }
    SkDEBUGCODE(gMaxRecursion[kQuad_RecursiveLimit] = SkTMax(gMaxRecursion[kQuad_RecursiveLimit],
            fRecursionDepth + 1));
    if (++fRecursionDepth > kRecursiveLimits[kQuad_RecursiveLimit]) {
        return false;  // just abort if projected quad isn't representable
    }
    SkQuadConstruct half;
    (void) half.initWithStart(quadPts);
    if (!this->quadStroke(quad, &half)) {
        return false;
    }
    (void) half.initWithEnd(quadPts);
    if (!this->quadStroke(quad, &half)) {
        return false;
    }
    --fRecursionDepth;
    return true;
}

#endif

void SkPathStroker::cubicTo(const SkPoint& pt1, const SkPoint& pt2,
                            const SkPoint& pt3) {
#if QUAD_STROKE_APPROXIMATION
    const SkPoint cubic[4] = { fPrevPt, pt1, pt2, pt3 };
    SkPoint reduction[3];
    const SkPoint* tangentPt;
    ReductionType reductionType = CheckCubicLinear(cubic, reduction, &tangentPt);
    if (kPoint_ReductionType == reductionType) {
        return;
    }
    if (kLine_ReductionType == reductionType) {
        this->lineTo(pt3);
        return;
    }
    if (kDegenerate_ReductionType <= reductionType && kDegenerate3_ReductionType >= reductionType) {
        this->lineTo(reduction[0]);
        SkStrokerPriv::JoinProc saveJoiner = fJoiner;
        fJoiner = SkStrokerPriv::JoinFactory(SkPaint::kRound_Join);
        if (kDegenerate2_ReductionType <= reductionType) {
            this->lineTo(reduction[1]);
        }
        if (kDegenerate3_ReductionType == reductionType) {
            this->lineTo(reduction[2]);
        }
        this->lineTo(pt3);
        fJoiner = saveJoiner;
        return;
    }
    SkASSERT(kQuad_ReductionType == reductionType);
    SkVector normalAB, unitAB, normalCD, unitCD;
    this->preJoinTo(*tangentPt, &normalAB, &unitAB, false);
    SkScalar tValues[2];
    int count = SkFindCubicInflections(cubic, tValues);
    SkScalar lastT = 0;
    for (int index = 0; index <= count; ++index) {
        SkScalar nextT = index < count ? tValues[index] : 1;
        SkQuadConstruct quadPts;
        this->init(kOuter_StrokeType, &quadPts, lastT, nextT);
        if (!this->cubicStroke(cubic, &quadPts)) {
            return;
        }
        this->init(kInner_StrokeType, &quadPts, lastT, nextT);
        if (!this->cubicStroke(cubic, &quadPts)) {
            return;
        }
        lastT = nextT;
    }
    this->setCubicEndNormal(cubic, normalAB, unitAB, &normalCD, &unitCD);
#else
    bool    degenerateAB = SkPath::IsLineDegenerate(fPrevPt, pt1);
    bool    degenerateBC = SkPath::IsLineDegenerate(pt1, pt2);
    bool    degenerateCD = SkPath::IsLineDegenerate(pt2, pt3);

    if (degenerateAB + degenerateBC + degenerateCD >= 2
            || (degenerateAB && SkPath::IsLineDegenerate(fPrevPt, pt2))) {
        this->lineTo(pt3);
        return;
    }

    SkVector    normalAB, unitAB, normalCD, unitCD;

    // find the first tangent (which might be pt1 or pt2
    {
        const SkPoint*  nextPt = &pt1;
        if (degenerateAB)
            nextPt = &pt2;
        this->preJoinTo(*nextPt, &normalAB, &unitAB, false);
    }

    {
        SkPoint pts[4], tmp[13];
        int         i, count;
        SkVector    n, u;
        SkScalar    tValues[3];

        pts[0] = fPrevPt;
        pts[1] = pt1;
        pts[2] = pt2;
        pts[3] = pt3;

        count = SkChopCubicAtMaxCurvature(pts, tmp, tValues);
        n = normalAB;
        u = unitAB;
        for (i = 0; i < count; i++) {
            this->cubic_to(&tmp[i * 3], n, u, &normalCD, &unitCD,
                           kMaxCubicSubdivide);
            if (i == count - 1) {
                break;
            }
            n = normalCD;
            u = unitCD;

        }
    }
#endif

    this->postJoinTo(pt3, normalCD, unitCD);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#include "SkPaintDefaults.h"

SkStroke::SkStroke() {
    fWidth      = SK_Scalar1;
    fMiterLimit = SkPaintDefaults_MiterLimit;
    fCap        = SkPaint::kDefault_Cap;
    fJoin       = SkPaint::kDefault_Join;
    fDoFill     = false;
}

SkStroke::SkStroke(const SkPaint& p) {
    fWidth      = p.getStrokeWidth();
    fMiterLimit = p.getStrokeMiter();
    fCap        = (uint8_t)p.getStrokeCap();
    fJoin       = (uint8_t)p.getStrokeJoin();
    fDoFill     = SkToU8(p.getStyle() == SkPaint::kStrokeAndFill_Style);
}

SkStroke::SkStroke(const SkPaint& p, SkScalar width) {
    fWidth      = width;
    fMiterLimit = p.getStrokeMiter();
    fCap        = (uint8_t)p.getStrokeCap();
    fJoin       = (uint8_t)p.getStrokeJoin();
    fDoFill     = SkToU8(p.getStyle() == SkPaint::kStrokeAndFill_Style);
}

#if QUAD_STROKE_APPROXIMATION
void SkStroke::setError(SkScalar error) {
    SkASSERT(error > 0);
    fError = error;
}
#endif

void SkStroke::setWidth(SkScalar width) {
    SkASSERT(width >= 0);
    fWidth = width;
}

void SkStroke::setMiterLimit(SkScalar miterLimit) {
    SkASSERT(miterLimit >= 0);
    fMiterLimit = miterLimit;
}

void SkStroke::setCap(SkPaint::Cap cap) {
    SkASSERT((unsigned)cap < SkPaint::kCapCount);
    fCap = SkToU8(cap);
}

void SkStroke::setJoin(SkPaint::Join join) {
    SkASSERT((unsigned)join < SkPaint::kJoinCount);
    fJoin = SkToU8(join);
}

///////////////////////////////////////////////////////////////////////////////

// If src==dst, then we use a tmp path to record the stroke, and then swap
// its contents with src when we're done.
class AutoTmpPath {
public:
    AutoTmpPath(const SkPath& src, SkPath** dst) : fSrc(src) {
        if (&src == *dst) {
            *dst = &fTmpDst;
            fSwapWithSrc = true;
        } else {
            (*dst)->reset();
            fSwapWithSrc = false;
        }
    }

    ~AutoTmpPath() {
        if (fSwapWithSrc) {
            fTmpDst.swap(*const_cast<SkPath*>(&fSrc));
        }
    }

private:
    SkPath          fTmpDst;
    const SkPath&   fSrc;
    bool            fSwapWithSrc;
};

void SkStroke::strokePath(const SkPath& src, SkPath* dst) const {
    SkASSERT(&src != NULL && dst != NULL);

    SkScalar radius = SkScalarHalf(fWidth);

    AutoTmpPath tmp(src, &dst);

    if (radius <= 0) {
        return;
    }

    // If src is really a rect, call our specialty strokeRect() method
    {
        bool isClosed;
        SkPath::Direction dir;
        if (src.isRect(&isClosed, &dir) && isClosed) {
            this->strokeRect(src.getBounds(), dst, dir);
            // our answer should preserve the inverseness of the src
            if (src.isInverseFillType()) {
                SkASSERT(!dst->isInverseFillType());
                dst->toggleInverseFillType();
            }
            return;
        }
    }

    SkAutoConicToQuads converter;
    const SkScalar conicTol = SK_Scalar1 / 4;

#if QUAD_STROKE_APPROXIMATION
    SkPathStroker   stroker(src, radius, fMiterLimit, fError, this->getCap(),
                            this->getJoin());
#else
    SkPathStroker   stroker(src, radius, fMiterLimit, this->getCap(),
                            this->getJoin());
#endif
    SkPath::Iter    iter(src, false);
    SkPath::Verb    lastSegment = SkPath::kMove_Verb;

    for (;;) {
        SkPoint  pts[4];
        switch (iter.next(pts, false)) {
            case SkPath::kMove_Verb:
                stroker.moveTo(pts[0]);
                break;
            case SkPath::kLine_Verb:
                stroker.lineTo(pts[1]);
                lastSegment = SkPath::kLine_Verb;
                break;
            case SkPath::kQuad_Verb:
                stroker.quadTo(pts[1], pts[2]);
                lastSegment = SkPath::kQuad_Verb;
                break;
            case SkPath::kConic_Verb: {
                // todo: if we had maxcurvature for conics, perhaps we should
                // natively extrude the conic instead of converting to quads.
                const SkPoint* quadPts =
                    converter.computeQuads(pts, iter.conicWeight(), conicTol);
                for (int i = 0; i < converter.countQuads(); ++i) {
                    stroker.quadTo(quadPts[1], quadPts[2]);
                    quadPts += 2;
                }
                lastSegment = SkPath::kQuad_Verb;
            } break;
            case SkPath::kCubic_Verb:
                stroker.cubicTo(pts[1], pts[2], pts[3]);
                lastSegment = SkPath::kCubic_Verb;
                break;
            case SkPath::kClose_Verb:
                stroker.close(lastSegment == SkPath::kLine_Verb);
                break;
            case SkPath::kDone_Verb:
                goto DONE;
        }
    }
DONE:
    stroker.done(dst, lastSegment == SkPath::kLine_Verb);

    if (fDoFill) {
        if (src.cheapIsDirection(SkPath::kCCW_Direction)) {
            dst->reverseAddPath(src);
        } else {
            dst->addPath(src);
        }
    } else {
        //  Seems like we can assume that a 2-point src would always result in
        //  a convex stroke, but testing has proved otherwise.
        //  TODO: fix the stroker to make this assumption true (without making
        //  it slower that the work that will be done in computeConvexity())
#if 0
        // this test results in a non-convex stroke :(
        static void test(SkCanvas* canvas) {
            SkPoint pts[] = { 146.333328,  192.333328, 300.333344, 293.333344 };
            SkPaint paint;
            paint.setStrokeWidth(7);
            paint.setStrokeCap(SkPaint::kRound_Cap);
            canvas->drawLine(pts[0].fX, pts[0].fY, pts[1].fX, pts[1].fY, paint);
        }
#endif
#if 0
        if (2 == src.countPoints()) {
            dst->setIsConvex(true);
        }
#endif
    }

    // our answer should preserve the inverseness of the src
    if (src.isInverseFillType()) {
        SkASSERT(!dst->isInverseFillType());
        dst->toggleInverseFillType();
    }
}

static SkPath::Direction reverse_direction(SkPath::Direction dir) {
    SkASSERT(SkPath::kUnknown_Direction != dir);
    return SkPath::kCW_Direction == dir ? SkPath::kCCW_Direction : SkPath::kCW_Direction;
}

static void addBevel(SkPath* path, const SkRect& r, const SkRect& outer, SkPath::Direction dir) {
    SkPoint pts[8];

    if (SkPath::kCW_Direction == dir) {
        pts[0].set(r.fLeft, outer.fTop);
        pts[1].set(r.fRight, outer.fTop);
        pts[2].set(outer.fRight, r.fTop);
        pts[3].set(outer.fRight, r.fBottom);
        pts[4].set(r.fRight, outer.fBottom);
        pts[5].set(r.fLeft, outer.fBottom);
        pts[6].set(outer.fLeft, r.fBottom);
        pts[7].set(outer.fLeft, r.fTop);
    } else {
        pts[7].set(r.fLeft, outer.fTop);
        pts[6].set(r.fRight, outer.fTop);
        pts[5].set(outer.fRight, r.fTop);
        pts[4].set(outer.fRight, r.fBottom);
        pts[3].set(r.fRight, outer.fBottom);
        pts[2].set(r.fLeft, outer.fBottom);
        pts[1].set(outer.fLeft, r.fBottom);
        pts[0].set(outer.fLeft, r.fTop);
    }
    path->addPoly(pts, 8, true);
}

void SkStroke::strokeRect(const SkRect& origRect, SkPath* dst,
                          SkPath::Direction dir) const {
    SkASSERT(dst != NULL);
    dst->reset();

    SkScalar radius = SkScalarHalf(fWidth);
    if (radius <= 0) {
        return;
    }

    SkScalar rw = origRect.width();
    SkScalar rh = origRect.height();
    if ((rw < 0) ^ (rh < 0)) {
        dir = reverse_direction(dir);
    }
    SkRect rect(origRect);
    rect.sort();
    // reassign these, now that we know they'll be >= 0
    rw = rect.width();
    rh = rect.height();

    SkRect r(rect);
    r.outset(radius, radius);

    SkPaint::Join join = (SkPaint::Join)fJoin;
    if (SkPaint::kMiter_Join == join && fMiterLimit < SK_ScalarSqrt2) {
        join = SkPaint::kBevel_Join;
    }

    switch (join) {
        case SkPaint::kMiter_Join:
            dst->addRect(r, dir);
            break;
        case SkPaint::kBevel_Join:
            addBevel(dst, rect, r, dir);
            break;
        case SkPaint::kRound_Join:
            dst->addRoundRect(r, radius, radius, dir);
            break;
        default:
            break;
    }

    if (fWidth < SkMinScalar(rw, rh) && !fDoFill) {
        r = rect;
        r.inset(radius, radius);
        dst->addRect(r, reverse_direction(dir));
    }
}
