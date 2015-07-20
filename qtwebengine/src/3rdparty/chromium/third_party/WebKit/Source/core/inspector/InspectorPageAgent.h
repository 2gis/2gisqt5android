/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorPageAgent_h
#define InspectorPageAgent_h


#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorResourceContentLoader.h"
#include "wtf/HashMap.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Resource;
class Document;
class DocumentLoader;
class LocalFrame;
class GraphicsContext;
class GraphicsLayer;
class InjectedScriptManager;
class InspectorClient;
class InspectorOverlay;
class InspectorResourceContentLoader;
class KURL;
class LayoutRect;
class Page;
class RenderObject;
class SharedBuffer;
class StyleResolver;
class TextResourceDecoder;

typedef String ErrorString;

class InspectorPageAgent final : public InspectorBaseAgent<InspectorPageAgent>, public InspectorBackendDispatcher::PageCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorPageAgent);
public:
    enum ResourceType {
        DocumentResource,
        StylesheetResource,
        ImageResource,
        FontResource,
        MediaResource,
        ScriptResource,
        TextTrackResource,
        XHRResource,
        WebSocketResource,
        OtherResource
    };

    static PassOwnPtrWillBeRawPtr<InspectorPageAgent> create(Page*, InjectedScriptManager*, InspectorClient*, InspectorOverlay*);

    // Settings overrides.
    void setTextAutosizingEnabled(bool);
    void setDeviceScaleAdjustment(float);
    void setPreferCompositingToLCDTextEnabled(bool);

    static Vector<Document*> importsForFrame(LocalFrame*);
    static bool cachedResourceContent(Resource*, String* result, bool* base64Encoded);
    static bool sharedBufferContent(PassRefPtr<SharedBuffer>, const String& textEncodingName, bool withBase64Encode, String* result);

    static PassRefPtr<SharedBuffer> resourceData(LocalFrame*, const KURL&, String* textEncodingName);
    static Resource* cachedResource(LocalFrame*, const KURL&);
    static TypeBuilder::Page::ResourceType::Enum resourceTypeJson(ResourceType);
    static ResourceType cachedResourceType(const Resource&);
    static TypeBuilder::Page::ResourceType::Enum cachedResourceTypeJson(const Resource&);
    static PassOwnPtr<TextResourceDecoder> createResourceTextDecoder(const String& mimeType, const String& textEncodingName);

    // Page API for InspectorFrontend
    virtual void enable(ErrorString*) override;
    virtual void disable(ErrorString*) override;
    virtual void addScriptToEvaluateOnLoad(ErrorString*, const String& source, String* result) override;
    virtual void removeScriptToEvaluateOnLoad(ErrorString*, const String& identifier) override;
    virtual void reload(ErrorString*, const bool* optionalIgnoreCache, const String* optionalScriptToEvaluateOnLoad, const String* optionalScriptPreprocessor) override;
    virtual void navigate(ErrorString*, const String& url, String* frameId) override;
    virtual void getCookies(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::Page::Cookie> >& cookies) override;
    virtual void deleteCookie(ErrorString*, const String& cookieName, const String& url) override;
    virtual void getResourceTree(ErrorString*, RefPtr<TypeBuilder::Page::FrameResourceTree>&) override;
    virtual void getResourceContent(ErrorString*, const String& frameId, const String& url, PassRefPtrWillBeRawPtr<GetResourceContentCallback>) override;
    virtual void searchInResource(ErrorString*, const String& frameId, const String& url, const String& query, const bool* optionalCaseSensitive, const bool* optionalIsRegex, RefPtr<TypeBuilder::Array<TypeBuilder::Page::SearchMatch> >&) override;
    virtual void setDocumentContent(ErrorString*, const String& frameId, const String& html) override;
    virtual void setDeviceMetricsOverride(ErrorString*, int width, int height, double deviceScaleFactor, bool mobile, bool fitWindow, const double* optionalScale, const double* optionalOffsetX, const double* optionalOffsetY) override;
    virtual void clearDeviceMetricsOverride(ErrorString*) override;
    virtual void resetScrollAndPageScaleFactor(ErrorString*) override;
    virtual void setPageScaleFactor(ErrorString*, double pageScaleFactor) override;
    virtual void setShowPaintRects(ErrorString*, bool show) override;
    virtual void setShowDebugBorders(ErrorString*, bool show) override;
    virtual void setShowFPSCounter(ErrorString*, bool show) override;
    virtual void setContinuousPaintingEnabled(ErrorString*, bool enabled) override;
    virtual void setShowScrollBottleneckRects(ErrorString*, bool show) override;
    virtual void getScriptExecutionStatus(ErrorString*, PageCommandHandler::Result::Enum*) override;
    virtual void setScriptExecutionDisabled(ErrorString*, bool) override;
    virtual void setTouchEmulationEnabled(ErrorString*, bool) override;
    virtual void setEmulatedMedia(ErrorString*, const String&) override;
    virtual void startScreencast(ErrorString*, const String* format, const int* quality, const int* maxWidth, const int* maxHeight) override;
    virtual void stopScreencast(ErrorString*) override;
    virtual void setShowViewportSizeOnResize(ErrorString*, bool show, const bool* showGrid) override;

    // InspectorInstrumentation API
    void didClearDocumentOfWindowObject(LocalFrame*);
    void domContentLoadedEventFired(LocalFrame*);
    void loadEventFired(LocalFrame*);
    void didCommitLoad(LocalFrame*, DocumentLoader*);
    void frameAttachedToParent(LocalFrame*);
    void frameDetachedFromParent(LocalFrame*);
    void loaderDetachedFromFrame(DocumentLoader*);
    void frameStartedLoading(LocalFrame*);
    void frameStoppedLoading(LocalFrame*);
    void frameScheduledNavigation(LocalFrame*, double delay);
    void frameClearedScheduledNavigation(LocalFrame*);
    void willRunJavaScriptDialog(const String& message);
    void didRunJavaScriptDialog();
    bool applyViewportStyleOverride(StyleResolver*);
    void applyEmulatedMedia(String*);
    void didPaint(RenderObject*, const GraphicsLayer*, GraphicsContext*, const LayoutRect&);
    void didLayout(RenderObject*);
    void didScroll();
    void didResizeMainFrame();
    void didRecalculateStyle(int);
    void scriptsEnabled(bool isEnabled);

    // Inspector Controller API
    virtual void setFrontend(InspectorFrontend*) override;
    virtual void clearFrontend() override;
    virtual void restore() override;
    virtual void discardAgent() override;

    // Cross-agents API
    Page* page() { return m_page; }
    LocalFrame* mainFrame();
    String createIdentifier();
    LocalFrame* frameForId(const String& frameId);
    String frameId(LocalFrame*);
    bool hasIdForFrame(LocalFrame*) const;
    String loaderId(DocumentLoader*);
    LocalFrame* findFrameWithSecurityOrigin(const String& originRawString);
    LocalFrame* assertFrame(ErrorString*, const String& frameId);
    String scriptPreprocessorSource() { return m_scriptPreprocessorSource; }
    const AtomicString& resourceSourceMapURL(const String& url);
    bool deviceMetricsOverrideEnabled();
    void deviceOrPageScaleFactorChanged();
    bool screencastEnabled();
    static DocumentLoader* assertDocumentLoader(ErrorString*, LocalFrame*);
    InspectorResourceContentLoader* resourceContentLoader() { return m_inspectorResourceContentLoader.get(); }
    void clearEditedResourcesContent();
    void addEditedResourceContent(const String& url, const String& content);
    bool getEditedResourceContent(const String& url, String* content);

    virtual void trace(Visitor*) override;

private:
    class GetResourceContentLoadListener;

    InspectorPageAgent(Page*, InjectedScriptManager*, InspectorClient*, InspectorOverlay*);
    bool deviceMetricsChanged(bool enabled, int width, int height, double deviceScaleFactor, bool mobile, bool fitWindow, double scale, double offsetX, double offsetY);
    void updateViewMetricsFromState();
    void updateViewMetrics(bool enabled, int width, int height, double deviceScaleFactor, bool mobile, bool fitWindow, double scale, double offsetX, double offsetY);
    void updateTouchEventEmulationInPage(bool);
    bool compositingEnabled(ErrorString*);

    void getResourceContentAfterResourcesContentLoaded(const String& frameId, const String& url, PassRefPtrWillBeRawPtr<GetResourceContentCallback>);

    static bool dataContent(const char* data, unsigned size, const String& textEncodingName, bool withBase64Encode, String* result);

    void viewportChanged();

    PassRefPtr<TypeBuilder::Page::Frame> buildObjectForFrame(LocalFrame*);
    PassRefPtr<TypeBuilder::Page::FrameResourceTree> buildObjectForFrameTree(LocalFrame*);
    RawPtrWillBeMember<Page> m_page;
    RawPtrWillBeMember<InjectedScriptManager> m_injectedScriptManager;
    InspectorClient* m_client;
    InspectorFrontend::Page* m_frontend;
    InspectorOverlay* m_overlay;
    long m_lastScriptIdentifier;
    String m_pendingScriptToEvaluateOnLoadOnce;
    String m_scriptToEvaluateOnLoadOnce;
    String m_pendingScriptPreprocessor;
    String m_scriptPreprocessorSource;
    HashMap<LocalFrame*, String> m_frameToIdentifier;
    HashMap<String, LocalFrame*> m_identifierToFrame;
    HashMap<DocumentLoader*, String> m_loaderToIdentifier;
    bool m_enabled;
    bool m_ignoreScriptsEnabledNotification;
    bool m_deviceMetricsOverridden;
    bool m_emulateMobileEnabled;

    bool m_touchEmulationEnabled;
    bool m_originalTouchEnabled;
    bool m_originalDeviceSupportsMouse;
    bool m_originalDeviceSupportsTouch;
    int m_originalMaxTouchPoints;

    bool m_embedderTextAutosizingEnabled;
    double m_embedderFontScaleFactor;
    bool m_embedderPreferCompositingToLCDTextEnabled;

    OwnPtrWillBeMember<InspectorResourceContentLoader> m_inspectorResourceContentLoader;
    HashMap<String, String> m_editedResourceContent;
};


} // namespace blink


#endif // !defined(InspectorPagerAgent_h)
