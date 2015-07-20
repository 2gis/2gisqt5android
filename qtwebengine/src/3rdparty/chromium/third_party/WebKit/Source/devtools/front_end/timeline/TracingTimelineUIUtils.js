// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @extends {WebInspector.TimelineUIUtils}
 */
WebInspector.TracingTimelineUIUtils = function()
{
    WebInspector.TimelineUIUtils.call(this);
}

WebInspector.TracingTimelineUIUtils.prototype = {
    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @return {boolean}
     */
    isBeginFrame: function(record)
    {
        return record.type() === WebInspector.TracingTimelineModel.RecordType.BeginFrame;
    },

    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @return {boolean}
     */
    isProgram: function(record)
    {
        return record.type() === WebInspector.TracingTimelineModel.RecordType.Program;
    },

    /**
     * @param {string} recordType
     * @return {boolean}
     */
    isCoalescable: function(recordType)
    {
        return !!WebInspector.TracingTimelineUIUtils._coalescableRecordTypes[recordType];
    },

    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @return {boolean}
     */
    isEventDivider: function(record)
    {
        return WebInspector.TracingTimelineUIUtils.isMarkerEvent(record.traceEvent());
    },

    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @return {?Object}
     */
    countersForRecord: function(record)
    {
        return record.type() === WebInspector.TracingTimelineModel.RecordType.UpdateCounters ? record.data() : null;
    },

    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @return {?Object}
     */
    highlightQuadForRecord: function(record)
    {
        return record.traceEvent().highlightQuad || null;
    },

    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @return {string}
     */
    titleForRecord: function(record)
    {
        var event = record.traceEvent();
        return WebInspector.TracingTimelineUIUtils.eventTitle(event, record.timelineModel());
    },

    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @return {!WebInspector.TimelineCategory}
     */
    categoryForRecord: function(record)
    {
        return WebInspector.TracingTimelineUIUtils.eventStyle(record.traceEvent()).category;
    },

    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @param {!WebInspector.Linkifier} linkifier
     * @return {?Node}
     */
    buildDetailsNode: function(record, linkifier)
    {
        return WebInspector.TracingTimelineUIUtils.buildDetailsNodeForTraceEvent(record.traceEvent(), linkifier);
    },

    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @param {!WebInspector.TimelineModel} model
     * @param {!WebInspector.Linkifier} linkifier
     * @param {function(!DocumentFragment)} callback
     */
    generateDetailsContent: function(record, model, linkifier, callback)
    {
        if (!(model instanceof WebInspector.TracingTimelineModel))
            throw new Error("Illegal argument.");
        var tracingTimelineModel = /** @type {!WebInspector.TracingTimelineModel} */ (model);
        WebInspector.TracingTimelineUIUtils.buildTraceEventDetails(record.traceEvent(), tracingTimelineModel, linkifier, callback);
    },

    /**
     * @return {!Element}
     */
    createBeginFrameDivider: function()
    {
        return this.createEventDivider(WebInspector.TracingTimelineModel.RecordType.BeginFrame);
    },

    /**
     * @param {string} recordType
     * @param {string=} title
     * @return {!Element}
     */
    createEventDivider: function(recordType, title)
    {
        return WebInspector.TracingTimelineUIUtils._createEventDivider(recordType, title);
    },

    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @param {!RegExp} regExp
     * @return {boolean}
     */
    testContentMatching: function(record, regExp)
    {
        var traceEvent = record.traceEvent();
        var title = WebInspector.TracingTimelineUIUtils.eventStyle(traceEvent).title;
        var tokens = [title];
        for (var argName in traceEvent.args) {
            var argValue = traceEvent.args[argName];
            for (var key in argValue)
                tokens.push(argValue[key]);
        }
        return regExp.test(tokens.join("|"));
    },

    /**
     * @param {!Object} total
     * @param {!WebInspector.TimelineModel.Record} record
     */
    aggregateTimeForRecord: function(total, record)
    {
        var traceEvent = record.traceEvent();
        var model = record._model;
        WebInspector.TracingTimelineUIUtils._aggregatedStatsForTraceEvent(total, model, traceEvent);
    },

    /**
     * @return {!WebInspector.TimelineModel.Filter}
     */
    hiddenRecordsFilter: function()
    {
        return new WebInspector.TimelineRecordVisibleTypeFilter(WebInspector.TracingTimelineUIUtils._visibleTypes());
    },

    /**
     * @return {?WebInspector.TimelineModel.Filter}
     */
    hiddenEmptyRecordsFilter: function()
    {
        var hiddenEmptyRecords = [WebInspector.TimelineModel.RecordType.EventDispatch, WebInspector.TimelineModel.RecordType.UpdateCounters];
        return new WebInspector.TimelineRecordHiddenEmptyTypeFilter(hiddenEmptyRecords);
    },

    __proto__: WebInspector.TimelineUIUtils.prototype
}

/**
 * @constructor
 * @param {string} title
 * @param {!WebInspector.TimelineCategory} category
 * @param {boolean=} hidden
 */
WebInspector.TimelineRecordStyle = function(title, category, hidden)
{
    this.title = title;
    this.category = category;
    this.hidden = !!hidden;
}

/**
 * @return {!Object.<string, !WebInspector.TimelineRecordStyle>}
 */
WebInspector.TracingTimelineUIUtils._initEventStyles = function()
{
    if (WebInspector.TracingTimelineUIUtils._eventStylesMap)
        return WebInspector.TracingTimelineUIUtils._eventStylesMap;

    var recordTypes = WebInspector.TracingTimelineModel.RecordType;
    var categories = WebInspector.TimelineUIUtils.categories();

    var eventStyles = {};
    eventStyles[recordTypes.Program] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Other"), categories["other"]);
    eventStyles[recordTypes.EventDispatch] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Event"), categories["scripting"]);
    eventStyles[recordTypes.RequestMainThreadFrame] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Request Main Thread Frame"), categories["rendering"], true);
    eventStyles[recordTypes.BeginFrame] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Frame Start"), categories["rendering"], true);
    eventStyles[recordTypes.BeginMainThreadFrame] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Frame Start (main thread)"), categories["rendering"], true);
    eventStyles[recordTypes.DrawFrame] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Draw Frame"), categories["rendering"], true);
    eventStyles[recordTypes.ScheduleStyleRecalculation] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Schedule Style Recalculation"), categories["rendering"], true);
    eventStyles[recordTypes.RecalculateStyles] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Recalculate Style"), categories["rendering"]);
    eventStyles[recordTypes.InvalidateLayout] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Invalidate Layout"), categories["rendering"], true);
    eventStyles[recordTypes.Layout] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Layout"), categories["rendering"]);
    eventStyles[recordTypes.PaintSetup] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Paint Setup"), categories["painting"]);
    eventStyles[recordTypes.UpdateLayer] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Update Layer"), categories["painting"], true);
    eventStyles[recordTypes.UpdateLayerTree] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Update Layer Tree"), categories["rendering"]);
    eventStyles[recordTypes.Paint] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Paint"), categories["painting"]);
    eventStyles[recordTypes.RasterTask] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Paint"), categories["painting"]);
    eventStyles[recordTypes.ScrollLayer] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Scroll"), categories["rendering"]);
    eventStyles[recordTypes.CompositeLayers] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Composite Layers"), categories["painting"]);
    eventStyles[recordTypes.ParseHTML] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Parse HTML"), categories["loading"]);
    eventStyles[recordTypes.TimerInstall] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Install Timer"), categories["scripting"]);
    eventStyles[recordTypes.TimerRemove] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Remove Timer"), categories["scripting"]);
    eventStyles[recordTypes.TimerFire] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Timer Fired"), categories["scripting"]);
    eventStyles[recordTypes.XHRReadyStateChange] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("XHR Ready State Change"), categories["scripting"]);
    eventStyles[recordTypes.XHRLoad] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("XHR Load"), categories["scripting"]);
    eventStyles[recordTypes.EvaluateScript] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Evaluate Script"), categories["scripting"]);
    eventStyles[recordTypes.MarkLoad] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Load event"), categories["scripting"], true);
    eventStyles[recordTypes.MarkDOMContent] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("DOMContentLoaded event"), categories["scripting"], true);
    eventStyles[recordTypes.MarkFirstPaint] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("First paint"), categories["painting"], true);
    eventStyles[recordTypes.TimeStamp] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Timestamp"), categories["scripting"]);
    eventStyles[recordTypes.ConsoleTime] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Console Time"), categories["scripting"]);
    eventStyles[recordTypes.ResourceSendRequest] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Send Request"), categories["loading"]);
    eventStyles[recordTypes.ResourceReceiveResponse] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Receive Response"), categories["loading"]);
    eventStyles[recordTypes.ResourceFinish] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Finish Loading"), categories["loading"]);
    eventStyles[recordTypes.ResourceReceivedData] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Receive Data"), categories["loading"]);
    eventStyles[recordTypes.FunctionCall] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Function Call"), categories["scripting"]);
    eventStyles[recordTypes.GCEvent] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("GC Event"), categories["scripting"]);
    eventStyles[recordTypes.JSFrame] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("JS Frame"), categories["scripting"]);
    eventStyles[recordTypes.RequestAnimationFrame] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Request Animation Frame"), categories["scripting"]);
    eventStyles[recordTypes.CancelAnimationFrame] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Cancel Animation Frame"), categories["scripting"]);
    eventStyles[recordTypes.FireAnimationFrame] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Animation Frame Fired"), categories["scripting"]);
    eventStyles[recordTypes.WebSocketCreate] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Create WebSocket"), categories["scripting"]);
    eventStyles[recordTypes.WebSocketSendHandshakeRequest] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Send WebSocket Handshake"), categories["scripting"]);
    eventStyles[recordTypes.WebSocketReceiveHandshakeResponse] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Receive WebSocket Handshake"), categories["scripting"]);
    eventStyles[recordTypes.WebSocketDestroy] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Destroy WebSocket"), categories["scripting"]);
    eventStyles[recordTypes.EmbedderCallback] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Embedder Callback"), categories["scripting"]);
    eventStyles[recordTypes.DecodeImage] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Image Decode"), categories["painting"]);
    eventStyles[recordTypes.ResizeImage] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Image Resize"), categories["painting"]);
    eventStyles[recordTypes.UpdateCounters] = new WebInspector.TimelineRecordStyle(WebInspector.UIString("UpdateCounters"), categories["other"]);
    WebInspector.TracingTimelineUIUtils._eventStylesMap = eventStyles;
    return eventStyles;
}

WebInspector.TracingTimelineUIUtils._coalescableRecordTypes = {};
WebInspector.TracingTimelineUIUtils._coalescableRecordTypes[WebInspector.TracingTimelineModel.RecordType.Layout] = 1;
WebInspector.TracingTimelineUIUtils._coalescableRecordTypes[WebInspector.TracingTimelineModel.RecordType.Paint] = 1;
WebInspector.TracingTimelineUIUtils._coalescableRecordTypes[WebInspector.TracingTimelineModel.RecordType.RasterTask] = 1;
WebInspector.TracingTimelineUIUtils._coalescableRecordTypes[WebInspector.TracingTimelineModel.RecordType.DecodeImage] = 1;
WebInspector.TracingTimelineUIUtils._coalescableRecordTypes[WebInspector.TracingTimelineModel.RecordType.ResizeImage] = 1;

/**
 * @param {!WebInspector.TracingModel.Event} event
 * @return {!{title: string, category: !WebInspector.TimelineCategory}}
 */
WebInspector.TracingTimelineUIUtils.eventStyle = function(event)
{
    var eventStyles = WebInspector.TracingTimelineUIUtils._initEventStyles();
    if (event.category === WebInspector.TracingModel.ConsoleEventCategory)
        return { title: event.name, category: WebInspector.TimelineUIUtils.categories()["scripting"] };

    var result = eventStyles[event.name];
    if (!result) {
        result = new WebInspector.TimelineRecordStyle(WebInspector.UIString("Unknown: %s", event.name),  WebInspector.TimelineUIUtils.categories()["other"]);
        eventStyles[event.name] = result;
    }
    return result;
}

/**
 * @param {!WebInspector.TracingModel.Event} event
 * @return {string}
 */
WebInspector.TracingTimelineUIUtils.markerEventColor = function(event)
{
    var red = "rgb(255, 0, 0)";
    var blue = "rgb(0, 0, 255)";
    var orange = "rgb(255, 178, 23)";
    var green = "rgb(0, 130, 0)";

    if (event.category === WebInspector.TracingModel.ConsoleEventCategory)
        return orange;

    var recordTypes = WebInspector.TracingTimelineModel.RecordType;
    var eventName = event.name;
    switch (eventName) {
    case recordTypes.MarkDOMContent: return blue;
    case recordTypes.MarkLoad: return red;
    case recordTypes.MarkFirstPaint: return green;
    case recordTypes.TimeStamp: return orange;
    }
    return green;
}

/**
 * @param {!WebInspector.TracingModel.Event} event
 * @param {!WebInspector.TimelineModel} model
 * @return {string}
 */
WebInspector.TracingTimelineUIUtils.eventTitle = function(event, model)
{
    var title = WebInspector.TracingTimelineUIUtils.eventStyle(event).title;
    if (event.category === WebInspector.TracingModel.ConsoleEventCategory)
        return title;
    if (event.name === WebInspector.TracingTimelineModel.RecordType.TimeStamp)
        return WebInspector.UIString("%s: %s", title, event.args["data"]["message"]);
    if (WebInspector.TracingTimelineUIUtils.isMarkerEvent(event)) {
        var startTime = Number.millisToString(event.startTime - model.minimumRecordTime());
        return WebInspector.UIString("%s at %s", title, startTime);
    }
    return title;
}

/**
 * @param {!WebInspector.TracingModel.Event} event
 * @return {boolean}
 */
WebInspector.TracingTimelineUIUtils.isMarkerEvent = function(event)
{
    var recordTypes = WebInspector.TracingTimelineModel.RecordType;
    switch (event.name) {
    case recordTypes.TimeStamp:
    case recordTypes.MarkFirstPaint:
        return true;
    case recordTypes.MarkDOMContent:
    case recordTypes.MarkLoad:
        return event.args["data"]["isMainFrame"];
    default:
        return false;
    }
}

/**
 * @param {!WebInspector.TracingModel.Event} event
 * @return {boolean}
 */
WebInspector.TracingTimelineUIUtils.isTallMarkerEvent = function(event)
{
    return event.name !== WebInspector.TracingTimelineModel.RecordType.TimeStamp;
}

/**
 * @param {!WebInspector.TracingModel.Event} event
 * @param {!WebInspector.Linkifier} linkifier
 * @return {?Node}
 */
WebInspector.TracingTimelineUIUtils.buildDetailsNodeForTraceEvent = function(event, linkifier)
{
    var recordType = WebInspector.TracingTimelineModel.RecordType;
    var target = event.thread.target();
    var details;
    var detailsText;
    var eventData = event.args["data"];
    switch (event.name) {
    case recordType.GCEvent:
        var delta = event.args["usedHeapSizeBefore"] - event.args["usedHeapSizeAfter"];
        detailsText = WebInspector.UIString("%s collected", Number.bytesToString(delta));
        break;
    case recordType.TimerFire:
        detailsText = eventData["timerId"];
        break;
    case recordType.FunctionCall:
        details = linkifyLocation(eventData["scriptId"], eventData["scriptName"], eventData["scriptLine"], 0);
        break;
    case recordType.JSFrame:
        details = linkifyLocation(eventData["scriptId"], eventData["url"], eventData["lineNumber"], eventData["columnNumber"]);
        detailsText = WebInspector.CPUProfileDataModel.beautifyFunctionName(eventData["functionName"]);
        if (details && detailsText)
            details.textContent = detailsText;
        break;
    case recordType.FireAnimationFrame:
        detailsText = eventData["id"];
        break;
    case recordType.EventDispatch:
        detailsText = eventData ? eventData["type"] : null;
        break;
    case recordType.Paint:
        var width = WebInspector.TimelineUIUtils.quadWidth(eventData.clip);
        var height = WebInspector.TimelineUIUtils.quadHeight(eventData.clip);
        if (width && height)
            detailsText = WebInspector.UIString("%d\u2009\u00d7\u2009%d", width, height);
        break;
    case recordType.TimerInstall:
    case recordType.TimerRemove:
        details = linkifyTopCallFrame();
        detailsText = eventData["timerId"];
        break;
    case recordType.RequestAnimationFrame:
    case recordType.CancelAnimationFrame:
        details = linkifyTopCallFrame();
        detailsText = eventData["id"];
        break;
    case recordType.ParseHTML:
    case recordType.RecalculateStyles:
        details = linkifyTopCallFrame();
        break;
    case recordType.EvaluateScript:
        var url = eventData["url"];
        if (url)
            details = linkifyLocation("", url, eventData["lineNumber"], 0);
        break;
    case recordType.XHRReadyStateChange:
    case recordType.XHRLoad:
    case recordType.ResourceSendRequest:
        var url = eventData["url"];
        if (url)
            detailsText = WebInspector.displayNameForURL(url);
        break;
    case recordType.ResourceReceivedData:
    case recordType.ResourceReceiveResponse:
    case recordType.ResourceFinish:
        var initiator = event.initiator;
        if (initiator) {
            var url = initiator.args["data"]["url"];
            if (url)
                detailsText = WebInspector.displayNameForURL(url);
        }
        break;
    case recordType.EmbedderCallback:
        detailsText = eventData["callbackName"];
        break;

    case recordType.PaintImage:
    case recordType.DecodeImage:
    case recordType.ResizeImage:
    case recordType.DecodeLazyPixelRef:
            var url = event.imageURL;
            if (url)
                detailsText = WebInspector.displayNameForURL(url);
        break;

    default:
        if (event.category === WebInspector.TracingModel.ConsoleEventCategory)
            detailsText = null;
        else
            details = linkifyTopCallFrame();
        break;
    }

    if (!details && detailsText)
        details = createTextNode(detailsText);
    return details;

    /**
     * @param {string} scriptId
     * @param {string} url
     * @param {number} lineNumber
     * @param {number=} columnNumber
     */
    function linkifyLocation(scriptId, url, lineNumber, columnNumber)
    {
        if (!url)
            return null;

        // FIXME(62725): stack trace line/column numbers are one-based.
        return linkifier.linkifyScriptLocation(target, scriptId, url, lineNumber - 1, (columnNumber ||1) - 1, "timeline-details");
    }

    /**
     * @return {?Element}
     */
    function linkifyTopCallFrame()
    {
        var stackTrace = event.stackTrace;
        if (!stackTrace) {
            var initiator = event.initiator;
            if (initiator)
                stackTrace = initiator.stackTrace;
        }
        if (!stackTrace || !stackTrace.length)
            return null;
        return linkifier.linkifyConsoleCallFrame(target, stackTrace[0], "timeline-details");
    }
}

/**
 * @param {!WebInspector.TracingModel.Event} event
 * @param {!WebInspector.TracingTimelineModel} model
 * @param {!WebInspector.Linkifier} linkifier
 * @param {function(!DocumentFragment)} callback
 */
WebInspector.TracingTimelineUIUtils.buildTraceEventDetails = function(event, model, linkifier, callback)
{
    var target = event.thread.target();
    var relatedNode = null;
    var barrier = new CallbackBarrier();
    if (!event.previewElement) {
        if (event.imageURL && target)
            WebInspector.DOMPresentationUtils.buildImagePreviewContents(target, event.imageURL, false, barrier.createCallback(saveImage));
        else if (event.picture)
            WebInspector.TracingTimelineUIUtils.buildPicturePreviewContent(event, barrier.createCallback(saveImage));
    }
    if (event.backendNodeId && target)
        target.domModel.pushNodesByBackendIdsToFrontend([event.backendNodeId], barrier.createCallback(setRelatedNode));
    if (event.invalidationTrackingEvents)
        WebInspector.TracingTimelineUIUtils._pushInvalidationNodeIdsToFrontend(event, barrier.createCallback(updateInvalidationNodeIds));
    barrier.callWhenDone(callbackWrapper);

    /**
     * @param {!Element=} element
     */
    function saveImage(element)
    {
        event.previewElement = element || null;
    }

    /**
     * @param {?Array.<!DOMAgent.NodeId>} nodeIds
     */
    function setRelatedNode(nodeIds)
    {
        if (nodeIds)
            relatedNode = target.domModel.nodeForId(nodeIds[0]);
    }

    /**
     * @param {?Array.<!DOMAgent.NodeId>} frontendNodeIds
     * @param {?Array.<!DOMAgent.NodeId>} backendNodeIds
     */
    function updateInvalidationNodeIds(frontendNodeIds, backendNodeIds)
    {
        if (!frontendNodeIds)
            return;
        if (frontendNodeIds.length !== backendNodeIds.length) {
            console.error("Did not resolve the correct number of invalidation node ids.");
            return;
        }

        var backendToFrontendNodeIdMap = {};
        backendNodeIds.forEach(function(backendNodeId, index) {
            backendToFrontendNodeIdMap[backendNodeId] = frontendNodeIds[index];
        });

        if (event.nodeId)
            event.frontendNodeId = backendToFrontendNodeIdMap[event.nodeId];
        event.invalidationTrackingEvents.forEach(function(invalidation) {
            if (invalidation.nodeId)
                invalidation.frontendNodeId = backendToFrontendNodeIdMap[invalidation.nodeId];
        });
    }

    function callbackWrapper()
    {
        callback(WebInspector.TracingTimelineUIUtils._buildTraceEventDetailsSynchronously(event, model, linkifier, relatedNode));
    }
}

/**
 * @param {!WebInspector.TracingModel.Event} event
 * @param {!WebInspector.TracingTimelineModel} model
 * @param {!WebInspector.Linkifier} linkifier
 * @param {?WebInspector.DOMNode} relatedNode
 * @return {!DocumentFragment}
 */
WebInspector.TracingTimelineUIUtils._buildTraceEventDetailsSynchronously = function(event, model, linkifier, relatedNode)
{
    var fragment = createDocumentFragment();
    var stats = {};
    var hasChildren = WebInspector.TracingTimelineUIUtils._aggregatedStatsForTraceEvent(stats, model, event);

    var selfTime = event.selfTime;
    var selfCategory = WebInspector.TracingTimelineUIUtils.eventStyle(event).category;
    // JSFrame events have 0 selfTime so we need to work around this below and add the event's time to scripting category.
    if (event.name === WebInspector.TracingTimelineModel.RecordType.JSFrame && !event.selfTime && event.duration) {
        selfTime = event.duration;
        for (var categoryName in stats)
            selfTime -= stats[categoryName];
        stats[selfCategory.name] = selfTime + (stats[selfCategory.name] || 0);
    }

    var pieChart = hasChildren ?
        WebInspector.TimelineUIUtils.generatePieChart(stats, selfCategory, selfTime) :
        WebInspector.TimelineUIUtils.generatePieChart(stats);

    var recordTypes = WebInspector.TracingTimelineModel.RecordType;

    // This message may vary per event.name;
    var relatedNodeLabel;

    var contentHelper = new WebInspector.TimelineDetailsContentHelper(event.thread.target(), linkifier, true);
    contentHelper.appendTextRow(WebInspector.UIString("Type"), WebInspector.TracingTimelineUIUtils.eventTitle(event, model));
    contentHelper.appendTextRow(WebInspector.UIString("Self Time"), Number.millisToString(event.selfTime, true));
    contentHelper.appendTextRow(WebInspector.UIString("Start Time"), Number.millisToString((event.startTime - model.minimumRecordTime())));
    contentHelper.appendElementRow(WebInspector.UIString("Aggregated Time"), pieChart);
    var eventData = event.args["data"];
    var initiator = event.initiator;

    switch (event.name) {
    case recordTypes.GCEvent:
        var delta = event.args["usedHeapSizeBefore"] - event.args["usedHeapSizeAfter"];
        contentHelper.appendTextRow(WebInspector.UIString("Collected"), Number.bytesToString(delta));
        break;
    case recordTypes.TimerFire:
    case recordTypes.TimerInstall:
    case recordTypes.TimerRemove:
        contentHelper.appendTextRow(WebInspector.UIString("Timer ID"), eventData["timerId"]);
        if (event.name === recordTypes.TimerInstall) {
            contentHelper.appendTextRow(WebInspector.UIString("Timeout"), Number.millisToString(eventData["timeout"]));
            contentHelper.appendTextRow(WebInspector.UIString("Repeats"), !eventData["singleShot"]);
        }
        break;
    case recordTypes.FireAnimationFrame:
        contentHelper.appendTextRow(WebInspector.UIString("Callback ID"), eventData["id"]);
        break;
    case recordTypes.FunctionCall:
        if (eventData["scriptName"])
            contentHelper.appendLocationRow(WebInspector.UIString("Location"), eventData["scriptName"], eventData["scriptLine"]);
        break;
    case recordTypes.ResourceSendRequest:
    case recordTypes.ResourceReceiveResponse:
    case recordTypes.ResourceReceivedData:
    case recordTypes.ResourceFinish:
        var url = (event.name === recordTypes.ResourceSendRequest) ? eventData["url"] : initiator.args["data"]["url"];
        if (url)
            contentHelper.appendElementRow(WebInspector.UIString("Resource"), WebInspector.linkifyResourceAsNode(url));
        if (eventData["requestMethod"])
            contentHelper.appendTextRow(WebInspector.UIString("Request Method"), eventData["requestMethod"]);
        if (typeof eventData["statusCode"] === "number")
            contentHelper.appendTextRow(WebInspector.UIString("Status Code"), eventData["statusCode"]);
        if (eventData["mimeType"])
            contentHelper.appendTextRow(WebInspector.UIString("MIME Type"), eventData["mimeType"]);
        if (eventData["encodedDataLength"])
            contentHelper.appendTextRow(WebInspector.UIString("Encoded Data Length"), WebInspector.UIString("%d Bytes", eventData["encodedDataLength"]));
        break;
    case recordTypes.EvaluateScript:
        var url = eventData["url"];
        if (url)
            contentHelper.appendLocationRow(WebInspector.UIString("Script"), url, eventData["lineNumber"]);
        break;
    case recordTypes.Paint:
        var clip = eventData["clip"];
        contentHelper.appendTextRow(WebInspector.UIString("Location"), WebInspector.UIString("(%d, %d)", clip[0], clip[1]));
        var clipWidth = WebInspector.TimelineUIUtils.quadWidth(clip);
        var clipHeight = WebInspector.TimelineUIUtils.quadHeight(clip);
        contentHelper.appendTextRow(WebInspector.UIString("Dimensions"), WebInspector.UIString("%d × %d", clipWidth, clipHeight));
        // Fall-through intended.

    case recordTypes.PaintSetup:
    case recordTypes.Rasterize:
    case recordTypes.ScrollLayer:
        relatedNodeLabel = WebInspector.UIString("Layer root");
        break;
    case recordTypes.PaintImage:
    case recordTypes.DecodeLazyPixelRef:
    case recordTypes.DecodeImage:
    case recordTypes.ResizeImage:
    case recordTypes.DrawLazyPixelRef:
        relatedNodeLabel = WebInspector.UIString("Image element");
        if (event.imageURL)
            contentHelper.appendElementRow(WebInspector.UIString("Image URL"), WebInspector.linkifyResourceAsNode(event.imageURL));
        break;
    case recordTypes.RecalculateStyles: // We don't want to see default details.
        contentHelper.appendTextRow(WebInspector.UIString("Elements affected"), event.args["elementCount"]);
        break;
    case recordTypes.Layout:
        var beginData = event.args["beginData"];
        contentHelper.appendTextRow(WebInspector.UIString("Nodes that need layout"), beginData["dirtyObjects"]);
        contentHelper.appendTextRow(WebInspector.UIString("Layout tree size"), beginData["totalObjects"]);
        contentHelper.appendTextRow(WebInspector.UIString("Layout scope"),
                                    beginData["partialLayout"] ? WebInspector.UIString("Partial") : WebInspector.UIString("Whole document"));
        relatedNodeLabel = WebInspector.UIString("Layout root");
        break;
    case recordTypes.ConsoleTime:
        contentHelper.appendTextRow(WebInspector.UIString("Message"), event.name);
        break;
    case recordTypes.WebSocketCreate:
    case recordTypes.WebSocketSendHandshakeRequest:
    case recordTypes.WebSocketReceiveHandshakeResponse:
    case recordTypes.WebSocketDestroy:
        var initiatorData = initiator ? initiator.args["data"] : eventData;
        if (typeof initiatorData["webSocketURL"] !== "undefined")
            contentHelper.appendTextRow(WebInspector.UIString("URL"), initiatorData["webSocketURL"]);
        if (typeof initiatorData["webSocketProtocol"] !== "undefined")
            contentHelper.appendTextRow(WebInspector.UIString("WebSocket Protocol"), initiatorData["webSocketProtocol"]);
        if (typeof eventData["message"] !== "undefined")
            contentHelper.appendTextRow(WebInspector.UIString("Message"), eventData["message"]);
        break;
    case recordTypes.EmbedderCallback:
        contentHelper.appendTextRow(WebInspector.UIString("Callback Function"), eventData["callbackName"]);
        break;
    default:
        var detailsNode = WebInspector.TracingTimelineUIUtils.buildDetailsNodeForTraceEvent(event, linkifier);
        if (detailsNode)
            contentHelper.appendElementRow(WebInspector.UIString("Details"), detailsNode);
        break;
    }

    if (relatedNode)
        contentHelper.appendElementRow(relatedNodeLabel || WebInspector.UIString("Related node"), WebInspector.DOMPresentationUtils.linkifyNodeReference(relatedNode));

    if (eventData && eventData["scriptName"] && event.name !== recordTypes.FunctionCall)
        contentHelper.appendLocationRow(WebInspector.UIString("Function Call"), eventData["scriptName"], eventData["scriptLine"]);

    var warning = event.warning;
    if (warning) {
        var div = createElement("div");
        div.textContent = warning;
        contentHelper.appendElementRow(WebInspector.UIString("Warning"), div);
    }
    if (event.previewElement)
        contentHelper.appendElementRow(WebInspector.UIString("Preview"), event.previewElement);

    if (event.stackTrace || (event.initiator && event.initiator.stackTrace) || event.invalidationTrackingEvents)
        WebInspector.TracingTimelineUIUtils._generateCauses(event, contentHelper);

    fragment.appendChild(contentHelper.element);

    return fragment;
}

/**
 * @param {!WebInspector.TracingModel.Event} event
 * @param {!WebInspector.TimelineDetailsContentHelper} contentHelper
 */
WebInspector.TracingTimelineUIUtils._generateCauses = function(event, contentHelper)
{
    var recordTypes = WebInspector.TracingTimelineModel.RecordType;

    var callSiteStackLabel;
    var stackLabel;
    var initiator = event.initiator;

    switch (event.name) {
    case recordTypes.TimerFire:
        callSiteStackLabel = WebInspector.UIString("Timer installed");
        break;
    case recordTypes.FireAnimationFrame:
        callSiteStackLabel = WebInspector.UIString("Animation frame requested");
        break;
    case recordTypes.RecalculateStyles:
        stackLabel = WebInspector.UIString("Recalculation was forced");
        break;
    case recordTypes.Layout:
        callSiteStackLabel = WebInspector.UIString("First layout invalidation");
        stackLabel = WebInspector.UIString("Layout forced");
        break;
    }

    // Direct cause.
    if (event.stackTrace)
        contentHelper.appendStackTrace(stackLabel || WebInspector.UIString("Stack trace"), event.stackTrace);

    // Indirect causes.
    if (event.invalidationTrackingEvents) { // Full invalidation tracking (experimental).
        WebInspector.TracingTimelineUIUtils._generateInvalidations(event, contentHelper);
    } else if (initiator && initiator.stackTrace) { // Partial invalidation tracking.
        contentHelper.appendStackTrace(callSiteStackLabel || WebInspector.UIString("First invalidated"), initiator.stackTrace);
    }
}

/**
 * @param {!WebInspector.TracingModel.Event} event
 * @param {!WebInspector.TimelineDetailsContentHelper} contentHelper
 */
WebInspector.TracingTimelineUIUtils._generateInvalidations = function(event, contentHelper)
{
    if (!event.invalidationTrackingEvents)
        return;

    var target = event.thread.target();
    var invalidations = {};
    event.invalidationTrackingEvents.forEach(function(invalidation) {
        if (!invalidations[invalidation.type])
            invalidations[invalidation.type] = [invalidation];
        else
            invalidations[invalidation.type].push(invalidation);
    });

    Object.keys(invalidations).forEach(function(type) {
        WebInspector.TracingTimelineUIUtils._generateInvalidationsForType(
            type, target, invalidations[type], contentHelper);
    });
}

/**
 * @param {string} type
 * @param {?WebInspector.Target} target
 * @param {!Object} invalidationEvents
 * @param {!WebInspector.TimelineDetailsContentHelper} contentHelper
 */
WebInspector.TracingTimelineUIUtils._generateInvalidationsForType = function(type, target, invalidationEvents, contentHelper)
{
    var title;
    switch (type) {
    case WebInspector.TracingTimelineModel.RecordType.StyleRecalcInvalidationTracking:
        title = WebInspector.UIString("Style invalidations");
        break;
    case WebInspector.TracingTimelineModel.RecordType.LayoutInvalidationTracking:
        title = WebInspector.UIString("Layout invalidations");
        break;
    default:
        title = WebInspector.UIString("Other invalidations");
        break;
    }

    var detailsNode = createElementWithClass("div", "timeline-details-view-row");
    var titleElement = detailsNode.createChild("span", "timeline-details-view-row-title");
    titleElement.textContent = WebInspector.UIString("%s: ", title);
    var eventsList = detailsNode.createChild("ol");
    invalidationEvents.forEach(appendInvalidations);

    contentHelper.element.appendChild(detailsNode);


    function appendInvalidations(invalidation, index)
    {
        var row = eventsList.createChild("li");
        var nodeRow = row.createChild("div");
        var node = target.domModel.nodeForId(invalidation.frontendNodeId);
        if (node)
            nodeRow.appendChild(WebInspector.DOMPresentationUtils.linkifyNodeReference(node));
        else if (invalidation.nodeName)
            nodeRow.textContent = WebInspector.UIString("[ %s ]", invalidation.nodeName);
        else
            nodeRow.textContent = WebInspector.UIString("[ unknown node ]");

        if (invalidation.reason) {
            var reasonRow = row.createChild("div");
            var reason = invalidation.reason;
            reasonRow.textContent = WebInspector.UIString("Reason: %s.", reason);
        }

        if (invalidation.stackTrace)
            contentHelper.createChildStackTraceElement(row, invalidation.stackTrace);
    }
}

/**
 * @param {!WebInspector.TracingModel.Event} event
 * @param {function(?Array.<number>, ?Array.<number>)} callback
 */
WebInspector.TracingTimelineUIUtils._pushInvalidationNodeIdsToFrontend = function(event, callback)
{
    var backendNodeIds = [];

    var dedupedNodeIds = {};
    if (event.nodeId) {
        backendNodeIds.push(event.nodeId);
        dedupedNodeIds[event.nodeId] = true;
    }
    event.invalidationTrackingEvents.forEach(function(invalidation) {
        if (invalidation.nodeId && !dedupedNodeIds[invalidation.nodeId]) {
            backendNodeIds.push(invalidation.nodeId);
            dedupedNodeIds[invalidation.nodeId] = true;
        }
    });

    var target = event.thread.target();
    target.domModel.pushNodesByBackendIdsToFrontend(backendNodeIds, function(frontendNodeIds) {
        callback(frontendNodeIds, backendNodeIds);
    });
}

/**
 * @param {!Object} total
 * @param {!WebInspector.TracingTimelineModel} model
 * @param {!WebInspector.TracingModel.Event} event
 * @return {boolean}
 */
WebInspector.TracingTimelineUIUtils._aggregatedStatsForTraceEvent = function(total, model, event)
{
    var events = model.inspectedTargetEvents();
    /**
     * @param {number} startTime
     * @param {!WebInspector.TracingModel.Event} e
     * @return {number}
     */
    function eventComparator(startTime, e)
    {
        return startTime - e.startTime;
    }
    var index = events.binaryIndexOf(event.startTime, eventComparator);
    var hasChildren = false;
    var endTime = event.endTime;
    if (endTime) {
        for (var i = index; i < events.length; i++) {
            var nextEvent = events[i];
            if (nextEvent.startTime >= endTime)
                break;
            if (!nextEvent.selfTime)
                continue;
            if (nextEvent.thread !== event.thread)
                continue;
            if (i > index)
                hasChildren = true;
            var categoryName = WebInspector.TracingTimelineUIUtils.eventStyle(nextEvent).category.name;
            total[categoryName] = (total[categoryName] || 0) + nextEvent.selfTime;
        }
    }
    if (WebInspector.TracingModel.isAsyncPhase(event.phase)) {
        if (event.endTime) {
            var aggregatedTotal = 0;
            for (var categoryName in total)
                aggregatedTotal += total[categoryName];
            total["idle"] = Math.max(0, event.endTime - event.startTime - aggregatedTotal);
        }
        return false;
    }
    return hasChildren;
}

/**
 * @param {!WebInspector.TracingModel.Event} event
 * @param {function(!Element=)} callback
 */
WebInspector.TracingTimelineUIUtils.buildPicturePreviewContent = function(event, callback)
{

    new WebInspector.LayerPaintEvent(event).loadPicture(onSnapshotLoaded);
    /**
     * @param {?Array.<number>} rect
     * @param {?WebInspector.PaintProfilerSnapshot} snapshot
     */
    function onSnapshotLoaded(rect, snapshot)
    {
        if (!snapshot) {
            callback();
            return;
        }
        snapshot.requestImage(null, null, 1, onGotImage);
        snapshot.dispose();
    }

    /**
     * @param {string=} imageURL
     */
    function onGotImage(imageURL)
    {
        if (!imageURL) {
            callback();
            return;
        }
        var container = createElement("div");
        container.className = "image-preview-container";
        var img = container.createChild("img");
        img.src = imageURL;
        callback(container);
    }
}

/**
 * @param {string} recordType
 * @param {string=} title
 * @return {!Element}
 */
WebInspector.TracingTimelineUIUtils._createEventDivider = function(recordType, title)
{
    var eventDivider = createElement("div");
    eventDivider.className = "resources-event-divider";
    var recordTypes = WebInspector.TracingTimelineModel.RecordType;

    if (recordType === recordTypes.MarkDOMContent)
        eventDivider.className += " resources-blue-divider";
    else if (recordType === recordTypes.MarkLoad)
        eventDivider.className += " resources-red-divider";
    else if (recordType === recordTypes.MarkFirstPaint)
        eventDivider.className += " resources-green-divider";
    else if (recordType === recordTypes.TimeStamp || recordType === recordTypes.ConsoleTime)
        eventDivider.className += " resources-orange-divider";
    else if (recordType === recordTypes.BeginFrame)
        eventDivider.className += " timeline-frame-divider";

    if (title)
        eventDivider.title = title;

    return eventDivider;
}

/**
 * @return {!Array.<string>}
 */
WebInspector.TracingTimelineUIUtils._visibleTypes = function()
{
    var eventStyles = WebInspector.TracingTimelineUIUtils._initEventStyles();
    var result = [];
    for (var name in eventStyles) {
        if (!eventStyles[name].hidden)
            result.push(name);
    }
    return result;
}

/**
 * @return {!WebInspector.TracingTimelineModel.Filter}
 */
WebInspector.TracingTimelineUIUtils.hiddenEventsFilter = function()
{
    return new WebInspector.TracingTimelineModel.InclusiveEventNameFilter(WebInspector.TracingTimelineUIUtils._visibleTypes());
}
