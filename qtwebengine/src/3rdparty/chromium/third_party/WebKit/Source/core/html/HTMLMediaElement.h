/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLMediaElement_h
#define HTMLMediaElement_h

#include "core/dom/ActiveDOMObject.h"
#include "core/events/GenericEventQueue.h"
#include "core/html/HTMLElement.h"
#include "core/html/track/TextTrack.h"
#include "core/html/track/TextTrackCue.h"
#include "core/html/track/vtt/VTTCue.h"
#include "platform/PODIntervalTree.h"
#include "platform/Supplementable.h"
#include "platform/graphics/media/MediaPlayer.h"
#include "public/platform/WebMediaPlayerClient.h"
#include "public/platform/WebMimeRegistry.h"

namespace blink {
class WebInbandTextTrack;
class WebLayer;
}

namespace blink {

#if ENABLE(WEB_AUDIO)
class AudioSourceProvider;
class AudioSourceProviderClient;
#endif
class AudioTrackList;
class ContentType;
class Event;
class ExceptionState;
class HTMLSourceElement;
class HTMLTrackElement;
class KURL;
class MediaController;
class MediaControls;
class MediaError;
class HTMLMediaSource;
class TextTrackList;
class TimeRanges;
class URLRegistry;
class VideoTrackList;

typedef PODIntervalTree<double, TextTrackCue*> CueIntervalTree;
typedef CueIntervalTree::IntervalType CueInterval;
typedef Vector<CueInterval> CueList;

// FIXME: The inheritance from MediaPlayerClient here should be private inheritance.
// But it can't be until the Chromium WebMediaPlayerClientImpl class is fixed so it
// no longer depends on typecasting a MediaPlayerClient to an HTMLMediaElement.

class HTMLMediaElement : public HTMLElement, public WillBeHeapSupplementable<HTMLMediaElement>, public MediaPlayerClient, public ActiveDOMObject {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(HTMLMediaElement);
public:
    static blink::WebMimeRegistry::SupportsType supportsType(const ContentType&, const String& keySystem = String());

    static void setMediaStreamRegistry(URLRegistry*);
    static bool isMediaStreamURL(const String& url);

    virtual void trace(Visitor*) override;
#if ENABLE(WEB_AUDIO)
    void clearWeakMembers(Visitor*);
#endif
    blink::WebMediaPlayer* webMediaPlayer() const { return m_player ? m_player->webMediaPlayer() : 0; }

    virtual bool hasVideo() const { return false; }
    bool hasAudio() const;

    bool supportsSave() const;

    blink::WebLayer* platformLayer() const;

    enum DelayedActionType {
        LoadMediaResource = 1 << 0,
        LoadTextTrackResource = 1 << 1,
        TextTrackChangesNotification = 1 << 2
    };
    void scheduleDelayedAction(DelayedActionType);

    bool isActive() const { return m_active; }

    bool hasRemoteRoutes() const { return m_remoteRoutesAvailable; }
    bool isPlayingRemotely() const { return m_playingRemotely; }

    // error state
    PassRefPtrWillBeRawPtr<MediaError> error() const;

    // network state
    void setSrc(const AtomicString&);
    const KURL& currentSrc() const { return m_currentSrc; }

    enum NetworkState { NETWORK_EMPTY, NETWORK_IDLE, NETWORK_LOADING, NETWORK_NO_SOURCE };
    NetworkState networkState() const;

    String preload() const;
    void setPreload(const AtomicString&);

    PassRefPtrWillBeRawPtr<TimeRanges> buffered() const;
    void load();
    String canPlayType(const String& mimeType, const String& keySystem = String()) const;

    // ready state
    enum ReadyState { HAVE_NOTHING, HAVE_METADATA, HAVE_CURRENT_DATA, HAVE_FUTURE_DATA, HAVE_ENOUGH_DATA };
    ReadyState readyState() const;
    bool seeking() const;

    // playback state
    double currentTime() const;
    void setCurrentTime(double, ExceptionState&);
    double duration() const;
    bool paused() const;
    double defaultPlaybackRate() const;
    void setDefaultPlaybackRate(double);
    double playbackRate() const;
    void setPlaybackRate(double);
    void updatePlaybackRate();
    PassRefPtrWillBeRawPtr<TimeRanges> played();
    PassRefPtrWillBeRawPtr<TimeRanges> seekable() const;
    bool ended() const;
    bool autoplay() const;
    bool loop() const;
    void setLoop(bool);
    void play();
    void pause();
    void requestRemotePlayback();
    void requestRemotePlaybackControl();

    // statistics
    unsigned webkitAudioDecodedByteCount() const;
    unsigned webkitVideoDecodedByteCount() const;

    // media source extensions
    void closeMediaSource();
    void durationChanged(double duration, bool requestSeek);

    // controls
    bool shouldShowControls() const;
    double volume() const;
    void setVolume(double, ExceptionState&);
    bool muted() const;
    void setMuted(bool);

    // play/pause toggling that uses the media controller if present. togglePlayStateWillPlay() is
    // true if togglePlayState() will call play() or unpause() on the media element or controller.
    bool togglePlayStateWillPlay() const;
    void togglePlayState();

    AudioTrackList& audioTracks();
    void audioTrackChanged();

    VideoTrackList& videoTracks();
    void selectedVideoTrackChanged(blink::WebMediaPlayer::TrackId*);

    PassRefPtrWillBeRawPtr<TextTrack> addTextTrack(const AtomicString& kind, const AtomicString& label, const AtomicString& language, ExceptionState&);
    PassRefPtrWillBeRawPtr<TextTrack> addTextTrack(const AtomicString& kind, const AtomicString& label, ExceptionState& exceptionState) { return addTextTrack(kind, label, emptyAtom, exceptionState); }
    PassRefPtrWillBeRawPtr<TextTrack> addTextTrack(const AtomicString& kind, ExceptionState& exceptionState) { return addTextTrack(kind, emptyAtom, emptyAtom, exceptionState); }

    TextTrackList* textTracks();
    CueList currentlyActiveCues() const { return m_currentlyActiveCues; }

    void addTextTrack(TextTrack*);
    void removeTextTrack(TextTrack*);
    void textTracksChanged();
    void notifyMediaPlayerOfTextTrackChanges();

    // Implements the "forget the media element's media-resource-specific tracks" algorithm in the HTML5 spec.
    void forgetResourceSpecificTracks();

    void didAddTrackElement(HTMLTrackElement*);
    void didRemoveTrackElement(HTMLTrackElement*);

    blink::WebMediaPlayer::TrackId addAudioTrack(const String& id, blink::WebMediaPlayerClient::AudioTrackKind, const AtomicString& label, const AtomicString& language, bool enabled);
    void removeAudioTrack(blink::WebMediaPlayer::TrackId);
    blink::WebMediaPlayer::TrackId addVideoTrack(const String& id, blink::WebMediaPlayerClient::VideoTrackKind, const AtomicString& label, const AtomicString& language, bool selected);
    void removeVideoTrack(blink::WebMediaPlayer::TrackId);

    virtual void mediaPlayerDidAddTextTrack(blink::WebInbandTextTrack*) override final;
    virtual void mediaPlayerDidRemoveTextTrack(blink::WebInbandTextTrack*) override final;
    // FIXME: Remove this when WebMediaPlayerClientImpl::loadInternal does not depend on it.
    virtual KURL mediaPlayerPosterURL() override { return KURL(); }

    class TrackGroup {
        STACK_ALLOCATED();
    public:
        enum GroupKind { CaptionsAndSubtitles, Description, Chapter, Metadata, Other };

        explicit TrackGroup(GroupKind kind)
            : visibleTrack(nullptr)
            , defaultTrack(nullptr)
            , kind(kind)
            , hasSrcLang(false)
        {
        }

        WillBeHeapVector<RefPtrWillBeMember<TextTrack>> tracks;
        RefPtrWillBeMember<TextTrack> visibleTrack;
        RefPtrWillBeMember<TextTrack> defaultTrack;
        GroupKind kind;
        bool hasSrcLang;
    };

    void configureTextTrackGroupForLanguage(const TrackGroup&) const;
    void configureTextTracks();
    void configureTextTrackGroup(const TrackGroup&);

    bool textTracksAreReady() const;
    enum VisibilityChangeAssumption {
        AssumeNoVisibleChange,
        AssumeVisibleChange
    };
    void configureTextTrackDisplay(VisibilityChangeAssumption);
    void updateTextTrackDisplay();
    void textTrackReadyStateChanged(TextTrack*);

    void textTrackKindChanged(TextTrack*);
    void textTrackModeChanged(TextTrack*);
    void textTrackAddCues(TextTrack*, const TextTrackCueList*);
    void textTrackRemoveCues(TextTrack*, const TextTrackCueList*);
    void textTrackAddCue(TextTrack*, PassRefPtrWillBeRawPtr<TextTrackCue>);
    void textTrackRemoveCue(TextTrack*, PassRefPtrWillBeRawPtr<TextTrackCue>);

    // EventTarget function.
    // Both Node (via HTMLElement) and ActiveDOMObject define this method, which
    // causes an ambiguity error at compile time. This class's constructor
    // ensures that both implementations return document, so return the result
    // of one of them here.
    using HTMLElement::executionContext;

    bool hasSingleSecurityOrigin() const { return !m_player || (webMediaPlayer() && webMediaPlayer()->hasSingleSecurityOrigin()); }

    bool isFullscreen() const;
    void enterFullscreen();
    void exitFullscreen();

    bool hasClosedCaptions() const;
    bool closedCaptionsVisible() const;
    void setClosedCaptionsVisible(bool);

    void remoteRouteAvailabilityChanged(bool);
    void connectedToRemoteDevice();
    void disconnectedFromRemoteDevice();

    MediaControls* mediaControls() const;

    void sourceWasRemoved(HTMLSourceElement*);
    void sourceWasAdded(HTMLSourceElement*);

    // ActiveDOMObject functions.
    virtual bool hasPendingActivity() const override final;
    virtual void contextDestroyed() override final;

#if ENABLE(WEB_AUDIO)
    AudioSourceProviderClient* audioSourceNode() { return m_audioSourceNode; }
    void setAudioSourceNode(AudioSourceProviderClient*);

    AudioSourceProvider* audioSourceProvider();
#endif

    enum InvalidURLAction { DoNothing, Complain };
    bool isSafeToLoadURL(const KURL&, InvalidURLAction);

    MediaController* controller() const;
    void setController(PassRefPtrWillBeRawPtr<MediaController>); // Resets the MediaGroup and sets the MediaController.

    void scheduleEvent(PassRefPtrWillBeRawPtr<Event>);

    // Returns the "effective media volume" value as specified in the HTML5 spec.
    double effectiveMediaVolume() const;

#if ENABLE(OILPAN)
    bool isFinalizing() const { return m_isFinalizing; }

    // Oilpan: finalization of the media element is observable from its
    // attached MediaSource; it entering a closed state.
    //
    // Express that by having the MediaSource keep a weak reference
    // to the media element and signal that it wants to be notified
    // of destruction if it survives a GC, but the media element
    // doesn't.
    void setCloseMediaSourceWhenFinalizing();
#endif

    // Predicates also used when dispatching wrapper creation (cf. [SpecialWrapFor] IDL attribute usage.)
    virtual bool isHTMLAudioElement() const { return false; }
    virtual bool isHTMLVideoElement() const { return false; }

protected:
    HTMLMediaElement(const QualifiedName&, Document&);
    virtual ~HTMLMediaElement();

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual void finishParsingChildren() override final;
    virtual bool isURLAttribute(const Attribute&) const override;
    virtual void attach(const AttachContext& = AttachContext()) override;

    virtual void didMoveToNewDocument(Document& oldDocument) override;

    enum DisplayMode { Unknown, Poster, PosterWaitingForVideo, Video };
    DisplayMode displayMode() const { return m_displayMode; }
    virtual void setDisplayMode(DisplayMode mode) { m_displayMode = mode; }

    void setControllerInternal(PassRefPtrWillBeRawPtr<MediaController>);

    bool ignoreTrackDisplayUpdateRequests() const { return m_ignoreTrackDisplayUpdate > 0; }
    void beginIgnoringTrackDisplayUpdateRequests();
    void endIgnoringTrackDisplayUpdateRequests();

private:
    void createMediaPlayer();

    virtual bool alwaysCreateUserAgentShadowRoot() const override final { return true; }
    virtual bool areAuthorShadowsAllowed() const override final { return false; }

    virtual bool supportsFocus() const override final;
    virtual bool isMouseFocusable() const override final;
    virtual bool rendererIsNeeded(const RenderStyle&) override;
    virtual RenderObject* createRenderer(RenderStyle*) override;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) override final;
    virtual void didNotifySubtreeInsertionsToDocument() override;
    virtual void removedFrom(ContainerNode*) override final;
    virtual void didRecalcStyle(StyleRecalcChange) override final;

    virtual void didBecomeFullscreenElement() override final;
    virtual void willStopBeingFullscreenElement() override final;
    virtual bool isInteractiveContent() const override final;
    virtual void defaultEventHandler(Event*) override final;

    // ActiveDOMObject functions.
    virtual void stop() override final;

    virtual void updateDisplayState() { }

    void setReadyState(ReadyState);
    void setNetworkState(blink::WebMediaPlayer::NetworkState);

    virtual void mediaPlayerNetworkStateChanged() override final;
    virtual void mediaPlayerReadyStateChanged() override final;
    virtual void mediaPlayerTimeChanged() override final;
    virtual void mediaPlayerDurationChanged() override final;
    virtual void mediaPlayerPlaybackStateChanged() override final;
    virtual void mediaPlayerRequestFullscreen() override final;
    virtual void mediaPlayerRequestSeek(double) override final;
    virtual void mediaPlayerRepaint() override final;
    virtual void mediaPlayerSizeChanged() override final;
    virtual void mediaPlayerSetWebLayer(blink::WebLayer*) override final;
    virtual void mediaPlayerMediaSourceOpened(blink::WebMediaSource*) override final;

    void loadTimerFired(Timer<HTMLMediaElement>*);
    void progressEventTimerFired(Timer<HTMLMediaElement>*);
    void playbackProgressTimerFired(Timer<HTMLMediaElement>*);
    void startPlaybackProgressTimer();
    void startProgressEventTimer();
    void stopPeriodicTimers();

    void seek(double time);
    void finishSeek();
    void checkIfSeekNeeded();
    void addPlayedRange(double start, double end);

    void scheduleTimeupdateEvent(bool periodicEvent);
    void scheduleEvent(const AtomicString& eventName); // FIXME: Rename to scheduleNamedEvent for clarity.

    // loading
    void prepareForLoad();
    void loadInternal();
    void selectMediaResource();
    void loadResource(const KURL&, ContentType&, const String& keySystem);
    void startPlayerLoad();
    void setPlayerPreload();
    blink::WebMediaPlayer::LoadType loadType() const;
    void scheduleNextSourceChild();
    void loadNextSourceChild();
    void userCancelledLoad();
    void clearMediaPlayer(int flags);
    void clearMediaPlayerAndAudioSourceProviderClientWithoutLocking();
    bool havePotentialSourceChild();
    void noneSupported();
    void mediaEngineError(PassRefPtrWillBeRawPtr<MediaError>);
    void cancelPendingEventsAndCallbacks();
    void waitForSourceChange();
    void prepareToPlay();

    KURL selectNextSourceChild(ContentType*, String* keySystem, InvalidURLAction);

    void mediaLoadingFailed(blink::WebMediaPlayer::NetworkState);

    // deferred loading (preload=none)
    bool loadIsDeferred() const;
    void deferLoad();
    void cancelDeferredLoad();
    void startDeferredLoad();
    void executeDeferredLoad();
    void deferredLoadTimerFired(Timer<HTMLMediaElement>*);

    void updateActiveTextTrackCues(double);
    HTMLTrackElement* showingTrackWithSameKind(HTMLTrackElement*) const;

    void markCaptionAndSubtitleTracksAsUnconfigured();

    // This does not check user gesture restrictions.
    void playInternal();

    void gesturelessInitialPlayHalted();
    void autoplayMediaEncountered();
    void allowVideoRendering();

    void updateVolume();
    void updatePlayState();
    bool potentiallyPlaying() const;
    bool endedPlayback() const;
    bool stoppedDueToErrors() const;
    bool couldPlayIfEnoughData() const;

    void setShouldDelayLoadEvent(bool);
    void invalidateCachedTime();
    void refreshCachedTime() const;

    bool hasMediaControls() const;
    bool createMediaControls();
    void configureMediaControls();

    virtual void* preDispatchEventHandler(Event*) override final;

    void changeNetworkStateFromLoadingToIdle();

    const AtomicString& mediaGroup() const;
    void setMediaGroup(const AtomicString&);
    void updateMediaController();
    bool isBlocked() const;
    bool isBlockedOnMediaController() const;
    bool isAutoplaying() const { return m_autoplaying; }

    blink::WebMediaPlayer::CORSMode corsMode() const;

    // Returns the "direction of playback" value as specified in the HTML5 spec.
    enum DirectionOfPlayback { Backward, Forward };
    DirectionOfPlayback directionOfPlayback() const;

    // Returns the "effective playback rate" value as specified in the HTML5 spec.
    double effectivePlaybackRate() const;

    // Creates placeholder AudioTrack and/or VideoTrack objects when WebMemediaPlayer objects
    // advertise they have audio and/or video, but don't explicitly signal them via
    // addAudioTrack() and addVideoTrack().
    // FIXME: Remove this once all WebMediaPlayer implementations properly report their track info.
    void createPlaceholderTracksIfNecessary();

    // Sets the selected/enabled tracks if they aren't set before we initially
    // transition to HAVE_METADATA.
    void selectInitialTracksIfNecessary();

    void audioTracksTimerFired(Timer<HTMLMediaElement>*);

    Timer<HTMLMediaElement> m_loadTimer;
    Timer<HTMLMediaElement> m_progressEventTimer;
    Timer<HTMLMediaElement> m_playbackProgressTimer;
    Timer<HTMLMediaElement> m_audioTracksTimer;
    RefPtrWillBeMember<TimeRanges> m_playedTimeRanges;
    OwnPtrWillBeMember<GenericEventQueue> m_asyncEventQueue;

    double m_playbackRate;
    double m_defaultPlaybackRate;
    NetworkState m_networkState;
    ReadyState m_readyState;
    ReadyState m_readyStateMaximum;
    KURL m_currentSrc;

    RefPtrWillBeMember<MediaError> m_error;

    double m_volume;
    double m_lastSeekTime;

    double m_previousProgressTime;

    // Cached duration to suppress duplicate events if duration unchanged.
    double m_duration;

    // The last time a timeupdate event was sent (wall clock).
    double m_lastTimeUpdateEventWallTime;

    // The last time a timeupdate event was sent in movie time.
    double m_lastTimeUpdateEventMovieTime;

    // The default playback start position.
    double m_defaultPlaybackStartPosition;

    // Loading state.
    enum LoadState { WaitingForSource, LoadingFromSrcAttr, LoadingFromSourceElement };
    LoadState m_loadState;
    RefPtrWillBeMember<HTMLSourceElement> m_currentSourceNode;
    RefPtrWillBeMember<Node> m_nextChildNodeToConsider;

    // "Deferred loading" state (for preload=none).
    enum DeferredLoadState {
        // The load is not deferred.
        NotDeferred,
        // The load is deferred, and waiting for the task to set the
        // delaying-the-load-event flag (to false).
        WaitingForStopDelayingLoadEventTask,
        // The load is the deferred, and waiting for a triggering event.
        WaitingForTrigger,
        // The load is deferred, and waiting for the task to set the
        // delaying-the-load-event flag, after which the load will be executed.
        ExecuteOnStopDelayingLoadEventTask
    };
    DeferredLoadState m_deferredLoadState;
    Timer<HTMLMediaElement> m_deferredLoadTimer;

    OwnPtr<MediaPlayer> m_player;
    blink::WebLayer* m_webLayer;

    MediaPlayer::Preload m_preload;

    DisplayMode m_displayMode;

    RefPtrWillBeMember<HTMLMediaSource> m_mediaSource;

    // Cached time value. Only valid when ready state is HAVE_METADATA or
    // higher, otherwise the current time is assumed to be zero.
    mutable double m_cachedTime;

    double m_fragmentEndTime;

    typedef unsigned PendingActionFlags;
    PendingActionFlags m_pendingActionFlags;

    // FIXME: MediaElement has way too many state bits.
    bool m_userGestureRequiredForPlay : 1;
    bool m_playing : 1;
    bool m_shouldDelayLoadEvent : 1;
    bool m_haveFiredLoadedData : 1;
    bool m_active : 1;
    bool m_autoplaying : 1;
    bool m_muted : 1;
    bool m_paused : 1;
    bool m_seeking : 1;

    // data has not been loaded since sending a "stalled" event
    bool m_sentStalledEvent : 1;

    // time has not changed since sending an "ended" event
    bool m_sentEndEvent : 1;

    bool m_closedCaptionsVisible : 1;

    bool m_completelyLoaded : 1;
    bool m_havePreparedToPlay : 1;
    bool m_delayingLoadForPreloadNone : 1;

    bool m_tracksAreReady : 1;
    bool m_haveVisibleTextTrack : 1;
    bool m_processingPreferenceChange : 1;
    bool m_remoteRoutesAvailable : 1;
    bool m_playingRemotely : 1;
#if ENABLE(OILPAN)
    bool m_isFinalizing : 1;
    bool m_closeMediaSourceWhenFinalizing : 1;
#endif
    double m_lastTextTrackUpdateTime;
    bool m_initialPlayWithoutUserGestures : 1;
    bool m_autoplayMediaCounted : 1;

    RefPtrWillBeMember<AudioTrackList> m_audioTracks;
    RefPtrWillBeMember<VideoTrackList> m_videoTracks;
    RefPtrWillBeMember<TextTrackList> m_textTracks;
    WillBeHeapVector<RefPtrWillBeMember<TextTrack>> m_textTracksWhenResourceSelectionBegan;

    CueIntervalTree m_cueTree;

    CueList m_currentlyActiveCues;
    int m_ignoreTrackDisplayUpdate;

#if ENABLE(WEB_AUDIO)
    // This is a weak reference, since m_audioSourceNode holds a reference to us.
    // FIXME: Oilpan: Consider making this a strongly traced pointer with oilpan where strong cycles are not a problem.
    GC_PLUGIN_IGNORE("http://crbug.com/404577")
    RawPtrWillBeWeakMember<AudioSourceProviderClient> m_audioSourceNode;
#endif

    friend class MediaController;
    RefPtrWillBeMember<MediaController> m_mediaController;

    friend class Internals;
    friend class TrackDisplayUpdateScope;

    static URLRegistry* s_mediaStreamRegistry;
};

#ifndef NDEBUG
// Template specializations required by PodIntervalTree in debug mode.
template <>
struct ValueToString<double> {
    static String string(const double value)
    {
        return String::number(value);
    }
};

template <>
struct ValueToString<TextTrackCue*> {
    static String string(TextTrackCue* const& cue)
    {
        return cue->toString();
    }
};
#endif

inline bool isHTMLMediaElement(const HTMLElement& element)
{
    return isHTMLAudioElement(element) || isHTMLVideoElement(element);
}

DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(HTMLMediaElement);

} // namespace blink

#endif // HTMLMediaElement_h
