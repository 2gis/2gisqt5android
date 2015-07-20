/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef InlineTextBox_h
#define InlineTextBox_h

#include "core/rendering/InlineBox.h"
#include "core/rendering/RenderText.h" // so textRenderer() can be inline
#include "platform/fonts/TextBlob.h"
#include "platform/text/TextRun.h"
#include "wtf/Forward.h"

namespace blink {

struct CompositionUnderline;
class DocumentMarker;
class GraphicsContext;

const unsigned short cNoTruncation = USHRT_MAX;
const unsigned short cFullTruncation = USHRT_MAX - 1;

class InlineTextBox : public InlineBox {
public:
    InlineTextBox(RenderObject& obj, int start, unsigned short length)
        : InlineBox(obj)
        , m_prevTextBox(0)
        , m_nextTextBox(0)
        , m_start(start)
        , m_len(length)
        , m_truncation(cNoTruncation)
    {
        setIsText(true);
    }

    RenderText& renderer() const { return toRenderText(InlineBox::renderer()); }

    virtual void destroy() override final;

    InlineTextBox* prevTextBox() const { return m_prevTextBox; }
    InlineTextBox* nextTextBox() const { return m_nextTextBox; }
    void setNextTextBox(InlineTextBox* n) { m_nextTextBox = n; }
    void setPreviousTextBox(InlineTextBox* p) { m_prevTextBox = p; }

    // FIXME: These accessors should ASSERT(!isDirty()). See https://bugs.webkit.org/show_bug.cgi?id=97264
    unsigned start() const { return m_start; }
    unsigned end() const { return m_len ? m_start + m_len - 1 : m_start; }
    unsigned len() const { return m_len; }

    void offsetRun(int delta);

    unsigned short truncation() { return m_truncation; }

    virtual void markDirty() override final;

    using InlineBox::hasHyphen;
    using InlineBox::setHasHyphen;
    using InlineBox::canHaveLeadingExpansion;
    using InlineBox::setCanHaveLeadingExpansion;

    static inline bool compareByStart(const InlineTextBox* first, const InlineTextBox* second) { return first->start() < second->start(); }

    virtual int baselinePosition(FontBaseline) const override final;
    virtual LayoutUnit lineHeight() const override final;

    bool getEmphasisMarkPosition(RenderStyle*, TextEmphasisPosition&) const;

    LayoutRect logicalOverflowRect() const;
    void setLogicalOverflowRect(const LayoutRect&);
    LayoutUnit logicalTopVisualOverflow() const { return logicalOverflowRect().y(); }
    LayoutUnit logicalBottomVisualOverflow() const { return logicalOverflowRect().maxY(); }

    // charactersWithHyphen, if provided, must not be destroyed before the TextRun.
    TextRun constructTextRun(RenderStyle*, const Font&, StringBuilder* charactersWithHyphen = 0) const;
    TextRun constructTextRun(RenderStyle*, const Font&, StringView, int maximumLength, StringBuilder* charactersWithHyphen = 0) const;

#ifndef NDEBUG
    virtual void showBox(int = 0) const override;
    virtual const char* boxName() const override;
#endif

    enum RotationDirection { Counterclockwise, Clockwise };
    static AffineTransform rotation(const FloatRect& boxRect, RotationDirection);

public:
    TextRun constructTextRunForInspector(RenderStyle*, const Font&) const;
    virtual FloatRect calculateBoundaries() const override { return FloatRect(x(), y(), width(), height()); }

    virtual LayoutRect localSelectionRect(int startPos, int endPos);
    bool isSelected(int startPos, int endPos) const;
    void selectionStartEnd(int& sPos, int& ePos) const;

    // These functions both paint markers and update the DocumentMarker's renderedRect.
    virtual void paintDocumentMarker(GraphicsContext*, const FloatPoint& boxOrigin, DocumentMarker*, RenderStyle*, const Font&, bool grammar);
    virtual void paintTextMatchMarker(GraphicsContext*, const FloatPoint& boxOrigin, DocumentMarker*, RenderStyle*, const Font&);

protected:
    virtual void paint(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) override;
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom) override;

private:
    virtual void deleteLine() override final;
    virtual void extractLine() override final;
    virtual void attachLine() override final;

public:
    virtual RenderObject::SelectionState selectionState() const override final;

private:
    virtual void clearTruncation() override final { m_truncation = cNoTruncation; }
    virtual float placeEllipsisBox(bool flowIsLTR, float visibleLeftEdge, float visibleRightEdge, float ellipsisWidth, float &truncatedWidth, bool& foundBox) override final;

public:
    virtual bool isLineBreak() const override final;

    void setExpansion(int newExpansion)
    {
        m_logicalWidth -= expansion();
        InlineBox::setExpansion(newExpansion);
        m_logicalWidth += newExpansion;
    }

private:
    virtual bool isInlineTextBox() const override final { return true; }

public:
    virtual int caretMinOffset() const override final;
    virtual int caretMaxOffset() const override final;

    float textPos() const; // returns the x position relative to the left start of the text line.

public:
    virtual int offsetForPosition(float x, bool includePartialGlyphs = true) const;
    virtual float positionForOffset(int offset) const;

    bool containsCaretOffset(int offset) const; // false for offset after line break

    // Fills a vector with the pixel width of each character.
    void characterWidths(Vector<float>&) const;

private:
    InlineTextBox* m_prevTextBox; // The previous box that also uses our RenderObject
    InlineTextBox* m_nextTextBox; // The next box that also uses our RenderObject

    int m_start;
    unsigned short m_len;

    unsigned short m_truncation; // Where to truncate when text overflow is applied.  We use special constants to
                      // denote no truncation (the whole run paints) and full truncation (nothing paints at all).

private:
    TextRun::ExpansionBehavior expansionBehavior() const
    {
        return (canHaveLeadingExpansion() ? TextRun::AllowLeadingExpansion : TextRun::ForbidLeadingExpansion)
            | (expansion() && nextLeafChild() ? TextRun::AllowTrailingExpansion : TextRun::ForbidTrailingExpansion);
    }
};

DEFINE_INLINE_BOX_TYPE_CASTS(InlineTextBox);

void alignSelectionRectToDevicePixels(FloatRect&);

inline AffineTransform InlineTextBox::rotation(const FloatRect& boxRect, RotationDirection rotationDirection)
{
    return rotationDirection == Clockwise ? AffineTransform(0, 1, -1, 0, boxRect.x() + boxRect.maxY(), boxRect.maxY() - boxRect.x())
        : AffineTransform(0, -1, 1, 0, boxRect.x() - boxRect.maxY(), boxRect.x() + boxRect.maxY());
}

} // namespace blink

#endif // InlineTextBox_h
