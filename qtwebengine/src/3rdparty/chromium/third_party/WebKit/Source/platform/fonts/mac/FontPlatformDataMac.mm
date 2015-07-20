/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2010 Google Inc. All rights reserved.
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
 *
 */

#import "config.h"
#import "platform/fonts/FontPlatformData.h"

#import <AppKit/NSFont.h>
#import <AvailabilityMacros.h>
#import <wtf/text/WTFString.h>

#include "platform/LayoutTestSupport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/Font.h"
#import "platform/fonts/shaping/HarfBuzzFace.h"
#include "third_party/skia/include/ports/SkTypeface_mac.h"



namespace blink {

unsigned FontPlatformData::hash() const
{
    ASSERT(m_font || !m_cgFont);
    uintptr_t hashCodes[3] = { (uintptr_t)m_font, m_widthVariant, static_cast<uintptr_t>(m_isHashTableDeletedValue << 3 | m_orientation << 2 | m_syntheticBold << 1 | m_syntheticItalic) };
    return StringHasher::hashMemory<sizeof(hashCodes)>(hashCodes);
}

void FontPlatformData::setupPaint(SkPaint* paint, GraphicsContext*, const Font* font) const
{
    bool shouldSmoothFonts = true;
    bool shouldAntialias = true;

    if (font) {
        switch (font->fontDescription().fontSmoothing()) {
        case Antialiased:
            shouldSmoothFonts = false;
            break;
        case SubpixelAntialiased:
            break;
        case NoSmoothing:
            shouldAntialias = false;
            shouldSmoothFonts = false;
            break;
        case AutoSmoothing:
            // For the AutoSmooth case, don't do anything! Keep the default settings.
            break;
        }
    }

    if (LayoutTestSupport::isRunningLayoutTest()) {
        shouldSmoothFonts = false;
        shouldAntialias = shouldAntialias && LayoutTestSupport::isFontAntialiasingEnabledForTest();
    }

    bool useSubpixelText = RuntimeEnabledFeatures::subpixelFontScalingEnabled();

    paint->setAntiAlias(shouldAntialias);
    paint->setEmbeddedBitmapText(false);
    const float ts = m_textSize >= 0 ? m_textSize : 12;
    paint->setTextSize(SkFloatToScalar(ts));
    paint->setTypeface(typeface());
    paint->setFakeBoldText(m_syntheticBold);
    paint->setTextSkewX(m_syntheticItalic ? -SK_Scalar1 / 4 : 0);
    paint->setLCDRenderText(shouldSmoothFonts);
    paint->setSubpixelText(useSubpixelText);

    // When rendering using CoreGraphics, disable hinting when webkit-font-smoothing:antialiased or
    // text-rendering:geometricPrecision is used.
    // See crbug.com/152304
    if (font && (font->fontDescription().fontSmoothing() == Antialiased || font->fontDescription().textRendering() == GeometricPrecision))
        paint->setHinting(SkPaint::kNo_Hinting);
}

// These CoreText Text Spacing feature selectors are not defined in CoreText.
enum TextSpacingCTFeatureSelector { TextSpacingProportional, TextSpacingFullWidth, TextSpacingHalfWidth, TextSpacingThirdWidth, TextSpacingQuarterWidth };

FontPlatformData::FontPlatformData(NSFont *nsFont, float size, bool syntheticBold, bool syntheticItalic, FontOrientation orientation, FontWidthVariant widthVariant)
    : m_textSize(size)
    , m_syntheticBold(syntheticBold)
    , m_syntheticItalic(syntheticItalic)
    , m_orientation(orientation)
    , m_isColorBitmapFont(false)
    , m_isCompositeFontReference(false)
    , m_widthVariant(widthVariant)
    , m_font(nsFont)
    , m_isHashTableDeletedValue(false)
{
    ASSERT_ARG(nsFont, nsFont);

    CGFontRef cgFont = 0;
    loadFont(nsFont, size, m_font, cgFont);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
    // FIXME: Chromium: The following code isn't correct for the Chromium port since the sandbox might
    // have blocked font loading, in which case we'll only have the real loaded font file after the call to loadFont().
    {
        CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(toCTFontRef(m_font));
        m_isColorBitmapFont = traits & kCTFontColorGlyphsTrait;
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
        m_isCompositeFontReference = traits & kCTFontCompositeTrait;
#endif
    }
#endif

    if (m_font)
        CFRetain(m_font);

    m_cgFont.adoptCF(cgFont);
}

void FontPlatformData::platformDataInit(const FontPlatformData& f)
{
    m_font = f.m_font ? [f.m_font retain] : f.m_font;

    m_cgFont = f.m_cgFont;
    m_CTFont = f.m_CTFont;

    m_inMemoryFont = f.m_inMemoryFont;
    m_harfBuzzFace = f.m_harfBuzzFace;
    m_typeface = f.m_typeface;
}

const FontPlatformData& FontPlatformData::platformDataAssign(const FontPlatformData& f)
{
    m_cgFont = f.m_cgFont;
    if (m_font == f.m_font)
        return *this;
    if (f.m_font)
        CFRetain(f.m_font);
    if (m_font)
        CFRelease(m_font);
    m_font = f.m_font;
    m_CTFont = f.m_CTFont;

    m_inMemoryFont = f.m_inMemoryFont;
    m_harfBuzzFace = f.m_harfBuzzFace;
    m_typeface = f.m_typeface;

    return *this;
}


void FontPlatformData::setFont(NSFont *font)
{
    ASSERT_ARG(font, font);

    if (m_font == font)
        return;

    CFRetain(font);
    if (m_font)
        CFRelease(m_font);
    m_font = font;
    m_textSize = [font pointSize];

    CGFontRef cgFont = 0;
    NSFont* loadedFont = 0;
    loadFont(m_font, m_textSize, loadedFont, cgFont);

    // If loadFont replaced m_font with a fallback font, then release the
    // previous font to counter the retain above. Then retain the new font.
    if (loadedFont != m_font) {
        CFRelease(m_font);
        CFRetain(loadedFont);
        m_font = loadedFont;
    }

    m_cgFont.adoptCF(cgFont);
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
    {
        CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(toCTFontRef(m_font));
        m_isColorBitmapFont = traits & kCTFontColorGlyphsTrait;
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
        m_isCompositeFontReference = traits & kCTFontCompositeTrait;
#endif
    }
#endif
    m_CTFont = nullptr;
}

bool FontPlatformData::roundsGlyphAdvances() const
{
    return [m_font renderingMode] == NSFontAntialiasedIntegerAdvancementsRenderingMode;
}

bool FontPlatformData::allowsLigatures() const
{
    return ![[m_font coveredCharacterSet] characterIsMember:'a'];
}

inline int mapFontWidthVariantToCTFeatureSelector(FontWidthVariant variant)
{
    switch(variant) {
    case RegularWidth:
        return TextSpacingProportional;

    case HalfWidth:
        return TextSpacingHalfWidth;

    case ThirdWidth:
        return TextSpacingThirdWidth;

    case QuarterWidth:
        return TextSpacingQuarterWidth;
    }

    ASSERT_NOT_REACHED();
    return TextSpacingProportional;
}

static CFDictionaryRef createFeatureSettingDictionary(int featureTypeIdentifier, int featureSelectorIdentifier)
{
    RetainPtr<CFNumberRef> featureTypeIdentifierNumber(AdoptCF, CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureTypeIdentifier));
    RetainPtr<CFNumberRef> featureSelectorIdentifierNumber(AdoptCF, CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureSelectorIdentifier));

    const void* settingKeys[] = { kCTFontFeatureTypeIdentifierKey, kCTFontFeatureSelectorIdentifierKey };
    const void* settingValues[] = { featureTypeIdentifierNumber.get(), featureSelectorIdentifierNumber.get() };

    return CFDictionaryCreate(kCFAllocatorDefault, settingKeys, settingValues, WTF_ARRAY_LENGTH(settingKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

static CTFontDescriptorRef cascadeToLastResortFontDescriptor()
{
    static CTFontDescriptorRef descriptor;
    if (descriptor)
        return descriptor;

    const void* keys[] = { kCTFontCascadeListAttribute };
    const void* descriptors[] = { CTFontDescriptorCreateWithNameAndSize(CFSTR("LastResort"), 0) };
    const void* values[] = { CFArrayCreate(kCFAllocatorDefault, descriptors, WTF_ARRAY_LENGTH(descriptors), &kCFTypeArrayCallBacks) };
    RetainPtr<CFDictionaryRef> attributes(AdoptCF, CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    descriptor = CTFontDescriptorCreateWithAttributes(attributes.get());

    return descriptor;
}

static CTFontDescriptorRef cascadeToLastResortAndDisableSwashesFontDescriptor()
{
    static CTFontDescriptorRef descriptor;
    if (descriptor)
        return descriptor;

    RetainPtr<CFDictionaryRef> lineInitialSwashesOffSetting(AdoptCF, createFeatureSettingDictionary(kSmartSwashType, kLineInitialSwashesOffSelector));
    RetainPtr<CFDictionaryRef> lineFinalSwashesOffSetting(AdoptCF, createFeatureSettingDictionary(kSmartSwashType, kLineFinalSwashesOffSelector));

    const void* settingDictionaries[] = { lineInitialSwashesOffSetting.get(), lineFinalSwashesOffSetting.get() };
    RetainPtr<CFArrayRef> featureSettings(AdoptCF, CFArrayCreate(kCFAllocatorDefault, settingDictionaries, WTF_ARRAY_LENGTH(settingDictionaries), &kCFTypeArrayCallBacks));

    const void* keys[] = { kCTFontFeatureSettingsAttribute };
    const void* values[] = { featureSettings.get() };
    RetainPtr<CFDictionaryRef> attributes(AdoptCF, CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    descriptor = CTFontDescriptorCreateCopyWithAttributes(cascadeToLastResortFontDescriptor(), attributes.get());

    return descriptor;
}

CTFontRef FontPlatformData::ctFont() const
{
    if (m_CTFont)
        return m_CTFont.get();

    if (m_inMemoryFont) {
        m_CTFont.adoptCF(CTFontCreateWithGraphicsFont(m_inMemoryFont->cgFont(), m_textSize, 0, cascadeToLastResortFontDescriptor()));
        return m_CTFont.get();
    }

    m_CTFont = toCTFontRef(m_font);
    if (m_CTFont) {
        CTFontDescriptorRef fontDescriptor;
        RetainPtr<CFStringRef> postScriptName(AdoptCF, CTFontCopyPostScriptName(m_CTFont.get()));
        // Hoefler Text Italic has line-initial and -final swashes enabled by default, so disable them.
        if (CFEqual(postScriptName.get(), CFSTR("HoeflerText-Italic")) || CFEqual(postScriptName.get(), CFSTR("HoeflerText-BlackItalic")))
            fontDescriptor = cascadeToLastResortAndDisableSwashesFontDescriptor();
        else
            fontDescriptor = cascadeToLastResortFontDescriptor();
        m_CTFont.adoptCF(CTFontCreateCopyWithAttributes(m_CTFont.get(), m_textSize, 0, fontDescriptor));
    } else
        m_CTFont.adoptCF(CTFontCreateWithGraphicsFont(m_cgFont.get(), m_textSize, 0, cascadeToLastResortFontDescriptor()));

    if (m_widthVariant != RegularWidth) {
        int featureTypeValue = kTextSpacingType;
        int featureSelectorValue = mapFontWidthVariantToCTFeatureSelector(m_widthVariant);
        RetainPtr<CTFontDescriptorRef> sourceDescriptor(AdoptCF, CTFontCopyFontDescriptor(m_CTFont.get()));
        RetainPtr<CFNumberRef> featureType(AdoptCF, CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureTypeValue));
        RetainPtr<CFNumberRef> featureSelector(AdoptCF, CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureSelectorValue));
        RetainPtr<CTFontDescriptorRef> newDescriptor(AdoptCF, CTFontDescriptorCreateCopyWithFeature(sourceDescriptor.get(), featureType.get(), featureSelector.get()));
        RetainPtr<CTFontRef> newFont(AdoptCF, CTFontCreateWithFontDescriptor(newDescriptor.get(), m_textSize, 0));

        if (newFont)
            m_CTFont = newFont;
    }

    return m_CTFont.get();
}

} // namespace blink
