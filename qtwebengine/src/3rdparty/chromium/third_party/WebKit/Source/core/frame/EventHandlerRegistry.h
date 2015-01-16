// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventHandlerRegistry_h
#define EventHandlerRegistry_h

#include "core/events/Event.h"
#include "core/frame/FrameHost.h"
#include "wtf/HashCountedSet.h"

namespace WebCore {

typedef HashCountedSet<EventTarget*> EventTargetSet;

// Registry for keeping track of event handlers. Note that only handlers on
// documents that can be rendered or can receive input (i.e., are attached to a
// FrameHost) are registered here.
class EventHandlerRegistry FINAL : public NoBaseWillBeGarbageCollectedFinalized<EventHandlerRegistry> {
public:
    explicit EventHandlerRegistry(FrameHost&);
    virtual ~EventHandlerRegistry();

    // Supported event handler classes. Note that each one may correspond to
    // multiple event types.
    enum EventHandlerClass {
        ScrollEvent,
        WheelEvent,
#if ASSERT_ENABLED
        // Additional event categories for verifying handler tracking logic.
        EventsForTesting,
#endif
        EventHandlerClassCount, // Must be the last entry.
    };

    // Returns true if the FrameHost has event handlers of the specified class.
    bool hasEventHandlers(EventHandlerClass) const;

    // Returns a set of EventTargets which have registered handlers of the given class.
    const EventTargetSet* eventHandlerTargets(EventHandlerClass) const;

    // Registration and management of event handlers attached to EventTargets.
    void didAddEventHandler(EventTarget&, const AtomicString& eventType);
    void didAddEventHandler(EventTarget&, EventHandlerClass);
    void didRemoveEventHandler(EventTarget&, const AtomicString& eventType);
    void didRemoveEventHandler(EventTarget&, EventHandlerClass);
    void didRemoveAllEventHandlers(EventTarget&);
    void didMoveIntoFrameHost(EventTarget&);
    void didMoveOutOfFrameHost(EventTarget&);

    // Either |documentDetached| or |didMoveOutOfFrameHost| must be called
    // whenever the FrameHost that is associated with a registered event target
    // changes. This ensures the registry does not end up with stale references
    // to handlers that are no longer related to it.
    void documentDetached(Document&);

    void trace(Visitor*);
    void clearWeakMembers(Visitor*);

private:
    enum ChangeOperation {
        Add, // Add a new event handler.
        Remove, // Remove an existing event handler.
        RemoveAll // Remove any and all existing event handlers for a given target.
    };

    // Returns true if |eventType| belongs to a class this registry tracks.
    static bool eventTypeToClass(const AtomicString& eventType, EventHandlerClass* result);

    // Returns true if the operation actually added a new target or completely
    // removed an existing one.
    bool updateEventHandlerTargets(ChangeOperation, EventHandlerClass, EventTarget*);

    // Called on the EventHandlerRegistry of the root Document to notify
    // clients when we have added the first handler or removed the last one for
    // a given event class. |hasActiveHandlers| can be used to distinguish
    // between the two cases.
    void notifyHasHandlersChanged(EventHandlerClass, bool hasActiveHandlers);

    // Record a change operation to a given event handler class and notify any
    // parent registry and other clients accordingly.
    void updateEventHandlerOfType(ChangeOperation, const AtomicString& eventType, EventTarget*);

    void updateEventHandlerInternal(ChangeOperation, EventHandlerClass, EventTarget*);

    void updateAllEventHandlers(ChangeOperation, EventTarget&);

    void checkConsistency() const;

    FrameHost& m_frameHost;
    EventTargetSet m_targets[EventHandlerClassCount];
};

} // namespace WebCore

#endif // EventHandlerRegistry_h
