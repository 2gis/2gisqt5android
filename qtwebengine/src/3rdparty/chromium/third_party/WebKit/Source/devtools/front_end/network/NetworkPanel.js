/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Anthony Ricaud <rik@webkit.org>
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

/**
 * @constructor
 * @implements {WebInspector.Searchable}
 * @implements {WebInspector.TargetManager.Observer}
 * @extends {WebInspector.VBox}
 * @param {!WebInspector.FilterBar} filterBar
 * @param {!WebInspector.Setting} coulmnsVisibilitySetting
 */
WebInspector.NetworkLogView = function(filterBar, coulmnsVisibilitySetting)
{
    WebInspector.VBox.call(this);
    this.registerRequiredCSS("network/networkLogView.css");
    this.registerRequiredCSS("ui/filter.css");

    this._filterBar = filterBar;
    this._coulmnsVisibilitySetting = coulmnsVisibilitySetting;
    this._allowRequestSelection = false;
    /** @type {!Map.<string, !WebInspector.NetworkDataGridNode>} */
    this._nodesByRequestId = new Map();
    /** @type {!Object.<string, boolean>} */
    this._staleRequestIds = {};
    /** @type {number} */
    this._mainRequestLoadTime = -1;
    /** @type {number} */
    this._mainRequestDOMContentLoadedTime = -1;
    this._matchedRequestCount = 0;
    this._highlightedSubstringChanges = [];

    /** @type {!Array.<!WebInspector.NetworkLogView.Filter>} */
    this._filters = [];

    this._currentMatchedRequestNode = null;
    this._currentMatchedRequestIndex = -1;

    this._createStatusbarButtons();
    this._createStatusBarItems();
    this._linkifier = new WebInspector.Linkifier();

    this._allowPopover = true;

    /** @type {number} */
    this._rowHeight = 0;

    this._addFilters();
    this._resetSuggestionBuilder();
    this._initializeView();
    this._toggleRecordButton(true);

    WebInspector.targetManager.observeTargets(this);
    WebInspector.targetManager.addModelListener(WebInspector.NetworkManager, WebInspector.NetworkManager.EventTypes.RequestStarted, this._onRequestStarted, this);
    WebInspector.targetManager.addModelListener(WebInspector.NetworkManager, WebInspector.NetworkManager.EventTypes.RequestUpdated, this._onRequestUpdated, this);
    WebInspector.targetManager.addModelListener(WebInspector.NetworkManager, WebInspector.NetworkManager.EventTypes.RequestFinished, this._onRequestUpdated, this);

    WebInspector.targetManager.addModelListener(WebInspector.ResourceTreeModel, WebInspector.ResourceTreeModel.EventTypes.WillReloadPage, this._willReloadPage, this);
    WebInspector.targetManager.addModelListener(WebInspector.ResourceTreeModel, WebInspector.ResourceTreeModel.EventTypes.MainFrameNavigated, this._mainFrameNavigated, this);
    WebInspector.targetManager.addModelListener(WebInspector.ResourceTreeModel, WebInspector.ResourceTreeModel.EventTypes.Load, this._loadEventFired, this);
    WebInspector.targetManager.addModelListener(WebInspector.ResourceTreeModel, WebInspector.ResourceTreeModel.EventTypes.DOMContentLoaded, this._domContentLoadedEventFired, this);
}

WebInspector.NetworkLogView.HTTPSchemas = {"http": true, "https": true, "ws": true, "wss": true};
WebInspector.NetworkLogView._responseHeaderColumns = ["Cache-Control", "Connection", "Content-Encoding", "Content-Length", "ETag", "Keep-Alive", "Last-Modified", "Server", "Vary"];
WebInspector.NetworkLogView.defaultColumnsVisibility = {
    method: true, status: true, scheme: false, domain: false, remoteAddress: false, type: true, initiator: true, cookies: false, setCookies: false, size: true, time: true, connectionId: false,
    "Cache-Control": false, "Connection": false, "Content-Encoding": false, "Content-Length": false, "ETag": false, "Keep-Alive": false, "Last-Modified": false, "Server": false, "Vary": false
};
WebInspector.NetworkLogView._defaultRefreshDelay = 500;

/** @enum {string} */
WebInspector.NetworkLogView.FilterType = {
    Domain: "Domain",
    HasResponseHeader: "HasResponseHeader",
    Is: "Is",
    Method: "Method",
    MimeType: "MimeType",
    Scheme: "Scheme",
    SetCookieDomain: "SetCookieDomain",
    SetCookieName: "SetCookieName",
    SetCookieValue: "SetCookieValue",
    StatusCode: "StatusCode"
};

/** @enum {string} */
WebInspector.NetworkLogView.IsFilterType = {
    Running: "running"
};

/** @type {!Array.<string>} */
WebInspector.NetworkLogView._searchKeys = Object.values(WebInspector.NetworkLogView.FilterType);

/** @type {!Object.<string, string>} */
WebInspector.NetworkLogView._columnTitles = {
    "name": WebInspector.UIString("Name"),
    "method": WebInspector.UIString("Method"),
    "status": WebInspector.UIString("Status"),
    "scheme": WebInspector.UIString("Scheme"),
    "domain": WebInspector.UIString("Domain"),
    "remoteAddress": WebInspector.UIString("Remote Address"),
    "type": WebInspector.UIString("Type"),
    "initiator": WebInspector.UIString("Initiator"),
    "cookies": WebInspector.UIString("Cookies"),
    "setCookies": WebInspector.UIString("Set-Cookies"),
    "size": WebInspector.UIString("Size"),
    "time": WebInspector.UIString("Time"),
    "connectionId": WebInspector.UIString("Connection Id"),
    "timeline": WebInspector.UIString("Timeline"),

    // Response header columns
    "Cache-Control": WebInspector.UIString("Cache-Control"),
    "Connection": WebInspector.UIString("Connection"),
    "Content-Encoding": WebInspector.UIString("Content-Encoding"),
    "Content-Length": WebInspector.UIString("Content-Length"),
    "ETag": WebInspector.UIString("ETag"),
    "Keep-Alive": WebInspector.UIString("Keep-Alive"),
    "Last-Modified": WebInspector.UIString("Last-Modified"),
    "Server": WebInspector.UIString("Server"),
    "Vary": WebInspector.UIString("Vary")
};

WebInspector.NetworkLogView.prototype = {
    /**
     * @param {!WebInspector.Target} target
     */
    targetAdded: function(target)
    {
        target.networkLog.requests.forEach(this._appendRequest.bind(this));
    },

    /**
     * @param {!WebInspector.Target} target
     */
    targetRemoved: function(target)
    {
    },

    /**
     * @return {boolean}
     */
    allowRequestSelection: function()
    {
        return this._allowRequestSelection;
    },

    _addFilters: function()
    {
        this._textFilterUI = new WebInspector.TextFilterUI();
        this._textFilterUI.addEventListener(WebInspector.FilterUI.Events.FilterChanged, this._filterChanged, this);
        this._filterBar.addFilter(this._textFilterUI);

        var types = [];
        for (var typeId in WebInspector.resourceTypes) {
            var resourceType = WebInspector.resourceTypes[typeId];
            if (resourceType === WebInspector.resourceTypes.TextTrack)
                continue;
            types.push({name: resourceType.name(), label: resourceType.categoryTitle()});
        }
        this._resourceTypeFilterUI = new WebInspector.NamedBitSetFilterUI(types, WebInspector.settings.networkResourceTypeFilters);
        this._resourceTypeFilterUI.addEventListener(WebInspector.FilterUI.Events.FilterChanged, this._filterChanged.bind(this), this);
        this._filterBar.addFilter(this._resourceTypeFilterUI);

        var dataURLSetting = WebInspector.settings.networkHideDataURL;
        this._dataURLFilterUI = new WebInspector.CheckboxFilterUI("hide-data-url", WebInspector.UIString("Hide data URLs"), true, dataURLSetting);
        this._dataURLFilterUI.addEventListener(WebInspector.FilterUI.Events.FilterChanged, this._filterChanged.bind(this), this);
        this._filterBar.addFilter(this._dataURLFilterUI);
    },

    _resetSuggestionBuilder: function()
    {
        this._suggestionBuilder = new WebInspector.FilterSuggestionBuilder(WebInspector.NetworkLogView._searchKeys);
        this._suggestionBuilder.addItem(WebInspector.NetworkLogView.FilterType.Is, WebInspector.NetworkLogView.IsFilterType.Running);
        this._textFilterUI.setSuggestionBuilder(this._suggestionBuilder);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _filterChanged: function(event)
    {
        this._removeAllNodeHighlights();
        this._parseFilterQuery(this._textFilterUI.value());
        this._filterRequests();
    },

    _initializeView: function()
    {
        this.element.id = "network-container";

        this._createSortingFunctions();
        this._createCalculators();
        this._createTable();
        this._createTimelineGrid();
        this._summaryBarElement = this.element.createChild("div", "network-summary-bar");

        this._updateRowsSize();

        this._popoverHelper = new WebInspector.PopoverHelper(this.element, this._getPopoverAnchor.bind(this), this._showPopover.bind(this), this._onHidePopover.bind(this));
        // Enable faster hint.
        this._popoverHelper.setTimeout(250, 250);

        this.switchViewMode(true);
    },

    /**
     * @return {!Array.<!Element>}
     */
    statusBarItems: function()
    {
        return [
            this._recordButton.element,
            this._clearButton.element,
            this._filterBar.filterButton().element,
            this._largerRequestsButton.element,
            this._preserveLogCheckbox.element,
            this._disableCacheCheckbox.element,
            this._progressBarContainer];
    },

    /**
     * @return {boolean}
     */
    usesLargeRows: function()
    {
        return !!WebInspector.settings.resourcesLargeRows.get();
    },

    /**
     * @param {boolean} flag
     */
    setAllowPopover: function(flag)
    {
        this._allowPopover = flag;
    },

    /**
     * @return {!Array.<!Element>}
     */
    elementsToRestoreScrollPositionsFor: function()
    {
        if (!this._dataGrid) // Not initialized yet.
            return [];
        return [this._dataGrid.scrollContainer];
    },

    _createTimelineGrid: function()
    {
        this._timelineGrid = new WebInspector.TimelineGrid();
        this._timelineGrid.element.classList.add("network-timeline-grid");
        this._dataGrid.element.appendChild(this._timelineGrid.element);
    },

    _createTable: function()
    {
        var columns = [];
        columns.push({
            id: "name",
            titleDOMFragment: this._makeHeaderFragment(WebInspector.UIString("Name"), WebInspector.UIString("Path")),
            title: WebInspector.NetworkLogView._columnTitles["name"],
            sortable: true,
            weight: 20,
            disclosure: true
        });

        columns.push({
            id: "method",
            title: WebInspector.NetworkLogView._columnTitles["method"],
            sortable: true,
            weight: 6
        });

        columns.push({
            id: "status",
            titleDOMFragment: this._makeHeaderFragment(WebInspector.UIString("Status"), WebInspector.UIString("Text")),
            title: WebInspector.NetworkLogView._columnTitles["status"],
            sortable: true,
            weight: 6
        });

        columns.push({
            id: "scheme",
            title: WebInspector.NetworkLogView._columnTitles["scheme"],
            sortable: true,
            weight: 6
        });

        columns.push({
            id: "domain",
            title: WebInspector.NetworkLogView._columnTitles["domain"],
            sortable: true,
            weight: 6
        });

        columns.push({
            id: "remoteAddress",
            title: WebInspector.NetworkLogView._columnTitles["remoteAddress"],
            sortable: true,
            weight: 10,
            align: WebInspector.DataGrid.Align.Right
        });

        columns.push({
            id: "type",
            title: WebInspector.NetworkLogView._columnTitles["type"],
            sortable: true,
            weight: 6
        });

        columns.push({
            id: "initiator",
            title: WebInspector.NetworkLogView._columnTitles["initiator"],
            sortable: true,
            weight: 10
        });

        columns.push({
            id: "cookies",
            title: WebInspector.NetworkLogView._columnTitles["cookies"],
            sortable: true,
            weight: 6,
            align: WebInspector.DataGrid.Align.Right
        });

        columns.push({
            id: "setCookies",
            title: WebInspector.NetworkLogView._columnTitles["setCookies"],
            sortable: true,
            weight: 6,
            align: WebInspector.DataGrid.Align.Right
        });

        columns.push({
            id: "size",
            titleDOMFragment: this._makeHeaderFragment(WebInspector.UIString("Size"), WebInspector.UIString("Content")),
            title: WebInspector.NetworkLogView._columnTitles["size"],
            sortable: true,
            weight: 6,
            align: WebInspector.DataGrid.Align.Right
        });

        columns.push({
            id: "time",
            titleDOMFragment: this._makeHeaderFragment(WebInspector.UIString("Time"), WebInspector.UIString("Latency")),
            title: WebInspector.NetworkLogView._columnTitles["time"],
            sortable: true,
            weight: 6,
            align: WebInspector.DataGrid.Align.Right
        });

        columns.push({
            id: "connectionId",
            title: WebInspector.NetworkLogView._columnTitles["connectionId"],
            sortable: true,
            weight: 6
        });

        var responseHeaderColumns = WebInspector.NetworkLogView._responseHeaderColumns;
        for (var i = 0; i < responseHeaderColumns.length; ++i) {
            var headerName = responseHeaderColumns[i];
            var descriptor = {
                id: headerName,
                title: WebInspector.NetworkLogView._columnTitles[headerName],
                weight: 6
            }
            if (headerName === "Content-Length")
                descriptor.align = WebInspector.DataGrid.Align.Right;
            columns.push(descriptor);
        }

        columns.push({
            id: "timeline",
            titleDOMFragment: createDocumentFragment(),
            title: WebInspector.NetworkLogView._columnTitles["timeline"],
            sortable: false,
            weight: 40,
            sort: WebInspector.DataGrid.Order.Ascending
        });

        this._dataGrid = new WebInspector.SortableDataGrid(columns);
        this._dataGrid.setStickToBottom(true);
        this._updateColumns();
        this._dataGrid.setName("networkLog");
        this._dataGrid.setResizeMethod(WebInspector.DataGrid.ResizeMethod.Last);
        this._dataGrid.element.classList.add("network-log-grid");
        this._dataGrid.element.addEventListener("contextmenu", this._contextMenu.bind(this), true);
        this._dataGrid.show(this.element);

        // Event listeners need to be added _after_ we attach to the document, so that owner document is properly update.
        this._dataGrid.addEventListener(WebInspector.DataGrid.Events.SortingChanged, this._sortItems, this);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Events.ColumnsResized, this._updateDividersIfNeeded, this);

        this._patchTimelineHeader();
        this._dataGrid.sortNodes(this._sortingFunctions.startTime, false);
    },

    /**
     * @param {string} title
     * @param {string} subtitle
     * @return {!DocumentFragment}
     */
    _makeHeaderFragment: function(title, subtitle)
    {
        var fragment = createDocumentFragment();
        fragment.createTextChild(title);
        var subtitleDiv = fragment.createChild("div", "network-header-subtitle");
        subtitleDiv.createTextChild(subtitle);
        return fragment;
    },

    _patchTimelineHeader: function()
    {
        var timelineSorting = createElement("select");

        var option = createElement("option");
        option.value = "startTime";
        option.label = WebInspector.UIString("Timeline");
        timelineSorting.appendChild(option);

        option = createElement("option");
        option.value = "startTime";
        option.label = WebInspector.UIString("Start Time");
        timelineSorting.appendChild(option);

        option = createElement("option");
        option.value = "responseTime";
        option.label = WebInspector.UIString("Response Time");
        timelineSorting.appendChild(option);

        option = createElement("option");
        option.value = "endTime";
        option.label = WebInspector.UIString("End Time");
        timelineSorting.appendChild(option);

        option = createElement("option");
        option.value = "duration";
        option.label = WebInspector.UIString("Duration");
        timelineSorting.appendChild(option);

        option = createElement("option");
        option.value = "latency";
        option.label = WebInspector.UIString("Latency");
        timelineSorting.appendChild(option);

        var header = this._dataGrid.headerTableHeader("timeline");
        header.replaceChild(timelineSorting, header.firstChild);

        timelineSorting.addEventListener("click", function(event) { event.consume() }, false);
        timelineSorting.addEventListener("change", this._sortByTimeline.bind(this), false);
        this._timelineSortSelector = timelineSorting;
    },

    _createSortingFunctions: function()
    {
        this._sortingFunctions = {};
        this._sortingFunctions.name = WebInspector.NetworkDataGridNode.NameComparator;
        this._sortingFunctions.method = WebInspector.NetworkDataGridNode.RequestPropertyComparator.bind(null, "method", false);
        this._sortingFunctions.status = WebInspector.NetworkDataGridNode.RequestPropertyComparator.bind(null, "statusCode", false);
        this._sortingFunctions.scheme = WebInspector.NetworkDataGridNode.RequestPropertyComparator.bind(null, "scheme", false);
        this._sortingFunctions.domain = WebInspector.NetworkDataGridNode.RequestPropertyComparator.bind(null, "domain", false);
        this._sortingFunctions.remoteAddress = WebInspector.NetworkDataGridNode.RemoteAddressComparator;
        this._sortingFunctions.type = WebInspector.NetworkDataGridNode.RequestPropertyComparator.bind(null, "mimeType", false);
        this._sortingFunctions.initiator = WebInspector.NetworkDataGridNode.InitiatorComparator;
        this._sortingFunctions.cookies = WebInspector.NetworkDataGridNode.RequestCookiesCountComparator;
        this._sortingFunctions.setCookies = WebInspector.NetworkDataGridNode.ResponseCookiesCountComparator;
        this._sortingFunctions.size = WebInspector.NetworkDataGridNode.SizeComparator;
        this._sortingFunctions.time = WebInspector.NetworkDataGridNode.RequestPropertyComparator.bind(null, "duration", false);
        this._sortingFunctions.connectionId = WebInspector.NetworkDataGridNode.RequestPropertyComparator.bind(null, "connectionId", false);
        this._sortingFunctions.timeline = WebInspector.NetworkDataGridNode.RequestPropertyComparator.bind(null, "startTime", false);
        this._sortingFunctions.startTime = WebInspector.NetworkDataGridNode.RequestPropertyComparator.bind(null, "startTime", false);
        this._sortingFunctions.endTime = WebInspector.NetworkDataGridNode.RequestPropertyComparator.bind(null, "endTime", false);
        this._sortingFunctions.responseTime = WebInspector.NetworkDataGridNode.RequestPropertyComparator.bind(null, "responseReceivedTime", false);
        this._sortingFunctions.duration = WebInspector.NetworkDataGridNode.RequestPropertyComparator.bind(null, "duration", true);
        this._sortingFunctions.latency = WebInspector.NetworkDataGridNode.RequestPropertyComparator.bind(null, "latency", true);
    },

    _createCalculators: function()
    {
        /** @type {!WebInspector.NetworkTransferTimeCalculator} */
        this._timeCalculator = new WebInspector.NetworkTransferTimeCalculator();
        /** @type {!WebInspector.NetworkTransferDurationCalculator} */
        this._durationCalculator = new WebInspector.NetworkTransferDurationCalculator();

        /** @type {!Object.<string, !WebInspector.NetworkTimeCalculator>} */
        this._calculators = {};
        this._calculators.timeline = this._timeCalculator;
        this._calculators.startTime = this._timeCalculator;
        this._calculators.endTime = this._timeCalculator;
        this._calculators.responseTime = this._timeCalculator;
        this._calculators.duration = this._durationCalculator;
        this._calculators.latency = this._durationCalculator;

        this._calculator = this._timeCalculator;
    },

    _sortItems: function()
    {
        this._removeAllNodeHighlights();
        var columnIdentifier = this._dataGrid.sortColumnIdentifier();
        if (columnIdentifier === "timeline") {
            this._sortByTimeline();
            return;
        }
        var sortingFunction = this._sortingFunctions[columnIdentifier];
        if (!sortingFunction)
            return;

        this._dataGrid.sortNodes(sortingFunction, !this._dataGrid.isSortOrderAscending());
        this._highlightNthMatchedRequestForSearch(this._updateMatchCountAndFindMatchIndex(this._currentMatchedRequestNode), false);
        this._timelineSortSelector.selectedIndex = 0;

        WebInspector.notifications.dispatchEventToListeners(WebInspector.UserMetrics.UserAction, {
            action: WebInspector.UserMetrics.UserActionNames.NetworkSort,
            column: columnIdentifier,
            sortOrder: this._dataGrid.sortOrder()
        });
    },

    _sortByTimeline: function()
    {
        this._removeAllNodeHighlights();
        var selectedIndex = this._timelineSortSelector.selectedIndex;
        if (!selectedIndex)
            selectedIndex = 1; // Sort by start time by default.
        var selectedOption = this._timelineSortSelector[selectedIndex];
        var value = selectedOption.value;

        this._setCalculator(this._calculators[value]);
        var sortingFunction = this._sortingFunctions[value];
        this._dataGrid.sortNodes(sortingFunction);
        this._highlightNthMatchedRequestForSearch(this._updateMatchCountAndFindMatchIndex(this._currentMatchedRequestNode), false);
        this._dataGrid.markColumnAsSortedBy("timeline", WebInspector.DataGrid.Order.Ascending);
    },

    _createStatusBarItems: function()
    {
        this._progressBarContainer = createElement("div");
        this._progressBarContainer.className = "status-bar-item";
    },

    _updateSummaryBar: function()
    {
        var requestsNumber = this._nodesByRequestId.size;

        if (!requestsNumber) {
            if (this._summaryBarElement._isDisplayingWarning)
                return;
            this._summaryBarElement._isDisplayingWarning = true;
            this._summaryBarElement.removeChildren();
            this._summaryBarElement.createChild("div", "warning-icon-small");
            var text = WebInspector.UIString("No requests captured. Reload the page to see detailed information on the network activity.");
            this._summaryBarElement.createTextChild(text);
            this._summaryBarElement.title = text;
            return;
        }
        delete this._summaryBarElement._isDisplayingWarning;

        var transferSize = 0;
        var selectedRequestsNumber = 0;
        var selectedTransferSize = 0;
        var baseTime = -1;
        var maxTime = -1;
        var nodes = this._nodesByRequestId.valuesArray();
        for (var i = 0; i < nodes.length; ++i) {
            var request = nodes[i].request();
            var requestTransferSize = request.transferSize;
            transferSize += requestTransferSize;
            if (!nodes[i]._isFilteredOut) {
                selectedRequestsNumber++;
                selectedTransferSize += requestTransferSize;
            }
            if (request.url === request.target().resourceTreeModel.inspectedPageURL())
                baseTime = request.startTime;
            if (request.endTime > maxTime)
                maxTime = request.endTime;
        }
        var text = "";
        if (selectedRequestsNumber !== requestsNumber) {
            text += String.sprintf(WebInspector.UIString("%d / %d requests"), selectedRequestsNumber, requestsNumber);
            text += "  \u2758  " + String.sprintf(WebInspector.UIString("%s / %s transferred"), Number.bytesToString(selectedTransferSize), Number.bytesToString(transferSize));
        } else {
            text += String.sprintf(WebInspector.UIString("%d requests"), requestsNumber);
            text += "  \u2758  " + String.sprintf(WebInspector.UIString("%s transferred"), Number.bytesToString(transferSize));
        }
        if (baseTime !== -1 && this._mainRequestLoadTime !== -1 && this._mainRequestDOMContentLoadedTime !== -1 && this._mainRequestDOMContentLoadedTime > baseTime) {
            text += "  \u2758  " + String.sprintf(WebInspector.UIString("%s (load: %s, DOMContentLoaded: %s)"),
                        Number.secondsToString(maxTime - baseTime),
                        Number.secondsToString(this._mainRequestLoadTime - baseTime),
                        Number.secondsToString(this._mainRequestDOMContentLoadedTime - baseTime));
        }
        this._summaryBarElement.textContent = text;
        this._summaryBarElement.title = text;
    },

    _scheduleRefresh: function()
    {
        if (this._needsRefresh)
            return;

        this._needsRefresh = true;

        if (this.isShowing() && !this._refreshTimeout)
            this._refreshTimeout = setTimeout(this.refresh.bind(this), WebInspector.NetworkLogView._defaultRefreshDelay);
    },

    _updateDividersIfNeeded: function()
    {
        var timelineOffset = this._dataGrid.columnOffset("timeline");
        // Position timline grid location.
        if (timelineOffset)
            this._timelineGrid.element.style.left = timelineOffset + "px";

        var calculator = this.calculator();
        var proceed = true;
        if (!this.isShowing()) {
            this._scheduleRefresh();
            proceed = false;
        } else {
            calculator.setDisplayWindow(this._timelineGrid.dividersElement.clientWidth);
            proceed = this._timelineGrid.updateDividers(calculator);
        }
        if (!proceed)
            return;

        if (calculator.startAtZero) {
            // If our current sorting method starts at zero, that means it shows all
            // requests starting at the same point, and so onLoad event and DOMContent
            // event lines really wouldn't make much sense here, so don't render them.
            return;
        }

        this._timelineGrid.removeEventDividers();
        if (this._mainRequestLoadTime !== -1) {
            var percent = calculator.computePercentageFromEventTime(this._mainRequestLoadTime);

            var loadDivider = createElement("div");
            loadDivider.className = "network-event-divider network-red-divider";

            var loadDividerPadding = createElement("div");
            loadDividerPadding.className = "network-event-divider-padding";
            loadDividerPadding.title = WebInspector.UIString("Load event");
            loadDividerPadding.appendChild(loadDivider);
            loadDividerPadding.style.left = percent + "%";
            this._timelineGrid.addEventDivider(loadDividerPadding);
        }

        if (this._mainRequestDOMContentLoadedTime !== -1) {
            var percent = calculator.computePercentageFromEventTime(this._mainRequestDOMContentLoadedTime);

            var domContentLoadedDivider = createElement("div");
            domContentLoadedDivider.className = "network-event-divider network-blue-divider";

            var domContentLoadedDividerPadding = createElement("div");
            domContentLoadedDividerPadding.className = "network-event-divider-padding";
            domContentLoadedDividerPadding.title = WebInspector.UIString("DOMContentLoaded event");
            domContentLoadedDividerPadding.appendChild(domContentLoadedDivider);
            domContentLoadedDividerPadding.style.left = percent + "%";
            this._timelineGrid.addEventDivider(domContentLoadedDividerPadding);
        }
    },

    _refreshIfNeeded: function()
    {
        if (this._needsRefresh)
            this.refresh();
    },

    _invalidateAllItems: function()
    {
        var requestIds = this._nodesByRequestId.keysArray();
        for (var i = 0; i < requestIds.length; ++i)
            this._staleRequestIds[requestIds[i]] = true;
    },

    /**
     * @return {!WebInspector.NetworkTimeCalculator}
     */
    calculator: function()
    {
        return this._calculator;
    },

    /**
     * @param {!WebInspector.NetworkTimeCalculator} x
     */
    _setCalculator: function(x)
    {
        if (!x || this._calculator === x)
            return;

        this._calculator = x;
        this._calculator.reset();

        if (this._calculator.startAtZero)
            this._timelineGrid.hideEventDividers();
        else
            this._timelineGrid.showEventDividers();

        this._invalidateAllItems();
        this.refresh();
    },

    _createStatusbarButtons: function()
    {
        this._recordButton = new WebInspector.StatusBarButton("", "record-profile-status-bar-item");
        this._recordButton.addEventListener("click", this._onRecordButtonClicked, this);

        this._clearButton = new WebInspector.StatusBarButton(WebInspector.UIString("Clear"), "clear-status-bar-item");
        this._clearButton.addEventListener("click", this._reset, this);

        this._largerRequestsButton = new WebInspector.StatusBarButton(WebInspector.UIString("Use small resource rows."), "network-larger-resources-status-bar-item");
        this._largerRequestsButton.toggled = WebInspector.settings.resourcesLargeRows.get();
        this._largerRequestsButton.addEventListener("click", this._toggleLargerRequests, this);

        this._preserveLogCheckbox = new WebInspector.StatusBarCheckbox(WebInspector.UIString("Preserve log"));
        this._preserveLogCheckbox.element.title = WebInspector.UIString("Do not clear log on page reload / navigation.");

        this._disableCacheCheckbox = new WebInspector.StatusBarCheckbox(WebInspector.UIString("Disable cache"));
        WebInspector.SettingsUI.bindCheckbox(this._disableCacheCheckbox.inputElement, WebInspector.settings.cacheDisabled);
        this._disableCacheCheckbox.element.title = WebInspector.UIString("Disable cache (while DevTools is open).");
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _loadEventFired: function(event)
    {
        if (!this._recordButton.toggled)
            return;

        var data = /** @type {number} */ (event.data);
        this._mainRequestLoadTime = data || -1;
        // Schedule refresh to update boundaries and draw the new line.
        this._scheduleRefresh();
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _domContentLoadedEventFired: function(event)
    {
        if (!this._recordButton.toggled)
            return;
        var data = /** @type {number} */ (event.data);
        this._mainRequestDOMContentLoadedTime = data || -1;
        // Schedule refresh to update boundaries and draw the new line.
        this._scheduleRefresh();
    },

    wasShown: function()
    {
        this._refreshIfNeeded();
    },

    willHide: function()
    {
        this._popoverHelper.hidePopover();
    },

    refresh: function()
    {
        this._needsRefresh = false;
        if (this._refreshTimeout) {
            clearTimeout(this._refreshTimeout);
            delete this._refreshTimeout;
        }

        this._removeAllNodeHighlights();
        var boundariesChanged = false;
        var calculator = this.calculator();
        if (calculator.updateBoundariesForEventTime) {
            boundariesChanged = calculator.updateBoundariesForEventTime(this._mainRequestLoadTime) || boundariesChanged;
            boundariesChanged = calculator.updateBoundariesForEventTime(this._mainRequestDOMContentLoadedTime) || boundariesChanged;
        }

        var dataGrid = this._dataGrid;
        var rootNode = dataGrid.rootNode();
        var nodesToInsert = [];
        for (var requestId in this._staleRequestIds) {
            var node = this._nodesByRequestId.get(requestId);
            if (!node)
                continue;
            if (!node._isFilteredOut)
                rootNode.removeChild(node);
            node._isFilteredOut = !this._applyFilter(node);
            if (!node._isFilteredOut)
                nodesToInsert.push(node);
        }

        for (var i = 0; i < nodesToInsert.length; ++i) {
            var node = nodesToInsert[i];
            var request = node.request();
            node.refresh();
            dataGrid.insertChild(node);
            node._isMatchingSearchQuery = this._matchRequest(request);
            if (calculator.updateBoundaries(request))
                boundariesChanged = true;
        }

        this._highlightNthMatchedRequestForSearch(this._updateMatchCountAndFindMatchIndex(this._currentMatchedRequestNode), false);

        if (boundariesChanged) {
            // The boundaries changed, so all item graphs are stale.
            this._updateDividersIfNeeded();
            var nodes = this._nodesByRequestId.valuesArray();
            for (var i = 0; i < nodes.length; ++i)
                nodes[i].refreshGraph();
        }

        this._staleRequestIds = {};
        this._updateSummaryBar();
    },

    _onRecordButtonClicked: function()
    {
        if (!this._recordButton.toggled)
            this._reset();
        this._toggleRecordButton(!this._recordButton.toggled);
    },

    /**
     * @param {boolean} toggled
     */
    _toggleRecordButton: function(toggled)
    {
        this._recordButton.toggled = toggled;
        this._recordButton.title = toggled ? WebInspector.UIString("Stop Recording Network Log") : WebInspector.UIString("Record Network Log");
    },

    _reset: function()
    {
        this.dispatchEventToListeners(WebInspector.NetworkLogView.EventTypes.ViewCleared);

        this._clearSearchMatchedList();
        if (this._popoverHelper)
            this._popoverHelper.hidePopover();

        if (this._calculator)
            this._calculator.reset();

        var nodes = this._nodesByRequestId.valuesArray();
        for (var i = 0; i < nodes.length; ++i)
            nodes[i].dispose();

        this._nodesByRequestId.clear();
        this._staleRequestIds = {};
        this._resetSuggestionBuilder();

        if (this._dataGrid) {
            this._dataGrid.rootNode().removeChildren();
            this._updateDividersIfNeeded();
            this._updateSummaryBar();
        }

        this._mainRequestLoadTime = -1;
        this._mainRequestDOMContentLoadedTime = -1;
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onRequestStarted: function(event)
    {
        if (this._recordButton.toggled) {
            var request = /** @type {!WebInspector.NetworkRequest} */ (event.data);
            this._appendRequest(request);
        }
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     */
    _appendRequest: function(request)
    {
        var node = new WebInspector.NetworkDataGridNode(this, request);

        // In case of redirect request id is reassigned to a redirected
        // request and we need to update _nodesByRequestId and search results.
        var originalRequestNode = this._nodesByRequestId.get(request.requestId);
        if (originalRequestNode)
            this._nodesByRequestId.set(originalRequestNode.request().requestId, originalRequestNode);
        this._nodesByRequestId.set(request.requestId, node);

        // Pull all the redirects of the main request upon commit load.
        if (request.redirects) {
            for (var i = 0; i < request.redirects.length; ++i)
                this._refreshRequest(request.redirects[i]);
        }

        this._refreshRequest(request);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onRequestUpdated: function(event)
    {
        var request = /** @type {!WebInspector.NetworkRequest} */ (event.data);
        this._refreshRequest(request);
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     */
    _refreshRequest: function(request)
    {
        if (!this._nodesByRequestId.get(request.requestId))
            return;

        this._suggestionBuilder.addItem(WebInspector.NetworkLogView.FilterType.Domain, request.domain);
        this._suggestionBuilder.addItem(WebInspector.NetworkLogView.FilterType.Method, request.requestMethod);
        this._suggestionBuilder.addItem(WebInspector.NetworkLogView.FilterType.MimeType, request.mimeType);
        this._suggestionBuilder.addItem(WebInspector.NetworkLogView.FilterType.Scheme, "" + request.scheme);
        this._suggestionBuilder.addItem(WebInspector.NetworkLogView.FilterType.StatusCode, "" + request.statusCode);

        var responseHeaders = request.responseHeaders;
        for (var i = 0, l = responseHeaders.length; i < l; ++i)
            this._suggestionBuilder.addItem(WebInspector.NetworkLogView.FilterType.HasResponseHeader, responseHeaders[i].name);
        var cookies = request.responseCookies;
        for (var i = 0, l = cookies ? cookies.length : 0; i < l; ++i) {
            var cookie = cookies[i];
            this._suggestionBuilder.addItem(WebInspector.NetworkLogView.FilterType.SetCookieDomain, cookie.domain());
            this._suggestionBuilder.addItem(WebInspector.NetworkLogView.FilterType.SetCookieName, cookie.name());
            this._suggestionBuilder.addItem(WebInspector.NetworkLogView.FilterType.SetCookieValue, cookie.value());
        }

        this._staleRequestIds[request.requestId] = true;
        this._scheduleRefresh();
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _willReloadPage: function(event)
    {
        this._recordButton.toggled = true;
        if (!this._preserveLogCheckbox.checked())
            this._reset();
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _mainFrameNavigated: function(event)
    {
        if (!this._recordButton.toggled || this._preserveLogCheckbox.checked())
            return;

        var frame = /** @type {!WebInspector.ResourceTreeFrame} */ (event.data);
        var loaderId = frame.loaderId;

        // Pick provisional load requests.
        var requestsToPick = [];
        var requests = frame.target().networkLog.requests;
        for (var i = 0; i < requests.length; ++i) {
            var request = requests[i];
            if (request.loaderId === loaderId)
                requestsToPick.push(request);
        }

        this._reset();

        for (var i = 0; i < requestsToPick.length; ++i)
            this._appendRequest(requestsToPick[i]);
    },

    /**
     * @param {boolean} detailed
     */
    switchViewMode: function(detailed)
    {
        if (this._detailedMode === detailed)
            return;
        this._detailedMode = detailed;

        if (detailed) {
            if (this._dataGrid.selectedNode)
                this._dataGrid.selectedNode.selected = false;
        } else {
            this._removeAllNodeHighlights();
            this._popoverHelper.hidePopover();
        }

        this.element.classList.toggle("brief-mode", !detailed);
        this._updateColumns();
    },

    _toggleLargerRequests: function()
    {
        WebInspector.settings.resourcesLargeRows.set(!WebInspector.settings.resourcesLargeRows.get());
        this._updateRowsSize();
    },

    /**
     * @return {number}
     */
    rowHeight: function()
    {
        return this._rowHeight;
    },

    _updateRowsSize: function()
    {
        var largeRows = this.usesLargeRows();
        this._largerRequestsButton.toggled = largeRows;
        this._rowHeight = largeRows ? 41 : 21;
        this._largerRequestsButton.title = WebInspector.UIString(largeRows ? "Use small resource rows." : "Use large resource rows.");
        this._dataGrid.element.classList.toggle("small", !largeRows);
        this._timelineGrid.element.classList.toggle("small", !largeRows);
        this.dispatchEventToListeners(WebInspector.NetworkLogView.EventTypes.RowSizeChanged, { largeRows: largeRows });
    },

    /**
     * @param {!Element} element
     * @param {!Event} event
     * @return {!Element|!AnchorBox|undefined}
     */
    _getPopoverAnchor: function(element, event)
    {
        if (!this._allowPopover)
            return;
        var anchor = element.enclosingNodeOrSelfWithClass("network-graph-bar") || element.enclosingNodeOrSelfWithClass("network-graph-label");
        if (anchor && anchor.parentElement.request && anchor.parentElement.request.timing)
            return anchor;
        anchor = element.enclosingNodeOrSelfWithClass("network-script-initiated");
        if (anchor && anchor.request) {
            var request = /** @type {!WebInspector.NetworkRequest} */ (anchor.request);
            var initiator = anchor.request.initiator();
            if (initiator && (initiator.stackTrace || initiator.asyncStackTrace))
                return anchor;
        }
    },

    /**
     * @param {!Element} anchor
     * @param {!WebInspector.Popover} popover
     */
    _showPopover: function(anchor, popover)
    {
        var content;
        if (anchor.classList.contains("network-script-initiated")) {
            content = this._generateScriptInitiatedPopoverContent(anchor.request);
            popover.setCanShrink(true);
        } else {
            content = WebInspector.RequestTimingView.createTimingTable(anchor.parentElement.request);
            popover.setCanShrink(false);
        }
        popover.show(content, anchor);
    },

    _onHidePopover: function()
    {
        this._linkifier.reset();
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     * @return {!Element}
     */
    _generateScriptInitiatedPopoverContent: function(request)
    {
        var framesTable = createElementWithClass("table", "network-stack-trace");

        /**
         * @param {!Array.<!ConsoleAgent.CallFrame>} stackTrace
         * @this {WebInspector.NetworkLogView}
         */
        function appendStackTrace(stackTrace)
        {
            for (var i = 0; i < stackTrace.length; ++i) {
                var stackFrame = stackTrace[i];
                var row = createElement("tr");
                row.createChild("td").textContent = stackFrame.functionName || WebInspector.UIString("(anonymous function)");
                row.createChild("td").textContent = " @ ";
                row.createChild("td").appendChild(this._linkifier.linkifyConsoleCallFrame(request.target(), stackFrame));
                framesTable.appendChild(row);
            }
        }

        // Initiator is not null, checked in _getPopoverAnchor.
        var initiator = /** @type {!NetworkAgent.Initiator} */ (request.initiator());
        if (initiator.stackTrace)
            appendStackTrace.call(this, initiator.stackTrace);

        var asyncStackTrace = initiator.asyncStackTrace;
        while (asyncStackTrace) {
            var callFrames = asyncStackTrace.callFrames;
            if (!callFrames || !callFrames.length)
                break;
            var row = framesTable.createChild("tr");
            row.createChild("td", "network-async-trace-description").textContent = WebInspector.asyncStackTraceLabel(asyncStackTrace.description);
            row.createChild("td");
            row.createChild("td");
            appendStackTrace.call(this, callFrames);
            asyncStackTrace = asyncStackTrace.asyncStackTrace;
        }

        return framesTable;
    },

    _updateColumns: function()
    {
        var detailedMode = !!this._detailedMode;
        var visibleColumns = {"name": true};
        if (detailedMode) {
            visibleColumns["timeline"] = true;
            var columnsVisibility = this._coulmnsVisibilitySetting.get();
            for (var columnIdentifier in columnsVisibility)
                visibleColumns[columnIdentifier] = columnsVisibility[columnIdentifier];
        }

        this._dataGrid.setColumnsVisiblity(visibleColumns);
    },

    /**
     * @param {string} columnIdentifier
     */
    _toggleColumnVisibility: function(columnIdentifier)
    {
        var columnsVisibility = this._coulmnsVisibilitySetting.get();
        columnsVisibility[columnIdentifier] = !columnsVisibility[columnIdentifier];
        this._coulmnsVisibilitySetting.set(columnsVisibility);

        this._updateColumns();
    },

    /**
     * @return {!Array.<string>}
     */
    _getConfigurableColumnIDs: function()
    {
        if (this._configurableColumnIDs)
            return this._configurableColumnIDs;

        var columnTitles = WebInspector.NetworkLogView._columnTitles;
        function compare(id1, id2)
        {
            return columnTitles[id1].compareTo(columnTitles[id2]);
        }

        var columnIDs = Object.keys(this._coulmnsVisibilitySetting.get());
        this._configurableColumnIDs = columnIDs.sort(compare);
        return this._configurableColumnIDs;
    },

    /**
     * @param {!Event} event
     */
    _contextMenu: function(event)
    {
        var contextMenu = new WebInspector.ContextMenu(event);

        if (this._detailedMode && event.target.isSelfOrDescendant(this._dataGrid.headerTableBody)) {
            var columnsVisibility = this._coulmnsVisibilitySetting.get();
            var columnIDs = this._getConfigurableColumnIDs();
            var columnTitles = WebInspector.NetworkLogView._columnTitles;
            for (var i = 0; i < columnIDs.length; ++i) {
                var columnIdentifier = columnIDs[i];
                contextMenu.appendCheckboxItem(columnTitles[columnIdentifier], this._toggleColumnVisibility.bind(this, columnIdentifier), !!columnsVisibility[columnIdentifier]);
            }
            contextMenu.show();
            return;
        }

        var gridNode = this._dataGrid.dataGridNodeFromNode(event.target);
        var request = gridNode && gridNode.request();

        /**
         * @param {string} url
         */
        function openResourceInNewTab(url)
        {
            InspectorFrontendHost.openInNewTab(url);
        }

        if (request) {
            contextMenu.appendItem(WebInspector.openLinkExternallyLabel(), openResourceInNewTab.bind(null, request.url));
            contextMenu.appendSeparator();
            contextMenu.appendItem(WebInspector.copyLinkAddressLabel(), this._copyLocation.bind(this, request));
            if (request.requestHeadersText())
                contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Copy request headers" : "Copy Request Headers"), this._copyRequestHeaders.bind(this, request));
            if (request.responseHeadersText)
                contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Copy response headers" : "Copy Response Headers"), this._copyResponseHeaders.bind(this, request));
            if (request.finished)
                contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Copy response" : "Copy Response"), this._copyResponse.bind(this, request));
            contextMenu.appendItem(WebInspector.UIString("Copy as cURL"), this._copyCurlCommand.bind(this, request));
        }
        contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Copy all as HAR" : "Copy All as HAR"), this._copyAll.bind(this));

        contextMenu.appendSeparator();
        contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Save as HAR with content" : "Save as HAR with Content"), this._exportAll.bind(this));

        contextMenu.appendSeparator();
        contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Clear browser cache" : "Clear Browser Cache"), this._clearBrowserCache.bind(this));
        contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Clear browser cookies" : "Clear Browser Cookies"), this._clearBrowserCookies.bind(this));

        if (request && request.resourceType() === WebInspector.resourceTypes.XHR) {
            contextMenu.appendSeparator();
            contextMenu.appendItem(WebInspector.UIString("Replay XHR"), request.replayXHR.bind(request));
            contextMenu.appendSeparator();
        }

        contextMenu.show();
    },

    _harRequests: function()
    {
        var requests = this._nodesByRequestId.valuesArray().map(function(node) { return node.request(); });
        var httpRequests = requests.filter(WebInspector.NetworkLogView.HTTPRequestsFilter);
        httpRequests = httpRequests.filter(WebInspector.NetworkLogView.FinishedRequestsFilter);
        return httpRequests.filter(WebInspector.NetworkLogView.NonDevToolsRequestsFilter);
    },

    _copyAll: function()
    {
        var harArchive = {
            log: (new WebInspector.HARLog(this._harRequests())).build()
        };
        InspectorFrontendHost.copyText(JSON.stringify(harArchive, null, 2));
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     */
    _copyLocation: function(request)
    {
        InspectorFrontendHost.copyText(request.url);
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     */
    _copyRequestHeaders: function(request)
    {
        InspectorFrontendHost.copyText(request.requestHeadersText());
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     */
    _copyResponse: function(request)
    {
        /**
         * @param {?string} content
         */
        function callback(content)
        {
            if (request.contentEncoded)
                content = request.asDataURL();
            InspectorFrontendHost.copyText(content || "");
        }
        request.requestContent(callback);
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     */
    _copyResponseHeaders: function(request)
    {
        InspectorFrontendHost.copyText(request.responseHeadersText);
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     */
    _copyCurlCommand: function(request)
    {
        InspectorFrontendHost.copyText(this._generateCurlCommand(request));
    },

    _exportAll: function()
    {
        var filename = WebInspector.targetManager.inspectedPageDomain() + ".har";
        var stream = new WebInspector.FileOutputStream();
        stream.open(filename, openCallback.bind(this));

        /**
         * @param {boolean} accepted
         * @this {WebInspector.NetworkLogView}
         */
        function openCallback(accepted)
        {
            if (!accepted)
                return;
            var progressIndicator = new WebInspector.ProgressIndicator();
            this._progressBarContainer.appendChild(progressIndicator.element);
            var harWriter = new WebInspector.HARWriter();
            harWriter.write(stream, this._harRequests(), progressIndicator);
        }
    },

    _clearBrowserCache: function()
    {
        if (confirm(WebInspector.UIString("Are you sure you want to clear browser cache?")))
            NetworkAgent.clearBrowserCache();
    },

    _clearBrowserCookies: function()
    {
        if (confirm(WebInspector.UIString("Are you sure you want to clear browser cookies?")))
            NetworkAgent.clearBrowserCookies();
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     * @return {boolean}
     */
    _matchRequest: function(request)
    {
        var re = this._searchRegExp;
        if (!re)
            return false;
        return re.test(request.name()) || re.test(request.path());
    },

    _clearSearchMatchedList: function()
    {
        this._matchedRequestCount = -1;
        this._currentMatchedRequestNode = null;
        this._removeAllHighlights();
    },

    _removeAllHighlights: function()
    {
        this._removeAllNodeHighlights();
        for (var i = 0; i < this._highlightedSubstringChanges.length; ++i)
            WebInspector.revertDomChanges(this._highlightedSubstringChanges[i]);
        this._highlightedSubstringChanges = [];
    },

    /**
     * @param {number} n
     * @param {boolean} reveal
     */
    _highlightNthMatchedRequestForSearch: function(n, reveal)
    {
        this._removeAllHighlights();

        /** @type {!Array.<!WebInspector.NetworkDataGridNode>} */
        var nodes = this._dataGrid.rootNode().children;
        var matchCount = 0;
        var node = null;
        for (var i = 0; i < nodes.length; ++i) {
            if (nodes[i]._isMatchingSearchQuery) {
                if (matchCount === n) {
                    node = nodes[i];
                    break;
                }
                matchCount++;
            }
        }
        if (!node) {
            this._currentMatchedRequestNode = null;
            return;
        }

        var request = node.request();
        var regExp = this._searchRegExp;
        var nameMatched = request.name().match(regExp);
        var pathMatched = request.path().match(regExp);
        if (!nameMatched && pathMatched && !this._largerRequestsButton.toggled)
            this._toggleLargerRequests();
        if (reveal)
            WebInspector.Revealer.reveal(request);
        var highlightedSubstringChanges = node.highlightMatchedSubstring(regExp);
        this._highlightedSubstringChanges.push(highlightedSubstringChanges);

        this._currentMatchedRequestNode = node;
        this._currentMatchedRequestIndex = n;
        this.dispatchEventToListeners(WebInspector.NetworkLogView.EventTypes.SearchIndexUpdated, n);
    },

    /**
     * @param {!WebInspector.SearchableView.SearchConfig} searchConfig
     * @param {boolean} shouldJump
     * @param {boolean=} jumpBackwards
     */
    performSearch: function(searchConfig, shouldJump, jumpBackwards)
    {
        var query = searchConfig.query;
        var currentMatchedRequestNode = this._currentMatchedRequestNode;
        this._clearSearchMatchedList();
        this._searchRegExp = createPlainTextSearchRegex(query, "i");

        /** @type {!Array.<!WebInspector.NetworkDataGridNode>} */
        var nodes = this._dataGrid.rootNode().children;
        for (var i = 0; i < nodes.length; ++i)
            nodes[i]._isMatchingSearchQuery = this._matchRequest(nodes[i].request());
        var newMatchedRequestIndex = this._updateMatchCountAndFindMatchIndex(currentMatchedRequestNode);
        if (!newMatchedRequestIndex && jumpBackwards)
            newMatchedRequestIndex = this._matchedRequestCount - 1;
        this._highlightNthMatchedRequestForSearch(newMatchedRequestIndex, shouldJump);
    },

    /**
     * @return {boolean}
     */
    supportsCaseSensitiveSearch: function()
    {
        return false;
    },

    /**
     * @return {boolean}
     */
    supportsRegexSearch: function()
    {
        return false;
    },

    /**
     * @param {?WebInspector.NetworkDataGridNode} node
     * @return {number}
     */
    _updateMatchCountAndFindMatchIndex: function(node)
    {
        /** @type {!Array.<!WebInspector.NetworkDataGridNode>} */
        var nodes = this._dataGrid.rootNode().children;
        var matchCount = 0;
        var matchIndex = 0;
        for (var i = 0; i < nodes.length; ++i) {
            if (!nodes[i]._isMatchingSearchQuery)
                continue;
            if (node === nodes[i])
                matchIndex = matchCount;
            matchCount++;
        }
        if (this._matchedRequestCount !== matchCount) {
            this._matchedRequestCount = matchCount;
            this.dispatchEventToListeners(WebInspector.NetworkLogView.EventTypes.SearchCountUpdated, matchCount);
        }
        return matchIndex;
    },

    /**
     * @param {number} index
     * @return {number}
     */
    _normalizeSearchResultIndex: function(index)
    {
        return (index + this._matchedRequestCount) % this._matchedRequestCount;
    },

    /**
     * @param {!WebInspector.NetworkDataGridNode} node
     * @return {boolean}
     */
    _applyFilter: function(node)
    {
        var request = node.request();
        var resourceType = request.resourceType();
        if (resourceType === WebInspector.resourceTypes.TextTrack)
            resourceType = WebInspector.resourceTypes.Other;
        if (!this._resourceTypeFilterUI.accept(resourceType.name()))
            return false;
        if (this._dataURLFilterUI.checked() && request.parsedURL.isDataURL())
            return false;
        for (var i = 0; i < this._filters.length; ++i) {
            if (!this._filters[i](request))
                return false;
        }
        return true;
    },

    /**
     * @param {string} query
     */
    _parseFilterQuery: function(query)
    {
        var parsedQuery = this._suggestionBuilder.parseQuery(query);
        this._filters = parsedQuery.text.map(this._createTextFilter);
        var filters = parsedQuery.filters;
        var n = parsedQuery.filters.length;
        for (var i = 0; i < n; ++i) {
            var filter = parsedQuery.filters[i];
            var filterType = /** @type {!WebInspector.NetworkLogView.FilterType} */ (filter.type);
            this._filters.push(this._createFilter(filterType, filter.data, filter.negative));
        }
    },

    /**
     * @param {string} text
     * @return {!WebInspector.NetworkLogView.Filter}
     */
    _createTextFilter: function(text)
    {
        var regexp = new RegExp(text.escapeForRegExp(), "i");
        return WebInspector.NetworkLogView._requestNameOrPathFilter.bind(null, regexp);
    },

    /**
     * @param {!WebInspector.NetworkLogView.FilterType} type
     * @param {string} value
     * @param {boolean} negative
     * @return {!WebInspector.NetworkLogView.Filter}
     */
    _createFilter: function(type, value, negative)
    {
        var filter = this._createSpecialFilter(type, value);
        if (!filter)
            return this._createTextFilter((negative ? "-" : "") + type + ":" + value);
        if (negative)
            return WebInspector.NetworkLogView._negativeFilter.bind(null, filter);
        return filter;
    },

    /**
     * @param {!WebInspector.NetworkLogView.FilterType} type
     * @param {string} value
     * @return {?WebInspector.NetworkLogView.Filter}
     */
    _createSpecialFilter: function(type, value)
    {
        switch (type) {
        case WebInspector.NetworkLogView.FilterType.Domain:
            return WebInspector.NetworkLogView._requestDomainFilter.bind(null, value);

        case WebInspector.NetworkLogView.FilterType.HasResponseHeader:
            return WebInspector.NetworkLogView._requestResponseHeaderFilter.bind(null, value);

        case WebInspector.NetworkLogView.FilterType.Is:
            if (value.toLowerCase() === WebInspector.NetworkLogView.IsFilterType.Running)
                return WebInspector.NetworkLogView._runningRequestFilter;
            break;

        case WebInspector.NetworkLogView.FilterType.Method:
            return WebInspector.NetworkLogView._requestMethodFilter.bind(null, value);

        case WebInspector.NetworkLogView.FilterType.MimeType:
            return WebInspector.NetworkLogView._requestMimeTypeFilter.bind(null, value);

        case WebInspector.NetworkLogView.FilterType.Scheme:
            return WebInspector.NetworkLogView._requestSchemeFilter.bind(null, value);

        case WebInspector.NetworkLogView.FilterType.SetCookieDomain:
            return WebInspector.NetworkLogView._requestSetCookieDomainFilter.bind(null, value);

        case WebInspector.NetworkLogView.FilterType.SetCookieName:
            return WebInspector.NetworkLogView._requestSetCookieNameFilter.bind(null, value);

        case WebInspector.NetworkLogView.FilterType.SetCookieValue:
            return WebInspector.NetworkLogView._requestSetCookieValueFilter.bind(null, value);

        case WebInspector.NetworkLogView.FilterType.StatusCode:
            return WebInspector.NetworkLogView._statusCodeFilter.bind(null, value);
        }
        return null;
    },

    _filterRequests: function()
    {
        this._removeAllHighlights();
        this._invalidateAllItems();
        this.refresh();
    },

    jumpToPreviousSearchResult: function()
    {
        if (!this._matchedRequestCount)
            return;
        var index = this._normalizeSearchResultIndex(this._currentMatchedRequestIndex - 1);
        this._highlightNthMatchedRequestForSearch(index, true);
    },

    jumpToNextSearchResult: function()
    {
        if (!this._matchedRequestCount)
            return;
        var index = this._normalizeSearchResultIndex(this._currentMatchedRequestIndex + 1);
        this._highlightNthMatchedRequestForSearch(index, true);
    },

    searchCanceled: function()
    {
        delete this._searchRegExp;
        this._clearSearchMatchedList();
        this.dispatchEventToListeners(WebInspector.NetworkLogView.EventTypes.SearchCountUpdated, 0);
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     */
    revealAndHighlightRequest: function(request)
    {
        this._removeAllNodeHighlights();

        var node = this._nodesByRequestId.get(request.requestId);
        if (node) {
            node.reveal();
            this._highlightNode(node);
        }
    },

    _removeAllNodeHighlights: function()
    {
        if (this._highlightedNode) {
            this._highlightedNode.element().classList.remove("highlighted-row");
            delete this._highlightedNode;
        }
    },

    /**
     * @param {!WebInspector.NetworkDataGridNode} node
     */
    _highlightNode: function(node)
    {
        WebInspector.runCSSAnimationOnce(node.element(), "highlighted-row");
        this._highlightedNode = node;
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     * @return {string}
     */
    _generateCurlCommand: function(request)
    {
        var command = ["curl"];
        // These headers are derived from URL (except "version") and would be added by cURL anyway.
        var ignoredHeaders = {"host": 1, "method": 1, "path": 1, "scheme": 1, "version": 1};

        function escapeStringWin(str)
        {
            /* Replace quote by double quote (but not by \") because it is
               recognized by both cmd.exe and MS Crt arguments parser.

               Replace % by "%" because it could be expanded to an environment
               variable value. So %% becomes "%""%". Even if an env variable ""
               (2 doublequotes) is declared, the cmd.exe will not
               substitute it with its value.

               Replace each backslash with double backslash to make sure
               MS Crt arguments parser won't collapse them.

               Replace new line outside of quotes since cmd.exe doesn't let
               to do it inside.
            */
            return "\"" + str.replace(/"/g, "\"\"")
                             .replace(/%/g, "\"%\"")
                             .replace(/\\/g, "\\\\")
                             .replace(/[\r\n]+/g, "\"^$&\"") + "\"";
        }

        function escapeStringPosix(str)
        {
            function escapeCharacter(x)
            {
                var code = x.charCodeAt(0);
                if (code < 256) {
                    // Add leading zero when needed to not care about the next character.
                    return code < 16 ? "\\x0" + code.toString(16) : "\\x" + code.toString(16);
                 }
                 code = code.toString(16);
                 return "\\u" + ("0000" + code).substr(code.length, 4);
             }

            if (/[^\x20-\x7E]|\'/.test(str)) {
                // Use ANSI-C quoting syntax.
                return "$\'" + str.replace(/\\/g, "\\\\")
                                  .replace(/\'/g, "\\\'")
                                  .replace(/\n/g, "\\n")
                                  .replace(/\r/g, "\\r")
                                  .replace(/[^\x20-\x7E]/g, escapeCharacter) + "'";
            } else {
                // Use single quote syntax.
                return "'" + str + "'";
            }
        }

        // cURL command expected to run on the same platform that DevTools run
        // (it may be different from the inspected page platform).
        var escapeString = WebInspector.isWin() ? escapeStringWin : escapeStringPosix;

        command.push(escapeString(request.url).replace(/[[{}\]]/g, "\\$&"));

        var inferredMethod = "GET";
        var data = [];
        var requestContentType = request.requestContentType();
        if (requestContentType && requestContentType.startsWith("application/x-www-form-urlencoded") && request.requestFormData) {
           data.push("--data");
           data.push(escapeString(request.requestFormData));
           ignoredHeaders["content-length"] = true;
           inferredMethod = "POST";
        } else if (request.requestFormData) {
           data.push("--data-binary");
           data.push(escapeString(request.requestFormData));
           ignoredHeaders["content-length"] = true;
           inferredMethod = "POST";
        }

        if (request.requestMethod !== inferredMethod) {
            command.push("-X");
            command.push(request.requestMethod);
        }

        var requestHeaders = request.requestHeaders();
        for (var i = 0; i < requestHeaders.length; i++) {
            var header = requestHeaders[i];
            var name = header.name.replace(/^:/, ""); // Translate SPDY v3 headers to HTTP headers.
            if (name.toLowerCase() in ignoredHeaders)
                continue;
            command.push("-H");
            command.push(escapeString(name + ": " + header.value));
        }
        command = command.concat(data);
        command.push("--compressed");
        return command.join(" ");
    },

    __proto__: WebInspector.VBox.prototype
}

/** @typedef {function(!WebInspector.NetworkRequest): boolean} */
WebInspector.NetworkLogView.Filter;

/**
 * @param {!WebInspector.NetworkLogView.Filter} filter
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView._negativeFilter = function(filter, request)
{
    return !filter(request);
}

/**
 * @param {!RegExp} regex
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView._requestNameOrPathFilter = function(regex, request)
{
    return regex.test(request.name()) || regex.test(request.path());
}

/**
 * @param {string} value
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView._requestDomainFilter = function(value, request)
{
    return request.domain === value;
}

/**
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView._runningRequestFilter = function(request)
{
    return !request.finished;
}

/**
 * @param {string} value
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView._requestResponseHeaderFilter = function(value, request)
{
    return request.responseHeaderValue(value) !== undefined;
}

/**
 * @param {string} value
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView._requestMethodFilter = function(value, request)
{
    return request.requestMethod === value;
}

/**
 * @param {string} value
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView._requestMimeTypeFilter = function(value, request)
{
    return request.mimeType === value;
}

/**
 * @param {string} value
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView._requestSchemeFilter = function(value, request)
{
    return request.scheme === value;
}

/**
 * @param {string} value
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView._requestSetCookieDomainFilter = function(value, request)
{
    var cookies = request.responseCookies;
    for (var i = 0, l = cookies ? cookies.length : 0; i < l; ++i) {
        if (cookies[i].domain() === value)
            return false;
    }
    return false;
}

/**
 * @param {string} value
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView._requestSetCookieNameFilter = function(value, request)
{
    var cookies = request.responseCookies;
    for (var i = 0, l = cookies ? cookies.length : 0; i < l; ++i) {
        if (cookies[i].name() === value)
            return false;
    }
    return false;
}

/**
 * @param {string} value
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView._requestSetCookieValueFilter = function(value, request)
{
    var cookies = request.responseCookies;
    for (var i = 0, l = cookies ? cookies.length : 0; i < l; ++i) {
        if (cookies[i].value() === value)
            return false;
    }
    return false;
}

/**
 * @param {string} value
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView._statusCodeFilter = function(value, request)
{
    return ("" + request.statusCode) === value;
}

/**
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView.HTTPRequestsFilter = function(request)
{
    return request.parsedURL.isValid && (request.scheme in WebInspector.NetworkLogView.HTTPSchemas);
}

/**
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView.NonDevToolsRequestsFilter = function(request)
{
    return !WebInspector.NetworkManager.hasDevToolsRequestHeader(request);
}

/**
 * @param {!WebInspector.NetworkRequest} request
 * @return {boolean}
 */
WebInspector.NetworkLogView.FinishedRequestsFilter = function(request)
{
    return request.finished;
}

WebInspector.NetworkLogView.EventTypes = {
    ViewCleared: "ViewCleared",
    RowSizeChanged: "RowSizeChanged",
    RequestSelected: "RequestSelected",
    SearchCountUpdated: "SearchCountUpdated",
    SearchIndexUpdated: "SearchIndexUpdated"
};

/**
 * @constructor
 * @implements {WebInspector.ContextMenu.Provider}
 * @implements {WebInspector.Searchable}
 * @extends {WebInspector.Panel}
 */
WebInspector.NetworkPanel = function()
{
    WebInspector.Panel.call(this, "network");
    this.registerRequiredCSS("network/networkPanel.css");

    this._panelStatusBarElement = this.element.createChild("div", "panel-status-bar");
    this._filterBar = new WebInspector.FilterBar();
    this._filtersContainer = this.element.createChild("div", "network-filters-header hidden");
    this._filtersContainer.appendChild(this._filterBar.filtersElement());
    this._filterBar.addEventListener(WebInspector.FilterBar.Events.FiltersToggled, this._onFiltersToggled, this);
    this._filterBar.setName("networkPanel");

    this._searchableView = new WebInspector.SearchableView(this);
    this._searchableView.show(this.element);
    var contentsElement = this._searchableView.element;

    this._splitView = new WebInspector.SplitView(true, false, "networkPanelSplitViewState");
    this._splitView.show(contentsElement);
    this._splitView.hideMain();

    var defaultColumnsVisibility = WebInspector.NetworkLogView.defaultColumnsVisibility;
    var networkLogColumnsVisibilitySetting = WebInspector.settings.createSetting("networkLogColumnsVisibility", defaultColumnsVisibility);
    var savedColumnsVisibility = networkLogColumnsVisibilitySetting.get();
    var columnsVisibility = {};
    for (var columnId in defaultColumnsVisibility)
        columnsVisibility[columnId] = savedColumnsVisibility.hasOwnProperty(columnId) ? savedColumnsVisibility[columnId] : defaultColumnsVisibility[columnId];
    networkLogColumnsVisibilitySetting.set(columnsVisibility);

    /** @type {!WebInspector.NetworkLogView} */
    this._networkLogView = new WebInspector.NetworkLogView(this._filterBar, networkLogColumnsVisibilitySetting);
    this._networkLogView.show(this._splitView.sidebarElement());

    var viewsContainerView = new WebInspector.VBox();
    this._viewsContainerElement = viewsContainerView.element;
    this._viewsContainerElement.id = "network-views";
    if (!this._networkLogView.usesLargeRows())
        this._viewsContainerElement.classList.add("small");
    viewsContainerView.show(this._splitView.mainElement());

    this._networkLogView.addEventListener(WebInspector.NetworkLogView.EventTypes.ViewCleared, this._onViewCleared, this);
    this._networkLogView.addEventListener(WebInspector.NetworkLogView.EventTypes.RowSizeChanged, this._onRowSizeChanged, this);
    this._networkLogView.addEventListener(WebInspector.NetworkLogView.EventTypes.RequestSelected, this._onRequestSelected, this);
    this._networkLogView.addEventListener(WebInspector.NetworkLogView.EventTypes.SearchCountUpdated, this._onSearchCountUpdated, this);
    this._networkLogView.addEventListener(WebInspector.NetworkLogView.EventTypes.SearchIndexUpdated, this._onSearchIndexUpdated, this);

    this._closeButtonElement = this._viewsContainerElement.createChild("div", "close-button");
    this._closeButtonElement.id = "network-close-button";
    this._closeButtonElement.addEventListener("click", this._toggleGridMode.bind(this), false);
    this._viewsContainerElement.appendChild(this._closeButtonElement);

    var statusBarItems = this._networkLogView.statusBarItems();
    for (var i = 0; i < statusBarItems.length; ++i)
        this._panelStatusBarElement.appendChild(statusBarItems[i]);

    /**
     * @this {WebInspector.NetworkPanel}
     * @return {?WebInspector.SourceFrame}
     */
    function sourceFrameGetter()
    {
        return this._networkItemView.currentSourceFrame();
    }
    WebInspector.GoToLineDialog.install(this, sourceFrameGetter.bind(this));
}

WebInspector.NetworkPanel.prototype = {
    /**
     * @param {!WebInspector.Event} event
     */
    _onFiltersToggled: function(event)
    {
        var toggled = /** @type {boolean} */ (event.data);
        this._filtersContainer.classList.toggle("hidden", !toggled);
        this.element.classList.toggle("filters-toggled", toggled);
        this.doResize();
    },

    /**
     * @return {!Array.<!Element>}
     */
    elementsToRestoreScrollPositionsFor: function()
    {
        return this._networkLogView.elementsToRestoreScrollPositionsFor();
    },

    /**
     * @return {!WebInspector.SearchableView}
     */
    searchableView: function()
    {
        return this._searchableView;
    },

    /**
     * @param {!KeyboardEvent} event
     */
    handleShortcut: function(event)
    {
        if (this._viewingRequestMode && event.keyCode === WebInspector.KeyboardShortcut.Keys.Esc.code) {
            this._toggleGridMode();
            event.handled = true;
            return;
        }

        WebInspector.Panel.prototype.handleShortcut.call(this, event);
    },

    wasShown: function()
    {
        WebInspector.Panel.prototype.wasShown.call(this);
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     */
    revealAndHighlightRequest: function(request)
    {
        this._toggleGridMode();
        if (request)
            this._networkLogView.revealAndHighlightRequest(request);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onViewCleared: function(event)
    {
        this._closeVisibleRequest();
        this._toggleGridMode();
        this._viewsContainerElement.removeChildren();
        this._viewsContainerElement.appendChild(this._closeButtonElement);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onRowSizeChanged: function(event)
    {
        this._viewsContainerElement.classList.toggle("small", !event.data.largeRows);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onSearchCountUpdated: function(event)
    {
        var count = /** @type {number} */ (event.data);
        this._searchableView.updateSearchMatchesCount(count);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onSearchIndexUpdated: function(event)
    {
        var index = /** @type {number} */ (event.data);
        this._searchableView.updateCurrentMatchIndex(index);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onRequestSelected: function(event)
    {
        var request = /** @type {!WebInspector.NetworkRequest} */ (event.data);
        this._showRequest(request);
    },

    /**
     * @param {?WebInspector.NetworkRequest} request
     */
    _showRequest: function(request)
    {
        if (!request)
            return;

        this._toggleViewingRequestMode();

        if (this._networkItemView) {
            this._networkItemView.detach();
            delete this._networkItemView;
        }

        var view = new WebInspector.NetworkItemView(request);
        view.show(this._viewsContainerElement);
        this._networkItemView = view;
    },

    _closeVisibleRequest: function()
    {
        this.element.classList.remove("viewing-resource");

        if (this._networkItemView) {
            this._networkItemView.detach();
            delete this._networkItemView;
        }
    },

    _toggleGridMode: function()
    {
        if (this._viewingRequestMode) {
            this._viewingRequestMode = false;
            this.element.classList.remove("viewing-resource");
            this._splitView.hideMain();
        }

        this._networkLogView.switchViewMode(true);
        this._networkLogView.setAllowPopover(true);
        this._networkLogView._allowRequestSelection = false;
    },

    _toggleViewingRequestMode: function()
    {
        if (this._viewingRequestMode)
            return;
        this._viewingRequestMode = true;

        this.element.classList.add("viewing-resource");
        this._splitView.showBoth();
        this._networkLogView.setAllowPopover(false);
        this._networkLogView._allowRequestSelection = true;
        this._networkLogView.switchViewMode(false);
    },

    /**
     * @param {!WebInspector.SearchableView.SearchConfig} searchConfig
     * @param {boolean} shouldJump
     * @param {boolean=} jumpBackwards
     */
    performSearch: function(searchConfig, shouldJump, jumpBackwards)
    {
        this._networkLogView.performSearch(searchConfig, shouldJump, jumpBackwards);
    },

    jumpToPreviousSearchResult: function()
    {
        this._networkLogView.jumpToPreviousSearchResult();
    },

    /**
     * @return {boolean}
     */
    supportsCaseSensitiveSearch: function()
    {
        return false;
    },

    /**
     * @return {boolean}
     */
    supportsRegexSearch: function()
    {
        return false;
    },

    jumpToNextSearchResult: function()
    {
        this._networkLogView.jumpToNextSearchResult();
    },

    searchCanceled: function()
    {
        this._networkLogView.searchCanceled();
    },

    /**
     * @param {!Event} event
     * @param {!WebInspector.ContextMenu} contextMenu
     * @param {!Object} target
     * @this {WebInspector.NetworkPanel}
     */
    appendApplicableItems: function(event, contextMenu, target)
    {
        /**
         * @this {WebInspector.NetworkPanel}
         */
        function reveal(request)
        {
            WebInspector.inspectorView.setCurrentPanel(this);
            this.revealAndHighlightRequest(request);
        }

        /**
         * @this {WebInspector.NetworkPanel}
         */
        function appendRevealItem(request)
        {
            var revealText = WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Reveal in Network panel" : "Reveal in Network Panel");
            contextMenu.appendItem(revealText, reveal.bind(this, request));
        }

        if (target instanceof WebInspector.Resource) {
            var resource = /** @type {!WebInspector.Resource} */ (target);
            if (resource.request)
                appendRevealItem.call(this, resource.request);
            return;
        }
        if (target instanceof WebInspector.UISourceCode) {
            var uiSourceCode = /** @type {!WebInspector.UISourceCode} */ (target);
            var resource = WebInspector.resourceForURL(uiSourceCode.url);
            if (resource && resource.request)
                appendRevealItem.call(this, resource.request);
            return;
        }

        if (!(target instanceof WebInspector.NetworkRequest))
            return;
        var request = /** @type {!WebInspector.NetworkRequest} */ (target);
        if (this._networkItemView && this._networkItemView.isShowing() && this._networkItemView.request() === request)
            return;

        appendRevealItem.call(this, request);
    },

    __proto__: WebInspector.Panel.prototype
}

/**
 * @constructor
 * @implements {WebInspector.ContextMenu.Provider}
 */
WebInspector.NetworkPanel.ContextMenuProvider = function()
{
}

WebInspector.NetworkPanel.ContextMenuProvider.prototype = {
    /**
     * @param {!Event} event
     * @param {!WebInspector.ContextMenu} contextMenu
     * @param {!Object} target
     */
    appendApplicableItems: function(event, contextMenu, target)
    {
        WebInspector.NetworkPanel._instance().appendApplicableItems(event, contextMenu, target);
    }
}

/**
 * @constructor
 * @implements {WebInspector.Revealer}
 */
WebInspector.NetworkPanel.RequestRevealer = function()
{
}

WebInspector.NetworkPanel.RequestRevealer.prototype = {
    /**
     * @param {!Object} request
     * @param {number=} lineNumber
     * @return {!Promise}
     */
    reveal: function(request, lineNumber)
    {
        if (request instanceof WebInspector.NetworkRequest) {

            var panel = WebInspector.NetworkPanel._instance();
            WebInspector.inspectorView.setCurrentPanel(panel);
            panel.revealAndHighlightRequest(request);
            return Promise.resolve();
        }
        return Promise.rejectWithError("Internal error: not a network request");
    }
}

/**
 * @constructor
 * @implements {WebInspector.TimelineGrid.Calculator}
 */
WebInspector.NetworkTimeCalculator = function(startAtZero)
{
    this.startAtZero = startAtZero;
}

/** @type {!WebInspector.UIStringFormat} */
WebInspector.NetworkTimeCalculator._latencyDownloadTotalFormat = new WebInspector.UIStringFormat("%s latency, %s download (%s total)");

/** @type {!WebInspector.UIStringFormat} */
WebInspector.NetworkTimeCalculator._latencyFormat = new WebInspector.UIStringFormat("%s latency");

/** @type {!WebInspector.UIStringFormat} */
WebInspector.NetworkTimeCalculator._downloadFormat = new WebInspector.UIStringFormat("%s download");

/** @type {!WebInspector.UIStringFormat} */
WebInspector.NetworkTimeCalculator._fromServiceWorkerFormat = new WebInspector.UIStringFormat("%s (from ServiceWorker)");

/** @type {!WebInspector.UIStringFormat} */
WebInspector.NetworkTimeCalculator._fromCacheFormat = new WebInspector.UIStringFormat("%s (from cache)");

WebInspector.NetworkTimeCalculator.prototype = {
    /**
     * @override
     * @return {number}
     */
    paddingLeft: function()
    {
        return 0;
    },

    /**
     * @override
     * @param {number} time
     * @return {number}
     */
    computePosition: function(time)
    {
        return (time - this._minimumBoundary) / this.boundarySpan() * this._workingArea;
    },

    /**
     * @override
     * @param {number} value
     * @param {number=} precision
     * @return {string}
     */
    formatTime: function(value, precision)
    {
        return Number.secondsToString(value);
    },

    /**
     * @override
     * @return {number}
     */
    minimumBoundary: function()
    {
        return this._minimumBoundary;
    },

    /**
     * @override
     * @return {number}
     */
    zeroTime: function()
    {
        return this._minimumBoundary;
    },

    /**
     * @override
     * @return {number}
     */
    maximumBoundary: function()
    {
        return this._maximumBoundary;
    },

    /**
     * @override
     * @return {number}
     */
    boundarySpan: function()
    {
        return this._maximumBoundary - this._minimumBoundary;
    },

    reset: function()
    {
        delete this._minimumBoundary;
        delete this._maximumBoundary;
    },

    /**
     * @return {number}
     */
    _value: function(item)
    {
        return 0;
    },

    /**
     * @param {number} clientWidth
     */
    setDisplayWindow: function(clientWidth)
    {
        this._workingArea = clientWidth;
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     * @return {!{start: number, middle: number, end: number}}
     */
    computeBarGraphPercentages: function(request)
    {
        if (request.startTime !== -1)
            var start = ((request.startTime - this._minimumBoundary) / this.boundarySpan()) * 100;
        else
            var start = 0;

        if (request.responseReceivedTime !== -1)
            var middle = ((request.responseReceivedTime - this._minimumBoundary) / this.boundarySpan()) * 100;
        else
            var middle = (this.startAtZero ? start : 100);

        if (request.endTime !== -1)
            var end = ((request.endTime - this._minimumBoundary) / this.boundarySpan()) * 100;
        else
            var end = (this.startAtZero ? middle : 100);

        if (this.startAtZero) {
            end -= start;
            middle -= start;
            start = 0;
        }

        return {start: start, middle: middle, end: end};
    },

    /**
     * @param {number} eventTime
     * @return {number}
     */
    computePercentageFromEventTime: function(eventTime)
    {
        // This function computes a percentage in terms of the total loading time
        // of a specific event. If startAtZero is set, then this is useless, and we
        // want to return 0.
        if (eventTime !== -1 && !this.startAtZero)
            return ((eventTime - this._minimumBoundary) / this.boundarySpan()) * 100;

        return 0;
    },

    /**
     * @param {number} eventTime
     * @return {boolean}
     */
    updateBoundariesForEventTime: function(eventTime)
    {
        if (eventTime === -1 || this.startAtZero)
            return false;

        if (typeof this._maximumBoundary === "undefined" || eventTime > this._maximumBoundary) {
            this._maximumBoundary = eventTime;
            return true;
        }
        return false;
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     * @return {!{left: string, right: string, tooltip: (string|undefined)}}
     */
    computeBarGraphLabels: function(request)
    {
        var rightLabel = "";
        if (request.responseReceivedTime !== -1 && request.endTime !== -1)
            rightLabel = Number.secondsToString(request.endTime - request.responseReceivedTime);

        var hasLatency = request.latency > 0;
        if (hasLatency)
            var leftLabel = Number.secondsToString(request.latency);
        else
            var leftLabel = rightLabel;

        if (request.timing)
            return {left: leftLabel, right: rightLabel};

        if (hasLatency && rightLabel) {
            var total = Number.secondsToString(request.duration);
            var tooltip = WebInspector.NetworkTimeCalculator._latencyDownloadTotalFormat.format(leftLabel, rightLabel, total);
        } else if (hasLatency)
            var tooltip = WebInspector.NetworkTimeCalculator._latencyFormat.format(leftLabel);
        else if (rightLabel)
            var tooltip = WebInspector.NetworkTimeCalculator._downloadFormat.format(rightLabel);

        if (request.fetchedViaServiceWorker)
            tooltip = WebInspector.NetworkTimeCalculator._fromServiceWorkerFormat.format(tooltip);
        else if (request.cached())
            tooltip = WebInspector.NetworkTimeCalculator._fromCacheFormat.format(tooltip);
        return {left: leftLabel, right: rightLabel, tooltip: tooltip};
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     * @return {boolean}
     */
    updateBoundaries: function(request)
    {
        var didChange = false;

        var lowerBound;
        if (this.startAtZero)
            lowerBound = 0;
        else
            lowerBound = this._lowerBound(request);

        if (lowerBound !== -1 && (typeof this._minimumBoundary === "undefined" || lowerBound < this._minimumBoundary)) {
            this._minimumBoundary = lowerBound;
            didChange = true;
        }

        var upperBound = this._upperBound(request);
        if (upperBound !== -1 && (typeof this._maximumBoundary === "undefined" || upperBound > this._maximumBoundary)) {
            this._maximumBoundary = upperBound;
            didChange = true;
        }

        return didChange;
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     * @return {number}
     */
    _lowerBound: function(request)
    {
        return 0;
    },

    /**
     * @param {!WebInspector.NetworkRequest} request
     * @return {number}
     */
    _upperBound: function(request)
    {
        return 0;
    }
}

/**
 * @constructor
 * @extends {WebInspector.NetworkTimeCalculator}
 */
WebInspector.NetworkTransferTimeCalculator = function()
{
    WebInspector.NetworkTimeCalculator.call(this, false);
}

WebInspector.NetworkTransferTimeCalculator.prototype = {
    /**
     * @override
     * @param {number} value
     * @param {number=} precision
     * @return {string}
     */
    formatTime: function(value, precision)
    {
        return Number.secondsToString(value - this.zeroTime());
    },

    /**
     * @override
     * @param {!WebInspector.NetworkRequest} request
     * @return {number}
     */
    _lowerBound: function(request)
    {
        return request.startTime;
    },

    /**
     * @override
     * @param {!WebInspector.NetworkRequest} request
     * @return {number}
     */
    _upperBound: function(request)
    {
        return request.endTime;
    },

    __proto__: WebInspector.NetworkTimeCalculator.prototype
}

/**
 * @constructor
 * @extends {WebInspector.NetworkTimeCalculator}
 */
WebInspector.NetworkTransferDurationCalculator = function()
{
    WebInspector.NetworkTimeCalculator.call(this, true);
}

WebInspector.NetworkTransferDurationCalculator.prototype = {
    /**
     * @override
     * @param {number} value
     * @param {number=} precision
     * @return {string}
     */
    formatTime: function(value, precision)
    {
        return Number.secondsToString(value);
    },

    /**
     * @override
     * @param {!WebInspector.NetworkRequest} request
     * @return {number}
     */
    _upperBound: function(request)
    {
        return request.duration;
    },

    __proto__: WebInspector.NetworkTimeCalculator.prototype
}

/**
 * @constructor
 * @extends {WebInspector.SortableDataGridNode}
 * @param {!WebInspector.NetworkLogView} parentView
 * @param {!WebInspector.NetworkRequest} request
 */
WebInspector.NetworkDataGridNode = function(parentView, request)
{
    WebInspector.SortableDataGridNode.call(this, {});
    this._parentView = parentView;
    this._request = request;
    this._linkifier = new WebInspector.Linkifier();
    this._isFilteredOut = true;
    this._isMatchingSearchQuery = false;
    this._staleGraph = true;
}

WebInspector.NetworkDataGridNode._hoveredRowSymbol = Symbol("hoveredRow");

WebInspector.NetworkDataGridNode.prototype = {
    /**
     * @return {!WebInspector.NetworkRequest}
     */
    request: function()
    {
        return this._request;
    },

    /**
     * @override
     * @return {number}
     */
    nodeSelfHeight: function()
    {
        return this._parentView.rowHeight();
    },

    /** override */
    createCells: function()
    {
        this._nameCell = null;
        this._timelineCell = null;
        this._initiatorCell = null;

        this._element.classList.toggle("network-error-row", this._isFailed());
        WebInspector.SortableDataGridNode.prototype.createCells.call(this);

        this._updateGraph();
    },

    /**
     * @override
     * @param {string} columnIdentifier
     * @return {!Element}
     */
    createCell: function(columnIdentifier)
    {
        var cell = this.createTD(columnIdentifier);
        switch (columnIdentifier) {
        case "name": this._renderNameCell(cell); break;
        case "timeline": this._createTimelineBar(cell); break;
        case "method": cell.setTextAndTitle(this._request.requestMethod); break;
        case "status": this._renderStatusCell(cell); break;
        case "scheme": cell.setTextAndTitle(this._request.scheme); break;
        case "domain": cell.setTextAndTitle(this._request.domain); break;
        case "remoteAddress": cell.setTextAndTitle(this._request.remoteAddress()); break;
        case "cookies": cell.setTextAndTitle(this._arrayLength(this._request.requestCookies)); break;
        case "setCookies": cell.setTextAndTitle(this._arrayLength(this._request.responseCookies)); break;
        case "connectionId": cell.setTextAndTitle(this._request.connectionId); break;
        case "type": cell.setTextAndTitle(this._request.mimeType || this._request.requestContentType() || ""); break;
        case "initiator": this._renderInitiatorCell(cell); break;
        case "size": this._renderSizeCell(cell); break;
        case "time": this._renderTimeCell(cell); break;
        default: cell.setTextAndTitle(this._request.responseHeaderValue(columnIdentifier) || ""); break;
        }

        return cell;
    },

    /**
     * @param {?Array} array
     * @return {string}
     */
    _arrayLength: function(array)
    {
        return array ? "" + array.length : "";
    },

    /**
     * @override
     * @protected
     */
    willAttach: function()
    {
        if (this._staleGraph)
            this._updateGraph();
        if (this._initiatorCell && this._request.initiatorInfo().type === WebInspector.NetworkRequest.InitiatorType.Script)
            this._initiatorCell.insertBefore(this._linkifiedInitiatorAnchor, this._initiatorCell.firstChild);
    },

    wasDetached: function()
    {
        if (this._linkifiedInitiatorAnchor)
            this._linkifiedInitiatorAnchor.remove();
    },

    dispose: function()
    {
        this._linkifier.reset();
    },

    _onClick: function()
    {
        if (!this._parentView.allowRequestSelection())
            this.select();
    },

    select: function()
    {
        this._parentView.dispatchEventToListeners(WebInspector.NetworkLogView.EventTypes.RequestSelected, this._request);
        WebInspector.SortableDataGridNode.prototype.select.apply(this, arguments);

        WebInspector.notifications.dispatchEventToListeners(WebInspector.UserMetrics.UserAction, {
            action: WebInspector.UserMetrics.UserActionNames.NetworkRequestSelected,
            url: this._request.url
        });
    },

    /**
     * @param {!RegExp=} regexp
     * @return {!Array.<!Object>}
     */
    highlightMatchedSubstring: function(regexp)
    {
        // Ensure element is created.
        this.element();
        var domChanges = [];
        var matchInfo = this._nameCell.textContent.match(regexp);
        if (matchInfo)
            WebInspector.highlightSearchResult(this._nameCell, matchInfo.index, matchInfo[0].length, domChanges);
        return domChanges;
    },

    _openInNewTab: function()
    {
        InspectorFrontendHost.openInNewTab(this._request.url);
    },

    get selectable()
    {
        return this._parentView.allowRequestSelection();
    },

    /**
     * @param {!Element} cell
     */
    _createTimelineBar: function(cell)
    {
        cell = cell.createChild("div");
        this._timelineCell = cell;

        cell.className = "network-graph-side";

        this._barAreaElement = cell.createChild("div", "network-graph-bar-area");
        this._barAreaElement.request = this._request;

        var type = this._request.resourceType().name();
        var cached = this._request.cached();

        this._barLeftElement = this._barAreaElement.createChild("div", "network-graph-bar");
        this._barLeftElement.classList.add(type, "waiting");
        this._barLeftElement.classList.toggle("cached", cached);

        this._barRightElement = this._barAreaElement.createChild("div", "network-graph-bar");
        this._barRightElement.classList.add(type);
        this._barRightElement.classList.toggle("cached", cached);

        this._labelLeftElement = this._barAreaElement.createChild("div", "network-graph-label");
        this._labelLeftElement.classList.add("waiting");

        this._labelRightElement = this._barAreaElement.createChild("div", "network-graph-label");

        cell.addEventListener("mouseover", this._onMouseOver.bind(this), false);
    },

    /**
     * @param {!Event} event
     */
    _onMouseOver: function(event)
    {
        this._refreshLabelPositions();
        this._parentView[WebInspector.NetworkDataGridNode._hoveredRowSymbol] = this;
    },

    /**
     * @return {boolean}
     */
    _isFailed: function()
    {
        return (this._request.failed && !this._request.statusCode) || (this._request.statusCode >= 400);
    },

    /**
     * @param {!Element} cell
     */
    _renderNameCell: function(cell)
    {
        this._nameCell = cell;
        cell.addEventListener("click", this._onClick.bind(this), false);
        cell.addEventListener("dblclick", this._openInNewTab.bind(this), false);
        var iconElement;
        if (this._request.resourceType() === WebInspector.resourceTypes.Image) {
            var previewImage = createElementWithClass("img", "image-network-icon-preview");
            this._request.populateImageSource(previewImage);

            iconElement = createElementWithClass("div", "icon");
            iconElement.appendChild(previewImage);
        } else {
            iconElement = createElementWithClass("img", "icon");
        }
        iconElement.classList.add(this._request.resourceType().name());

        cell.appendChild(iconElement);
        cell.createTextChild(this._request.name());
        this._appendSubtitle(cell, this._request.path());
        cell.title = this._request.url;
    },

    /**
     * @param {!Element} cell
     */
    _renderStatusCell: function(cell)
    {
        cell.classList.toggle("network-dim-cell", !this._isFailed() && (this._request.cached() || !this._request.statusCode));

        if (this._request.failed && !this._request.canceled) {
            var failText = WebInspector.UIString("(failed)");
            if (this._request.localizedFailDescription) {
                cell.createTextChild(failText);
                this._appendSubtitle(cell, this._request.localizedFailDescription);
                cell.title = failText + " " + this._request.localizedFailDescription;
            } else
                cell.setTextAndTitle(failText);
        } else if (this._request.statusCode) {
            cell.createTextChild("" + this._request.statusCode);
            this._appendSubtitle(cell, this._request.statusText);
            cell.title = this._request.statusCode + " " + this._request.statusText;
        } else if (this._request.parsedURL.isDataURL()) {
            cell.setTextAndTitle(WebInspector.UIString("(data)"));
        } else if (this._request.canceled) {
            cell.setTextAndTitle(WebInspector.UIString("(canceled)"));
        } else if (this._request.finished) {
            cell.setTextAndTitle(WebInspector.UIString("Finished"));
        } else {
            cell.setTextAndTitle(WebInspector.UIString("(pending)"));
        }
    },

    /**
     * @param {!Element} cell
     */
    _renderInitiatorCell: function(cell)
    {
        this._initiatorCell = cell;
        var request = this._request;
        var initiator = request.initiatorInfo();

        switch (initiator.type) {
        case WebInspector.NetworkRequest.InitiatorType.Parser:
            cell.title = initiator.url + ":" + initiator.lineNumber;
            cell.appendChild(WebInspector.linkifyResourceAsNode(initiator.url, initiator.lineNumber - 1));
            this._appendSubtitle(cell, WebInspector.UIString("Parser"));
            break;

        case WebInspector.NetworkRequest.InitiatorType.Redirect:
            cell.title = initiator.url;
            console.assert(request.redirectSource);
            var redirectSource = /** @type {!WebInspector.NetworkRequest} */ (request.redirectSource);
            cell.appendChild(WebInspector.linkifyRequestAsNode(redirectSource));
            this._appendSubtitle(cell, WebInspector.UIString("Redirect"));
            break;

        case WebInspector.NetworkRequest.InitiatorType.Script:
            if (!this._linkifiedInitiatorAnchor) {
                this._linkifiedInitiatorAnchor = this._linkifier.linkifyScriptLocation(request.target(), null, initiator.url, initiator.lineNumber - 1, initiator.columnNumber - 1);
                this._linkifiedInitiatorAnchor.title = "";
            }
            cell.appendChild(this._linkifiedInitiatorAnchor);
            this._appendSubtitle(cell, WebInspector.UIString("Script"));
            cell.classList.add("network-script-initiated");
            cell.request = request;
            break;

        default:
            cell.title = "";
            cell.classList.add("network-dim-cell");
            cell.setTextAndTitle(WebInspector.UIString("Other"));
        }
    },

    /**
     * @param {!Element} cell
     */
    _renderSizeCell: function(cell)
    {
        if (this._request.fetchedViaServiceWorker) {
            cell.setTextAndTitle(WebInspector.UIString("(from ServiceWorker)"));
            cell.classList.add("network-dim-cell");
        } else if (this._request.cached()) {
            cell.setTextAndTitle(WebInspector.UIString("(from cache)"));
            cell.classList.add("network-dim-cell");
        } else {
            var resourceSize = Number.bytesToString(this._request.resourceSize);
            var transferSize = Number.bytesToString(this._request.transferSize);
            cell.setTextAndTitle(transferSize);
            this._appendSubtitle(cell, resourceSize);
        }
    },

    /**
     * @param {!Element} cell
     */
    _renderTimeCell: function(cell)
    {
        if (this._request.duration > 0) {
            cell.setTextAndTitle(Number.secondsToString(this._request.duration));
            this._appendSubtitle(cell, Number.secondsToString(this._request.latency));
        } else {
            cell.classList.add("network-dim-cell");
            cell.setTextAndTitle(WebInspector.UIString("Pending"));
        }
    },

    /**
     * @param {!Element} cellElement
     * @param {string} subtitleText
     */
    _appendSubtitle: function(cellElement, subtitleText)
    {
        var subtitleElement = createElement("div");
        subtitleElement.className = "network-cell-subtitle";
        subtitleElement.textContent = subtitleText;
        cellElement.appendChild(subtitleElement);
    },

    refreshGraph: function()
    {
        if (!this._timelineCell)
            return;
        this._staleGraph = true;
        if (this.attached())
            this.dataGrid.scheduleUpdate();
    },

    _updateGraph: function()
    {
        this._staleGraph = false;
        if (!this._timelineCell)
            return;

        var calculator = this._parentView.calculator();
        var percentages = calculator.computeBarGraphPercentages(this._request);
        this._percentages = percentages;

        this._barAreaElement.classList.remove("hidden");

        this._barLeftElement.style.setProperty("left", percentages.start + "%");
        this._barLeftElement.style.setProperty("right", (100 - percentages.middle) + "%");

        this._barRightElement.style.setProperty("left", percentages.middle + "%");
        this._barRightElement.style.setProperty("right", (100 - percentages.end) + "%");

        var labels = calculator.computeBarGraphLabels(this._request);
        this._labelLeftElement.textContent = labels.left;
        this._labelRightElement.textContent = labels.right;

        var tooltip = (labels.tooltip || "");
        this._barLeftElement.title = tooltip;
        this._labelLeftElement.title = tooltip;
        this._labelRightElement.title = tooltip;
        this._barRightElement.title = tooltip;

        if (this._parentView[WebInspector.NetworkDataGridNode._hoveredRowSymbol] === this)
            this._refreshLabelPositions();
    },

    _refreshLabelPositions: function()
    {
        if (!this._percentages)
            return;
        this._labelLeftElement.style.removeProperty("left");
        this._labelLeftElement.style.removeProperty("right");
        this._labelLeftElement.classList.remove("before");
        this._labelLeftElement.classList.remove("hidden");

        this._labelRightElement.style.removeProperty("left");
        this._labelRightElement.style.removeProperty("right");
        this._labelRightElement.classList.remove("after");
        this._labelRightElement.classList.remove("hidden");

        const labelPadding = 10;
        const barRightElementOffsetWidth = this._barRightElement.offsetWidth;
        const barLeftElementOffsetWidth = this._barLeftElement.offsetWidth;

        if (this._barLeftElement) {
            var leftBarWidth = barLeftElementOffsetWidth - labelPadding;
            var rightBarWidth = (barRightElementOffsetWidth - barLeftElementOffsetWidth) - labelPadding;
        } else {
            var leftBarWidth = (barLeftElementOffsetWidth - barRightElementOffsetWidth) - labelPadding;
            var rightBarWidth = barRightElementOffsetWidth - labelPadding;
        }

        const labelLeftElementOffsetWidth = this._labelLeftElement.offsetWidth;
        const labelRightElementOffsetWidth = this._labelRightElement.offsetWidth;

        const labelBefore = (labelLeftElementOffsetWidth > leftBarWidth);
        const labelAfter = (labelRightElementOffsetWidth > rightBarWidth);
        const graphElementOffsetWidth = this._timelineCell.offsetWidth;

        if (labelBefore && (graphElementOffsetWidth * (this._percentages.start / 100)) < (labelLeftElementOffsetWidth + 10))
            var leftHidden = true;

        if (labelAfter && (graphElementOffsetWidth * ((100 - this._percentages.end) / 100)) < (labelRightElementOffsetWidth + 10))
            var rightHidden = true;

        if (barLeftElementOffsetWidth == barRightElementOffsetWidth) {
            // The left/right label data are the same, so a before/after label can be replaced by an on-bar label.
            if (labelBefore && !labelAfter)
                leftHidden = true;
            else if (labelAfter && !labelBefore)
                rightHidden = true;
        }

        if (labelBefore) {
            if (leftHidden)
                this._labelLeftElement.classList.add("hidden");
            this._labelLeftElement.style.setProperty("right", (100 - this._percentages.start) + "%");
            this._labelLeftElement.classList.add("before");
        } else {
            this._labelLeftElement.style.setProperty("left", this._percentages.start + "%");
            this._labelLeftElement.style.setProperty("right", (100 - this._percentages.middle) + "%");
        }

        if (labelAfter) {
            if (rightHidden)
                this._labelRightElement.classList.add("hidden");
            this._labelRightElement.style.setProperty("left", this._percentages.end + "%");
            this._labelRightElement.classList.add("after");
        } else {
            this._labelRightElement.style.setProperty("left", this._percentages.middle + "%");
            this._labelRightElement.style.setProperty("right", (100 - this._percentages.end) + "%");
        }
    },

    __proto__: WebInspector.SortableDataGridNode.prototype
}

/**
 * @param {!WebInspector.NetworkDataGridNode} a
 * @param {!WebInspector.NetworkDataGridNode} b
 * @return {number}
 */
WebInspector.NetworkDataGridNode.NameComparator = function(a, b)
{
    var aFileName = a._request.name();
    var bFileName = b._request.name();
    if (aFileName > bFileName)
        return 1;
    if (bFileName > aFileName)
        return -1;
    return a._request.indentityCompare(b._request);
}

/**
 * @param {!WebInspector.NetworkDataGridNode} a
 * @param {!WebInspector.NetworkDataGridNode} b
 * @return {number}
 */
WebInspector.NetworkDataGridNode.RemoteAddressComparator = function(a, b)
{
    var aRemoteAddress = a._request.remoteAddress();
    var bRemoteAddress = b._request.remoteAddress();
    if (aRemoteAddress > bRemoteAddress)
        return 1;
    if (bRemoteAddress > aRemoteAddress)
        return -1;
    return a._request.indentityCompare(b._request);
}

/**
 * @param {!WebInspector.NetworkDataGridNode} a
 * @param {!WebInspector.NetworkDataGridNode} b
 * @return {number}
 */
WebInspector.NetworkDataGridNode.SizeComparator = function(a, b)
{
    if (b._request.cached() && !a._request.cached())
        return 1;
    if (a._request.cached() && !b._request.cached())
        return -1;
    return (a._request.transferSize - b._request.transferSize) || a._request.indentityCompare(b._request);
}

/**
 * @param {!WebInspector.NetworkDataGridNode} a
 * @param {!WebInspector.NetworkDataGridNode} b
 * @return {number}
 */
WebInspector.NetworkDataGridNode.InitiatorComparator = function(a, b)
{
    var aInitiator = a._request.initiatorInfo();
    var bInitiator = b._request.initiatorInfo();

    if (aInitiator.type < bInitiator.type)
        return -1;
    if (aInitiator.type > bInitiator.type)
        return 1;

    if (typeof aInitiator.__source === "undefined")
        aInitiator.__source = WebInspector.displayNameForURL(aInitiator.url);
    if (typeof bInitiator.__source === "undefined")
        bInitiator.__source = WebInspector.displayNameForURL(bInitiator.url);

    if (aInitiator.__source < bInitiator.__source)
        return -1;
    if (aInitiator.__source > bInitiator.__source)
        return 1;

    if (aInitiator.lineNumber < bInitiator.lineNumber)
        return -1;
    if (aInitiator.lineNumber > bInitiator.lineNumber)
        return 1;

    if (aInitiator.columnNumber < bInitiator.columnNumber)
        return -1;
    if (aInitiator.columnNumber > bInitiator.columnNumber)
        return 1;

    return a._request.indentityCompare(b._request);
}

/**
 * @param {!WebInspector.NetworkDataGridNode} a
 * @param {!WebInspector.NetworkDataGridNode} b
 * @return {number}
 */
WebInspector.NetworkDataGridNode.RequestCookiesCountComparator = function(a, b)
{
    var aScore = a._request.requestCookies ? a._request.requestCookies.length : 0;
    var bScore = b._request.requestCookies ? b._request.requestCookies.length : 0;
    return (aScore - bScore) || a._request.indentityCompare(b._request);
}

/**
 * @param {!WebInspector.NetworkDataGridNode} a
 * @param {!WebInspector.NetworkDataGridNode} b
 * @return {number}
 */
WebInspector.NetworkDataGridNode.ResponseCookiesCountComparator = function(a, b)
{
    var aScore = a._request.responseCookies ? a._request.responseCookies.length : 0;
    var bScore = b._request.responseCookies ? b._request.responseCookies.length : 0;
    return (aScore - bScore) || a._request.indentityCompare(b._request);
}

/**
 * @param {string} propertyName
 * @param {boolean} revert
 * @param {!WebInspector.NetworkDataGridNode} a
 * @param {!WebInspector.NetworkDataGridNode} b
 * @return {number}
 */
WebInspector.NetworkDataGridNode.RequestPropertyComparator = function(propertyName, revert, a, b)
{
    var aValue = a._request[propertyName];
    var bValue = b._request[propertyName];
    if (aValue > bValue)
        return revert ? -1 : 1;
    if (bValue > aValue)
        return revert ? 1 : -1;
    return a._request.indentityCompare(b._request);
}

WebInspector.NetworkPanel.show = function()
{
    WebInspector.inspectorView.setCurrentPanel(WebInspector.NetworkPanel._instance());
}

/**
 * @return {!WebInspector.NetworkPanel}
 */
WebInspector.NetworkPanel._instance = function()
{
    if (!WebInspector.NetworkPanel._instanceObject)
        WebInspector.NetworkPanel._instanceObject = new WebInspector.NetworkPanel();
    return WebInspector.NetworkPanel._instanceObject;
}

/**
 * @constructor
 * @implements {WebInspector.PanelFactory}
 */
WebInspector.NetworkPanelFactory = function()
{
}

WebInspector.NetworkPanelFactory.prototype = {
    /**
     * @return {!WebInspector.Panel}
     */
    createPanel: function()
    {
        return WebInspector.NetworkPanel._instance();
    }
}

/**
 * @constructor
 */
WebInspector.HARWriter = function()
{
}

WebInspector.HARWriter.prototype = {
    /**
     * @param {!WebInspector.OutputStream} stream
     * @param {!Array.<!WebInspector.NetworkRequest>} requests
     * @param {!WebInspector.Progress} progress
     */
    write: function(stream, requests, progress)
    {
        this._stream = stream;
        this._harLog = (new WebInspector.HARLog(requests)).build();
        this._pendingRequests = 1; // Guard against completing resource transfer before all requests are made.
        var entries = this._harLog.entries;
        for (var i = 0; i < entries.length; ++i) {
            var content = requests[i].content;
            if (typeof content === "undefined" && requests[i].finished) {
                ++this._pendingRequests;
                requests[i].requestContent(this._onContentAvailable.bind(this, entries[i], requests[i]));
            } else if (content !== null)
                this._setEntryContent(entries[i], requests[i]);
        }
        var compositeProgress = new WebInspector.CompositeProgress(progress);
        this._writeProgress = compositeProgress.createSubProgress();
        if (--this._pendingRequests) {
            this._requestsProgress = compositeProgress.createSubProgress();
            this._requestsProgress.setTitle(WebInspector.UIString("Collecting content…"));
            this._requestsProgress.setTotalWork(this._pendingRequests);
        } else
            this._beginWrite();
    },

    /**
     * @param {!Object} entry
     * @param {!WebInspector.NetworkRequest} request
     */
    _setEntryContent: function(entry, request)
    {
        if (request.content !== null)
            entry.response.content.text = request.content;
        if (request.contentEncoded)
            entry.response.content.encoding = "base64";
    },

    /**
     * @param {!Object} entry
     * @param {!WebInspector.NetworkRequest} request
     * @param {?string} content
     */
    _onContentAvailable: function(entry, request, content)
    {
        this._setEntryContent(entry, request);
        if (this._requestsProgress)
            this._requestsProgress.worked();
        if (!--this._pendingRequests) {
            this._requestsProgress.done();
            this._beginWrite();
        }
    },

    _beginWrite: function()
    {
        const jsonIndent = 2;
        this._text = JSON.stringify({log: this._harLog}, null, jsonIndent);
        this._writeProgress.setTitle(WebInspector.UIString("Writing file…"));
        this._writeProgress.setTotalWork(this._text.length);
        this._bytesWritten = 0;
        this._writeNextChunk(this._stream);
    },

    /**
     * @param {!WebInspector.OutputStream} stream
     * @param {string=} error
     */
    _writeNextChunk: function(stream, error)
    {
        if (this._bytesWritten >= this._text.length || error) {
            stream.close();
            this._writeProgress.done();
            return;
        }
        const chunkSize = 100000;
        var text = this._text.substring(this._bytesWritten, this._bytesWritten + chunkSize);
        this._bytesWritten += text.length;
        stream.write(text, this._writeNextChunk.bind(this));
        this._writeProgress.setWorked(this._bytesWritten);
    }
}
