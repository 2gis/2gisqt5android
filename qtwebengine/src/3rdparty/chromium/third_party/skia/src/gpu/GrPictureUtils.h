/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrPictureUtils_DEFINED
#define GrPictureUtils_DEFINED

#include "SkPicture.h"
#include "SkTArray.h"

// This class encapsulates the GPU-backend-specific acceleration data
// for a single SkPicture
class GrAccelData : public SkPicture::AccelData {
public:
    // Information about a given saveLayer in an SkPicture
    class SaveLayerInfo {
    public:
        SaveLayerInfo() : fPicture(NULL), fPaint(NULL) {}
        ~SaveLayerInfo() { SkSafeUnref(fPicture); SkDELETE(fPaint); }

        // The picture owning the layer. If the owning picture is the top-most
        // one (i.e., the picture for which this GrAccelData was created) then
        // this pointer is NULL. If it is a nested picture then the pointer
        // is non-NULL and owns a ref on the picture.
        const SkPicture* fPicture;
        // The device space bounds of this layer.
        SkRect fBounds;
        // The pre-matrix begins as the identity and accumulates the transforms
        // of the containing SkPictures (if any). This matrix state has to be
        // part of the initial matrix during replay so that it will be 
        // preserved across setMatrix calls.
        SkMatrix fPreMat;
        // The matrix state (in the leaf picture) in which this layer's draws 
        // must occur. It will/can be overridden by setMatrix calls in the
        // layer itself. It does not include the translation needed to map the 
        // layer's top-left point to the origin (which must be part of the
        // initial matrix).
        SkMatrix fLocalMat;
        // The paint to use on restore. Can be NULL since it is optional.
        const SkPaint* fPaint;
        // The ID of this saveLayer in the picture. 0 is an invalid ID.
        size_t  fSaveLayerOpID;
        // The ID of the matching restore in the picture. 0 is an invalid ID.
        size_t  fRestoreOpID;
        // True if this saveLayer has at least one other saveLayer nested within it.
        // False otherwise.
        bool    fHasNestedLayers;
        // True if this saveLayer is nested within another. False otherwise.
        bool    fIsNested;
    };

    GrAccelData(Key key) : INHERITED(key) { }

    virtual ~GrAccelData() { }

    SaveLayerInfo& addSaveLayerInfo() { return fSaveLayerInfo.push_back(); }

    int numSaveLayers() const { return fSaveLayerInfo.count(); }

    const SaveLayerInfo& saveLayerInfo(int index) const {
        SkASSERT(index < fSaveLayerInfo.count());

        return fSaveLayerInfo[index];
    }

    // We may, in the future, need to pass in the GPUDevice in order to
    // incorporate the clip and matrix state into the key
    static SkPicture::AccelData::Key ComputeAccelDataKey();

private:
    SkTArray<SaveLayerInfo, true> fSaveLayerInfo;

    typedef SkPicture::AccelData INHERITED;
};

const GrAccelData* GPUOptimize(const SkPicture* pict);

#endif // GrPictureUtils_DEFINED
