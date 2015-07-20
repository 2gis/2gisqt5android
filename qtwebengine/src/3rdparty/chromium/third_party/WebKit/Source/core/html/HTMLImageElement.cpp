/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/html/HTMLImageElement.h"

#include "bindings/core/v8/ScriptEventListener.h"
#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/MediaTypeNames.h"
#include "core/css/MediaQueryMatcher.h"
#include "core/css/MediaValuesDynamic.h"
#include "core/css/parser/SizesAttributeParser.h"
#include "core/dom/Attribute.h"
#include "core/dom/NodeTraversal.h"
#include "core/fetch/ImageResource.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLSourceElement.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/parser/HTMLSrcsetParser.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/rendering/RenderImage.h"
#include "platform/ContentType.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

using namespace HTMLNames;

class HTMLImageElement::ViewportChangeListener final : public MediaQueryListListener {
public:
    static RefPtrWillBeRawPtr<ViewportChangeListener> create(HTMLImageElement* element)
    {
        return adoptRefWillBeNoop(new ViewportChangeListener(element));
    }

    virtual void notifyMediaQueryChanged() override
    {
        if (m_element)
            m_element->notifyViewportChanged();
    }

#if !ENABLE(OILPAN)
    void clearElement() { m_element = nullptr; }
#endif
    virtual void trace(Visitor* visitor) override
    {
        visitor->trace(m_element);
        MediaQueryListListener::trace(visitor);
    }
private:
    explicit ViewportChangeListener(HTMLImageElement* element) : m_element(element) { }
    RawPtrWillBeMember<HTMLImageElement> m_element;
};

HTMLImageElement::HTMLImageElement(Document& document, HTMLFormElement* form, bool createdByParser)
    : HTMLElement(imgTag, document)
    , m_imageLoader(HTMLImageLoader::create(this))
    , m_imageDevicePixelRatio(1.0f)
    , m_formWasSetByParser(false)
    , m_elementCreatedByParser(createdByParser)
    , m_intrinsicSizingViewportDependant(false)
{
    if (form && form->inDocument()) {
#if ENABLE(OILPAN)
        m_form = form;
#else
        m_form = form->createWeakPtr();
#endif
        m_formWasSetByParser = true;
        m_form->associate(*this);
        m_form->didAssociateByParser();
    }
}

PassRefPtrWillBeRawPtr<HTMLImageElement> HTMLImageElement::create(Document& document)
{
    return adoptRefWillBeNoop(new HTMLImageElement(document));
}

PassRefPtrWillBeRawPtr<HTMLImageElement> HTMLImageElement::create(Document& document, HTMLFormElement* form, bool createdByParser)
{
    return adoptRefWillBeNoop(new HTMLImageElement(document, form, createdByParser));
}

HTMLImageElement::~HTMLImageElement()
{
#if !ENABLE(OILPAN)
    if (m_listener) {
        document().mediaQueryMatcher().removeViewportListener(m_listener.get());
        m_listener->clearElement();
    }
    if (m_form)
        m_form->disassociate(*this);
#endif
}

void HTMLImageElement::trace(Visitor* visitor)
{
    visitor->trace(m_imageLoader);
    visitor->trace(m_listener);
    visitor->trace(m_form);
    HTMLElement::trace(visitor);
}

void HTMLImageElement::notifyViewportChanged()
{
    // Re-selecting the source URL in order to pick a more fitting resource
    // And update the image's intrinsic dimensions when the viewport changes.
    // Picking of a better fitting resource is UA dependant, not spec required.
    selectSourceURL(ImageLoader::UpdateSizeChanged);
}

PassRefPtrWillBeRawPtr<HTMLImageElement> HTMLImageElement::createForJSConstructor(Document& document, int width, int height)
{
    RefPtrWillBeRawPtr<HTMLImageElement> image = adoptRefWillBeNoop(new HTMLImageElement(document));
    if (width)
        image->setWidth(width);
    if (height)
        image->setHeight(height);
    image->m_elementCreatedByParser = false;
    return image.release();
}

bool HTMLImageElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == borderAttr || name == vspaceAttr || name == hspaceAttr || name == alignAttr || name == valignAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLImageElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    else if (name == borderAttr)
        applyBorderAttributeToStyle(value, style);
    else if (name == vspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
    } else if (name == hspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
    } else if (name == alignAttr)
        applyAlignmentAttributeToStyle(value, style);
    else if (name == valignAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyVerticalAlign, value);
    else
        HTMLElement::collectStyleForPresentationAttribute(name, value, style);
}

const AtomicString HTMLImageElement::imageSourceURL() const
{
    return m_bestFitImageURL.isNull() ? fastGetAttribute(srcAttr) : m_bestFitImageURL;
}

HTMLFormElement* HTMLImageElement::formOwner() const
{
    return m_form.get();
}

void HTMLImageElement::formRemovedFromTree(const Node& formRoot)
{
    ASSERT(m_form);
    if (NodeTraversal::highestAncestorOrSelf(*this) != formRoot)
        resetFormOwner();
}

void HTMLImageElement::resetFormOwner()
{
    m_formWasSetByParser = false;
    HTMLFormElement* nearestForm = findFormAncestor();
    if (m_form) {
        if (nearestForm == m_form.get())
            return;
        m_form->disassociate(*this);
    }
    if (nearestForm) {
#if ENABLE(OILPAN)
        m_form = nearestForm;
#else
        m_form = nearestForm->createWeakPtr();
#endif
        m_form->associate(*this);
    } else {
#if ENABLE(OILPAN)
        m_form = nullptr;
#else
        m_form = WeakPtr<HTMLFormElement>();
#endif
    }
}

void HTMLImageElement::setBestFitURLAndDPRFromImageCandidate(const ImageCandidate& candidate)
{
    m_bestFitImageURL = candidate.url();
    float candidateDensity = candidate.density();
    if (candidateDensity >= 0)
        m_imageDevicePixelRatio = 1.0 / candidateDensity;
    if (candidate.resourceWidth() > 0) {
        m_intrinsicSizingViewportDependant = true;
        UseCounter::count(document(), UseCounter::SrcsetWDescriptor);
    } else if (!candidate.srcOrigin()) {
        UseCounter::count(document(), UseCounter::SrcsetXDescriptor);
    }
    if (renderer() && renderer()->isImage())
        toRenderImage(renderer())->setImageDevicePixelRatio(m_imageDevicePixelRatio);
}

void HTMLImageElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == altAttr) {
        if (renderer() && renderer()->isImage())
            toRenderImage(renderer())->updateAltText();
    } else if (name == srcAttr || name == srcsetAttr || name == sizesAttr) {
        selectSourceURL(ImageLoader::UpdateIgnorePreviousError);
    } else if (name == usemapAttr) {
        setIsLink(!value.isNull());
    } else {
        HTMLElement::parseAttribute(name, value);
    }
}

const AtomicString& HTMLImageElement::altText() const
{
    // lets figure out the alt text.. magic stuff
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    const AtomicString& alt = fastGetAttribute(altAttr);
    if (!alt.isNull())
        return alt;
    // fall back to title attribute
    return fastGetAttribute(titleAttr);
}

static bool supportedImageType(const String& type)
{
    String trimmedType = ContentType(type).type();
    // An empty type attribute is implicitly supported.
    if (trimmedType.isEmpty())
        return true;
    return MIMETypeRegistry::isSupportedImagePrefixedMIMEType(trimmedType);
}

// http://picture.responsiveimages.org/#update-source-set
ImageCandidate HTMLImageElement::findBestFitImageFromPictureParent()
{
    ASSERT(isMainThread());
    Node* parent = parentNode();
    if (!parent || !isHTMLPictureElement(*parent))
        return ImageCandidate();
    for (Node* child = parent->firstChild(); child; child = child->nextSibling()) {
        if (child == this)
            return ImageCandidate();

        if (!isHTMLSourceElement(*child))
            continue;

        HTMLSourceElement* source = toHTMLSourceElement(child);
        if (!source->fastGetAttribute(srcAttr).isNull())
            UseCounter::countDeprecation(document(), UseCounter::PictureSourceSrc);
        String srcset = source->fastGetAttribute(srcsetAttr);
        if (srcset.isEmpty())
            continue;
        String type = source->fastGetAttribute(typeAttr);
        if (!type.isEmpty() && !supportedImageType(type))
            continue;

        if (!source->mediaQueryMatches())
            continue;

        String sizes = source->fastGetAttribute(sizesAttr);
        if (!sizes.isNull())
            UseCounter::count(document(), UseCounter::Sizes);
        SizesAttributeParser parser = SizesAttributeParser(MediaValuesDynamic::create(document()), sizes);
        float effectiveSize = parser.length();
        ImageCandidate candidate = bestFitSourceForSrcsetAttribute(document().devicePixelRatio(), effectiveSize, source->fastGetAttribute(srcsetAttr), &document());
        if (candidate.isEmpty())
            continue;
        return candidate;
    }
    return ImageCandidate();
}

RenderObject* HTMLImageElement::createRenderer(RenderStyle* style)
{
    if (style->hasContent())
        return RenderObject::createObject(this, style);

    RenderImage* image = new RenderImage(this);
    image->setImageResource(RenderImageResource::create());
    image->setImageDevicePixelRatio(m_imageDevicePixelRatio);
    return image;
}

bool HTMLImageElement::canStartSelection() const
{
    if (shadow())
        return HTMLElement::canStartSelection();

    return false;
}

void HTMLImageElement::attach(const AttachContext& context)
{
    HTMLElement::attach(context);

    if (renderer() && renderer()->isImage()) {
        RenderImage* renderImage = toRenderImage(renderer());
        RenderImageResource* renderImageResource = renderImage->imageResource();
        if (renderImageResource->hasImage())
            return;

        // If we have no image at all because we have no src attribute, set
        // image height and width for the alt text instead.
        if (!imageLoader().image() && !renderImageResource->cachedImage())
            renderImage->setImageSizeForAltText();
        else
            renderImageResource->setImageResource(imageLoader().image());

    }
}

Node::InsertionNotificationRequest HTMLImageElement::insertedInto(ContainerNode* insertionPoint)
{
    if (!m_formWasSetByParser || NodeTraversal::highestAncestorOrSelf(*insertionPoint) != NodeTraversal::highestAncestorOrSelf(*m_form.get()))
        resetFormOwner();
    if (m_listener)
        document().mediaQueryMatcher().addViewportListener(m_listener);

    bool imageWasModified = false;
    if (RuntimeEnabledFeatures::pictureEnabled() && document().isActive()) {
        ImageCandidate candidate = findBestFitImageFromPictureParent();
        if (!candidate.isEmpty()) {
            setBestFitURLAndDPRFromImageCandidate(candidate);
            imageWasModified = true;
        }
    }

    // If we have been inserted from a renderer-less document,
    // our loader may have not fetched the image, so do it now.
    if ((insertionPoint->inDocument() && !imageLoader().image()) || imageWasModified)
        imageLoader().updateFromElement(ImageLoader::UpdateNormal, m_elementCreatedByParser ? ImageLoader::ForceLoadImmediately : ImageLoader::LoadNormally);

    return HTMLElement::insertedInto(insertionPoint);
}

void HTMLImageElement::removedFrom(ContainerNode* insertionPoint)
{
    if (!m_form || NodeTraversal::highestAncestorOrSelf(*m_form.get()) != NodeTraversal::highestAncestorOrSelf(*this))
        resetFormOwner();
    if (m_listener)
        document().mediaQueryMatcher().removeViewportListener(m_listener);
    HTMLElement::removedFrom(insertionPoint);
}

int HTMLImageElement::width(bool ignorePendingStylesheets)
{
    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        bool ok;
        int width = getAttribute(widthAttr).toInt(&ok);
        if (ok)
            return width;

        // if the image is available, use its width
        if (imageLoader().image())
            return imageLoader().image()->imageSizeForRenderer(renderer(), 1.0f).width();
    }

    if (ignorePendingStylesheets)
        document().updateLayoutIgnorePendingStylesheets();
    else
        document().updateLayout();

    RenderBox* box = renderBox();
    return box ? adjustForAbsoluteZoom(box->contentBoxRect().pixelSnappedWidth(), box) : 0;
}

int HTMLImageElement::height(bool ignorePendingStylesheets)
{
    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        bool ok;
        int height = getAttribute(heightAttr).toInt(&ok);
        if (ok)
            return height;

        // if the image is available, use its height
        if (imageLoader().image())
            return imageLoader().image()->imageSizeForRenderer(renderer(), 1.0f).height();
    }

    if (ignorePendingStylesheets)
        document().updateLayoutIgnorePendingStylesheets();
    else
        document().updateLayout();

    RenderBox* box = renderBox();
    return box ? adjustForAbsoluteZoom(box->contentBoxRect().pixelSnappedHeight(), box) : 0;
}

int HTMLImageElement::naturalWidth() const
{
    if (!imageLoader().image())
        return 0;

    return imageLoader().image()->imageSizeForRenderer(renderer(), 1.0f, ImageResource::IntrinsicSize).width();
}

int HTMLImageElement::naturalHeight() const
{
    if (!imageLoader().image())
        return 0;

    return imageLoader().image()->imageSizeForRenderer(renderer(), 1.0f, ImageResource::IntrinsicSize).height();
}

const String& HTMLImageElement::currentSrc() const
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/edits.html#dom-img-currentsrc
    // The currentSrc IDL attribute must return the img element's current request's current URL.
    // Initially, the pending request turns into current request when it is either available or broken.
    // We use the image's dimensions as a proxy to it being in any of these states.
    if (!imageLoader().image() || !imageLoader().image()->image() || !imageLoader().image()->image()->width())
        return emptyAtom;

    return imageLoader().image()->url().string();
}

bool HTMLImageElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr
        || attribute.name() == lowsrcAttr
        || attribute.name() == longdescAttr
        || (attribute.name() == usemapAttr && attribute.value()[0] != '#')
        || HTMLElement::isURLAttribute(attribute);
}

bool HTMLImageElement::hasLegalLinkAttribute(const QualifiedName& name) const
{
    return name == srcAttr || HTMLElement::hasLegalLinkAttribute(name);
}

const QualifiedName& HTMLImageElement::subResourceAttributeName() const
{
    return srcAttr;
}

bool HTMLImageElement::draggable() const
{
    // Image elements are draggable by default.
    return !equalIgnoringCase(getAttribute(draggableAttr), "false");
}

void HTMLImageElement::setHeight(int value)
{
    setIntegralAttribute(heightAttr, value);
}

KURL HTMLImageElement::src() const
{
    return document().completeURL(getAttribute(srcAttr));
}

void HTMLImageElement::setSrc(const String& value)
{
    setAttribute(srcAttr, AtomicString(value));
}

void HTMLImageElement::setWidth(int value)
{
    setIntegralAttribute(widthAttr, value);
}

int HTMLImageElement::x() const
{
    document().updateLayoutIgnorePendingStylesheets();
    RenderObject* r = renderer();
    if (!r)
        return 0;

    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absPos = r->localToAbsolute();
    return absPos.x();
}

int HTMLImageElement::y() const
{
    document().updateLayoutIgnorePendingStylesheets();
    RenderObject* r = renderer();
    if (!r)
        return 0;

    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absPos = r->localToAbsolute();
    return absPos.y();
}

bool HTMLImageElement::complete() const
{
    return imageLoader().imageComplete();
}

void HTMLImageElement::didMoveToNewDocument(Document& oldDocument)
{
    imageLoader().elementDidMoveToNewDocument();
    HTMLElement::didMoveToNewDocument(oldDocument);
}

bool HTMLImageElement::isServerMap() const
{
    if (!fastHasAttribute(ismapAttr))
        return false;

    const AtomicString& usemap = fastGetAttribute(usemapAttr);

    // If the usemap attribute starts with '#', it refers to a map element in the document.
    if (usemap[0] == '#')
        return false;

    return document().completeURL(stripLeadingAndTrailingHTMLSpaces(usemap)).isEmpty();
}

Image* HTMLImageElement::imageContents()
{
    if (!imageLoader().imageComplete())
        return nullptr;

    return imageLoader().image()->image();
}

bool HTMLImageElement::isInteractiveContent() const
{
    return fastHasAttribute(usemapAttr);
}

PassRefPtr<Image> HTMLImageElement::getSourceImageForCanvas(SourceImageMode, SourceImageStatus* status) const
{
    if (!complete() || !cachedImage()) {
        *status = IncompleteSourceImageStatus;
        return nullptr;
    }

    if (cachedImage()->errorOccurred()) {
        *status = UndecodableSourceImageStatus;
        return nullptr;
    }

    RefPtr<Image> sourceImage = cachedImage()->imageForRenderer(renderer());

    // We need to synthesize a container size if a renderer is not available to provide one.
    if (!renderer() && sourceImage->usesContainerSize())
        sourceImage->setContainerSize(sourceImage->size());

    *status = NormalSourceImageStatus;
    return sourceImage->imageForDefaultFrame();
}

bool HTMLImageElement::wouldTaintOrigin(SecurityOrigin* destinationSecurityOrigin) const
{
    ImageResource* image = cachedImage();
    if (!image)
        return false;
    return !image->isAccessAllowed(destinationSecurityOrigin);
}

FloatSize HTMLImageElement::sourceSize() const
{
    ImageResource* image = cachedImage();
    if (!image)
        return FloatSize();
    LayoutSize size;
    size = image->imageSizeForRenderer(renderer(), 1.0f); // FIXME: Not sure about this.

    return size;
}

FloatSize HTMLImageElement::defaultDestinationSize() const
{
    ImageResource* image = cachedImage();
    if (!image)
        return FloatSize();
    LayoutSize size;
    size = image->imageSizeForRenderer(renderer(), 1.0f); // FIXME: Not sure about this.
    if (renderer() && renderer()->isRenderImage() && image->image() && !image->image()->hasRelativeWidth())
        size.scale(toRenderImage(renderer())->imageDevicePixelRatio());
    return size;
}

void HTMLImageElement::selectSourceURL(ImageLoader::UpdateFromElementBehavior behavior)
{
    if (!document().isActive())
        return;

    bool foundURL = false;
    if (RuntimeEnabledFeatures::pictureEnabled()) {
        ImageCandidate candidate = findBestFitImageFromPictureParent();
        if (!candidate.isEmpty()) {
            setBestFitURLAndDPRFromImageCandidate(candidate);
            foundURL = true;
        }
    }

    if (!foundURL) {
        float effectiveSize = 0;
        if (RuntimeEnabledFeatures::pictureSizesEnabled()) {
            String sizes = fastGetAttribute(sizesAttr);
            if (!sizes.isNull())
                UseCounter::count(document(), UseCounter::Sizes);
            SizesAttributeParser parser = SizesAttributeParser(MediaValuesDynamic::create(document()), sizes);
            effectiveSize = parser.length();
        }
        ImageCandidate candidate = bestFitSourceForImageAttributes(document().devicePixelRatio(), effectiveSize, fastGetAttribute(srcAttr), fastGetAttribute(srcsetAttr), &document());
        setBestFitURLAndDPRFromImageCandidate(candidate);
    }
    if (m_intrinsicSizingViewportDependant && !m_listener) {
        m_listener = ViewportChangeListener::create(this);
        document().mediaQueryMatcher().addViewportListener(m_listener);
    }
    imageLoader().updateFromElement(behavior);
}

const KURL& HTMLImageElement::sourceURL() const
{
    return cachedImage()->response().url();
}

}
