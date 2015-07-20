/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CopyTilesRenderer_DEFINED
#define CopyTilesRenderer_DEFINED

#include "PictureRenderer.h"
#include "SkTypes.h"

class SkPicture;
class SkString;

namespace sk_tools {
    /**
     *  PictureRenderer that draws the picture and then extracts it into tiles. For large pictures,
     *  it will divide the picture into large tiles and draw the picture once for each large tile.
     */
    class CopyTilesRenderer : public TiledPictureRenderer {

    public:
#if SK_SUPPORT_GPU
        CopyTilesRenderer(const GrContext::Options &opts, int x, int y);
#else
        CopyTilesRenderer(int x, int y);
#endif
        virtual void init(const SkPicture* pict, 
                          const SkString* writePath, 
                          const SkString* mismatchPath,
                          const SkString* inputFilename,
                          bool useChecksumBasedFilenames,
                          bool useMultiPictureDraw) SK_OVERRIDE;

        /**
         *  Similar to TiledPictureRenderer, this will draw a PNG for each tile. However, the
         *  numbering (and actual tiles) will be different.
         */
        virtual bool render(SkBitmap** out) SK_OVERRIDE;

        virtual bool supportsTimingIndividualTiles() SK_OVERRIDE { return false; }

    private:
        int fXTilesPerLargeTile;
        int fYTilesPerLargeTile;

        int fLargeTileWidth;
        int fLargeTileHeight;

        virtual SkString getConfigNameInternal() SK_OVERRIDE;

        typedef TiledPictureRenderer INHERITED;
    };
} // sk_tools
#endif // CopyTilesRenderer_DEFINED
