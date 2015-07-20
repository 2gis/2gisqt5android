/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrFragmentProcessor_DEFINED
#define GrFragmentProcessor_DEFINED

#include "GrProcessor.h"

class GrCoordTransform;

/** Provides custom fragment shader code. Fragment processors receive an input color (vec4f) and
    produce an output color. They may reference textures and uniforms. They may use
    GrCoordTransforms to receive a transformation of the local coordinates that map from local space
    to the fragment being processed.
 */
class GrFragmentProcessor : public GrProcessor {
public:
    GrFragmentProcessor()
        : INHERITED()
        , fWillReadDstColor(false)
        , fWillUseInputColor(true) {}

    virtual const GrBackendFragmentProcessorFactory& getFactory() const = 0;

    int numTransforms() const { return fCoordTransforms.count(); }

    /** Returns the coordinate transformation at index. index must be valid according to
        numTransforms(). */
    const GrCoordTransform& coordTransform(int index) const { return *fCoordTransforms[index]; }

    /** Will this prceossor read the destination pixel value? */
    bool willReadDstColor() const { return fWillReadDstColor; }

    /** Will this prceossor read the source color value? */
    bool willUseInputColor() const { return fWillUseInputColor; }

    /** Returns true if this and other prceossor conservatively draw identically. It can only return
        true when the two prceossor are of the same subclass (i.e. they return the same object from
        from getFactory()).

        A return value of true from isEqual() should not be used to test whether the prceossor would
        generate the same shader code. To test for identical code generation use the prceossor' keys
        computed by the GrBackendProcessorFactory. */
    bool isEqual(const GrFragmentProcessor& that) const {
        if (&this->getFactory() != &that.getFactory() ||
            !this->hasSameTransforms(that) ||
            !this->hasSameTextureAccesses(that)) {
            return false;
        }
        return this->onIsEqual(that);
    }

protected:
    /**
     * Fragment Processor subclasses call this from their constructor to register coordinate
     * transformations. Coord transforms provide a mechanism for a processor to receive coordinates
     * in their FS code. The matrix expresses a transformation from local space. For a given
     * fragment the matrix will be applied to the local coordinate that maps to the fragment.
     *
     * When the transformation has perspective, the transformed coordinates will have
     * 3 components. Otherwise they'll have 2. 
     *
     * This must only be called from the constructor because GrProcessors are immutable. The
     * processor subclass manages the lifetime of the transformations (this function only stores a
     * pointer). The GrCoordTransform is typically a member field of the GrProcessor subclass. 
     *
     * A processor subclass that has multiple methods of construction should always add its coord
     * transforms in a consistent order. The non-virtual implementation of isEqual() automatically
     * compares transforms and will assume they line up across the two processor instances.
     */
    void addCoordTransform(const GrCoordTransform*);

    /**
     * If the prceossor subclass will read the destination pixel value then it must call this
     * function from its constructor. Otherwise, when its generated backend-specific prceossor class
     * attempts to generate code that reads the destination pixel it will fail.
     */
    void setWillReadDstColor() { fWillReadDstColor = true; }

    /**
     * If the prceossor will generate a result that does not depend on the input color value then it
     * must call this function from its constructor. Otherwise, when its generated backend-specific
     * code might fail during variable binding due to unused variables.
     */
    void setWillNotUseInputColor() { fWillUseInputColor = false; }

private:
    /**
     * Subclass implements this to support isEqual(). It will only be called if it is known that
     * the two processors are of the same subclass (i.e. they return the same object from
     * getFactory()). The processor subclass should not compare its coord transforms as that will
     * be performed automatically in the non-virtual isEqual().
     */
    virtual bool onIsEqual(const GrFragmentProcessor&) const = 0;

    bool hasSameTransforms(const GrFragmentProcessor&) const;

    SkSTArray<4, const GrCoordTransform*, true>  fCoordTransforms;
    bool                                         fWillReadDstColor;
    bool                                         fWillUseInputColor;

    typedef GrProcessor INHERITED;
};

#endif
