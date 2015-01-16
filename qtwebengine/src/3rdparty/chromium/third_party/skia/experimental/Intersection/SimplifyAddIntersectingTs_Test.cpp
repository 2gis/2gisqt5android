/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "Simplify.h"

namespace SimplifyAddIntersectingTsTest {

#include "Simplify.cpp"

} // end of SimplifyAddIntersectingTsTest namespace
/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Intersection_Tests.h"

static const SkPoint lines[][2] = {
    {{ 1,  1}, { 1,  1}},   // degenerate
    {{ 1,  1}, { 4,  1}},   // horizontal
    {{ 4,  1}, { 9,  1}},
    {{ 2,  1}, { 3,  1}},
    {{ 2,  1}, { 6,  1}},
    {{ 5,  1}, { 9,  1}},
    {{ 1,  1}, { 1,  4}},   // vertical
    {{ 1,  2}, { 1,  3}},
    {{ 1,  2}, { 1,  6}},
    {{ 1,  5}, { 1,  9}},
    {{ 1,  1}, { 3,  3}},   // diagonal
    {{ 2,  2}, { 4,  4}},
    {{ 2,  4}, { 4,  2}},
};

static const size_t lineCount = sizeof(lines) / sizeof(lines[0]);

static const SkPoint quads[][3] = {
    {{ 1,  1}, { 1,  1}, { 1,  1}},   // degenerate
    {{ 1,  1}, { 4,  1}, { 5,  1}},   // line
    {{ 1,  1}, { 4,  1}, { 4,  4}},   // curve
};

static const size_t quadCount = sizeof(quads) / sizeof(quads[0]);

static const SkPoint cubics[][4] = {
    {{ 1,  1}, { 1,  1}, { 1,  1}, { 1,  1}},   // degenerate
    {{ 1,  1}, { 4,  1}, { 5,  1}, { 6,  1}},   // line
    {{ 1,  1}, { 3,  1}, { 4,  2}, { 4,  4}},   // curve
};

static const size_t cubicCount = sizeof(cubics) / sizeof(cubics[0]);
static const size_t testCount = lineCount + quadCount + cubicCount;

static SkPath::Verb setPath(size_t outer, SkPath& path, const SkPoint*& pts1) {
    SkPath::Verb c1Type;
    if (outer < lineCount) {
        path.moveTo(lines[outer][0].fX, lines[outer][0].fY);
        path.lineTo(lines[outer][1].fX, lines[outer][1].fY);
        c1Type = SkPath::kLine_Verb;
        pts1 = lines[outer];
    } else {
        outer -= lineCount;
        if (outer < quadCount) {
        path.moveTo(quads[outer][0].fX, quads[outer][0].fY);
        path.quadTo(quads[outer][1].fX, quads[outer][1].fY,
                quads[outer][2].fX, quads[outer][2].fY);
            c1Type = SkPath::kQuad_Verb;
            pts1 = quads[outer];
        } else {
            outer -= quadCount;
            path.moveTo(cubics[outer][0].fX, cubics[outer][0].fY);
            path.cubicTo(cubics[outer][1].fX, cubics[outer][1].fY,
                    cubics[outer][2].fX, cubics[outer][2].fY,
                    cubics[outer][3].fX, cubics[outer][3].fY);
            c1Type = SkPath::kCubic_Verb;
            pts1 = cubics[outer];
        }
    }
    return c1Type;
}

static void testPath(const SkPath& path, const SkPoint* pts1, SkPath::Verb c1Type,
        const SkPoint* pts2, SkPath::Verb c2Type) {
    SkTArray<SimplifyAddIntersectingTsTest::Contour> contour;
    SimplifyAddIntersectingTsTest::EdgeBuilder builder(path, contour);
    if (contour.count() < 2) {
        return;
    }
    SimplifyAddIntersectingTsTest::Contour& c1 = contour[0];
    SimplifyAddIntersectingTsTest::Contour& c2 = contour[1];
    addIntersectTs(&c1, &c2);
#if DEBUG_DUMP
    bool c1Intersected = c1.segments()[0].intersected();
    // bool c2Intersected = c2.fSegments[0].intersected();
    SkDebugf("%s %s (%1.9g,%1.9g %1.9g,%1.9g) %s %s (%1.9g,%1.9g %1.9g,%1.9g)\n",
            __FUNCTION__, SimplifyAddIntersectingTsTest::kLVerbStr[c1Type],
            pts1[0].fX, pts1[0].fY,
            pts1[c1Type].fX, pts1[c1Type].fY,
            c1Intersected ? "intersects" : "does not intersect",
            SimplifyAddIntersectingTsTest::kLVerbStr[c2Type],
            pts2[0].fX, pts2[0].fY,
            pts2[c2Type].fX, pts2[c2Type].fY);
    if (c1Intersected) {
        c1.dump();
        c2.dump();
    }
#endif
}

static const size_t firstO = 6;
static const size_t firstI = 1;

void SimplifyAddIntersectingTs_Test() {
    const SkPoint* pts1, * pts2;
    if (firstO > 0 || firstI > 0) {
        SkPath path;
        SkPath::Verb c1Type = setPath(firstO, path, pts1);
        SkPath path2(path);
        SkPath::Verb c2Type = setPath(firstI, path2, pts2);
        testPath(path2, pts1, c1Type, pts2, c2Type);
    }
    for (size_t o = 0; o < testCount; ++o) {
        SkPath path;
        SkPath::Verb c1Type = setPath(o, path, pts1);
        for (size_t i = 0; i < testCount; ++i) {
            SkPath path2(path);
            SkPath::Verb c2Type = setPath(i, path2, pts2);
            testPath(path2, pts1, c1Type, pts2, c2Type);
        }
    }
}
