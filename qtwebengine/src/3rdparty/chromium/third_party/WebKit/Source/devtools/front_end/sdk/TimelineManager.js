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

/**
 * @constructor
 * @extends {WebInspector.SDKModel}
 * @param {!WebInspector.Target} target
 */
WebInspector.TimelineManager = function(target)
{
    WebInspector.SDKModel.call(this, WebInspector.TimelineManager, target);
    this._dispatcher = new WebInspector.TimelineDispatcher(this);
    this._enablementCount = 0;
    target.timelineAgent().enable();
}

WebInspector.TimelineManager.EventTypes = {
    TimelineStarted: "TimelineStarted",
    TimelineStopped: "TimelineStopped",
    TimelineEventRecorded: "TimelineEventRecorded",
    TimelineProgress: "TimelineProgress"
}

WebInspector.TimelineManager.prototype = {
    /**
     * @return {boolean}
     */
    isStarted: function()
    {
        return this._dispatcher.isStarted();
    },

    /**
     * @param {number=} maxCallStackDepth
     * @param {string=} liveEvents
     * @param {boolean=} includeCounters
     * @param {boolean=} includeGPUEvents
     * @param {function(?Protocol.Error)=} callback
     */
    start: function(maxCallStackDepth, liveEvents, includeCounters, includeGPUEvents, callback)
    {
        this._enablementCount++;
        WebInspector.targetManager.suspendAllTargets();
        if (this._enablementCount === 1)
            this.target().timelineAgent().start(maxCallStackDepth, true, liveEvents, includeCounters, includeGPUEvents, callback);
        else if (callback)
            callback(null);
    },

    /**
     * @param {function(?Protocol.Error)} callback
     */
    stop: function(callback)
    {
        this._enablementCount--;
        if (this._enablementCount < 0) {
            console.error("WebInspector.TimelineManager start/stop calls are unbalanced " + new Error().stack);
            return;
        }

        if (!this._enablementCount)
            this.target().timelineAgent().stop(allDoneCallback);

        /**
         * @param {?Protocol.Error} error
         */
        function allDoneCallback(error)
        {
            WebInspector.targetManager.resumeAllTargets();
            callback(error);
        }
    },

    /**
     * @param {boolean=} consoleTimeline
     * @param {!Array.<!TimelineAgent.TimelineEvent>=} events
     */
    _stopped: function(consoleTimeline, events)
    {
        var data = {
            consoleTimeline: consoleTimeline,
            events: events || []
        };
        this.dispatchEventToListeners(WebInspector.TimelineManager.EventTypes.TimelineStopped, data);
    },

    __proto__: WebInspector.SDKModel.prototype
}

/**
 * @constructor
 * @implements {TimelineAgent.Dispatcher}
 */
WebInspector.TimelineDispatcher = function(manager)
{
    this._manager = manager;
    this._manager.target().registerTimelineDispatcher(this);
}

WebInspector.TimelineDispatcher.prototype = {
    /**
     * @param {!TimelineAgent.TimelineEvent} record
     */
    eventRecorded: function(record)
    {
        this._manager.dispatchEventToListeners(WebInspector.TimelineManager.EventTypes.TimelineEventRecorded, record);
    },

    /**
     * @return {boolean}
     */
    isStarted: function()
    {
        return !!this._started;
    },

    /**
     * @param {boolean=} consoleTimeline
     */
    started: function(consoleTimeline)
    {
        this._started = true;
        this._manager.dispatchEventToListeners(WebInspector.TimelineManager.EventTypes.TimelineStarted, consoleTimeline);
    },

    /**
     * @param {boolean=} consoleTimeline
     * @param {!Array.<!TimelineAgent.TimelineEvent>=} events
     */
    stopped: function(consoleTimeline, events)
    {
        this._started = false;
        this._manager._stopped(consoleTimeline, events);
    },

    /**
     * @param {number} count
     */
    progress: function(count)
    {
        this._manager.dispatchEventToListeners(WebInspector.TimelineManager.EventTypes.TimelineProgress, count);
    }
}
