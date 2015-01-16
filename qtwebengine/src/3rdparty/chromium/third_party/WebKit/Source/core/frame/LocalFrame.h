/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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

#ifndef LocalFrame_h
#define LocalFrame_h

#include "core/frame/Frame.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/NavigationScheduler.h"
#include "core/page/FrameTree.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"

namespace WebCore {

    class Color;
    class Document;
    class DragImage;
    class Editor;
    class EventHandler;
    class FetchContext;
    class FloatSize;
    class FrameConsole;
    class FrameSelection;
    class FrameView;
    class InputMethodController;
    class IntPoint;
    class IntSize;
    class Node;
    class Range;
    class RenderView;
    class TreeScope;
    class ScriptController;
    class SpellChecker;
    class TreeScope;
    class VisiblePosition;

    class LocalFrame : public Frame, public WillBePersistentHeapSupplementable<LocalFrame>  {
    public:
        static PassRefPtr<LocalFrame> create(FrameLoaderClient*, FrameHost*, FrameOwner*);

        virtual bool isLocalFrame() const OVERRIDE { return true; }

        void init();
        void setView(PassRefPtr<FrameView>);
        void createView(const IntSize&, const Color&, bool,
            ScrollbarMode = ScrollbarAuto, bool horizontalLock = false,
            ScrollbarMode = ScrollbarAuto, bool verticalLock = false);

        virtual ~LocalFrame();

        virtual void willDetachFrameHost() OVERRIDE;
        virtual void detachFromFrameHost() OVERRIDE;

        virtual void disconnectOwnerElement() OVERRIDE;

        virtual void setDOMWindow(PassRefPtrWillBeRawPtr<LocalDOMWindow>) OVERRIDE;
        FrameView* view() const;
        Document* document() const;

        RenderView* contentRenderer() const; // Root of the render tree for the document contained in this frame.

        Editor& editor() const;
        EventHandler& eventHandler() const;
        FrameLoader& loader() const;
        NavigationScheduler& navigationScheduler() const;
        FrameSelection& selection() const;
        InputMethodController& inputMethodController() const;
        FetchContext& fetchContext() const { return loader().fetchContext(); }
        ScriptController& script();
        SpellChecker& spellChecker() const;
        FrameConsole& console() const;

        void didChangeVisibilityState();

        // FIXME: This method is only used by EventHandler to get the highest level
        // LocalFrame in this frame's in-process subtree. When user gesture tokens
        // are synchronized across processes this method should be removed.
        LocalFrame* localFrameRoot();

    // ======== All public functions below this point are candidates to move out of LocalFrame into another class. ========

        bool inScope(TreeScope*) const;

        void countObjectsNeedingLayout(unsigned& needsLayoutObjects, unsigned& totalObjects, bool& isPartial);

        // See GraphicsLayerClient.h for accepted flags.
        String layerTreeAsText(unsigned flags = 0) const;
        String trackedRepaintRectsAsText() const;

        void setPrinting(bool printing, const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkRatio);
        bool shouldUsePrintingLayout() const;
        FloatSize resizePageRectsKeepingRatio(const FloatSize& originalSize, const FloatSize& expectedSize);

        bool inViewSourceMode() const;
        void setInViewSourceMode(bool = true);

        void setPageZoomFactor(float factor);
        float pageZoomFactor() const { return m_pageZoomFactor; }
        void setTextZoomFactor(float factor);
        float textZoomFactor() const { return m_textZoomFactor; }
        void setPageAndTextZoomFactors(float pageZoomFactor, float textZoomFactor);

        void deviceOrPageScaleFactorChanged();
        double devicePixelRatio() const;

        void sendOrientationChangeEvent();

        String documentTypeString() const;

        PassOwnPtr<DragImage> nodeImage(Node&);
        PassOwnPtr<DragImage> dragImageForSelection();

        String selectedText() const;
        String selectedTextForClipboard() const;

        VisiblePosition visiblePositionForPoint(const IntPoint& framePoint);
        Document* documentAtPoint(const IntPoint& windowPoint);
        PassRefPtrWillBeRawPtr<Range> rangeForPoint(const IntPoint& framePoint);

        bool isURLAllowed(const KURL&) const;

    // ========

    private:
        LocalFrame(FrameLoaderClient*, FrameHost*, FrameOwner*);

        String localLayerTreeAsText(unsigned flags) const;

        mutable FrameLoader m_loader;
        mutable NavigationScheduler m_navigationScheduler;

        RefPtr<FrameView> m_view;

        OwnPtr<ScriptController> m_script;
        const OwnPtrWillBePersistent<Editor> m_editor;
        const OwnPtr<SpellChecker> m_spellChecker;
        const OwnPtrWillBePersistent<FrameSelection> m_selection;
        const OwnPtrWillBePersistent<EventHandler> m_eventHandler;
        const OwnPtr<FrameConsole> m_console;
        OwnPtr<InputMethodController> m_inputMethodController;

        float m_pageZoomFactor;
        float m_textZoomFactor;

        bool m_inViewSourceMode;
    };

    inline void LocalFrame::init()
    {
        m_loader.init();
    }

    inline FrameLoader& LocalFrame::loader() const
    {
        return m_loader;
    }

    inline NavigationScheduler& LocalFrame::navigationScheduler() const
    {
        return m_navigationScheduler;
    }

    inline FrameView* LocalFrame::view() const
    {
        return m_view.get();
    }

    inline ScriptController& LocalFrame::script()
    {
        return *m_script;
    }

    inline FrameSelection& LocalFrame::selection() const
    {
        return *m_selection;
    }

    inline Editor& LocalFrame::editor() const
    {
        return *m_editor;
    }

    inline SpellChecker& LocalFrame::spellChecker() const
    {
        return *m_spellChecker;
    }

    inline FrameConsole& LocalFrame::console() const
    {
        return *m_console;
    }

    inline InputMethodController& LocalFrame::inputMethodController() const
    {
        return *m_inputMethodController;
    }

    inline bool LocalFrame::inViewSourceMode() const
    {
        return m_inViewSourceMode;
    }

    inline void LocalFrame::setInViewSourceMode(bool mode)
    {
        m_inViewSourceMode = mode;
    }

    inline EventHandler& LocalFrame::eventHandler() const
    {
        ASSERT(m_eventHandler);
        return *m_eventHandler;
    }

    DEFINE_TYPE_CASTS(LocalFrame, Frame, localFrame, localFrame->isLocalFrame(), localFrame.isLocalFrame());

} // namespace WebCore

// During refactoring, there are some places where we need to do type conversions that
// will not be needed once all instances of LocalFrame and RemoteFrame are sorted out.
// At that time this #define will be removed and all the uses of it will need to be corrected.
#define toLocalFrameTemporary toLocalFrame

#endif // LocalFrame_h
