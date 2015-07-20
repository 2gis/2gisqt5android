/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
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
 */
WebInspector.TimelineUIUtils = function() { }

WebInspector.TimelineUIUtils.prototype = {
    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @return {boolean}
     */
    isBeginFrame: function(record)
    {
        throw new Error("Not implemented.");
    },
    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @return {boolean}
     */
    isProgram: function(record)
    {
        throw new Error("Not implemented.");
    },
    /**
     * @param {string} recordType
     * @return {boolean}
     */
    isCoalescable: function(recordType)
    {
        throw new Error("Not implemented.");
    },
    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @return {boolean}
     */
    isEventDivider: function(record)
    {
        throw new Error("Not implemented.");
    },
    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @return {?Object}
     */
    countersForRecord: function(record)
    {
        throw new Error("Not implemented.");
    },
    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @return {?Object}
     */
    highlightQuadForRecord: function(record)
    {
        throw new Error("Not implemented.");
    },
    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @return {string}
     */
    titleForRecord: function(record)
    {
        throw new Error("Not implemented.");
    },
    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @return {!WebInspector.TimelineCategory}
     */
    categoryForRecord: function(record)
    {
        throw new Error("Not implemented.");
    },
    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @param {!WebInspector.Linkifier} linkifier
     * @return {?Node}
     */
    buildDetailsNode: function(record, linkifier)
    {
        throw new Error("Not implemented.");
    },
    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @param {!WebInspector.TimelineModel} model
     * @param {!WebInspector.Linkifier} linkifier
     * @param {function(!DocumentFragment)} callback
     */
    generateDetailsContent: function(record, model, linkifier, callback)
    {
        throw new Error("Not implemented.");
    },
    /**
     * @return {!Element}
     */
    createBeginFrameDivider: function()
    {
        throw new Error("Not implemented.");
    },
    /**
     * @param {string} recordType
     * @param {string=} title
     * @return {!Element}
     */
    createEventDivider: function(recordType, title)
    {
        throw new Error("Not implemented.");
    },
    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @param {!RegExp} regExp
     * @return {boolean}
     */
    testContentMatching: function(record, regExp)
    {
        throw new Error("Not implemented.");
    },
    /**
     * @param {!Object} total
     * @param {!WebInspector.TimelineModel.Record} record
     */
    aggregateTimeForRecord: function(total, record)
    {
        throw new Error("Not implemented.");
    },
    /**
     * @return {!WebInspector.TimelineModel.Filter}
     */
    hiddenRecordsFilter: function()
    {
        throw new Error("Not implemented.");
    },
    /**
     * @return {?WebInspector.TimelineModel.Filter}
     */
    hiddenEmptyRecordsFilter: function()
    {
        return null;
    }
}

/**
 * @return {!Object.<string, !WebInspector.TimelineCategory>}
 */
WebInspector.TimelineUIUtils.categories = function()
{
    if (WebInspector.TimelineUIUtils._categories)
        return WebInspector.TimelineUIUtils._categories;
    WebInspector.TimelineUIUtils._categories = {
        loading: new WebInspector.TimelineCategory("loading", WebInspector.UIString("Loading"), 0, "hsl(214, 53%, 58%)", "hsl(214, 67%, 90%)", "hsl(214, 67%, 74%)", "hsl(214, 67%, 66%)"),
        scripting: new WebInspector.TimelineCategory("scripting", WebInspector.UIString("Scripting"), 1, "hsl(43, 90%, 45%)", "hsl(43, 83%, 90%)", "hsl(43, 83%, 72%)", "hsl(43, 83%, 64%) "),
        rendering: new WebInspector.TimelineCategory("rendering", WebInspector.UIString("Rendering"), 2, "hsl(256, 50%, 60%)", "hsl(256, 67%, 90%)", "hsl(256, 67%, 76%)", "hsl(256, 67%, 70%)"),
        painting: new WebInspector.TimelineCategory("painting", WebInspector.UIString("Painting"), 2, "hsl(109, 33%, 47%)", "hsl(109, 33%, 90%)", "hsl(109, 33%, 64%)", "hsl(109, 33%, 55%)"),
        other: new WebInspector.TimelineCategory("other", WebInspector.UIString("Other"), -1, "hsl(0, 0%, 73%)", "hsl(0, 0%, 90%)", "hsl(0, 0%, 87%)", "hsl(0, 0%, 79%)"),
        idle: new WebInspector.TimelineCategory("idle", WebInspector.UIString("Idle"), -1, "hsl(0, 0%, 87%)", "hsl(0, 100%, 100%)", "hsl(0, 100%, 100%)", "hsl(0, 100%, 100%)")
    };
    return WebInspector.TimelineUIUtils._categories;
};

/**
 * @param {!WebInspector.TimelineModel} model
 * @param {!{name: string, tasks: !Array.<!WebInspector.TimelineModel.Record>, firstTaskIndex: number, lastTaskIndex: number}} info
 * @return {!Element}
 */
WebInspector.TimelineUIUtils.generateMainThreadBarPopupContent = function(model, info)
{
    var firstTaskIndex = info.firstTaskIndex;
    var lastTaskIndex = info.lastTaskIndex;
    var tasks = info.tasks;
    var messageCount = lastTaskIndex - firstTaskIndex + 1;
    var cpuTime = 0;

    for (var i = firstTaskIndex; i <= lastTaskIndex; ++i) {
        var task = tasks[i];
        cpuTime += task.endTime() - task.startTime();
    }
    var startTime = tasks[firstTaskIndex].startTime();
    var endTime = tasks[lastTaskIndex].endTime();
    var duration = endTime - startTime;

    var contentHelper = new WebInspector.TimelinePopupContentHelper(info.name);
    var durationText = WebInspector.UIString("%s (at %s)", Number.millisToString(duration, true),
        Number.millisToString(startTime - model.minimumRecordTime(), true));
    contentHelper.appendTextRow(WebInspector.UIString("Duration"), durationText);
    contentHelper.appendTextRow(WebInspector.UIString("CPU time"), Number.millisToString(cpuTime, true));
    contentHelper.appendTextRow(WebInspector.UIString("Message Count"), messageCount);
    return contentHelper.contentTable();
}

/**
 * @param {!Object} aggregatedStats
 * @param {!WebInspector.TimelineCategory=} selfCategory
 * @param {number=} selfTime
 * @return {!Element}
 */
WebInspector.TimelineUIUtils.generatePieChart = function(aggregatedStats, selfCategory, selfTime)
{
    var element = createElementWithClass("div", "timeline-details-view-pie-chart hbox");

    var total = 0;
    for (var categoryName in aggregatedStats)
        total += aggregatedStats[categoryName];

    function formatter(value)
    {
        return Number.millisToString(value, true);
    }
    var pieChart = new WebInspector.PieChart(100, formatter);
    pieChart.setTotal(total);
    element.appendChild(pieChart.element);
    var footerElement = element.createChild("div", "timeline-aggregated-info timeline-aggregated-info-legend");
    var rowElement = footerElement.createChild("div");
    rowElement.createTextChild(formatter(total));

    // In case of self time, first add self, then children of the same category.
    if (selfCategory && selfTime) {
        // Self.
        pieChart.addSlice(selfTime, selfCategory.fillColorStop1);
        rowElement = footerElement.createChild("div");
        rowElement.createChild("div", "timeline-aggregated-category timeline-" + selfCategory.name);
        rowElement.createTextChild(WebInspector.UIString("%s %s (Self)", formatter(selfTime), selfCategory.title));

        // Children of the same category.
        var categoryTime = aggregatedStats[selfCategory.name];
        var value = categoryTime - selfTime;
        if (value > 0) {
            pieChart.addSlice(value, selfCategory.fillColorStop0);
            rowElement = footerElement.createChild("div");
            rowElement.createChild("div", "timeline-aggregated-category timeline-" + selfCategory.name);
            rowElement.createTextChild(WebInspector.UIString("%s %s (Children)", formatter(value), selfCategory.title));
        }
    }

    // Add other categories.
    for (var categoryName in WebInspector.TimelineUIUtils.categories()) {
        var category = WebInspector.TimelineUIUtils.categories()[categoryName];
         if (category === selfCategory)
             continue;
         var value = aggregatedStats[category.name];
         if (!value)
             continue;
         pieChart.addSlice(value, category.fillColorStop0);
         rowElement = footerElement.createChild("div");
         rowElement.createChild("div", "timeline-aggregated-category timeline-" + category.name);
         rowElement.createTextChild(WebInspector.UIString("%s %s", formatter(value), category.title));
    }
    return element;
}

/**
 * @param {!WebInspector.TimelineFrameModel} frameModel
 * @param {!WebInspector.TimelineFrame} frame
 * @return {!Element}
 */
WebInspector.TimelineUIUtils.generateDetailsContentForFrame = function(frameModel, frame)
{
    var durationInMillis = frame.endTime - frame.startTime;
    var durationText = WebInspector.UIString("%s (at %s)", Number.millisToString(frame.endTime - frame.startTime, true),
        Number.millisToString(frame.startTimeOffset, true));
    var pieChart = WebInspector.TimelineUIUtils.generatePieChart(frame.timeByCategory);
    var contentHelper = new WebInspector.TimelineDetailsContentHelper(null, null, true);
    contentHelper.appendTextRow(WebInspector.UIString("Duration"), durationText);
    contentHelper.appendTextRow(WebInspector.UIString("FPS"), Math.floor(1000 / durationInMillis));
    contentHelper.appendTextRow(WebInspector.UIString("CPU time"), Number.millisToString(frame.cpuTime, true));
    contentHelper.appendElementRow(WebInspector.UIString("Aggregated Time"), pieChart);
    if (Runtime.experiments.isEnabled("layersPanel") && frame.layerTree) {
        contentHelper.appendElementRow(WebInspector.UIString("Layer tree"),
                                       WebInspector.Linkifier.linkifyUsingRevealer(frame.layerTree, WebInspector.UIString("show")));
    }
    return contentHelper.element;
}

/**
 * @param {!CanvasRenderingContext2D} context
 * @param {number} width
 * @param {number} height
 * @param {string} color0
 * @param {string} color1
 * @param {string} color2
 * @return {!CanvasGradient}
 */
WebInspector.TimelineUIUtils.createFillStyle = function(context, width, height, color0, color1, color2)
{
    var gradient = context.createLinearGradient(0, 0, width, height);
    gradient.addColorStop(0, color0);
    gradient.addColorStop(0.25, color1);
    gradient.addColorStop(0.75, color1);
    gradient.addColorStop(1, color2);
    return gradient;
}

/**
 * @param {!CanvasRenderingContext2D} context
 * @param {number} width
 * @param {number} height
 * @param {!WebInspector.TimelineCategory} category
 * @return {!CanvasGradient}
 */
WebInspector.TimelineUIUtils.createFillStyleForCategory = function(context, width, height, category)
{
    return WebInspector.TimelineUIUtils.createFillStyle(context, width, height, category.fillColorStop0, category.fillColorStop1, category.borderColor);
}

/**
 * @param {!WebInspector.TimelineCategory} category
 * @return {string}
 */
WebInspector.TimelineUIUtils.createStyleRuleForCategory = function(category)
{
    var selector = ".timeline-category-" + category.name + " .timeline-graph-bar, " +
        ".panel.timeline .timeline-filters-header .filter-checkbox-filter.filter-checkbox-filter-" + category.name + " .checkbox-filter-checkbox, " +
        ".timeline-details-view .timeline-" + category.name + ", " +
        ".timeline-category-" + category.name + " .timeline-tree-icon"

    return selector + " { background-image: linear-gradient(" +
       category.fillColorStop0 + ", " + category.fillColorStop1 + " 25%, " + category.fillColorStop1 + " 25%, " + category.fillColorStop1 + ");" +
       " border-color: " + category.borderColor +
       "}";
}

/**
 * @param {!Array.<number>} quad
 * @return {number}
 */
WebInspector.TimelineUIUtils.quadWidth = function(quad)
{
    return Math.round(Math.sqrt(Math.pow(quad[0] - quad[2], 2) + Math.pow(quad[1] - quad[3], 2)));
}

/**
 * @param {!Array.<number>} quad
 * @return {number}
 */
WebInspector.TimelineUIUtils.quadHeight = function(quad)
{
    return Math.round(Math.sqrt(Math.pow(quad[0] - quad[6], 2) + Math.pow(quad[1] - quad[7], 2)));
}

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @param {string} name
 * @param {string} title
 * @param {number} overviewStripGroupIndex
 * @param {string} borderColor
 * @param {string} backgroundColor
 * @param {string} fillColorStop0
 * @param {string} fillColorStop1
 */
WebInspector.TimelineCategory = function(name, title, overviewStripGroupIndex, borderColor, backgroundColor, fillColorStop0, fillColorStop1)
{
    this.name = name;
    this.title = title;
    this.overviewStripGroupIndex = overviewStripGroupIndex;
    this.borderColor = borderColor;
    this.backgroundColor = backgroundColor;
    this.fillColorStop0 = fillColorStop0;
    this.fillColorStop1 = fillColorStop1;
    this.hidden = false;
}

WebInspector.TimelineCategory.Events = {
    VisibilityChanged: "VisibilityChanged"
};

WebInspector.TimelineCategory.prototype = {
    /**
     * @return {boolean}
     */
    get hidden()
    {
        return this._hidden;
    },

    set hidden(hidden)
    {
        this._hidden = hidden;
        this.dispatchEventToListeners(WebInspector.TimelineCategory.Events.VisibilityChanged, this);
    },

    __proto__: WebInspector.Object.prototype
}

/**
 * @constructor
 * @param {string} title
 */
WebInspector.TimelinePopupContentHelper = function(title)
{
    this._contentTable = createElement("table");
    var titleCell = this._createCell(WebInspector.UIString("%s - Details", title), "timeline-details-title");
    titleCell.colSpan = 2;
    var titleRow = createElement("tr");
    titleRow.appendChild(titleCell);
    this._contentTable.appendChild(titleRow);
}

WebInspector.TimelinePopupContentHelper.prototype = {
    /**
     * @return {!Element}
     */
    contentTable: function()
    {
        return this._contentTable;
    },

    /**
     * @param {string|number} content
     * @param {string=} styleName
     */
    _createCell: function(content, styleName)
    {
        var text = createElement("label");
        text.createTextChild(String(content));
        var cell = createElement("td");
        cell.className = "timeline-details";
        if (styleName)
            cell.className += " " + styleName;
        cell.textContent = content;
        return cell;
    },

    /**
     * @param {string} title
     * @param {string|number} content
     */
    appendTextRow: function(title, content)
    {
        var row = createElement("tr");
        row.appendChild(this._createCell(title, "timeline-details-row-title"));
        row.appendChild(this._createCell(content, "timeline-details-row-data"));
        this._contentTable.appendChild(row);
    },

    /**
     * @param {string} title
     * @param {!Node|string} content
     */
    appendElementRow: function(title, content)
    {
        var row = createElement("tr");
        var titleCell = this._createCell(title, "timeline-details-row-title");
        row.appendChild(titleCell);
        var cell = createElement("td");
        cell.className = "details";
        if (content instanceof Node)
            cell.appendChild(content);
        else
            cell.createTextChild(content || "");
        row.appendChild(cell);
        this._contentTable.appendChild(row);
    }
}

/**
 * @constructor
 * @param {?WebInspector.Target} target
 * @param {?WebInspector.Linkifier} linkifier
 * @param {boolean} monospaceValues
 */
WebInspector.TimelineDetailsContentHelper = function(target, linkifier, monospaceValues)
{
    this._linkifier = linkifier;
    this._target = target;
    this.element = createElement("div");
    this.element.className = "timeline-details-view-block";
    this._monospaceValues = monospaceValues;
}

WebInspector.TimelineDetailsContentHelper.prototype = {
    /**
     * @param {string} title
     * @param {string|number|boolean} value
     */
    appendTextRow: function(title, value)
    {
        var rowElement = this.element.createChild("div", "timeline-details-view-row");
        rowElement.createChild("div", "timeline-details-view-row-title").textContent = title;
        rowElement.createChild("div", "timeline-details-view-row-value" + (this._monospaceValues ? " monospace" : "")).textContent = value;
    },

    /**
     * @param {string} title
     * @param {!Node|string} content
     */
    appendElementRow: function(title, content)
    {
        var rowElement = this.element.createChild("div", "timeline-details-view-row");
        rowElement.createChild("div", "timeline-details-view-row-title").textContent = title;
        var valueElement = rowElement.createChild("div", "timeline-details-view-row-value timeline-details-view-row-details" + (this._monospaceValues ? " monospace" : ""));
        if (content instanceof Node)
            valueElement.appendChild(content);
        else
            valueElement.createTextChild(content || "");
    },

    /**
     * @param {string} title
     * @param {string} url
     * @param {number} line
     */
    appendLocationRow: function(title, url, line)
    {
        if (!this._linkifier || !this._target)
            return;
        this.appendElementRow(title, this._linkifier.linkifyScriptLocation(this._target, null, url, line - 1) || "");
    },

    /**
     * @param {string} title
     * @param {!Array.<!ConsoleAgent.CallFrame>} stackTrace
     */
    appendStackTrace: function(title, stackTrace)
    {
        if (!this._linkifier || !this._target)
            return;

        var rowElement = this.element.createChild("div", "timeline-details-view-row");
        rowElement.createChild("div", "timeline-details-view-row-title").textContent = title;
        this.createChildStackTraceElement(rowElement, stackTrace);
    },

    /**
     * @param {!Element} parentElement
     * @param {!Array.<!ConsoleAgent.CallFrame>} stackTrace
     */
    createChildStackTraceElement: function(parentElement, stackTrace)
    {
        var stackTraceElement = parentElement.createChild("div", "timeline-details-view-row-value timeline-details-view-row-stack-trace monospace");
        for (var i = 0; i < stackTrace.length; ++i) {
            var stackFrame = stackTrace[i];
            var row = stackTraceElement.createChild("div");
            row.createTextChild(stackFrame.functionName || WebInspector.UIString("(anonymous function)"));
            row.createTextChild(" @ ");
            var urlElement = this._linkifier.linkifyScriptLocation(this._target, stackFrame.scriptId, stackFrame.url, stackFrame.lineNumber - 1, stackFrame.columnNumber - 1);
            row.appendChild(urlElement);
        }
    }
}
