// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @implements {WebInspector.TabbedEditorContainerDelegate}
 * @implements {WebInspector.Searchable}
 * @implements {WebInspector.Replaceable}
 * @extends {WebInspector.VBox}
 * @param {!WebInspector.Workspace} workspace
 * @param {!WebInspector.SourcesPanel} sourcesPanel
 */
WebInspector.SourcesView = function(workspace, sourcesPanel)
{
    WebInspector.VBox.call(this);
    this.registerRequiredCSS("sourcesView.css");
    this.element.id = "sources-panel-sources-view";
    this.setMinimumAndPreferredSizes(50, 25, 150, 100);

    this._workspace = workspace;
    this._sourcesPanel = sourcesPanel;

    this._searchableView = new WebInspector.SearchableView(this);
    this._searchableView.setMinimalSearchQuerySize(0);
    this._searchableView.show(this.element);

    /** @type {!Map.<!WebInspector.UISourceCode, !WebInspector.UISourceCodeFrame>} */
    this._sourceFramesByUISourceCode = new Map();

    var tabbedEditorPlaceholderText = WebInspector.isMac() ? WebInspector.UIString("Hit Cmd+P to open a file") : WebInspector.UIString("Hit Ctrl+P to open a file");
    this._editorContainer = new WebInspector.TabbedEditorContainer(this, "previouslyViewedFiles", tabbedEditorPlaceholderText);
    this._editorContainer.show(this._searchableView.element);
    this._editorContainer.addEventListener(WebInspector.TabbedEditorContainer.Events.EditorSelected, this._editorSelected, this);
    this._editorContainer.addEventListener(WebInspector.TabbedEditorContainer.Events.EditorClosed, this._editorClosed, this);

    this._historyManager = new WebInspector.EditingLocationHistoryManager(this, this.currentSourceFrame.bind(this));

    this._scriptViewStatusBarItemsContainer = document.createElement("div");
    this._scriptViewStatusBarItemsContainer.className = "inline-block";

    this._scriptViewStatusBarTextContainer = document.createElement("div");
    this._scriptViewStatusBarTextContainer.className = "hbox";

    this._statusBarContainerElement = this.element.createChild("div", "sources-status-bar");

    /**
     * @this {WebInspector.SourcesView}
     * @param {!WebInspector.SourcesView.EditorAction} EditorAction
     */
    function appendButtonForExtension(EditorAction)
    {
        this._statusBarContainerElement.appendChild(EditorAction.button(this));
    }
    var editorActions = /** @type {!Array.<!WebInspector.SourcesView.EditorAction>} */ (WebInspector.moduleManager.instances(WebInspector.SourcesView.EditorAction));
    editorActions.forEach(appendButtonForExtension.bind(this));

    this._statusBarContainerElement.appendChild(this._scriptViewStatusBarItemsContainer);
    this._statusBarContainerElement.appendChild(this._scriptViewStatusBarTextContainer);

    WebInspector.startBatchUpdate();
    this._workspace.uiSourceCodes().forEach(this._addUISourceCode.bind(this));
    WebInspector.endBatchUpdate();

    this._workspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeAdded, this._uiSourceCodeAdded, this);
    this._workspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeRemoved, this._uiSourceCodeRemoved, this);
    this._workspace.addEventListener(WebInspector.Workspace.Events.ProjectRemoved, this._projectRemoved.bind(this), this);

    function handleBeforeUnload(event)
    {
        if (event.returnValue)
            return;
        var unsavedSourceCodes = WebInspector.workspace.unsavedSourceCodes();
        if (!unsavedSourceCodes.length)
            return;

        event.returnValue = WebInspector.UIString("DevTools have unsaved changes that will be permanently lost.");
        WebInspector.inspectorView.showPanel("sources");
        for (var i = 0; i < unsavedSourceCodes.length; ++i)
            WebInspector.Revealer.reveal(unsavedSourceCodes[i]);
    }
    window.addEventListener("beforeunload", handleBeforeUnload, true);

    this._shortcuts = {};
    this.element.addEventListener("keydown", this._handleKeyDown.bind(this), false);
}

WebInspector.SourcesView.Events = {
    EditorClosed: "EditorClosed",
    EditorSelected: "EditorSelected",
}

WebInspector.SourcesView.prototype = {
    /**
     * @param {function(!Array.<!WebInspector.KeyboardShortcut.Descriptor>, function(?Event=):boolean)} registerShortcutDelegate
     */
    registerShortcuts: function(registerShortcutDelegate)
    {
        /**
         * @this {WebInspector.SourcesView}
         * @param {!Array.<!WebInspector.KeyboardShortcut.Descriptor>} shortcuts
         * @param {function(?Event=):boolean} handler
         */
        function registerShortcut(shortcuts, handler)
        {
            registerShortcutDelegate(shortcuts, handler);
            this._registerShortcuts(shortcuts, handler);
        }

        registerShortcut.call(this, WebInspector.ShortcutsScreen.SourcesPanelShortcuts.JumpToPreviousLocation, this._onJumpToPreviousLocation.bind(this));
        registerShortcut.call(this, WebInspector.ShortcutsScreen.SourcesPanelShortcuts.JumpToNextLocation, this._onJumpToNextLocation.bind(this));
        registerShortcut.call(this, WebInspector.ShortcutsScreen.SourcesPanelShortcuts.CloseEditorTab, this._onCloseEditorTab.bind(this));
        registerShortcut.call(this, WebInspector.ShortcutsScreen.SourcesPanelShortcuts.GoToLine, this._showGoToLineDialog.bind(this));
        registerShortcut.call(this, WebInspector.ShortcutsScreen.SourcesPanelShortcuts.GoToMember, this._showOutlineDialog.bind(this));
        registerShortcut.call(this, [WebInspector.KeyboardShortcut.makeDescriptor("o", WebInspector.KeyboardShortcut.Modifiers.CtrlOrMeta | WebInspector.KeyboardShortcut.Modifiers.Shift)], this._showOutlineDialog.bind(this));
        registerShortcut.call(this, WebInspector.ShortcutsScreen.SourcesPanelShortcuts.ToggleBreakpoint, this._toggleBreakpoint.bind(this));
        registerShortcut.call(this, WebInspector.ShortcutsScreen.SourcesPanelShortcuts.Save, this._save.bind(this));
        registerShortcut.call(this, WebInspector.ShortcutsScreen.SourcesPanelShortcuts.SaveAll, this._saveAll.bind(this));
    },

    /**
     * @param {!Array.<!WebInspector.KeyboardShortcut.Descriptor>} keys
     * @param {function(?Event=):boolean} handler
     */
    _registerShortcuts: function(keys, handler)
    {
        for (var i = 0; i < keys.length; ++i)
            this._shortcuts[keys[i].key] = handler;
    },

    _handleKeyDown: function(event)
    {
        var shortcutKey = WebInspector.KeyboardShortcut.makeKeyFromEvent(event);
        var handler = this._shortcuts[shortcutKey];
        if (handler && handler())
            event.consume(true);
    },

    /**
     * @return {!Element}
     */
    statusBarContainerElement: function()
    {
        return this._statusBarContainerElement;
    },

    /**
     * @return {!Element}
     */
    defaultFocusedElement: function()
    {
        return this._editorContainer.view.defaultFocusedElement();
    },

    /**
     * @return {!WebInspector.SearchableView}
     */
    searchableView: function()
    {
        return this._searchableView;
    },

    /**
     * @return {!WebInspector.View}
     */
    visibleView: function()
    {
        return this._editorContainer.visibleView;
    },

    /**
     * @return {?WebInspector.SourceFrame}
     */
    currentSourceFrame: function()
    {
        var view = this.visibleView();
        if (!(view instanceof WebInspector.SourceFrame))
            return null;
        return /** @type {!WebInspector.SourceFrame} */ (view);
    },

    /**
     * @return {?WebInspector.UISourceCode}
     */
    currentUISourceCode: function()
    {
        return this._currentUISourceCode;
    },

    /**
     * @param {?Event=} event
     */
    _onCloseEditorTab: function(event)
    {
        var uiSourceCode = this.currentUISourceCode();
        if (!uiSourceCode)
            return false;
        this._editorContainer.closeFile(uiSourceCode);
        return true;
    },

    /**
     * @param {?Event=} event
     */
    _onJumpToPreviousLocation: function(event)
    {
        this._historyManager.rollback();
        return true;
    },

    /**
     * @param {?Event=} event
     */
    _onJumpToNextLocation: function(event)
    {
        this._historyManager.rollover();
        return true;
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _uiSourceCodeAdded: function(event)
    {
        var uiSourceCode = /** @type {!WebInspector.UISourceCode} */ (event.data);
        this._addUISourceCode(uiSourceCode);
    },

    /**
     * @param {!WebInspector.UISourceCode} uiSourceCode
     */
    _addUISourceCode: function(uiSourceCode)
    {
        if (uiSourceCode.project().isServiceProject())
            return;
        this._editorContainer.addUISourceCode(uiSourceCode);
        // Replace debugger script-based uiSourceCode with a network-based one.
        var currentUISourceCode = this._currentUISourceCode;
        if (currentUISourceCode && currentUISourceCode.project().isServiceProject() && currentUISourceCode !== uiSourceCode && currentUISourceCode.url === uiSourceCode.url) {
            this._showFile(uiSourceCode);
            this._editorContainer.removeUISourceCode(currentUISourceCode);
        }
    },

    _uiSourceCodeRemoved: function(event)
    {
        var uiSourceCode = /** @type {!WebInspector.UISourceCode} */ (event.data);
        this._removeUISourceCodes([uiSourceCode]);
    },

    /**
     * @param {!Array.<!WebInspector.UISourceCode>} uiSourceCodes
     */
    _removeUISourceCodes: function(uiSourceCodes)
    {
        this._editorContainer.removeUISourceCodes(uiSourceCodes);
        for (var i = 0; i < uiSourceCodes.length; ++i) {
            this._removeSourceFrame(uiSourceCodes[i]);
            this._historyManager.removeHistoryForSourceCode(uiSourceCodes[i]);
        }
    },

    _projectRemoved: function(event)
    {
        var project = event.data;
        var uiSourceCodes = project.uiSourceCodes();
        this._removeUISourceCodes(uiSourceCodes);
        if (project.type() === WebInspector.projectTypes.Network)
            this._editorContainer.reset();
    },

    _updateScriptViewStatusBarItems: function()
    {
        this._scriptViewStatusBarItemsContainer.removeChildren();
        this._scriptViewStatusBarTextContainer.removeChildren();
        var sourceFrame = this.currentSourceFrame();
        if (!sourceFrame)
            return;

        var statusBarItems = sourceFrame.statusBarItems() || [];
        for (var i = 0; i < statusBarItems.length; ++i)
            this._scriptViewStatusBarItemsContainer.appendChild(statusBarItems[i]);
        var statusBarText = sourceFrame.statusBarText();
        if (statusBarText)
            this._scriptViewStatusBarTextContainer.appendChild(statusBarText);
    },

    /**
     * @param {!WebInspector.UISourceCode} uiSourceCode
     * @param {number=} lineNumber
     * @param {number=} columnNumber
     * @param {boolean=} omitFocus
     * @param {boolean=} omitHighlight
     */
    showSourceLocation: function(uiSourceCode, lineNumber, columnNumber, omitFocus, omitHighlight)
    {
        this._historyManager.updateCurrentState();
        var sourceFrame = this._showFile(uiSourceCode);
        if (typeof lineNumber === "number")
            sourceFrame.revealPosition(lineNumber, columnNumber, !omitHighlight);
        this._historyManager.pushNewState();
        if (!omitFocus)
            sourceFrame.focus();
        WebInspector.notifications.dispatchEventToListeners(WebInspector.UserMetrics.UserAction, {
            action: WebInspector.UserMetrics.UserActionNames.OpenSourceLink,
            url: uiSourceCode.originURL(),
            lineNumber: lineNumber
        });
    },

    /**
     * @param {!WebInspector.UISourceCode} uiSourceCode
     * @return {!WebInspector.SourceFrame}
     */
    _showFile: function(uiSourceCode)
    {
        var sourceFrame = this._getOrCreateSourceFrame(uiSourceCode);
        if (this._currentUISourceCode === uiSourceCode)
            return sourceFrame;

        this._currentUISourceCode = uiSourceCode;
        this._editorContainer.showFile(uiSourceCode);
        this._updateScriptViewStatusBarItems();
        return sourceFrame;
    },

    /**
     * @param {!WebInspector.UISourceCode} uiSourceCode
     * @return {!WebInspector.UISourceCodeFrame}
     */
    _createSourceFrame: function(uiSourceCode)
    {
        var sourceFrame;
        switch (uiSourceCode.contentType()) {
        case WebInspector.resourceTypes.Script:
            sourceFrame = new WebInspector.JavaScriptSourceFrame(this._sourcesPanel, uiSourceCode);
            break;
        case WebInspector.resourceTypes.Document:
            sourceFrame = new WebInspector.JavaScriptSourceFrame(this._sourcesPanel, uiSourceCode);
            break;
        case WebInspector.resourceTypes.Stylesheet:
            sourceFrame = new WebInspector.CSSSourceFrame(uiSourceCode);
            break;
        default:
            sourceFrame = new WebInspector.UISourceCodeFrame(uiSourceCode);
        break;
        }
        sourceFrame.setHighlighterType(uiSourceCode.highlighterType());
        this._sourceFramesByUISourceCode.put(uiSourceCode, sourceFrame);
        this._historyManager.trackSourceFrameCursorJumps(sourceFrame);
        return sourceFrame;
    },

    /**
     * @param {!WebInspector.UISourceCode} uiSourceCode
     * @return {!WebInspector.UISourceCodeFrame}
     */
    _getOrCreateSourceFrame: function(uiSourceCode)
    {
        return this._sourceFramesByUISourceCode.get(uiSourceCode) || this._createSourceFrame(uiSourceCode);
    },

    /**
     * @param {!WebInspector.SourceFrame} sourceFrame
     * @param {!WebInspector.UISourceCode} uiSourceCode
     * @return {boolean}
     */
    _sourceFrameMatchesUISourceCode: function(sourceFrame, uiSourceCode)
    {
        switch (uiSourceCode.contentType()) {
        case WebInspector.resourceTypes.Script:
        case WebInspector.resourceTypes.Document:
            return sourceFrame instanceof WebInspector.JavaScriptSourceFrame;
        case WebInspector.resourceTypes.Stylesheet:
            return sourceFrame instanceof WebInspector.CSSSourceFrame;
        default:
            return !(sourceFrame instanceof WebInspector.JavaScriptSourceFrame);
        }
    },

    /**
     * @param {!WebInspector.UISourceCode} uiSourceCode
     */
    _recreateSourceFrameIfNeeded: function(uiSourceCode)
    {
        var oldSourceFrame = this._sourceFramesByUISourceCode.get(uiSourceCode);
        if (!oldSourceFrame)
            return;
        if (this._sourceFrameMatchesUISourceCode(oldSourceFrame, uiSourceCode)) {
            oldSourceFrame.setHighlighterType(uiSourceCode.highlighterType());
        } else {
            this._editorContainer.removeUISourceCode(uiSourceCode);
            this._removeSourceFrame(uiSourceCode);
        }
    },

    /**
     * @param {!WebInspector.UISourceCode} uiSourceCode
     * @return {!WebInspector.SourceFrame}
     */
    viewForFile: function(uiSourceCode)
    {
        return this._getOrCreateSourceFrame(uiSourceCode);
    },

    /**
     * @param {!WebInspector.UISourceCode} uiSourceCode
     */
    _removeSourceFrame: function(uiSourceCode)
    {
        var sourceFrame = this._sourceFramesByUISourceCode.get(uiSourceCode);
        if (!sourceFrame)
            return;
        this._sourceFramesByUISourceCode.remove(uiSourceCode);
        sourceFrame.dispose();
    },

    clearCurrentExecutionLine: function()
    {
        if (this._executionSourceFrame)
            this._executionSourceFrame.clearExecutionLine();
        delete this._executionSourceFrame;
    },

    setExecutionLine: function(uiLocation)
    {
        var sourceFrame = this._getOrCreateSourceFrame(uiLocation.uiSourceCode);
        sourceFrame.setExecutionLine(uiLocation.lineNumber);
        this._executionSourceFrame = sourceFrame;
    },

    _editorClosed: function(event)
    {
        var uiSourceCode = /** @type {!WebInspector.UISourceCode} */ (event.data);
        this._historyManager.removeHistoryForSourceCode(uiSourceCode);

        var wasSelected = false;
        if (this._currentUISourceCode === uiSourceCode) {
            delete this._currentUISourceCode;
            wasSelected = true;
        }

        // SourcesNavigator does not need to update on EditorClosed.
        this._updateScriptViewStatusBarItems();
        this._searchableView.resetSearch();

        var data = {};
        data.uiSourceCode = uiSourceCode;
        data.wasSelected = wasSelected;
        this.dispatchEventToListeners(WebInspector.SourcesView.Events.EditorClosed, data);
    },

    _editorSelected: function(event)
    {
        var uiSourceCode = /** @type {!WebInspector.UISourceCode} */ (event.data.currentFile);
        var shouldUseHistoryManager = uiSourceCode !== this._currentUISourceCode && event.data.userGesture;
        if (shouldUseHistoryManager)
            this._historyManager.updateCurrentState();
        var sourceFrame = this._showFile(uiSourceCode);
        if (shouldUseHistoryManager)
            this._historyManager.pushNewState();

        this._searchableView.setReplaceable(!!sourceFrame && sourceFrame.canEditSource());
        this._searchableView.resetSearch();

        this.dispatchEventToListeners(WebInspector.SourcesView.Events.EditorSelected, uiSourceCode);
    },

    /**
     * @param {!WebInspector.UISourceCode} uiSourceCode
     */
    sourceRenamed: function(uiSourceCode)
    {
        this._recreateSourceFrameIfNeeded(uiSourceCode);
    },

    searchCanceled: function()
    {
        if (this._searchView)
            this._searchView.searchCanceled();

        delete this._searchView;
        delete this._searchQuery;
    },

    /**
     * @param {string} query
     * @param {boolean} shouldJump
     * @param {boolean=} jumpBackwards
     */
    performSearch: function(query, shouldJump, jumpBackwards)
    {
        this._searchableView.updateSearchMatchesCount(0);

        var sourceFrame = this.currentSourceFrame();
        if (!sourceFrame)
            return;

        this._searchView = sourceFrame;
        this._searchQuery = query;

        /**
         * @param {!WebInspector.View} view
         * @param {number} searchMatches
         * @this {WebInspector.SourcesView}
         */
        function finishedCallback(view, searchMatches)
        {
            if (!searchMatches)
                return;

            this._searchableView.updateSearchMatchesCount(searchMatches);
        }

        /**
         * @param {number} currentMatchIndex
         * @this {WebInspector.SourcesView}
         */
        function currentMatchChanged(currentMatchIndex)
        {
            this._searchableView.updateCurrentMatchIndex(currentMatchIndex);
        }

        /**
         * @this {WebInspector.SourcesView}
         */
        function searchResultsChanged()
        {
            this._searchableView.cancelSearch();
        }

        this._searchView.performSearch(query, shouldJump, !!jumpBackwards, finishedCallback.bind(this), currentMatchChanged.bind(this), searchResultsChanged.bind(this));
    },

    jumpToNextSearchResult: function()
    {
        if (!this._searchView)
            return;

        if (this._searchView !== this.currentSourceFrame()) {
            this.performSearch(this._searchQuery, true);
            return;
        }

        this._searchView.jumpToNextSearchResult();
    },

    jumpToPreviousSearchResult: function()
    {
        if (!this._searchView)
            return;

        if (this._searchView !== this.currentSourceFrame()) {
            this.performSearch(this._searchQuery, true);
            if (this._searchView)
                this._searchView.jumpToLastSearchResult();
            return;
        }

        this._searchView.jumpToPreviousSearchResult();
    },

    /**
     * @param {string} text
     */
    replaceSelectionWith: function(text)
    {
        var sourceFrame = this.currentSourceFrame();
        if (!sourceFrame) {
            console.assert(sourceFrame);
            return;
        }
        sourceFrame.replaceSelectionWith(text);
    },

    /**
     * @param {string} query
     * @param {string} text
     */
    replaceAllWith: function(query, text)
    {
        var sourceFrame = this.currentSourceFrame();
        if (!sourceFrame) {
            console.assert(sourceFrame);
            return;
        }
        sourceFrame.replaceAllWith(query, text);
    },

    /**
     * @param {?Event=} event
     * @return {boolean}
     */
    _showOutlineDialog: function(event)
    {
        var uiSourceCode = this._editorContainer.currentFile();
        if (!uiSourceCode)
            return false;

        switch (uiSourceCode.contentType()) {
        case WebInspector.resourceTypes.Document:
        case WebInspector.resourceTypes.Script:
            WebInspector.JavaScriptOutlineDialog.show(this, uiSourceCode, this.showSourceLocation.bind(this, uiSourceCode));
            return true;
        case WebInspector.resourceTypes.Stylesheet:
            WebInspector.StyleSheetOutlineDialog.show(this, uiSourceCode, this.showSourceLocation.bind(this, uiSourceCode));
            return true;
        }
        return false;
    },

    /**
     * @param {string=} query
     */
    showOpenResourceDialog: function(query)
    {
        var uiSourceCodes = this._editorContainer.historyUISourceCodes();
        /** @type {!Map.<!WebInspector.UISourceCode, number>} */
        var defaultScores = new Map();
        for (var i = 1; i < uiSourceCodes.length; ++i) // Skip current element
            defaultScores.put(uiSourceCodes[i], uiSourceCodes.length - i);
        WebInspector.OpenResourceDialog.show(this, this.element, query, defaultScores);
    },

    /**
     * @param {?Event=} event
     * @return {boolean}
     */
    _showGoToLineDialog: function(event)
    {
        if (this._currentUISourceCode)
            this.showOpenResourceDialog(":");
        return true;
    },

    /**
     * @return {boolean}
     */
    _save: function()
    {
        this._saveSourceFrame(this.currentSourceFrame());
        return true;
    },

    /**
     * @return {boolean}
     */
    _saveAll: function()
    {
        var sourceFrames = this._editorContainer.fileViews();
        sourceFrames.forEach(this._saveSourceFrame.bind(this));
        return true;
    },

    /**
     * @param {?WebInspector.SourceFrame} sourceFrame
     */
    _saveSourceFrame: function(sourceFrame)
    {
        if (!sourceFrame)
            return;
        if (!(sourceFrame instanceof WebInspector.UISourceCodeFrame))
            return;
        var uiSourceCodeFrame = /** @type {!WebInspector.UISourceCodeFrame} */ (sourceFrame);
        uiSourceCodeFrame.commitEditing();
    },
    /**
     * @return {boolean}
     */
    _toggleBreakpoint: function()
    {
        var sourceFrame = this.currentSourceFrame();
        if (!sourceFrame)
            return false;

        if (sourceFrame instanceof WebInspector.JavaScriptSourceFrame) {
            var javaScriptSourceFrame = /** @type {!WebInspector.JavaScriptSourceFrame} */ (sourceFrame);
            javaScriptSourceFrame.toggleBreakpointOnCurrentLine();
            return true;
        }
        return false;
    },

    /**
     * @param {boolean} active
     */
    toggleBreakpointsActiveState: function(active)
    {
        this._editorContainer.view.element.classList.toggle("breakpoints-deactivated", !active);
    },

    __proto__: WebInspector.VBox.prototype
}

/**
 * @interface
 */
WebInspector.SourcesView.EditorAction = function()
{
}

WebInspector.SourcesView.EditorAction.prototype = {
    /**
     * @param {!WebInspector.SourcesView} sourcesView
     * @return {!Element}
     */
    button: function(sourcesView) { }
}
