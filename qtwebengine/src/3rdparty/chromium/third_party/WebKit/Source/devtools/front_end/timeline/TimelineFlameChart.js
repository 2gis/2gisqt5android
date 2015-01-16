/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
 * @implements {WebInspector.FlameChartDataProvider}
 * @implements {WebInspector.TimelineFlameChart.SelectionProvider}
 * @param {!WebInspector.TimelineModelImpl} model
 * @param {!WebInspector.TimelineFrameModelBase} frameModel
 */
WebInspector.TimelineFlameChartDataProvider = function(model, frameModel)
{
    WebInspector.FlameChartDataProvider.call(this);
    this._model = model;
    this._frameModel = frameModel;
    this._font = "12px " + WebInspector.fontFamily();
    this._linkifier = new WebInspector.Linkifier();
}

WebInspector.TimelineFlameChartDataProvider.prototype = {
    /**
     * @return {number}
     */
    barHeight: function()
    {
        return 20;
    },

    /**
     * @return {number}
     */
    textBaseline: function()
    {
        return 6;
    },

    /**
     * @return {number}
     */
    textPadding: function()
    {
        return 5;
    },

    /**
     * @param {number} entryIndex
     * @return {string}
     */
    entryFont: function(entryIndex)
    {
        return this._font;
    },

    /**
     * @param {number} entryIndex
     * @return {?string}
     */
    entryTitle: function(entryIndex)
    {
        var record = this._records[entryIndex];
        if (record === this._cpuThreadRecord)
            return WebInspector.UIString("CPU");
        else if (record === this._gpuThreadRecord)
            return WebInspector.UIString("GPU");
        var details = WebInspector.TimelineUIUtilsImpl.buildDetailsNode(record, this._linkifier, this._model.loadedFromFile());
        var title = WebInspector.TimelineUIUtilsImpl.recordTitle(record);
        return details ? WebInspector.UIString("%s (%s)", title, details.textContent) : title;
    },

    /**
     * @param {number} startTime
     * @param {number} endTime
     * @return {?Array.<number>}
     */
    dividerOffsets: function(startTime, endTime)
    {
        // While we have tracing and timeline flame chart on screen at a time,
        // we don't want to render frame-based grid.
        return null;
    },

    reset: function()
    {
        this._timelineData = null;
    },

    /**
     * @return {!WebInspector.FlameChart.TimelineData}
     */
    timelineData: function()
    {
        if (this._timelineData)
            return this._timelineData;

        this._linkifier.reset();

        /**
         * @type {?WebInspector.FlameChart.TimelineData}
         */
        this._timelineData = {
            entryLevels: [],
            entryTotalTimes: [],
            entryStartTimes: []
        };

        this._records = [];
        this._entryThreadDepths = {};
        this._minimumBoundary = this._model.minimumRecordTime();

        var cpuThreadRecordPayload = { type: WebInspector.TimelineModel.RecordType.Program };
        this._cpuThreadRecord = new WebInspector.TimelineModel.RecordImpl(this._model, /** @type {!TimelineAgent.TimelineEvent} */ (cpuThreadRecordPayload), null);
        this._pushRecord(this._cpuThreadRecord, 0, this.minimumBoundary(), Math.max(this._model.maximumRecordTime(), this.totalTime() + this.minimumBoundary()));

        this._gpuThreadRecord = null;

        var records = this._model.records();
        for (var i = 0; i < records.length; ++i) {
            var record = records[i];
            var thread = record.thread();
            if (thread === "gpu")
                continue;
            if (!thread) {
                for (var j = 0; j < record.children().length; ++j)
                    this._appendRecord(record.children()[j], 1);
            } else {
                var visible = this._appendRecord(records[i], 1);
                if (visible && !this._gpuThreadRecord) {
                    var gpuThreadRecordPayload = { type: WebInspector.TimelineModel.RecordType.Program };
                    this._gpuThreadRecord = new WebInspector.TimelineModel.RecordImpl(this._model, /** @type {!TimelineAgent.TimelineEvent} */ (gpuThreadRecordPayload), null);
                    this._pushRecord(this._gpuThreadRecord, 0, this.minimumBoundary(), Math.max(this._model.maximumRecordTime(), this.totalTime() + this.minimumBoundary()));
                }
            }
        }

        var cpuStackDepth = Math.max(4, this._entryThreadDepths[undefined]);
        delete this._entryThreadDepths[undefined];
        this._maxStackDepth = cpuStackDepth;

        if (this._gpuThreadRecord) {
            // We have multiple threads, update levels.
            var threadBaselines = {};
            var threadBaseline = cpuStackDepth + 2;

            for (var thread in this._entryThreadDepths) {
                threadBaselines[thread] = threadBaseline;
                threadBaseline += this._entryThreadDepths[thread];
            }
            this._maxStackDepth = threadBaseline;

            for (var i = 0; i < this._records.length; ++i) {
                var record = this._records[i];
                var level = this._timelineData.entryLevels[i];
                if (record === this._cpuThreadRecord)
                    level = 0;
                else if (record === this._gpuThreadRecord)
                    level = cpuStackDepth + 2;
                else if (record.thread())
                    level += threadBaselines[record.thread()];
                this._timelineData.entryLevels[i] = level;
            }
        }

        return this._timelineData;
    },

    /**
     * @return {number}
     */
    minimumBoundary: function()
    {
        return this._minimumBoundary;
    },

    /**
     * @return {number}
     */
    totalTime: function()
    {
        return Math.max(1000, this._model.maximumRecordTime() - this._model.minimumRecordTime());
    },

    /**
     * @return {number}
     */
    maxStackDepth: function()
    {
        return this._maxStackDepth;
    },

    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @param {number} level
     * @return {boolean}
     */
    _appendRecord: function(record, level)
    {
        var result = false;
        if (!this._model.isVisible(record)) {
            for (var i = 0; i < record.children().length; ++i)
                result = this._appendRecord(record.children()[i], level) || result;
            return result;
        }

        this._pushRecord(record, level, record.startTime(), record.endTime());
        for (var i = 0; i < record.children().length; ++i)
            this._appendRecord(record.children()[i], level + 1);
        return true;
    },

    /**
     * @param {!WebInspector.TimelineModel.Record} record
     * @param {number} level
     * @param {number} startTime
     * @param {number} endTime
     * @return {number}
     */
    _pushRecord: function(record, level, startTime, endTime)
    {
        var index = this._records.length;
        this._records.push(record);
        this._timelineData.entryStartTimes[index] = startTime;
        this._timelineData.entryLevels[index] = level;
        this._timelineData.entryTotalTimes[index] = endTime - startTime;
        this._entryThreadDepths[record.thread()] = Math.max(level, this._entryThreadDepths[record.thread()] || 0);
        return index;
    },

    /**
     * @param {number} entryIndex
     * @return {?Array.<!{title: string, text: string}>}
     */
    prepareHighlightedEntryInfo: function(entryIndex)
    {
        return null;
    },

    /**
     * @param {number} entryIndex
     * @return {boolean}
     */
    canJumpToEntry: function(entryIndex)
    {
        return false;
    },

    /**
     * @param {number} entryIndex
     * @return {string}
     */
    entryColor: function(entryIndex)
    {
        var record = this._records[entryIndex];
        if (record === this._cpuThreadRecord || record === this._gpuThreadRecord)
            return "#555";

        if (record.type() === WebInspector.TimelineModel.RecordType.JSFrame)
            return WebInspector.TimelineFlameChartDataProvider.jsFrameColorGenerator().colorForID(record.data()["functionName"]);

        return record.category().fillColorStop1;
    },


    /**
     * @param {number} entryIndex
     * @param {!CanvasRenderingContext2D} context
     * @param {?string} text
     * @param {number} barX
     * @param {number} barY
     * @param {number} barWidth
     * @param {number} barHeight
     * @param {function(number):number} offsetToPosition
     * @return {boolean}
     */
    decorateEntry: function(entryIndex, context, text, barX, barY, barWidth, barHeight, offsetToPosition)
    {
        if (barWidth < 5)
            return false;

        var record = this._records[entryIndex];
        var timelineData = this._timelineData;

        var category = record.category();
        // Paint text using white color on dark background.
        if (text) {
            context.save();
            context.fillStyle = "white";
            context.shadowColor = "rgba(0, 0, 0, 0.1)";
            context.shadowOffsetX = 1;
            context.shadowOffsetY = 1;
            context.font = this._font;
            context.fillText(text, barX + this.textPadding(), barY + barHeight - this.textBaseline());
            context.restore();
        }

        if (record.children().length) {
            var entryStartTime = timelineData.entryStartTimes[entryIndex];
            var barSelf = offsetToPosition(entryStartTime + record.selfTime())

            context.beginPath();
            context.fillStyle = category.backgroundColor;
            context.rect(barSelf, barY, barX + barWidth - barSelf, barHeight);
            context.fill();

            // Paint text using dark color on light background.
            if (text) {
                context.save();
                context.clip();
                context.fillStyle = category.borderColor;
                context.shadowColor = "rgba(0, 0, 0, 0.1)";
                context.shadowOffsetX = 1;
                context.shadowOffsetY = 1;
                context.fillText(text, barX + this.textPadding(), barY + barHeight - this.textBaseline());
                context.restore();
            }
        }

        if (record.warnings()) {
            context.save();

            context.rect(barX, barY, barWidth, this.barHeight());
            context.clip();

            context.beginPath();
            context.fillStyle = record.warnings() ? "red" : "rgba(255, 0, 0, 0.5)";
            context.moveTo(barX + barWidth - 15, barY + 1);
            context.lineTo(barX + barWidth - 1, barY + 1);
            context.lineTo(barX + barWidth - 1, barY + 15);
            context.fill();

            context.restore();
        }

        return true;
    },

    /**
     * @param {number} entryIndex
     * @return {boolean}
     */
    forceDecoration: function(entryIndex)
    {
        var record = this._records[entryIndex];
        return !!record.warnings();
    },

    /**
     * @param {number} entryIndex
     * @return {?{startTime: number, endTime: number}}
     */
    highlightTimeRange: function(entryIndex)
    {
        var record = this._records[entryIndex];
        if (record === this._cpuThreadRecord || record === this._gpuThreadRecord)
            return null;
        return {
            startTime: record.startTime(),
            endTime: record.endTime()
        };
    },

    /**
     * @return {number}
     */
    paddingLeft: function()
    {
        return 0;
    },

    /**
     * @param {number} entryIndex
     * @return {string}
     */
    textColor: function(entryIndex)
    {
        return "white";
    },

    /**
     * @param {number} entryIndex
     * @return {?WebInspector.TimelineSelection}
     */
    createSelection: function(entryIndex)
    {
        var record = this._records[entryIndex];
        if (record instanceof WebInspector.TimelineModel.RecordImpl) {
            this._lastSelection = new WebInspector.TimelineFlameChart.Selection(WebInspector.TimelineSelection.fromRecord(record), entryIndex);
            return this._lastSelection.timelineSelection;
        }
        return null;
    },

    /**
     * @param {?WebInspector.TimelineSelection} selection
     * @return {number}
     */
    entryIndexForSelection: function(selection)
    {
        if (!selection || selection.type() !== WebInspector.TimelineSelection.Type.Record)
            return -1;
        var record = /** @type{!WebInspector.TimelineModel.Record} */ (selection.object());
        if (this._lastSelection && this._lastSelection.timelineSelection.object() === record)
            return this._lastSelection.entryIndex;
        var entryRecords = this._records;
        for (var entryIndex = 0; entryIndex < entryRecords.length; ++entryIndex) {
            if (entryRecords[entryIndex] === record) {
                this._lastSelection = new WebInspector.TimelineFlameChart.Selection(WebInspector.TimelineSelection.fromRecord(record), entryIndex);
                return entryIndex;
            }
        }
        return -1;
    }
}

/**
 * @constructor
 * @implements {WebInspector.FlameChartDataProvider}
 * @implements {WebInspector.TimelineFlameChart.SelectionProvider}
 * @param {!WebInspector.TracingTimelineModel} model
 * @param {!WebInspector.TimelineFrameModelBase} frameModel
 * @param {!WebInspector.Target} target
 */
WebInspector.TracingBasedTimelineFlameChartDataProvider = function(model, frameModel, target)
{
    WebInspector.FlameChartDataProvider.call(this);
    this._model = model;
    this._frameModel = frameModel;
    this._target = target;
    this._font = "12px " + WebInspector.fontFamily();
    this._linkifier = new WebInspector.Linkifier();
    this._palette = new WebInspector.TraceViewPalette();
    this._entryIndexToTitle = {};
}

WebInspector.TracingBasedTimelineFlameChartDataProvider.prototype = {
    /**
     * @return {number}
     */
    barHeight: function()
    {
        return 20;
    },

    /**
     * @return {number}
     */
    textBaseline: function()
    {
        return 6;
    },

    /**
     * @return {number}
     */
    textPadding: function()
    {
        return 5;
    },

    /**
     * @param {number} entryIndex
     * @return {string}
     */
    entryFont: function(entryIndex)
    {
        return this._font;
    },

    /**
     * @param {number} entryIndex
     * @return {?string}
     */
    entryTitle: function(entryIndex)
    {
        var event = this._entryEvents[entryIndex];
        if (event) {
            var name = WebInspector.TracingTimelineUIUtils.styleForTraceEvent(event.name).title;
            // TODO(yurys): support event dividers
            var details = WebInspector.TracingTimelineUIUtils.buildDetailsNodeForTraceEvent(event, this._linkifier, false, this._target);
            return details ? WebInspector.UIString("%s (%s)", name, details.textContent) : name;
        }
        var title = this._entryIndexToTitle[entryIndex];
        if (!title) {
            title = WebInspector.UIString("Unexpected entryIndex %d", entryIndex);
            console.error(title);
        }
        return title;
    },

    /**
     * @param {number} startTime
     * @param {number} endTime
     * @return {?Array.<number>}
     */
    dividerOffsets: function(startTime, endTime)
    {
        return null;
    },

    reset: function()
    {
        this._timelineData = null;
        /** @type {!Array.<!WebInspector.TracingModel.Event>} */
        this._entryEvents = [];
        this._entryIndexToTitle = {};
    },

    /**
     * @return {!WebInspector.FlameChart.TimelineData}
     */
    timelineData: function()
    {
        if (this._timelineData)
            return this._timelineData;

        /**
         * @type {?WebInspector.FlameChart.TimelineData}
         */
        this._timelineData = {
            entryLevels: [],
            entryTotalTimes: [],
            entryStartTimes: []
        };

        this._currentLevel = 0;
        this._minimumBoundary = this._model.minimumRecordTime();
        this._timeSpan = Math.max(this._model.maximumRecordTime() - this._minimumBoundary, 1000);
        this._appendHeaderRecord("CPU");
        var events = this._model.mainThreadEvents();
        var maxStackDepth = 0;
        for (var eventIndex = 0; eventIndex < events.length; ++eventIndex) {
            var event = events[eventIndex];
            var category = event.category;
            if (category !== "disabled-by-default-devtools.timeline" && category !== "devtools")
                continue;
            if (event.duration || event.phase === WebInspector.TracingModel.Phase.Instant) {
                this._appendEvent(event);
                if (maxStackDepth < event.level)
                    maxStackDepth = event.level;
            }
        }
        this._currentLevel += maxStackDepth + 1;

        this._appendHeaderRecord("GPU");
        return this._timelineData;
    },

    /**
     * @return {number}
     */
    minimumBoundary: function()
    {
        return this._minimumBoundary;
    },

    /**
     * @return {number}
     */
    totalTime: function()
    {
        return this._timeSpan;
    },

    /**
     * @return {number}
     */
    maxStackDepth: function()
    {
        return this._currentLevel;
    },

    /**
     * @param {number} entryIndex
     * @return {?Array.<!{title: string, text: string}>}
     */
    prepareHighlightedEntryInfo: function(entryIndex)
    {
        return null;
    },

    /**
     * @param {number} entryIndex
     * @return {boolean}
     */
    canJumpToEntry: function(entryIndex)
    {
        return false;
    },

    /**
     * @param {number} entryIndex
     * @return {string}
     */
    entryColor: function(entryIndex)
    {
        var event = this._entryEvents[entryIndex];
        if (!event)
            return "#555";
        var style = WebInspector.TracingTimelineUIUtils.styleForTraceEvent(event.name);
        return style.category.fillColorStop1;
    },

    /**
     * @param {number} entryIndex
     * @param {!CanvasRenderingContext2D} context
     * @param {?string} text
     * @param {number} barX
     * @param {number} barY
     * @param {number} barWidth
     * @param {number} barHeight
     * @param {function(number):number} offsetToPosition
     * @return {boolean}
     */
    decorateEntry: function(entryIndex, context, text, barX, barY, barWidth, barHeight, offsetToPosition)
    {
        if (barWidth < 5)
            return false;

        var timelineData = this._timelineData;

        // Paint text using white color on dark background.
        if (text) {
            context.save();
            context.fillStyle = "white";
            context.shadowColor = "rgba(0, 0, 0, 0.1)";
            context.shadowOffsetX = 1;
            context.shadowOffsetY = 1;
            context.font = this._font;
            context.fillText(text, barX + this.textPadding(), barY + barHeight - this.textBaseline());
            context.restore();
        }

        var event = this._entryEvents[entryIndex];
        if (event && event.warning) {
            context.save();

            context.rect(barX, barY, barWidth, this.barHeight());
            context.clip();

            context.beginPath();
            context.fillStyle = "red";
            context.moveTo(barX + barWidth - 15, barY + 1);
            context.lineTo(barX + barWidth - 1, barY + 1);
            context.lineTo(barX + barWidth - 1, barY + 15);
            context.fill();

            context.restore();
        }

        return true;
    },

    /**
     * @param {number} entryIndex
     * @return {boolean}
     */
    forceDecoration: function(entryIndex)
    {
        var event = this._entryEvents[entryIndex];
        if (!event)
            return false;
        return !!event.warning;
    },

   /**
     * @param {number} entryIndex
     * @return {?{startTime: number, endTime: number}}
     */
    highlightTimeRange: function(entryIndex)
    {
        var event = this._entryEvents[entryIndex];
        if (!event)
            return null;
        return {
            startTime: event.startTime,
            endTime: event.endTime
        }
    },

    /**
     * @return {number}
     */
    paddingLeft: function()
    {
        return 0;
    },

    /**
     * @param {number} entryIndex
     * @return {string}
     */
    textColor: function(entryIndex)
    {
        return "white";
    },

    /**
     * @param {string} title
     */
    _appendHeaderRecord: function(title)
    {
        var index = this._entryEvents.length;
        this._entryIndexToTitle[index] = title;
        this._entryEvents.push(null);
        this._timelineData.entryLevels[index] = this._currentLevel++;
        this._timelineData.entryTotalTimes[index] = this._timeSpan;
        this._timelineData.entryStartTimes[index] = this._minimumBoundary;
    },

    /**
     * @param {!WebInspector.TracingModel.Event} event
     */
    _appendEvent: function(event)
    {
        var index = this._entryEvents.length;
        this._entryEvents.push(event);
        this._timelineData.entryLevels[index] = this._currentLevel + event.level;
        this._timelineData.entryTotalTimes[index] = event.duration || 1;
        this._timelineData.entryStartTimes[index] = event.startTime;
    },

    /**
     * @param {number} entryIndex
     * @return {?WebInspector.TimelineSelection}
     */
    createSelection: function(entryIndex)
    {
        var event = this._entryEvents[entryIndex];
        if (!event)
            return null;
        this._lastSelection = new WebInspector.TimelineFlameChart.Selection(WebInspector.TimelineSelection.fromTraceEvent(event), entryIndex);
        return this._lastSelection.timelineSelection;
    },

    /**
     * @param {?WebInspector.TimelineSelection} selection
     * @return {number}
     */
    entryIndexForSelection: function(selection)
    {
        if (!selection || selection.type() !== WebInspector.TimelineSelection.Type.TraceEvent)
            return -1;
        var event = /** @type{!WebInspector.TracingModel.Event} */ (selection.object());
        if (this._lastSelection && this._lastSelection.timelineSelection.object() === event)
            return this._lastSelection.entryIndex;
        var entryEvents = this._entryEvents;
        for (var entryIndex = 0; entryIndex < entryEvents.length; ++entryIndex) {
            if (entryEvents[entryIndex] === event) {
                this._lastSelection = new WebInspector.TimelineFlameChart.Selection(WebInspector.TimelineSelection.fromTraceEvent(event), entryIndex);
                return entryIndex;
            }
        }
        return -1;
    }
}

/**
 * @return {!WebInspector.FlameChart.ColorGenerator}
 */
WebInspector.TimelineFlameChartDataProvider.jsFrameColorGenerator = function()
{
    if (!WebInspector.TimelineFlameChartDataProvider._jsFrameColorGenerator) {
        var hueSpace = { min: 30, max: 55, count: 5 };
        var satSpace = { min: 70, max: 100, count: 6 };
        var colorGenerator = new WebInspector.FlameChart.ColorGenerator(hueSpace, satSpace, 50);
        colorGenerator.setColorForID("(idle)", "hsl(0, 0%, 60%)");
        colorGenerator.setColorForID("(program)", "hsl(0, 0%, 60%)");
        colorGenerator.setColorForID("(garbage collector)", "hsl(0, 0%, 60%)");
        WebInspector.TimelineFlameChartDataProvider._jsFrameColorGenerator = colorGenerator;
    }
    return WebInspector.TimelineFlameChartDataProvider._jsFrameColorGenerator;
}

/**
 * @constructor
 * @extends {WebInspector.VBox}
 * @implements {WebInspector.TimelineModeView}
 * @implements {WebInspector.FlameChartDelegate}
 * @param {!WebInspector.TimelineModeViewDelegate} delegate
 * @param {!WebInspector.TimelineModel} model
 * @param {?WebInspector.TracingTimelineModel} tracingModel
 * @param {!WebInspector.TimelineFrameModelBase} frameModel
 */
WebInspector.TimelineFlameChart = function(delegate, model, tracingModel, frameModel)
{
    WebInspector.VBox.call(this);
    this.element.classList.add("timeline-flamechart");
    this.registerRequiredCSS("flameChart.css");
    this._delegate = delegate;
    this._model = model;
    this._dataProvider = tracingModel
        ? new WebInspector.TracingBasedTimelineFlameChartDataProvider(tracingModel, frameModel, model.target())
        : new WebInspector.TimelineFlameChartDataProvider(/** @type {!WebInspector.TimelineModelImpl} */(model), frameModel);
    this._mainView = new WebInspector.FlameChart(this._dataProvider, this, true);
    this._mainView.show(this.element);
    this._model.addEventListener(WebInspector.TimelineModel.Events.RecordingStarted, this._onRecordingStarted, this);
    this._mainView.addEventListener(WebInspector.FlameChart.Events.EntrySelected, this._onEntrySelected, this);
}

WebInspector.TimelineFlameChart.prototype = {
    dispose: function()
    {
        this._model.removeEventListener(WebInspector.TimelineModel.Events.RecordingStarted, this._onRecordingStarted, this);
        this._mainView.removeEventListener(WebInspector.FlameChart.Events.EntrySelected, this._onEntrySelected, this);
    },

    /**
     * @param {number} windowStartTime
     * @param {number} windowEndTime
     */
    requestWindowTimes: function(windowStartTime, windowEndTime)
    {
        this._delegate.requestWindowTimes(windowStartTime, windowEndTime);
    },

    /**
     * @param {?RegExp} textFilter
     */
    refreshRecords: function(textFilter)
    {
        this._dataProvider.reset();
        this._mainView._scheduleUpdate();
    },

    wasShown: function()
    {
        this._mainView._scheduleUpdate();
    },


    /**
     * @return {!WebInspector.View}
     */
    view: function()
    {
        return this;
    },

    reset: function()
    {
        this._automaticallySizeWindow = true;
        this._dataProvider.reset();
        this._mainView.reset();
        this._mainView.setWindowTimes(0, Infinity);
    },

    _onRecordingStarted: function()
    {
        this._automaticallySizeWindow = true;
        this._mainView.reset();
    },

    /**
     * @param {!WebInspector.TimelineModel.Record} record
     */
    addRecord: function(record)
    {
        this._dataProvider.reset();
        if (this._automaticallySizeWindow) {
            var minimumRecordTime = this._model.minimumRecordTime();
            if (record.startTime() > (minimumRecordTime + 1000)) {
                this._automaticallySizeWindow = false;
                this._delegate.requestWindowTimes(minimumRecordTime, minimumRecordTime + 1000);
            }
            this._mainView._scheduleUpdate();
        } else {
            if (!this._pendingUpdateTimer)
                this._pendingUpdateTimer = window.setTimeout(this._updateOnAddRecord.bind(this), 300);
        }
    },

    _updateOnAddRecord: function()
    {
        delete this._pendingUpdateTimer;
        this._mainView._scheduleUpdate();
    },

    /**
     * @param {number} startTime
     * @param {number} endTime
     */
    setWindowTimes: function(startTime, endTime)
    {
        this._mainView.setWindowTimes(startTime, endTime);
        this._delegate.select(null);
    },

    /**
     * @param {number} width
     */
    setSidebarSize: function(width)
    {
    },

    /**
     * @param {?WebInspector.TimelineModel.Record} record
     * @param {string=} regex
     * @param {boolean=} selectRecord
     */
    highlightSearchResult: function(record, regex, selectRecord)
    {
    },

    /**
     * @param {?WebInspector.TimelineSelection} selection
     */
    setSelection: function(selection)
    {
        var index = this._dataProvider.entryIndexForSelection(selection);
        this._mainView.setSelectedEntry(index);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onEntrySelected: function(event)
    {
        var entryIndex = /** @type{number} */ (event.data);
        var timelineSelection = this._dataProvider.createSelection(entryIndex);
        if (timelineSelection)
            this._delegate.select(timelineSelection);
    },

    __proto__: WebInspector.VBox.prototype
}

/**
  * @constructor
  * @param {!WebInspector.TimelineSelection} selection
  * @param {number} entryIndex
  */
WebInspector.TimelineFlameChart.Selection = function(selection, entryIndex)
{
    this.timelineSelection = selection;
    this.entryIndex = entryIndex;
}

/**
  * @interface
  */
WebInspector.TimelineFlameChart.SelectionProvider = function() { }

WebInspector.TimelineFlameChart.SelectionProvider.prototype = {
    /**
     * @param {number} entryIndex
     * @return {?WebInspector.TimelineSelection}
     */
    createSelection: function(entryIndex) { },
    /**
     * @param {?WebInspector.TimelineSelection} selection
     * @return {number}
     */
    entryIndexForSelection: function(selection) { }
}
