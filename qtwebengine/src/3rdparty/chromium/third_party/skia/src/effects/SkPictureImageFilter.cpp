/*
 * Copyright 2013 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkPictureImageFilter.h"
#include "SkDevice.h"
#include "SkCanvas.h"
#include "SkReadBuffer.h"
#include "SkWriteBuffer.h"
#include "SkValidationUtils.h"

SkPictureImageFilter::SkPictureImageFilter(const SkPicture* picture, uint32_t uniqueID)
    : INHERITED(0, 0, NULL, uniqueID)
    , fPicture(SkSafeRef(picture))
    , fCropRect(picture ? picture->cullRect() : SkRect::MakeEmpty()) {
}

SkPictureImageFilter::SkPictureImageFilter(const SkPicture* picture, const SkRect& cropRect,
                                           uint32_t uniqueID)
    : INHERITED(0, 0, NULL, uniqueID)
    , fPicture(SkSafeRef(picture))
    , fCropRect(cropRect) {
}

SkPictureImageFilter::~SkPictureImageFilter() {
    SkSafeUnref(fPicture);
}

#ifdef SK_SUPPORT_LEGACY_DEEPFLATTENING
SkPictureImageFilter::SkPictureImageFilter(SkReadBuffer& buffer)
  : INHERITED(0, buffer),
    fPicture(NULL) {
    if (!buffer.isCrossProcess()) {
        if (buffer.readBool()) {
            fPicture = SkPicture::CreateFromBuffer(buffer);
        }
    } else {
        buffer.validate(!buffer.readBool());
    }
    buffer.readRect(&fCropRect);
}
#endif

SkFlattenable* SkPictureImageFilter::CreateProc(SkReadBuffer& buffer) {
    SkAutoTUnref<SkPicture> picture;
    SkRect cropRect;

    if (!buffer.isCrossProcess()) {
        if (buffer.readBool()) {
            picture.reset(SkPicture::CreateFromBuffer(buffer));
        }
    } else {
        buffer.validate(!buffer.readBool());
    }
    buffer.readRect(&cropRect);

    return Create(picture, cropRect);
}

void SkPictureImageFilter::flatten(SkWriteBuffer& buffer) const {
    if (!buffer.isCrossProcess()) {
        bool hasPicture = (fPicture != NULL);
        buffer.writeBool(hasPicture);
        if (hasPicture) {
            fPicture->flatten(buffer);
        }
    } else {
        buffer.writeBool(false);
    }
    buffer.writeRect(fCropRect);
}

bool SkPictureImageFilter::onFilterImage(Proxy* proxy, const SkBitmap&, const Context& ctx,
                                         SkBitmap* result, SkIPoint* offset) const {
    if (!fPicture) {
        offset->fX = offset->fY = 0;
        return true;
    }

    SkRect floatBounds;
    SkIRect bounds;
    ctx.ctm().mapRect(&floatBounds, fCropRect);
    floatBounds.roundOut(&bounds);
    if (!bounds.intersect(ctx.clipBounds())) {
        return false;
    }

    if (bounds.isEmpty()) {
        offset->fX = offset->fY = 0;
        return true;
    }

    SkAutoTUnref<SkBaseDevice> device(proxy->createDevice(bounds.width(), bounds.height()));
    if (NULL == device.get()) {
        return false;
    }

    SkCanvas canvas(device.get());
    SkPaint paint;

    canvas.translate(-SkIntToScalar(bounds.fLeft), -SkIntToScalar(bounds.fTop));
    canvas.concat(ctx.ctm());
    canvas.drawPicture(fPicture);

    *result = device.get()->accessBitmap(false);
    offset->fX = bounds.fLeft;
    offset->fY = bounds.fTop;
    return true;
}
