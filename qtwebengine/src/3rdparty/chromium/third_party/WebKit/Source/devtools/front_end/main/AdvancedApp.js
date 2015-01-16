// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @extends {WebInspector.App}
 */
WebInspector.AdvancedApp = function()
{
    WebInspector.App.call(this);
    WebInspector.dockController.addEventListener(WebInspector.DockController.Events.BeforeDockSideChanged, this._openToolboxWindow, this);
};

WebInspector.AdvancedApp.prototype = {
    createRootView: function()
    {
        var rootView = new WebInspector.RootView();

        this._rootSplitView = new WebInspector.SplitView(false, true, WebInspector.dockController.canDock() ? "InspectorView.splitViewState" : "InspectorView.dummySplitViewState", 300, 300, true);
        this._rootSplitView.show(rootView.element);

        WebInspector.inspectorView.show(this._rootSplitView.sidebarElement());

        this._inspectedPagePlaceholder = new WebInspector.InspectedPagePlaceholder();
        this._inspectedPagePlaceholder.addEventListener(WebInspector.InspectedPagePlaceholder.Events.Update, this._onSetInspectedPageBounds.bind(this, false), this);
        if (WebInspector.experimentsSettings.responsiveDesign.isEnabled()) {
            this._responsiveDesignView = new WebInspector.ResponsiveDesignView(this._inspectedPagePlaceholder);
            this._responsiveDesignView.show(this._rootSplitView.mainElement());
        } else {
            this._inspectedPagePlaceholder.show(this._rootSplitView.mainElement());
        }

        WebInspector.dockController.addEventListener(WebInspector.DockController.Events.BeforeDockSideChanged, this._onBeforeDockSideChange, this);
        WebInspector.dockController.addEventListener(WebInspector.DockController.Events.DockSideChanged, this._onDockSideChange, this);
        WebInspector.dockController.addEventListener(WebInspector.DockController.Events.AfterDockSideChanged, this._onAfterDockSideChange, this);
        this._onDockSideChange();

        rootView.attachToBody();
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _openToolboxWindow: function(event)
    {
        if (/** @type {string} */ (event.data.to) !== WebInspector.DockController.State.Undocked)
            return;

        if (this._toolboxWindow || !WebInspector.experimentsSettings.responsiveDesign.isEnabled())
            return;

        var toolbox = (window.location.search ? "&" : "?") + "toolbox=true";
        var hash = window.location.hash;
        var url = window.location.href.replace(hash, "") + toolbox + hash;
        this._toolboxWindow = window.open(url, undefined);
    },

    /**
     * @param {!WebInspector.Toolbox} toolbox
     */
    _toolboxLoaded: function(toolbox)
    {
        this._toolbox = toolbox;
        this._updatePageResizer();
    },

    _updatePageResizer: function()
    {
        if (!WebInspector.experimentsSettings.responsiveDesign.isEnabled())
            return;

        if (this._isDocked())
            this._responsiveDesignView.updatePageResizer();
        else if (this._toolbox)
            this._toolbox._responsiveDesignView.updatePageResizer();
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onBeforeDockSideChange: function(event)
    {
        if (/** @type {string} */ (event.data.to) === WebInspector.DockController.State.Undocked && this._toolbox) {
            // Hide inspectorView and force layout to mimic the undocked state.
            this._rootSplitView.hideSidebar();
            this._inspectedPagePlaceholder.update();
        }

        this._changingDockSide = true;
    },

    /**
     * @param {!WebInspector.Event=} event
     */
    _onDockSideChange: function(event)
    {
        this._updatePageResizer();

        var toDockSide = event ? /** @type {string} */ (event.data.to) : WebInspector.dockController.dockSide();
        if (toDockSide === WebInspector.DockController.State.Undocked) {
            this._updateForUndocked();
        } else if (this._toolbox && event && /** @type {string} */ (event.data.from) === WebInspector.DockController.State.Undocked) {
            // Don't update yet for smooth transition.
            this._rootSplitView.hideSidebar();
        } else {
            this._updateForDocked(toDockSide);
        }
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onAfterDockSideChange: function(event)
    {
        // We may get here on the first dock side change while loading without BeforeDockSideChange.
        if (!this._changingDockSide)
            return;
        this._changingDockSide = false;
        if (/** @type {string} */ (event.data.from) === WebInspector.DockController.State.Undocked) {
            // Restore docked layout in case of smooth transition.
            this._updateForDocked(/** @type {string} */ (event.data.to));
        }
        this._inspectedPagePlaceholder.update();
    },

    /**
     * @param {string} dockSide
     */
    _updateForDocked: function(dockSide)
    {
        this._rootSplitView.setVertical(dockSide === WebInspector.DockController.State.DockedToLeft || dockSide === WebInspector.DockController.State.DockedToRight);
        this._rootSplitView.setSecondIsSidebar(dockSide === WebInspector.DockController.State.DockedToRight || dockSide === WebInspector.DockController.State.DockedToBottom);
        this._rootSplitView.toggleResizer(this._rootSplitView.resizerElement(), true);
        this._rootSplitView.toggleResizer(WebInspector.inspectorView.topResizerElement(), dockSide === WebInspector.DockController.State.DockedToBottom);
        this._rootSplitView.showBoth();
    },

    _updateForUndocked: function()
    {
        this._rootSplitView.toggleResizer(this._rootSplitView.resizerElement(), false);
        this._rootSplitView.toggleResizer(WebInspector.inspectorView.topResizerElement(), false);
        this._rootSplitView.hideMain();
    },

    _isDocked: function()
    {
        return WebInspector.dockController.dockSide() !== WebInspector.DockController.State.Undocked;
    },

    /**
     * @param {boolean} toolbox
     * @param {!WebInspector.Event} event
     */
    _onSetInspectedPageBounds: function(toolbox, event)
    {
        if (this._changingDockSide || (this._isDocked() === toolbox))
            return;
        if (!window.innerWidth || !window.innerHeight)
            return;
        var bounds = /** @type {{x: number, y: number, width: number, height: number}} */ (event.data);
        InspectorFrontendHost.setInspectedPageBounds(bounds);
    },

    __proto__: WebInspector.App.prototype
};

/**
 * @constructor
 */
WebInspector.Toolbox = function()
{
    if (!window.opener)
        return;

    WebInspector.zoomManager = window.opener.WebInspector.zoomManager;
    WebInspector.overridesSupport = window.opener.WebInspector.overridesSupport;
    WebInspector.settings = window.opener.WebInspector.settings;
    WebInspector.experimentsSettings = window.opener.WebInspector.experimentsSettings;
    WebInspector.cssModel = window.opener.WebInspector.cssModel;
    WebInspector.domModel = window.opener.WebInspector.domModel;
    WebInspector.workspace = window.opener.WebInspector.workspace;
    WebInspector.Revealer = window.opener.WebInspector.Revealer;
    WebInspector.installPortStyles();

    var advancedApp = /** @type {!WebInspector.AdvancedApp} */ (window.opener.WebInspector.app);
    var rootView = new WebInspector.RootView();
    this._inspectedPagePlaceholder = new WebInspector.InspectedPagePlaceholder();
    this._inspectedPagePlaceholder.addEventListener(WebInspector.InspectedPagePlaceholder.Events.Update, advancedApp._onSetInspectedPageBounds.bind(advancedApp, true));
    if (WebInspector.experimentsSettings.responsiveDesign.isEnabled()) {
        this._responsiveDesignView = new WebInspector.ResponsiveDesignView(this._inspectedPagePlaceholder);
        this._responsiveDesignView.show(rootView.element);
    } else {
        this._inspectedPagePlaceholder.show(rootView.element);
    }
    rootView.attachToBody();
    advancedApp._toolboxLoaded(this);
}
