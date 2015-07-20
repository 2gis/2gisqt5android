/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/inspector/InspectorFrontendHost.h"

#include "bindings/core/v8/ScriptFunctionCall.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/clipboard/Pasteboard.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/events/EventTarget.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorFrontendClient.h"
#include "core/loader/FrameLoader.h"
#include "core/page/ContextMenuController.h"
#include "core/page/ContextMenuProvider.h"
#include "core/page/Page.h"
#include "core/rendering/RenderTheme.h"
#include "platform/ContextMenu.h"
#include "platform/ContextMenuItem.h"
#include "platform/SharedBuffer.h"
#include "platform/UserGestureIndicator.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"

namespace blink {

class FrontendMenuProvider final : public ContextMenuProvider {
public:
    static PassRefPtrWillBeRawPtr<FrontendMenuProvider> create(InspectorFrontendHost* frontendHost, ScriptValue frontendApiObject, const Vector<ContextMenuItem>& items)
    {
        return adoptRefWillBeNoop(new FrontendMenuProvider(frontendHost, frontendApiObject, items));
    }

    virtual ~FrontendMenuProvider()
    {
        // Verify that this menu provider has been detached.
        ASSERT(!m_frontendHost);
    }

    virtual void trace(Visitor* visitor) override
    {
        visitor->trace(m_frontendHost);
        ContextMenuProvider::trace(visitor);
    }

    void disconnect()
    {
        m_frontendApiObject = ScriptValue();
        m_frontendHost = nullptr;
    }

    virtual void contextMenuCleared() override
    {
        if (m_frontendHost) {
            ScriptFunctionCall function(m_frontendApiObject, "contextMenuCleared");
            function.call();

            m_frontendHost->clearMenuProvider();
            m_frontendHost = nullptr;
        }
        m_items.clear();
    }

    virtual void populateContextMenu(ContextMenu* menu) override
    {
        for (size_t i = 0; i < m_items.size(); ++i)
            menu->appendItem(m_items[i]);
    }

    virtual void contextMenuItemSelected(const ContextMenuItem* item) override
    {
        if (!m_frontendHost)
            return;

        UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
        int itemNumber = item->action() - ContextMenuItemBaseCustomTag;

        ScriptFunctionCall function(m_frontendApiObject, "contextMenuItemSelected");
        function.appendArgument(itemNumber);
        function.call();
    }

private:
    FrontendMenuProvider(InspectorFrontendHost* frontendHost, ScriptValue frontendApiObject, const Vector<ContextMenuItem>& items)
        : m_frontendHost(frontendHost)
        , m_frontendApiObject(frontendApiObject)
        , m_items(items)
    {
    }

    RawPtrWillBeMember<InspectorFrontendHost> m_frontendHost;
    ScriptValue m_frontendApiObject;

    // FIXME: Oilpan: remove when http://crbug.com/424962 Blink GC plugin
    // changes have been deployed. ContextMenuItem triggers looping behavior.
    GC_PLUGIN_IGNORE("crbug.com/424962")
    Vector<ContextMenuItem> m_items;
};

InspectorFrontendHost::InspectorFrontendHost(InspectorFrontendClient* client, Page* frontendPage)
    : m_client(client)
    , m_frontendPage(frontendPage)
    , m_menuProvider(nullptr)
{
}

InspectorFrontendHost::~InspectorFrontendHost()
{
    ASSERT(!m_client);
}

void InspectorFrontendHost::trace(Visitor* visitor)
{
    visitor->trace(m_frontendPage);
    visitor->trace(m_menuProvider);
}

void InspectorFrontendHost::disconnectClient()
{
    m_client = 0;
    if (m_menuProvider) {
        m_menuProvider->disconnect();
        m_menuProvider = nullptr;
    }
    m_frontendPage = nullptr;
}

void InspectorFrontendHost::setZoomFactor(float zoom)
{
    if (!m_frontendPage)
        return;
    if (LocalFrame* frame = m_frontendPage->deprecatedLocalMainFrame())
        frame->setPageAndTextZoomFactors(zoom, 1);
}

float InspectorFrontendHost::zoomFactor()
{
    if (!m_frontendPage)
        return 1;
    if (LocalFrame* frame = m_frontendPage->deprecatedLocalMainFrame())
        return frame->pageZoomFactor();
    return 1;
}

void InspectorFrontendHost::setInjectedScriptForOrigin(const String& origin, const String& script)
{
    if (!m_frontendPage)
        return;
    m_frontendPage->inspectorController().setInjectedScriptForOrigin(origin, script);
}

void InspectorFrontendHost::copyText(const String& text)
{
    Pasteboard::generalPasteboard()->writePlainText(text, Pasteboard::CannotSmartReplace);
}

static String escapeUnicodeNonCharacters(const String& str)
{
    const UChar nonChar = 0xD800;

    unsigned i = 0;
    while (i < str.length() && str[i] < nonChar)
        ++i;
    if (i == str.length())
        return str;

    StringBuilder dst;
    dst.append(str, 0, i);
    for (; i < str.length(); ++i) {
        UChar c = str[i];
        if (c >= nonChar) {
            unsigned symbol = static_cast<unsigned>(c);
            String symbolCode = String::format("\\u%04X", symbol);
            dst.append(symbolCode);
        } else {
            dst.append(c);
        }
    }
    return dst.toString();
}

void InspectorFrontendHost::sendMessageToBackend(const String& message)
{
    if (m_client)
        m_client->sendMessageToBackend(escapeUnicodeNonCharacters(message));
}

void InspectorFrontendHost::sendMessageToEmbedder(const String& message)
{
    if (m_client)
        m_client->sendMessageToEmbedder(escapeUnicodeNonCharacters(message));
}

void InspectorFrontendHost::showContextMenu(Page* page, float x, float y, const Vector<ContextMenuItem>& items)
{
    ASSERT(m_frontendPage);
    ScriptState* frontendScriptState = ScriptState::forMainWorld(m_frontendPage->deprecatedLocalMainFrame());
    ScriptValue frontendApiObject = frontendScriptState->getFromGlobalObject("InspectorFrontendAPI");
    ASSERT(frontendApiObject.isObject());

    RefPtrWillBeRawPtr<FrontendMenuProvider> menuProvider = FrontendMenuProvider::create(this, frontendApiObject, items);
    m_menuProvider = menuProvider.get();
    float zoom = page->deprecatedLocalMainFrame()->pageZoomFactor();
    page->inspectorController().showContextMenu(x * zoom, y * zoom, menuProvider);
}

void InspectorFrontendHost::showContextMenu(Event* event, const Vector<ContextMenuItem>& items)
{
    if (!event)
        return;

    ASSERT(m_frontendPage);
    ScriptState* frontendScriptState = ScriptState::forMainWorld(m_frontendPage->deprecatedLocalMainFrame());
    ScriptValue frontendApiObject = frontendScriptState->getFromGlobalObject("InspectorFrontendAPI");
    ASSERT(frontendApiObject.isObject());

    Page* targetPage = m_frontendPage;
    if (event->target() && event->target()->executionContext() && event->target()->executionContext()->executingWindow()) {
        LocalDOMWindow* window = event->target()->executionContext()->executingWindow();
        if (window->document() && window->document()->page())
            targetPage = window->document()->page();
    }

    RefPtrWillBeRawPtr<FrontendMenuProvider> menuProvider = FrontendMenuProvider::create(this, frontendApiObject, items);
    targetPage->contextMenuController().showContextMenu(event, menuProvider);
    m_menuProvider = menuProvider.get();
}

String InspectorFrontendHost::getSelectionBackgroundColor()
{
    return RenderTheme::theme().activeSelectionBackgroundColor().serialized();
}

String InspectorFrontendHost::getSelectionForegroundColor()
{
    return RenderTheme::theme().activeSelectionForegroundColor().serialized();
}

bool InspectorFrontendHost::isUnderTest()
{
    return m_client && m_client->isUnderTest();
}

bool InspectorFrontendHost::isHostedMode()
{
    return false;
}

} // namespace blink
