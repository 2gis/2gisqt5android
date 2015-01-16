/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * @implements {WebInspector.TargetManager.Observer}
 * @param {!WebInspector.NetworkWorkspaceBinding} networkWorkspaceBinding
 * @param {!WebInspector.Workspace} workspace
 */
WebInspector.NetworkUISourceCodeProvider = function(networkWorkspaceBinding, workspace)
{
    this._networkWorkspaceBinding = networkWorkspaceBinding;
    this._workspace = workspace;
    WebInspector.targetManager.observeTargets(this);
    this._processedURLs = {};
}

WebInspector.NetworkUISourceCodeProvider.prototype = {
    /**
     * @param {!WebInspector.Target} target
     */
    targetAdded: function(target)
    {
        target.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.ResourceAdded, this._resourceAdded, this);
        target.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.MainFrameNavigated, this._mainFrameNavigated, this);
        target.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.ParsedScriptSource, this._parsedScriptSource, this);
        target.cssModel.addEventListener(WebInspector.CSSStyleModel.Events.StyleSheetAdded, this._styleSheetAdded, this);
    },

    /**
     * @param {!WebInspector.Target} target
     */
    targetRemoved: function(target)
    {
        // FIXME: add workspace cleanup here.
        target.resourceTreeModel.removeEventListener(WebInspector.ResourceTreeModel.EventTypes.ResourceAdded, this._resourceAdded, this);
        target.resourceTreeModel.removeEventListener(WebInspector.ResourceTreeModel.EventTypes.MainFrameNavigated, this._mainFrameNavigated, this);
        target.debuggerModel.removeEventListener(WebInspector.DebuggerModel.Events.ParsedScriptSource, this._parsedScriptSource, this);
        target.cssModel.removeEventListener(WebInspector.CSSStyleModel.Events.StyleSheetAdded, this._styleSheetAdded, this);
    },

    _populate: function()
    {
        /**
         * @param {!WebInspector.ResourceTreeFrame} frame
         * @this {WebInspector.NetworkUISourceCodeProvider}
         */
        function populateFrame(frame)
        {
            for (var i = 0; i < frame.childFrames.length; ++i)
                populateFrame.call(this, frame.childFrames[i]);

            var resources = frame.resources();
            for (var i = 0; i < resources.length; ++i)
                this._resourceAdded({data:resources[i]});
        }

        populateFrame.call(this, WebInspector.resourceTreeModel.mainFrame);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _parsedScriptSource: function(event)
    {
        var script = /** @type {!WebInspector.Script} */ (event.data);
        if (!script.sourceURL || script.isInlineScript() || script.isSnippet())
            return;
        // Filter out embedder injected content scripts.
        if (script.isContentScript() && !script.hasSourceURL) {
            var parsedURL = new WebInspector.ParsedURL(script.sourceURL);
            if (!parsedURL.isValid)
                return;
        }
        this._addFile(script.sourceURL, script, script.isContentScript());
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _styleSheetAdded: function(event)
    {
        var header = /** @type {!WebInspector.CSSStyleSheetHeader} */ (event.data);
        if ((!header.hasSourceURL || header.isInline) && header.origin !== "inspector")
            return;

        this._addFile(header.resourceURL(), header, false);
    },

    /**
     * @param {!WebInspector.Event|!{data: !WebInspector.Resource}} event
     */
    _resourceAdded: function(event)
    {
        var resource = /** @type {!WebInspector.Resource} */ (event.data);
        this._addFile(resource.url, new WebInspector.NetworkUISourceCodeProvider.FallbackResource(resource));
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _mainFrameNavigated: function(event)
    {
        this._reset();
    },

    /**
     * @param {string} url
     * @param {!WebInspector.ContentProvider} contentProvider
     * @param {boolean=} isContentScript
     */
    _addFile: function(url, contentProvider, isContentScript)
    {
        if (this._workspace.hasMappingForURL(url))
            return;

        var type = contentProvider.contentType();
        if (type !== WebInspector.resourceTypes.Stylesheet && type !== WebInspector.resourceTypes.Document && type !== WebInspector.resourceTypes.Script)
            return;
        if (this._processedURLs[url])
            return;
        this._processedURLs[url] = true;
        this._networkWorkspaceBinding.addFileForURL(url, contentProvider, isContentScript);
    },

    _reset: function()
    {
        this._processedURLs = {};
        this._networkWorkspaceBinding.reset();
        this._populate();
    }
}

/**
 * @constructor
 * @implements {WebInspector.ContentProvider}
 * @param {!WebInspector.Resource} resource
 */
WebInspector.NetworkUISourceCodeProvider.FallbackResource = function(resource)
{
    this._resource = resource;
}

WebInspector.NetworkUISourceCodeProvider.FallbackResource.prototype = {

    /**
     * @return {string}
     */
    contentURL: function()
    {
        return this._resource.contentURL();
    },

    /**
     * @return {!WebInspector.ResourceType}
     */
    contentType: function()
    {
        return this._resource.contentType();
    },

    /**
     * @param {function(?string)} callback
     */
    requestContent: function(callback)
    {
        /**
         * @this {WebInspector.NetworkUISourceCodeProvider.FallbackResource}
         */
        function loadFallbackContent()
        {
            var scripts = WebInspector.debuggerModel.scriptsForSourceURL(this._resource.url);
            if (!scripts.length) {
                callback(null);
                return;
            }

            var contentProvider;
            if (this._resource.type === WebInspector.resourceTypes.Document)
                contentProvider = new WebInspector.ConcatenatedScriptsContentProvider(scripts);
            else if (this._resource.type === WebInspector.resourceTypes.Script)
                contentProvider = scripts[0];

            console.assert(contentProvider, "Resource content request failed. " + this._resource.url);

            contentProvider.requestContent(callback);
        }

        /**
         * @param {?string} content
         * @this {WebInspector.NetworkUISourceCodeProvider.FallbackResource}
         */
        function requestContentLoaded(content)
        {
            if (content)
                callback(content)
            else
                loadFallbackContent.call(this);
        }

        this._resource.requestContent(requestContentLoaded.bind(this));
    },

    /**
     * @param {string} query
     * @param {boolean} caseSensitive
     * @param {boolean} isRegex
     * @param {function(!Array.<!WebInspector.ContentProvider.SearchMatch>)} callback
     */
    searchInContent: function(query, caseSensitive, isRegex, callback)
    {
        /**
         * @param {?string} content
         */
        function documentContentLoaded(content)
        {
            if (content === null) {
                callback([]);
                return;
            }

            var result = WebInspector.ContentProvider.performSearchInContent(content, query, caseSensitive, isRegex);
            callback(result);
        }

        if (this.contentType() === WebInspector.resourceTypes.Document) {
            this.requestContent(documentContentLoaded);
            return;
        }

        this._resource.searchInContent(query, caseSensitive, isRegex, callback);
    }
}

/**
 * @type {!WebInspector.NetworkWorkspaceBinding}
 */
WebInspector.networkWorkspaceBinding;
