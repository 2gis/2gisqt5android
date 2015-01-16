/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef CurveIntersection_DEFINE
#define CurveIntersection_DEFINE

#include "DataTypes.h"

class Intersections;

// unit-testable utilities
double axialIntersect(const Quadratic& q1, const _Point& p, bool vert);
bool bezier_clip(const Cubic& cubic1, const Cubic& cubic2, double& minT, double& maxT);
bool bezier_clip(const Quadratic& q1, const Quadratic& q2, double& minT, double& maxT);
int convex_hull(const Cubic& cubic, char order[4]);
bool convex_x_hull(const Cubic& cubic, char connectTo0[2], char connectTo3[2]);
bool implicit_matches(const Cubic& cubic1, const Cubic& cubic2);
bool implicit_matches(const _Line& line1, const _Line& line2);
bool implicit_matches_ulps(const _Line& one, const _Line& two, int ulps);
bool implicit_matches(const Quadratic& quad1, const Quadratic& quad2);
void tangent(const Cubic& cubic, double t, _Point& result);
void tangent(const _Line& line, _Point& result);
void tangent(const Quadratic& quad, double t, _Point& result);

// main functions
enum ReduceOrder_Quadratics {
    kReduceOrder_NoQuadraticsAllowed,
    kReduceOrder_QuadraticsAllowed
};
enum ReduceOrder_Styles {
    kReduceOrder_TreatAsStroke,
    kReduceOrder_TreatAsFill
};
int reduceOrder(const Cubic& cubic, Cubic& reduction, ReduceOrder_Quadratics ,
        ReduceOrder_Styles );
int reduceOrder(const _Line& line, _Line& reduction);
int reduceOrder(const Quadratic& quad, Quadratic& reduction, ReduceOrder_Styles );
int horizontalIntersect(const Cubic& cubic, double y, double tRange[3]);
int horizontalIntersect(const Cubic& cubic, double left, double right, double y,
        double tRange[3]);
int horizontalIntersect(const Cubic& cubic, double left, double right, double y,
        bool flipped, Intersections&);
int horizontalIntersect(const _Line& line, double left, double right,
        double y, bool flipped, Intersections& );
int horizontalIntersect(const Quadratic& quad, double left, double right,
        double y, double tRange[2]);
int horizontalIntersect(const Quadratic& quad, double left, double right,
        double y, bool flipped, Intersections& );
bool intersect(const Cubic& cubic1, const Cubic& cubic2, Intersections& );
// the following flavor uses quadratic approximation instead of convex hulls
//bool intersect2(const Cubic& cubic1, const Cubic& cubic2, Intersections& );
// like '2', but iterates on centers instead of possible edges
bool intersect3(const Cubic& cubic1, const Cubic& cubic2, Intersections& );
int intersect(const Cubic& cubic, Intersections& i); // return true if cubic self-intersects
int intersect(const Cubic& cubic, const Quadratic& quad, Intersections& );
int intersect(const Cubic& cubic, const _Line& line, Intersections& );
int intersectRay(const Cubic& quad, const _Line& line, Intersections& i);
bool intersect(const Quadratic& q1, const Quadratic& q2, Intersections& );
int intersect(const Quadratic& quad, const _Line& line, Intersections& );
// the following flavor uses the implicit form instead of convex hulls
bool intersect2(const Quadratic& q1, const Quadratic& q2, Intersections& i);
int intersectRay(const Quadratic& quad, const _Line& line, Intersections& i);


bool isLinear(const Quadratic& quad, int startIndex, int endIndex);
bool isLinear(const Cubic& cubic, int startIndex, int endIndex);
double leftMostT(const Cubic& , double startT, double endT);
double leftMostT(const _Line& , double startT, double endT);
double leftMostT(const Quadratic& , double startT, double endT);
int verticalIntersect(const Cubic& cubic, double top, double bottom, double x,
        bool flipped, Intersections& );
int verticalIntersect(const _Line& line, double top, double bottom, double x,
        bool flipped, Intersections& );
int verticalIntersect(const Quadratic& quad, double top, double bottom,
        double x, bool flipped, Intersections& );

#endif
