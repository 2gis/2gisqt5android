// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @extends {WebInspector.VBox}
 */
WebInspector.AdvancedSearchView = function()
{
    WebInspector.VBox.call(this);

    this._searchId = 0;

    this.element.classList.add("search-view");

    this._searchPanelElement = this.element.createChild("div", "search-drawer-header");
    this._searchPanelElement.addEventListener("keydown", this._onKeyDown.bind(this), false);

    this._searchResultsElement = this.element.createChild("div");
    this._searchResultsElement.className = "search-results";

    this._search = this._searchPanelElement.createChild("input");
    this._search.placeholder = WebInspector.UIString("Search sources");
    this._search.setAttribute("type", "text");
    this._search.classList.add("search-config-search");
    this._search.setAttribute("results", "0");
    this._search.setAttribute("size", 30);

    this._ignoreCaseLabel = this._searchPanelElement.createChild("label");
    this._ignoreCaseLabel.classList.add("search-config-label");
    this._ignoreCaseCheckbox = this._ignoreCaseLabel.createChild("input");
    this._ignoreCaseCheckbox.setAttribute("type", "checkbox");
    this._ignoreCaseCheckbox.classList.add("search-config-checkbox");
    this._ignoreCaseLabel.appendChild(document.createTextNode(WebInspector.UIString("Ignore case")));

    this._regexLabel = this._searchPanelElement.createChild("label");
    this._regexLabel.classList.add("search-config-label");
    this._regexCheckbox = this._regexLabel.createChild("input");
    this._regexCheckbox.setAttribute("type", "checkbox");
    this._regexCheckbox.classList.add("search-config-checkbox");
    this._regexLabel.appendChild(document.createTextNode(WebInspector.UIString("Regular expression")));

    this._searchStatusBarElement = this.element.createChild("div", "search-status-bar-summary");
    this._searchMessageElement = this._searchStatusBarElement.createChild("span");
    this._searchResultsMessageElement = document.createElement("span");

    WebInspector.settings.advancedSearchConfig = WebInspector.settings.createSetting("advancedSearchConfig", new WebInspector.SearchConfig("", true, false).toPlainObject());
    this._load();
}

WebInspector.AdvancedSearchView.prototype = {
    /**
     * @return {!WebInspector.SearchConfig}
     */
    _buildSearchConfig: function()
    {
        return new WebInspector.SearchConfig(this._search.value, this._ignoreCaseCheckbox.checked, this._regexCheckbox.checked);
    },

    toggle: function()
    {
        var selection = window.getSelection();
        var queryCandidate;
        if (selection.rangeCount)
            queryCandidate = selection.toString().replace(/\r?\n.*/, "");

        if (!this.isShowing())
            WebInspector.inspectorView.showViewInDrawer("search");
        if (queryCandidate)
            this._search.value = queryCandidate;
        this.focus();

        this._startIndexing();
    },

    /**
     * @param {boolean} finished
     */
    _onIndexingFinished: function(finished)
    {
        delete this._isIndexing;
        this._indexingFinished(finished);
        if (!finished)
            delete this._pendingSearchConfig;
        if (!this._pendingSearchConfig)
            return;
        var searchConfig = this._pendingSearchConfig
        delete this._pendingSearchConfig;
        this._innerStartSearch(searchConfig);
    },

    _startIndexing: function()
    {
        this._isIndexing = true;
        // FIXME: this._currentSearchScope should be initialized based on searchConfig
        this._currentSearchScope = this._searchScopes()[0];
        if (this._progressIndicator)
            this._progressIndicator.done();
        this._progressIndicator = new WebInspector.ProgressIndicator();
        this._indexingStarted(this._progressIndicator);
        this._currentSearchScope.performIndexing(this._progressIndicator, this._onIndexingFinished.bind(this));
    },

    /**
     * @param {number} searchId
     * @param {!WebInspector.FileBasedSearchResult} searchResult
     */
    _onSearchResult: function(searchId, searchResult)
    {
        if (searchId !== this._searchId)
            return;
        this._addSearchResult(searchResult);
        if (!searchResult.searchMatches.length)
            return;
        if (!this._searchResultsPane)
            this._searchResultsPane = this._currentSearchScope.createSearchResultsPane(this._searchConfig);
        this._resetResults();
        this._searchResultsElement.appendChild(this._searchResultsPane.element);
        this._searchResultsPane.addSearchResult(searchResult);
    },

    /**
     * @param {number} searchId
     * @param {boolean} finished
     */
    _onSearchFinished: function(searchId, finished)
    {
        if (searchId !== this._searchId)
            return;
        if (!this._searchResultsPane)
            this._nothingFound();
        this._searchFinished(finished);
        delete this._searchConfig;
    },

    /**
     * @param {!WebInspector.SearchConfig} searchConfig
     */
    _startSearch: function(searchConfig)
    {
        this._resetSearch();
        ++this._searchId;
        if (!this._isIndexing)
            this._startIndexing();
        this._pendingSearchConfig = searchConfig;
    },

    /**
     * @param {!WebInspector.SearchConfig} searchConfig
     */
    _innerStartSearch: function(searchConfig)
    {
        this._searchConfig = searchConfig;
        // FIXME: this._currentSearchScope should be initialized based on searchConfig
        this._currentSearchScope = this._searchScopes()[0];

        if (this._progressIndicator)
            this._progressIndicator.done();
        this._progressIndicator = new WebInspector.ProgressIndicator();
        this._searchStarted(this._progressIndicator);
        this._currentSearchScope.performSearch(searchConfig, this._progressIndicator, this._onSearchResult.bind(this, this._searchId), this._onSearchFinished.bind(this, this._searchId));
    },

    _resetSearch: function()
    {
        this._stopSearch();

        if (this._searchResultsPane) {
            this._resetResults();
            delete this._searchResultsPane;
        }
    },

    _stopSearch: function()
    {
        if (this._progressIndicator)
            this._progressIndicator.cancel();
        if (this._currentSearchScope)
            this._currentSearchScope.stopSearch();
        delete this._searchConfig;
    },

    /**
     * @return {!Array.<!WebInspector.SearchScope>}
     */
    _searchScopes: function()
    {
        // FIXME: implement multiple search scopes.
        return /** @type {!Array.<!WebInspector.SearchScope>} */ (WebInspector.moduleManager.instances(WebInspector.SearchScope));
    },

    /**
     * @param {!WebInspector.ProgressIndicator} progressIndicator
     */
    _searchStarted: function(progressIndicator)
    {
        this._resetResults();
        this._resetCounters();

        this._searchMessageElement.textContent = WebInspector.UIString("Searching...");
        progressIndicator.show(this._searchStatusBarElement);
        this._updateSearchResultsMessage();

        if (!this._searchingView)
            this._searchingView = new WebInspector.EmptyView(WebInspector.UIString("Searching..."));
        this._searchingView.show(this._searchResultsElement);
    },

    /**
     * @param {!WebInspector.ProgressIndicator} progressIndicator
     */
    _indexingStarted: function(progressIndicator)
    {
        this._searchMessageElement.textContent = WebInspector.UIString("Indexing...");
        progressIndicator.show(this._searchStatusBarElement);
    },

    /**
     * @param {boolean} finished
     */
    _indexingFinished: function(finished)
    {
        this._searchMessageElement.textContent = finished ? "" : WebInspector.UIString("Indexing interrupted.");
    },

    _updateSearchResultsMessage: function()
    {
        if (this._searchMatchesCount && this._searchResultsCount)
            this._searchResultsMessageElement.textContent = WebInspector.UIString("Found %d matches in %d files.", this._searchMatchesCount, this._nonEmptySearchResultsCount);
        else
            this._searchResultsMessageElement.textContent = "";
    },

    _resetResults: function()
    {
        if (this._searchingView)
            this._searchingView.detach();
        if (this._notFoundView)
            this._notFoundView.detach();
        this._searchResultsElement.removeChildren();
    },

    _resetCounters: function()
    {
        this._searchMatchesCount = 0;
        this._searchResultsCount = 0;
        this._nonEmptySearchResultsCount = 0;
    },

    _nothingFound: function()
    {
        this._resetResults();

        if (!this._notFoundView)
            this._notFoundView = new WebInspector.EmptyView(WebInspector.UIString("No matches found."));
        this._notFoundView.show(this._searchResultsElement);
        this._searchResultsMessageElement.textContent = WebInspector.UIString("No matches found.");
    },

    /**
     * @param {!WebInspector.FileBasedSearchResult} searchResult
     */
    _addSearchResult: function(searchResult)
    {
        this._searchMatchesCount += searchResult.searchMatches.length;
        this._searchResultsCount++;
        if (searchResult.searchMatches.length)
            this._nonEmptySearchResultsCount++;
        this._updateSearchResultsMessage();
    },

    /**
     * @param {boolean} finished
     */
    _searchFinished: function(finished)
    {
        this._searchMessageElement.textContent = finished ? WebInspector.UIString("Search finished.") : WebInspector.UIString("Search interrupted.");
    },

    focus: function()
    {
        WebInspector.setCurrentFocusElement(this._search);
        this._search.select();
    },

    willHide: function()
    {
        this._stopSearch();
    },

    /**
     * @param {?Event} event
     */
    _onKeyDown: function(event)
    {
        switch (event.keyCode) {
        case WebInspector.KeyboardShortcut.Keys.Enter.code:
            this._onAction();
            break;
        }
    },

    _save: function()
    {
        WebInspector.settings.advancedSearchConfig.set(this._buildSearchConfig().toPlainObject());
    },

    _load: function()
    {
        var searchConfig = WebInspector.SearchConfig.fromPlainObject(WebInspector.settings.advancedSearchConfig.get());
        this._search.value = searchConfig.query();
        this._ignoreCaseCheckbox.checked = searchConfig.ignoreCase();
        this._regexCheckbox.checked = searchConfig.isRegex();
    },

    _onAction: function()
    {
        var searchConfig = this._buildSearchConfig();
        if (!searchConfig.query() || !searchConfig.query().length)
            return;

        this._save();
        this._startSearch(searchConfig);
    },

    __proto__: WebInspector.VBox.prototype
}

/**
 * @constructor
 * @param {!WebInspector.ProjectSearchConfig} searchConfig
 */
WebInspector.SearchResultsPane = function(searchConfig)
{
    this._searchConfig = searchConfig;
    this.element = document.createElement("div");
}

WebInspector.SearchResultsPane.prototype = {
    /**
     * @return {!WebInspector.ProjectSearchConfig}
     */
    get searchConfig()
    {
        return this._searchConfig;
    },

    /**
     * @param {!WebInspector.FileBasedSearchResult} searchResult
     */
    addSearchResult: function(searchResult) { }
}

/**
 * @constructor
 * @implements {WebInspector.ActionDelegate}
 */
WebInspector.AdvancedSearchView.ToggleDrawerViewActionDelegate = function()
{
}

WebInspector.AdvancedSearchView.ToggleDrawerViewActionDelegate.prototype = {
    /**
     * @return {boolean}
     */
    handleAction: function()
    {
        var searchView = this._searchView();
        if (!searchView)
            return false;
        if (!searchView.isShowing() || searchView._search !== document.activeElement) {
            WebInspector.inspectorView.showPanel("sources");
            searchView.toggle();
        } else {
            WebInspector.inspectorView.closeDrawer();
        }
        return true;
    },

    /**
     * @return {?WebInspector.AdvancedSearchView}
     */
    _searchView: function()
    {
        if (!this._view) {
            var extensions = WebInspector.moduleManager.extensions("drawer-view");
            for (var i = 0; i < extensions.length; ++i) {
                if (extensions[i].descriptor()["name"] === "search") {
                    this._view = extensions[i].instance();
                    break;
                }
            }
        }
        return this._view;
    }
}


/**
 * @constructor
 * @param {!WebInspector.UISourceCode} uiSourceCode
 * @param {!Array.<!Object>} searchMatches
 */
WebInspector.FileBasedSearchResult = function(uiSourceCode, searchMatches) {
    this.uiSourceCode = uiSourceCode;
    this.searchMatches = searchMatches;
}

/**
 * @interface
 */
WebInspector.SearchScope = function()
{
}

WebInspector.SearchScope.prototype = {
    /**
     * @param {!WebInspector.SearchConfig} searchConfig
     * @param {!WebInspector.Progress} progress
     * @param {function(!WebInspector.FileBasedSearchResult)} searchResultCallback
     * @param {function(boolean)} searchFinishedCallback
     */
    performSearch: function(searchConfig, progress, searchResultCallback, searchFinishedCallback) { },

    /**
     * @param {!WebInspector.Progress} progress
     * @param {function(boolean)} callback
     */
    performIndexing: function(progress, callback) { },

    stopSearch: function() { },

    /**
     * @param {!WebInspector.ProjectSearchConfig} searchConfig
     * @return {!WebInspector.SearchResultsPane}
     */
    createSearchResultsPane: function(searchConfig) { }
}

importScript("FileBasedSearchResultsPane.js");
importScript("SourcesSearchScope.js");
