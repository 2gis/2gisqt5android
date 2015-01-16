/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AudioContext_h
#define AudioContext_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventListener.h"
#include "modules/EventTargetModules.h"
#include "modules/webaudio/AsyncAudioDecoder.h"
#include "modules/webaudio/AudioDestinationNode.h"
#include "platform/audio/AudioBus.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/MainThread.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/Threading.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

class AnalyserNode;
class AudioBuffer;
class AudioBufferCallback;
class AudioBufferSourceNode;
class AudioListener;
class AudioSummingJunction;
class BiquadFilterNode;
class ChannelMergerNode;
class ChannelSplitterNode;
class ConvolverNode;
class DelayNode;
class Document;
class DynamicsCompressorNode;
class ExceptionState;
class GainNode;
class HTMLMediaElement;
class MediaElementAudioSourceNode;
class MediaStreamAudioDestinationNode;
class MediaStreamAudioSourceNode;
class OscillatorNode;
class PannerNode;
class PeriodicWave;
class ScriptProcessorNode;
class WaveShaperNode;

// AudioContext is the cornerstone of the web audio API and all AudioNodes are created from it.
// For thread safety between the audio thread and the main thread, it has a rendering graph locking mechanism.

class AudioContext : public ThreadSafeRefCountedWillBeThreadSafeRefCountedGarbageCollected<AudioContext>, public ActiveDOMObject, public ScriptWrappable, public EventTargetWithInlineData {
    DEFINE_EVENT_TARGET_REFCOUNTING(ThreadSafeRefCountedWillBeThreadSafeRefCountedGarbageCollected<AudioContext>);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(AudioContext);
public:
    // Create an AudioContext for rendering to the audio hardware.
    static PassRefPtrWillBeRawPtr<AudioContext> create(Document&, ExceptionState&);

    virtual ~AudioContext();

    virtual void trace(Visitor*) OVERRIDE;

    bool isInitialized() const { return m_isInitialized; }
    bool isOfflineContext() { return m_isOfflineContext; }

    // Document notification
    virtual void stop() OVERRIDE FINAL;
    virtual bool hasPendingActivity() const OVERRIDE;

    AudioDestinationNode* destination() { return m_destinationNode.get(); }
    size_t currentSampleFrame() const { return m_destinationNode->currentSampleFrame(); }
    double currentTime() const { return m_destinationNode->currentTime(); }
    float sampleRate() const { return m_destinationNode->sampleRate(); }

    PassRefPtrWillBeRawPtr<AudioBuffer> createBuffer(unsigned numberOfChannels, size_t numberOfFrames, float sampleRate, ExceptionState&);

    // Asynchronous audio file data decoding.
    void decodeAudioData(ArrayBuffer*, PassOwnPtr<AudioBufferCallback>, PassOwnPtr<AudioBufferCallback>, ExceptionState&);

    AudioListener* listener() { return m_listener.get(); }

    // The AudioNode create methods are called on the main thread (from JavaScript).
    PassRefPtrWillBeRawPtr<AudioBufferSourceNode> createBufferSource();
    PassRefPtrWillBeRawPtr<MediaElementAudioSourceNode> createMediaElementSource(HTMLMediaElement*, ExceptionState&);
    PassRefPtrWillBeRawPtr<MediaStreamAudioSourceNode> createMediaStreamSource(MediaStream*, ExceptionState&);
    PassRefPtrWillBeRawPtr<MediaStreamAudioDestinationNode> createMediaStreamDestination();
    PassRefPtrWillBeRawPtr<GainNode> createGain();
    PassRefPtrWillBeRawPtr<BiquadFilterNode> createBiquadFilter();
    PassRefPtrWillBeRawPtr<WaveShaperNode> createWaveShaper();
    PassRefPtrWillBeRawPtr<DelayNode> createDelay(ExceptionState&);
    PassRefPtrWillBeRawPtr<DelayNode> createDelay(double maxDelayTime, ExceptionState&);
    PassRefPtrWillBeRawPtr<PannerNode> createPanner();
    PassRefPtrWillBeRawPtr<ConvolverNode> createConvolver();
    PassRefPtrWillBeRawPtr<DynamicsCompressorNode> createDynamicsCompressor();
    PassRefPtrWillBeRawPtr<AnalyserNode> createAnalyser();
    PassRefPtrWillBeRawPtr<ScriptProcessorNode> createScriptProcessor(ExceptionState&);
    PassRefPtrWillBeRawPtr<ScriptProcessorNode> createScriptProcessor(size_t bufferSize, ExceptionState&);
    PassRefPtrWillBeRawPtr<ScriptProcessorNode> createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, ExceptionState&);
    PassRefPtrWillBeRawPtr<ScriptProcessorNode> createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, size_t numberOfOutputChannels, ExceptionState&);
    PassRefPtrWillBeRawPtr<ChannelSplitterNode> createChannelSplitter(ExceptionState&);
    PassRefPtrWillBeRawPtr<ChannelSplitterNode> createChannelSplitter(size_t numberOfOutputs, ExceptionState&);
    PassRefPtrWillBeRawPtr<ChannelMergerNode> createChannelMerger(ExceptionState&);
    PassRefPtrWillBeRawPtr<ChannelMergerNode> createChannelMerger(size_t numberOfInputs, ExceptionState&);
    PassRefPtrWillBeRawPtr<OscillatorNode> createOscillator();
    PassRefPtrWillBeRawPtr<PeriodicWave> createPeriodicWave(Float32Array* real, Float32Array* imag, ExceptionState&);

    // When a source node has no more processing to do (has finished playing), then it tells the context to dereference it.
    void notifyNodeFinishedProcessing(AudioNode*);

    // Called at the start of each render quantum.
    void handlePreRenderTasks();

    // Called at the end of each render quantum.
    void handlePostRenderTasks();

    // Called periodically at the end of each render quantum to dereference finished source nodes.
    void derefFinishedSourceNodes();

    // We schedule deletion of all marked nodes at the end of each realtime render quantum.
    void markForDeletion(AudioNode*);
    void deleteMarkedNodes();

    // AudioContext can pull node(s) at the end of each render quantum even when they are not connected to any downstream nodes.
    // These two methods are called by the nodes who want to add/remove themselves into/from the automatic pull lists.
    void addAutomaticPullNode(AudioNode*);
    void removeAutomaticPullNode(AudioNode*);

    // Called right before handlePostRenderTasks() to handle nodes which need to be pulled even when they are not connected to anything.
    void processAutomaticPullNodes(size_t framesToProcess);

    // Keeps track of the number of connections made.
    void incrementConnectionCount()
    {
        ASSERT(isMainThread());
        m_connectionCount++;
    }

    unsigned connectionCount() const { return m_connectionCount; }

    //
    // Thread Safety and Graph Locking:
    //

    void setAudioThread(ThreadIdentifier thread) { m_audioThread = thread; } // FIXME: check either not initialized or the same
    ThreadIdentifier audioThread() const { return m_audioThread; }
    bool isAudioThread() const;

    // mustReleaseLock is set to true if we acquired the lock in this method call and caller must unlock(), false if it was previously acquired.
    void lock(bool& mustReleaseLock);

    // Returns true if we own the lock.
    // mustReleaseLock is set to true if we acquired the lock in this method call and caller must unlock(), false if it was previously acquired.
    bool tryLock(bool& mustReleaseLock);

    void unlock();

    // Returns true if this thread owns the context's lock.
    bool isGraphOwner() const;

    // Returns the maximum numuber of channels we can support.
    static unsigned maxNumberOfChannels() { return MaxNumberOfChannels;}

    class AutoLocker {
    public:
        AutoLocker(AudioContext* context)
            : m_context(context)
        {
            ASSERT(context);
            context->lock(m_mustReleaseLock);
        }

        ~AutoLocker()
        {
            if (m_mustReleaseLock)
                m_context->unlock();
        }
    private:
        AudioContext* m_context;
        bool m_mustReleaseLock;
    };

    // In AudioNode::deref() a tryLock() is used for calling finishDeref(), but if it fails keep track here.
    void addDeferredFinishDeref(AudioNode*);

    // In the audio thread at the start of each render cycle, we'll call handleDeferredFinishDerefs().
    void handleDeferredFinishDerefs();

    // Only accessed when the graph lock is held.
    void markSummingJunctionDirty(AudioSummingJunction*);
    void markAudioNodeOutputDirty(AudioNodeOutput*);

    // Must be called on main thread.
    void removeMarkedSummingJunction(AudioSummingJunction*);

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE FINAL;
    virtual ExecutionContext* executionContext() const OVERRIDE FINAL;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(complete);

    void startRendering();
    void fireCompletionEvent();

    static unsigned s_hardwareContextCount;

protected:
    explicit AudioContext(Document*);
    AudioContext(Document*, unsigned numberOfChannels, size_t numberOfFrames, float sampleRate);

    static bool isSampleRateRangeGood(float sampleRate);

private:
    void initialize();
    void uninitialize();

    // ExecutionContext calls stop twice.
    // We'd like to schedule only one stop action for them.
    bool m_isStopScheduled;
    static void stopDispatch(void* userData);
    bool m_isCleared;
    void clear();

    void scheduleNodeDeletion();
    static void deleteMarkedNodesDispatch(void* userData);

    // Set to true when the destination node has been initialized and is ready to process data.
    bool m_isInitialized;

    // The context itself keeps a reference to all source nodes.  The source nodes, then reference all nodes they're connected to.
    // In turn, these nodes reference all nodes they're connected to.  All nodes are ultimately connected to the AudioDestinationNode.
    // When the context dereferences a source node, it will be deactivated from the rendering graph along with all other nodes it is
    // uniquely connected to.  See the AudioNode::ref() and AudioNode::deref() methods for more details.
    void refNode(AudioNode*);
    void derefNode(AudioNode*);

    // When the context goes away, there might still be some sources which haven't finished playing.
    // Make sure to dereference them here.
    void derefUnfinishedSourceNodes();

    RefPtrWillBeMember<AudioDestinationNode> m_destinationNode;
    RefPtrWillBeMember<AudioListener> m_listener;

    // Only accessed in the audio thread.
    Vector<AudioNode*> m_finishedNodes;

    // We don't use RefPtr<AudioNode> here because AudioNode has a more complex ref() / deref() implementation
    // with an optional argument for refType.  We need to use the special refType: RefTypeConnection
    // Either accessed when the graph lock is held, or on the main thread when the audio thread has finished.
    Vector<AudioNode*> m_referencedNodes;

    // Accumulate nodes which need to be deleted here.
    // This is copied to m_nodesToDelete at the end of a render cycle in handlePostRenderTasks(), where we're assured of a stable graph
    // state which will have no references to any of the nodes in m_nodesToDelete once the context lock is released
    // (when handlePostRenderTasks() has completed).
    Vector<AudioNode*> m_nodesMarkedForDeletion;

    // They will be scheduled for deletion (on the main thread) at the end of a render cycle (in realtime thread).
    Vector<AudioNode*> m_nodesToDelete;
    bool m_isDeletionScheduled;

    // Only accessed when the graph lock is held.
    HashSet<AudioSummingJunction* > m_dirtySummingJunctions;
    HashSet<AudioNodeOutput*> m_dirtyAudioNodeOutputs;
    void handleDirtyAudioSummingJunctions();
    void handleDirtyAudioNodeOutputs();

    // For the sake of thread safety, we maintain a seperate Vector of automatic pull nodes for rendering in m_renderingAutomaticPullNodes.
    // It will be copied from m_automaticPullNodes by updateAutomaticPullNodes() at the very start or end of the rendering quantum.
    HashSet<AudioNode*> m_automaticPullNodes;
    Vector<AudioNode*> m_renderingAutomaticPullNodes;
    // m_automaticPullNodesNeedUpdating keeps track if m_automaticPullNodes is modified.
    bool m_automaticPullNodesNeedUpdating;
    void updateAutomaticPullNodes();

    unsigned m_connectionCount;

    // Graph locking.
    Mutex m_contextGraphMutex;
    volatile ThreadIdentifier m_audioThread;
    volatile ThreadIdentifier m_graphOwnerThread; // if the lock is held then this is the thread which owns it, otherwise == UndefinedThreadIdentifier

    // Only accessed in the audio thread.
    Vector<AudioNode*> m_deferredFinishDerefList;

    RefPtrWillBeMember<AudioBuffer> m_renderTarget;

    bool m_isOfflineContext;

    AsyncAudioDecoder m_audioDecoder;

    // This is considering 32 is large enough for multiple channels audio.
    // It is somewhat arbitrary and could be increased if necessary.
    enum { MaxNumberOfChannels = 32 };
};

} // WebCore

#endif // AudioContext_h
