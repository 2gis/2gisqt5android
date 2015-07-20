
/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This test only works with the GPU backend.

#include "gm.h"

#if SK_SUPPORT_GPU

#include "GrContext.h"
#include "GrPathUtils.h"
#include "GrTest.h"
#include "SkColorPriv.h"
#include "SkDevice.h"
#include "SkGeometry.h"
#include "SkTLList.h"

#include "effects/GrConvexPolyEffect.h"

namespace skiagm {
/**
 * This GM directly exercises a GrProcessor that draws convex polygons.
 */
class ConvexPolyEffect : public GM {
public:
    ConvexPolyEffect() {
        this->setBGColor(0xFFFFFFFF);
    }

protected:
    virtual SkString onShortName() SK_OVERRIDE {
        return SkString("convex_poly_effect");
    }

    virtual SkISize onISize() SK_OVERRIDE {
        return SkISize::Make(720, 800);
    }

    virtual uint32_t onGetFlags() const SK_OVERRIDE {
        // This is a GPU-specific GM.
        return kGPUOnly_Flag;
    }

    virtual void onOnceBeforeDraw() SK_OVERRIDE {
        SkPath tri;
        tri.moveTo(5.f, 5.f);
        tri.lineTo(100.f, 20.f);
        tri.lineTo(15.f, 100.f);

        fPaths.addToTail(tri);
        fPaths.addToTail(SkPath())->reverseAddPath(tri);

        tri.close();
        fPaths.addToTail(tri);

        SkPath ngon;
        static const SkScalar kRadius = 50.f;
        const SkPoint center = { kRadius, kRadius };
        for (int i = 0; i < GrConvexPolyEffect::kMaxEdges; ++i) {
            SkScalar angle = 2 * SK_ScalarPI * i / GrConvexPolyEffect::kMaxEdges;
            SkPoint point;
            point.fY = SkScalarSinCos(angle, &point.fX);
            point.scale(kRadius);
            point = center + point;
            if (0 == i) {
                ngon.moveTo(point);
            } else {
                ngon.lineTo(point);
            }
        }

        fPaths.addToTail(ngon);
        SkMatrix scaleM;
        scaleM.setScale(1.1f, 0.4f);
        ngon.transform(scaleM);
        fPaths.addToTail(ngon);

        // integer edges
        fRects.addToTail(SkRect::MakeLTRB(5.f, 1.f, 30.f, 25.f));
        // half-integer edges
        fRects.addToTail(SkRect::MakeLTRB(5.5f, 0.5f, 29.5f, 24.5f));
        // vertically/horizontally thin rects that cover pixel centers
        fRects.addToTail(SkRect::MakeLTRB(5.25f, 0.5f, 5.75f, 24.5f));
        fRects.addToTail(SkRect::MakeLTRB(5.5f,  0.5f, 29.5f, 0.75f));
        // vertically/horizontally thin rects that don't cover pixel centers
        fRects.addToTail(SkRect::MakeLTRB(5.55f, 0.5f, 5.75f, 24.5f));
        fRects.addToTail(SkRect::MakeLTRB(5.5f, .05f, 29.5f, .25f));
        // small in x and y
        fRects.addToTail(SkRect::MakeLTRB(5.05f, .55f, 5.45f, .85f));
        // inverted in x and y
        fRects.addToTail(SkRect::MakeLTRB(100.f, 50.5f, 5.f, 0.5f));
    }

    virtual void onDraw(SkCanvas* canvas) SK_OVERRIDE {
        GrRenderTarget* rt = canvas->internal_private_accessTopLayerRenderTarget();
        if (NULL == rt) {
            return;
        }
        GrContext* context = rt->getContext();
        if (NULL == context) {
            return;
        }

        SkScalar y = 0;
        for (SkTLList<SkPath>::Iter iter(fPaths, SkTLList<SkPath>::Iter::kHead_IterStart);
             iter.get();
             iter.next()) {
            const SkPath* path = iter.get();
            SkScalar x = 0;

            for (int et = 0; et < kGrProcessorEdgeTypeCnt; ++et) {
                GrTestTarget tt;
                context->getTestTarget(&tt);
                if (NULL == tt.target()) {
                    SkDEBUGFAIL("Couldn't get Gr test target.");
                    return;
                }
                GrDrawState* drawState = tt.target()->drawState();

                SkMatrix m;
                SkPath p;
                m.setTranslate(x, y);
                path->transform(m, &p);

                GrPrimitiveEdgeType edgeType = (GrPrimitiveEdgeType) et;
                SkAutoTUnref<GrFragmentProcessor> fp(GrConvexPolyEffect::Create(edgeType, p));
                if (!fp) {
                    continue;
                }
                drawState->addCoverageProcessor(fp);
                drawState->setIdentityViewMatrix();
                drawState->setRenderTarget(rt);
                drawState->setColor(0xff000000);

                // TODO hack
                GrDrawTarget::AutoReleaseGeometry geo(tt.target(), 4, 0);
                SkPoint* verts = reinterpret_cast<SkPoint*>(geo.vertices());

                //SkPoint verts[4];
                SkRect bounds = p.getBounds();
                // Make sure any artifacts around the exterior of path are visible by using overly
                // conservative bounding geometry.
                bounds.outset(5.f, 5.f);
                bounds.toQuad(verts);

                //tt.target()->setVertexSourceToArray(verts, 4);
                tt.target()->setIndexSourceToBuffer(context->getQuadIndexBuffer());
                tt.target()->drawIndexed(kTriangleFan_GrPrimitiveType, 0, 0, 4, 6);

                x += SkScalarCeilToScalar(path->getBounds().width() + 10.f);
            }

            // Draw AA and non AA paths using normal API for reference.
            canvas->save();
            canvas->translate(x, y);
            SkPaint paint;
            canvas->drawPath(*path, paint);
            canvas->translate(path->getBounds().width() + 10.f, 0);
            paint.setAntiAlias(true);
            canvas->drawPath(*path, paint);
            canvas->restore();

            y += SkScalarCeilToScalar(path->getBounds().height() + 20.f);
        }

        for (SkTLList<SkRect>::Iter iter(fRects, SkTLList<SkRect>::Iter::kHead_IterStart);
             iter.get();
             iter.next()) {

            SkScalar x = 0;

            for (int et = 0; et < kGrProcessorEdgeTypeCnt; ++et) {
                GrTestTarget tt;
                context->getTestTarget(&tt);
                if (NULL == tt.target()) {
                    SkDEBUGFAIL("Couldn't get Gr test target.");
                    return;
                }
                SkRect rect = *iter.get();
                rect.offset(x, y);
                GrPrimitiveEdgeType edgeType = (GrPrimitiveEdgeType) et;
                SkAutoTUnref<GrFragmentProcessor> fp(GrConvexPolyEffect::Create(edgeType, rect));
                if (!fp) {
                    continue;
                }

                GrDrawState* drawState = tt.target()->drawState();
                drawState->addCoverageProcessor(fp);
                drawState->setIdentityViewMatrix();
                drawState->setRenderTarget(rt);
                drawState->setColor(0xff000000);

                // TODO hack
                GrDrawTarget::AutoReleaseGeometry geo(tt.target(), 4, 0);
                SkPoint* verts = reinterpret_cast<SkPoint*>(geo.vertices());

                //SkPoint verts[4];
                SkRect bounds = rect;
                bounds.outset(5.f, 5.f);
                bounds.toQuad(verts);

                //tt.target()->setVertexSourceToArray(verts, 4);
                tt.target()->setIndexSourceToBuffer(context->getQuadIndexBuffer());
                tt.target()->drawIndexed(kTriangleFan_GrPrimitiveType, 0, 0, 4, 6);

                x += SkScalarCeilToScalar(rect.width() + 10.f);
            }

            // Draw rect without and with AA using normal API for reference
            canvas->save();
            canvas->translate(x, y);
            SkPaint paint;
            canvas->drawRect(*iter.get(), paint);
            x += SkScalarCeilToScalar(iter.get()->width() + 10.f);
            paint.setAntiAlias(true);
            canvas->drawRect(*iter.get(), paint);
            canvas->restore();

            y += SkScalarCeilToScalar(iter.get()->height() + 20.f);
        }
    }

private:
    SkTLList<SkPath> fPaths;
    SkTLList<SkRect> fRects;

    typedef GM INHERITED;
};

DEF_GM( return SkNEW(ConvexPolyEffect); )
}

#endif
