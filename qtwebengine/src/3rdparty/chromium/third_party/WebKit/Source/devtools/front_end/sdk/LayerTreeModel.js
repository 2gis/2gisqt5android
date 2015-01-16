/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

/** @typedef {!{
        bounds: {height: number, width: number},
        children: Array.<!WebInspector.TracingLayerPayload>,
        layer_id: number,
        position: Array.<number>,
        scroll_offset: Array.<number>,
        layer_quad: Array.<number>,
        draws_content: number,
        transform: Array.<number>,
        owner_node: number
    }}
*/
WebInspector.TracingLayerPayload;

/**
  * @constructor
  * @extends {WebInspector.TargetAwareObject}
  */
WebInspector.LayerTreeModel = function(target)
{
    WebInspector.TargetAwareObject.call(this, target);
    InspectorBackend.registerLayerTreeDispatcher(new WebInspector.LayerTreeDispatcher(this));
    target.domModel.addEventListener(WebInspector.DOMModel.Events.DocumentUpdated, this._onDocumentUpdated, this);
    /** @type {?WebInspector.LayerTreeBase} */
    this._layerTree = null;
}

WebInspector.LayerTreeModel.Events = {
    LayerTreeChanged: "LayerTreeChanged",
    LayerPainted: "LayerPainted",
}

WebInspector.LayerTreeModel.ScrollRectType = {
    NonFastScrollable: {name: "NonFastScrollable", description: "Non fast scrollable"},
    TouchEventHandler: {name: "TouchEventHandler", description: "Touch event handler"},
    WheelEventHandler: {name: "WheelEventHandler", description: "Wheel event handler"},
    RepaintsOnScroll: {name: "RepaintsOnScroll", description: "Repaints on scroll"}
}

WebInspector.LayerTreeModel.prototype = {
    disable: function()
    {
        if (!this._enabled)
            return;
        this._enabled = false;
        this._layerTree = null;
        LayerTreeAgent.disable();
    },

    enable: function()
    {
        if (this._enabled)
            return;
        this._enabled = true;
        this._layerTree = new WebInspector.AgentLayerTree(this._target);
        this._lastPaintRectByLayerId = {};
        LayerTreeAgent.enable();
    },

    /**
     * @param {!WebInspector.LayerTreeBase} layerTree
     */
    setLayerTree: function(layerTree)
    {
        this.disable();
        this._layerTree = layerTree;
        this.dispatchEventToListeners(WebInspector.LayerTreeModel.Events.LayerTreeChanged);
    },

    /**
     * @return {?WebInspector.LayerTreeBase}
     */
    layerTree: function()
    {
        return this._layerTree;
    },

    /**
     * @param {?Array.<!LayerTreeAgent.Layer>} layers
     */
    _layerTreeChanged: function(layers)
    {
        if (!this._enabled)
            return;
        var layerTree = /** @type {!WebInspector.AgentLayerTree} */ (this._layerTree);
        layerTree.setLayers(layers, onLayersSet.bind(this));

        /**
         * @this {WebInspector.LayerTreeModel}
         */
        function onLayersSet()
        {
            for (var layerId in this._lastPaintRectByLayerId) {
                var lastPaintRect = this._lastPaintRectByLayerId[layerId];
                var layer = layerTree.layerById(layerId);
                if (layer)
                    layer._lastPaintRect = lastPaintRect;
            }
            this._lastPaintRectByLayerId = {};

            this.dispatchEventToListeners(WebInspector.LayerTreeModel.Events.LayerTreeChanged);
        }
    },

    /**
     * @param {!LayerTreeAgent.LayerId} layerId
     * @param {!DOMAgent.Rect} clipRect
     */
    _layerPainted: function(layerId, clipRect)
    {
        if (!this._enabled)
            return;
        var layerTree = /** @type {!WebInspector.AgentLayerTree} */ (this._layerTree);
        var layer = layerTree.layerById(layerId);
        if (!layer) {
            this._lastPaintRectByLayerId[layerId] = clipRect;
            return;
        }
        layer._didPaint(clipRect);
        this.dispatchEventToListeners(WebInspector.LayerTreeModel.Events.LayerPainted, layer);
    },

    _onDocumentUpdated: function()
    {
        if (!this._enabled)
            return;
        this.disable();
        this.enable();
    },

    __proto__: WebInspector.TargetAwareObject.prototype
}

/**
  * @constructor
  * @extends {WebInspector.TargetAwareObject}
  * @param {!WebInspector.Target} target
  */
WebInspector.LayerTreeBase = function(target)
{
    WebInspector.TargetAwareObject.call(this, target);
    this._layersById = {};
    this._backendNodeIdToNodeId = {};
    this._reset();
}

WebInspector.LayerTreeBase.prototype = {
    _reset: function()
    {
        this._root = null;
        this._contentRoot = null;
    },

    /**
     * @return {?WebInspector.Layer}
     */
    root: function()
    {
        return this._root;
    },

    /**
     * @return {?WebInspector.Layer}
     */
    contentRoot: function()
    {
        return this._contentRoot;
    },

    /**
     * @param {function(!WebInspector.Layer)} callback
     * @param {?WebInspector.Layer=} root
     * @return {boolean}
     */
    forEachLayer: function(callback, root)
    {
        if (!root) {
            root = this.root();
            if (!root)
                return false;
        }
        return callback(root) || root.children().some(this.forEachLayer.bind(this, callback));
    },

    /**
     * @param {string} id
     * @return {?WebInspector.Layer}
     */
    layerById: function(id)
    {
        return this._layersById[id] || null;
    },

    /**
     * @param {!Array.<number>} requestedNodeIds
     * @param {function()} callback
     */
    _resolveBackendNodeIds: function(requestedNodeIds, callback)
    {
        if (!requestedNodeIds.length) {
            callback();
            return;
        }

        this.target().domModel.pushNodesByBackendIdsToFrontend(requestedNodeIds, populateBackendNodeIdMap.bind(this));

        /**
         * @this {WebInspector.LayerTreeBase}
         * @param {?Array.<number>} nodeIds
         */
        function populateBackendNodeIdMap(nodeIds)
        {
            if (nodeIds) {
                for (var i = 0; i < requestedNodeIds.length; ++i) {
                    var nodeId = nodeIds[i];
                    if (nodeId)
                        this._backendNodeIdToNodeId[requestedNodeIds[i]] = nodeId;
                }
            }
            callback();
        }
    },

    /**
     * @param {!Object} viewportSize
     */
    setViewportSize: function(viewportSize)
    {
        this._viewportSize = viewportSize;
    },

    /**
     * @return {!Object | undefined}
     */
    viewportSize: function()
    {
        return this._viewportSize;
    },

    __proto__: WebInspector.TargetAwareObject.prototype
};

/**
  * @constructor
  * @extends {WebInspector.LayerTreeBase}
  * @param {!WebInspector.Target} target
  */
WebInspector.TracingLayerTree = function(target)
{
    WebInspector.LayerTreeBase.call(this, target);
}

WebInspector.TracingLayerTree.prototype = {
    /**
     * @param {!WebInspector.TracingLayerPayload} root
     * @param {!function()} callback
     */
    setLayers: function(root, callback)
    {
        var idsToResolve = [];
        this._extractNodeIdsToResolve(idsToResolve, {}, root);
        this._resolveBackendNodeIds(idsToResolve, onBackendNodeIdsResolved.bind(this));

        /**
         * @this {WebInspector.TracingLayerTree}
         */
        function onBackendNodeIdsResolved()
        {
            var oldLayersById = this._layersById;
            this._layersById = {};
            this._contentRoot = null;
            this._root = this._innerSetLayers(oldLayersById, root);
            callback();
        }
    },

    /**
     * @param {!Object.<(string|number), !WebInspector.Layer>} oldLayersById
     * @param {!WebInspector.TracingLayerPayload} payload
     * @return {!WebInspector.TracingLayer}
     */
    _innerSetLayers: function(oldLayersById, payload)
    {
        var layer = /** @type {?WebInspector.TracingLayer} */ (oldLayersById[payload.layer_id]);
        if (layer)
            layer._reset(payload);
        else
            layer = new WebInspector.TracingLayer(payload);
        this._layersById[payload.layer_id] = layer;
        if (!this._contentRoot && payload.draws_content)
            this._contentRoot = layer;

        if (payload.owner_node && this._backendNodeIdToNodeId[payload.owner_node])
            layer._setNode(this._target.domModel.nodeForId(this._backendNodeIdToNodeId[payload.owner_node]));

        for (var i = 0; payload.children && i < payload.children.length; ++i)
            layer.addChild(this._innerSetLayers(oldLayersById, payload.children[i]));
        return layer;
    },

    /**
     * @param {!Array.<number>} nodeIdsToResolve
     * @param {!Object} seenNodeIds
     * @param {!WebInspector.TracingLayerPayload} payload
     */
    _extractNodeIdsToResolve: function(nodeIdsToResolve, seenNodeIds, payload)
    {
        var backendNodeId = payload.owner_node;
        if (backendNodeId && !seenNodeIds[backendNodeId] && !(this._backendNodeIdToNodeId[backendNodeId] && this.target().domModel.nodeForId(backendNodeId))) {
            seenNodeIds[backendNodeId] = true;
            nodeIdsToResolve.push(backendNodeId);
        }
        for (var i = 0; payload.children && i < payload.children.length; ++i)
            this._extractNodeIdsToResolve(nodeIdsToResolve, seenNodeIds, payload.children[i]);
    },

    __proto__: WebInspector.LayerTreeBase.prototype
}

/**
  * @constructor
  * @extends {WebInspector.LayerTreeBase}
  */
WebInspector.AgentLayerTree = function(target)
{
    WebInspector.LayerTreeBase.call(this, target);
}

WebInspector.AgentLayerTree.prototype = {
    /**
     * @param {?Array.<!LayerTreeAgent.Layer>} payload
     * @param {function()} callback
     */
    setLayers: function(payload, callback)
    {
        if (!payload) {
            onBackendNodeIdsResolved.call(this);
            return;
        }

        var idsToResolve = {};
        var requestedIds = [];
        for (var i = 0; i < payload.length; ++i) {
            var backendNodeId = payload[i].backendNodeId;
            if (!backendNodeId || idsToResolve[backendNodeId] ||
                (this._backendNodeIdToNodeId[backendNodeId] && this.target().domModel.nodeForId(this._backendNodeIdToNodeId[backendNodeId]))) {
                continue;
            }
            idsToResolve[backendNodeId] = true;
            requestedIds.push(backendNodeId);
        }
        this._resolveBackendNodeIds(requestedIds, onBackendNodeIdsResolved.bind(this));

        /**
         * @this {WebInspector.AgentLayerTree}
         */
        function onBackendNodeIdsResolved()
        {
            this._innerSetLayers(payload);
            callback();
        }
    },

    /**
     * @param {?Array.<!LayerTreeAgent.Layer>} layers
     */
    _innerSetLayers: function(layers)
    {
        this._reset();
        // Payload will be null when not in the composited mode.
        if (!layers)
            return;
        var oldLayersById = this._layersById;
        this._layersById = {};
        for (var i = 0; i < layers.length; ++i) {
            var layerId = layers[i].layerId;
            var layer = oldLayersById[layerId];
            if (layer)
                layer._reset(layers[i]);
            else
                layer = new WebInspector.AgentLayer(layers[i]);
            this._layersById[layerId] = layer;
            if (layers[i].backendNodeId) {
                layer._setNode(this._target.domModel.nodeForId(this._backendNodeIdToNodeId[layers[i].backendNodeId]));
                if (!this._contentRoot)
                    this._contentRoot = layer;
            }
            var parentId = layer.parentId();
            if (parentId) {
                var parent = this._layersById[parentId];
                if (!parent)
                    console.assert(parent, "missing parent " + parentId + " for layer " + layerId);
                parent.addChild(layer);
            } else {
                if (this._root)
                    console.assert(false, "Multiple root layers");
                this._root = layer;
            }
        }
        if (this._root)
            this._root._calculateQuad(new WebKitCSSMatrix());
    },

    __proto__: WebInspector.LayerTreeBase.prototype
}

/**
 * @interface
 */
WebInspector.Layer = function()
{
}

WebInspector.Layer.prototype = {
    /**
     * @return {string}
     */
    id: function() { },

    /**
     * @return {?string}
     */
    parentId: function() { },

    /**
     * @return {?WebInspector.Layer}
     */
    parent: function() { },

    /**
     * @return {boolean}
     */
    isRoot: function() { },

    /**
     * @return {!Array.<!WebInspector.Layer>}
     */
    children: function() { },

    /**
     * @param {!WebInspector.Layer} child
     */
    addChild: function(child) { },

    /**
     * @return {?WebInspector.DOMNode}
     */
    node: function() { },

    /**
     * @return {?WebInspector.DOMNode}
     */
    nodeForSelfOrAncestor: function() { },

    /**
     * @return {number}
     */
    offsetX: function() { },

    /**
     * @return {number}
     */
    offsetY: function() { },

    /**
     * @return {number}
     */
    width: function() { },

    /**
     * @return {number}
     */
    height: function() { },

    /**
     * @return {?Array.<number>}
     */
    transform: function() { },

    /**
     * @return {!Array.<number>}
     */
    quad: function() { },

    /**
     * @return {!Array.<number>}
     */
    anchorPoint: function() { },

    /**
     * @return {boolean}
     */
    invisible: function() { },

    /**
     * @return {number}
     */
    paintCount: function() { },

    /**
     * @return {?DOMAgent.Rect}
     */
    lastPaintRect: function() { },

    /**
     * @return {!Array.<!LayerTreeAgent.ScrollRect>}
     */
    scrollRects: function() { },

    /**
     * @param {function(!Array.<string>)} callback
     */
    requestCompositingReasons: function(callback) { },

    /**
     * @param {function(!WebInspector.PaintProfilerSnapshot=)} callback
     */
    requestSnapshot: function(callback) { },
}

/**
 * @constructor
 * @implements {WebInspector.Layer}
 * @param {!LayerTreeAgent.Layer} layerPayload
 */
WebInspector.AgentLayer = function(layerPayload)
{
    this._reset(layerPayload);
}

WebInspector.AgentLayer.prototype = {
    /**
     * @return {string}
     */
    id: function()
    {
        return this._layerPayload.layerId;
    },

    /**
     * @return {?string}
     */
    parentId: function()
    {
        return this._layerPayload.parentLayerId;
    },

    /**
     * @return {?WebInspector.Layer}
     */
    parent: function()
    {
        return this._parent;
    },

    /**
     * @return {boolean}
     */
    isRoot: function()
    {
        return !this.parentId();
    },

    /**
     * @return {!Array.<!WebInspector.Layer>}
     */
    children: function()
    {
        return this._children;
    },

    /**
     * @param {!WebInspector.Layer} child
     */
    addChild: function(child)
    {
        if (child._parent)
            console.assert(false, "Child already has a parent");
        this._children.push(child);
        child._parent = this;
    },

    /**
     * @param {?WebInspector.DOMNode} node
     */
    _setNode: function(node)
    {
        this._node = node;
    },

    /**
     * @return {?WebInspector.DOMNode}
     */
    node: function()
    {
        return this._node;
    },

    /**
     * @return {?WebInspector.DOMNode}
     */
    nodeForSelfOrAncestor: function()
    {
        for (var layer = this; layer; layer = layer._parent) {
            if (layer._node)
                return layer._node;
        }
        return null;
    },

    /**
     * @return {number}
     */
    offsetX: function()
    {
        return this._layerPayload.offsetX;
    },

    /**
     * @return {number}
     */
    offsetY: function()
    {
        return this._layerPayload.offsetY;
    },

    /**
     * @return {number}
     */
    width: function()
    {
        return this._layerPayload.width;
    },

    /**
     * @return {number}
     */
    height: function()
    {
        return this._layerPayload.height;
    },

    /**
     * @return {?Array.<number>}
     */
    transform: function()
    {
        return this._layerPayload.transform;
    },

    /**
     * @return {!Array.<number>}
     */
    quad: function()
    {
        return this._quad;
    },

    /**
     * @return {!Array.<number>}
     */
    anchorPoint: function()
    {
        return [
            this._layerPayload.anchorX || 0,
            this._layerPayload.anchorY || 0,
            this._layerPayload.anchorZ || 0,
        ];
    },

    /**
     * @return {boolean}
     */
    invisible: function()
    {
        return this._layerPayload.invisible;
    },

    /**
     * @return {number}
     */
    paintCount: function()
    {
        return this._paintCount || this._layerPayload.paintCount;
    },

    /**
     * @return {?DOMAgent.Rect}
     */
    lastPaintRect: function()
    {
        return this._lastPaintRect;
    },

    /**
     * @return {!Array.<!LayerTreeAgent.ScrollRect>}
     */
    scrollRects: function()
    {
        return this._scrollRects;
    },

    /**
     * @param {function(!Array.<string>)} callback
     */
    requestCompositingReasons: function(callback)
    {
        var wrappedCallback = InspectorBackend.wrapClientCallback(callback, "LayerTreeAgent.reasonsForCompositingLayer(): ", undefined, []);
        LayerTreeAgent.compositingReasons(this.id(), wrappedCallback);
    },

    /**
     * @param {function(!WebInspector.PaintProfilerSnapshot=)} callback
     */
    requestSnapshot: function(callback)
    {
        var wrappedCallback = InspectorBackend.wrapClientCallback(callback, "LayerTreeAgent.makeSnapshot(): ", WebInspector.PaintProfilerSnapshot);
        LayerTreeAgent.makeSnapshot(this.id(), wrappedCallback);
    },

    /**
     * @param {!DOMAgent.Rect} rect
     */
    _didPaint: function(rect)
    {
        this._lastPaintRect = rect;
        this._paintCount = this.paintCount() + 1;
        this._image = null;
    },

    /**
     * @param {!LayerTreeAgent.Layer} layerPayload
     */
    _reset: function(layerPayload)
    {
        /** @type {?WebInspector.DOMNode} */
        this._node = null;
        this._children = [];
        this._parent = null;
        this._paintCount = 0;
        this._layerPayload = layerPayload;
        this._image = null;
        this._scrollRects = this._layerPayload.scrollRects || [];
    },

    /**
     * @param {!Array.<number>} a
     * @return {!CSSMatrix}
     */
    _matrixFromArray: function(a)
    {
        function toFixed9(x) { return x.toFixed(9); }
        return new WebKitCSSMatrix("matrix3d(" + a.map(toFixed9).join(",") + ")");
    },

    /**
     * @param {!CSSMatrix} parentTransform
     * @return {!CSSMatrix}
     */
    _calculateTransformToViewport: function(parentTransform)
    {
        var offsetMatrix = new WebKitCSSMatrix().translate(this._layerPayload.offsetX, this._layerPayload.offsetY);
        var matrix = offsetMatrix;

        if (this._layerPayload.transform) {
            var transformMatrix = this._matrixFromArray(this._layerPayload.transform);
            var anchorVector = new WebInspector.Geometry.Vector(this._layerPayload.width * this.anchorPoint()[0], this._layerPayload.height * this.anchorPoint()[1], this.anchorPoint()[2]);
            var anchorPoint = WebInspector.Geometry.multiplyVectorByMatrixAndNormalize(anchorVector, matrix);
            var anchorMatrix = new WebKitCSSMatrix().translate(-anchorPoint.x, -anchorPoint.y, -anchorPoint.z);
            matrix = anchorMatrix.inverse().multiply(transformMatrix.multiply(anchorMatrix.multiply(matrix)));
        }

        matrix = parentTransform.multiply(matrix);
        return matrix;
    },

    /**
     * @param {number} width
     * @param {number} height
     * @return {!Array.<number>}
     */
    _createVertexArrayForRect: function(width, height)
    {
        return [0, 0, 0, width, 0, 0, width, height, 0, 0, height, 0];
    },

    /**
     * @param {!CSSMatrix} parentTransform
     */
    _calculateQuad: function(parentTransform)
    {
        var matrix = this._calculateTransformToViewport(parentTransform);
        this._quad = [];
        var vertices = this._createVertexArrayForRect(this._layerPayload.width, this._layerPayload.height);
        for (var i = 0; i < 4; ++i) {
            var point = WebInspector.Geometry.multiplyVectorByMatrixAndNormalize(new WebInspector.Geometry.Vector(vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2]), matrix);
            this._quad.push(point.x, point.y);
        }

        function calculateQuadForLayer(layer)
        {
            layer._calculateQuad(matrix);
        }

        this._children.forEach(calculateQuadForLayer);
    }
}

/**
 * @constructor
 * @param {!WebInspector.TracingLayerPayload} payload
 * @implements {WebInspector.Layer}
 */
WebInspector.TracingLayer = function(payload)
{
    this._reset(payload);
}

WebInspector.TracingLayer.prototype = {
    /**
     * @param {!WebInspector.TracingLayerPayload} payload
     */
    _reset: function(payload)
    {
        /** @type {?WebInspector.DOMNode} */
        this._node = null;
        this._layerId = String(payload.layer_id);
        this._offsetX = payload.position[0];
        this._offsetY = payload.position[1];
        this._width = payload.bounds.width;
        this._height = payload.bounds.height;
        this._children = [];
        this._parentLayerId = null;
        this._parent = null;
        this._quad = payload.layer_quad || [];
        this._createScrollRects(payload);
    },

    /**
     * @return {string}
     */
    id: function()
    {
        return this._layerId;
    },

    /**
     * @return {?string}
     */
    parentId: function()
    {
        return this._parentLayerId;
    },

    /**
     * @return {?WebInspector.Layer}
     */
    parent: function()
    {
        return this._parent;
    },

    /**
     * @return {boolean}
     */
    isRoot: function()
    {
        return !this.parentId();
    },

    /**
     * @return {!Array.<!WebInspector.Layer>}
     */
    children: function()
    {
        return this._children;
    },

    /**
     * @param {!WebInspector.Layer} child
     */
    addChild: function(child)
    {
        if (child._parent)
            console.assert(false, "Child already has a parent");
        this._children.push(child);
        child._parent = this;
        child._parentLayerId = this._layerId;
    },


    /**
     * @param {?WebInspector.DOMNode} node
     */
    _setNode: function(node)
    {
        this._node = node;
    },

    /**
     * @return {?WebInspector.DOMNode}
     */
    node: function()
    {
        return this._node;
    },

    /**
     * @return {?WebInspector.DOMNode}
     */
    nodeForSelfOrAncestor: function()
    {
        for (var layer = this; layer; layer = layer._parent) {
            if (layer._node)
                return layer._node;
        }
        return null;
    },

    /**
     * @return {number}
     */
    offsetX: function()
    {
        return this._offsetX;
    },

    /**
     * @return {number}
     */
    offsetY: function()
    {
        return this._offsetY;
    },

    /**
     * @return {number}
     */
    width: function()
    {
        return this._width;
    },

    /**
     * @return {number}
     */
    height: function()
    {
        return this._height;
    },

    /**
     * @return {?Array.<number>}
     */
    transform: function()
    {
        return null;
    },

    /**
     * @return {!Array.<number>}
     */
    quad: function()
    {
        return this._quad;
    },

    /**
     * @return {!Array.<number>}
     */
    anchorPoint: function()
    {
        return [0.5, 0.5, 0];
    },

    /**
     * @return {boolean}
     */
    invisible: function()
    {
        return false;
    },

    /**
     * @return {number}
     */
    paintCount: function()
    {
        return 0;
    },

    /**
     * @return {?DOMAgent.Rect}
     */
    lastPaintRect: function()
    {
        return null;
    },

    /**
     * @return {!Array.<!LayerTreeAgent.ScrollRect>}
     */
    scrollRects: function()
    {
        return this._scrollRects;
    },

    /**
     * @param {!Array.<number>} params
     * @param {string} type
     * @return {!Object}
     */
    _scrollRectsFromParams: function(params, type)
    {
        return {rect: {x: params[0], y: params[1], width: params[2], height: params[3]}, type: type};
    },

    /**
     * @param {!WebInspector.TracingLayerPayload} payload
     */
    _createScrollRects: function(payload)
    {
        this._scrollRects = [];
        if (payload.non_fast_scrollable_region)
            this._scrollRects.push(this._scrollRectsFromParams(payload.non_fast_scrollable_region, WebInspector.LayerTreeModel.ScrollRectType.NonFastScrollable.name));
        if (payload.touch_event_handler_region)
            this._scrollRects.push(this._scrollRectsFromParams(payload.touch_event_handler_region, WebInspector.LayerTreeModel.ScrollRectType.TouchEventHandler.name));
        if (payload.wheel_event_handler_region)
            this._scrollRects.push(this._scrollRectsFromParams(payload.wheel_event_handler_region, WebInspector.LayerTreeModel.ScrollRectType.WheelEventHandler.name));
        if (payload.scroll_event_handler_region)
            this._scrollRects.push(this._scrollRectsFromParams(payload.scroll_event_handler_region, WebInspector.LayerTreeModel.ScrollRectType.RepaintsOnScroll.name));
    },

    /**
     * @param {function(!Array.<string>)} callback
     */
    requestCompositingReasons: function(callback)
    {
        var wrappedCallback = InspectorBackend.wrapClientCallback(callback, "LayerTreeAgent.reasonsForCompositingLayer(): ", undefined, []);
        LayerTreeAgent.compositingReasons(this.id(), wrappedCallback);
    },

    /**
     * @param {function(!WebInspector.PaintProfilerSnapshot=)} callback
     */
    requestSnapshot: function(callback)
    {
        var wrappedCallback = InspectorBackend.wrapClientCallback(callback, "LayerTreeAgent.makeSnapshot(): ", WebInspector.PaintProfilerSnapshot);
        LayerTreeAgent.makeSnapshot(this.id(), wrappedCallback);
    }
}

/**
 * @constructor
 * @param {!WebInspector.Target} target
 */
WebInspector.DeferredLayerTree = function(target)
{
    this._target = target;
}

WebInspector.DeferredLayerTree.prototype = {
    /**
     * @param {function(!WebInspector.LayerTreeBase)} callback
     */
    resolve: function(callback) { },

    /**
     * @return {!WebInspector.Target}
     */
    target: function()
    {
        return this._target;
    }
};

/**
 * @constructor
 * @extends {WebInspector.DeferredLayerTree}
 * @param {!WebInspector.Target} target
 * @param {!Array.<!LayerTreeAgent.Layer>} layers
 */
WebInspector.DeferredAgentLayerTree = function(target, layers)
{
    WebInspector.DeferredLayerTree.call(this, target);
    this._layers = layers;
}

WebInspector.DeferredAgentLayerTree.prototype = {
    /**
     * @param {function(!WebInspector.LayerTreeBase)} callback
     */
    resolve: function(callback)
    {
        var result = new WebInspector.AgentLayerTree(this._target);
        result.setLayers(this._layers, callback.bind(null, result));
    },

    __proto__: WebInspector.DeferredLayerTree.prototype
};

/**
 * @constructor
 * @extends {WebInspector.DeferredLayerTree}
 * @param {!WebInspector.Target} target
 * @param {!WebInspector.TracingLayerPayload} root
 * @param {!Object} viewportSize
 */
WebInspector.DeferredTracingLayerTree = function(target, root, viewportSize)
{
    WebInspector.DeferredLayerTree.call(this, target);
    this._root = root;
    this._viewportSize = viewportSize;
}

WebInspector.DeferredTracingLayerTree.prototype = {
    /**
     * @param {function(!WebInspector.LayerTreeBase)} callback
     */
    resolve: function(callback)
    {
        var result = new WebInspector.TracingLayerTree(this._target);
        result.setViewportSize(this._viewportSize);
        result.setLayers(this._root, callback.bind(null, result));
    },

    __proto__: WebInspector.DeferredLayerTree.prototype
};

/**
 * @constructor
 * @implements {LayerTreeAgent.Dispatcher}
 * @param {!WebInspector.LayerTreeModel} layerTreeModel
 */
WebInspector.LayerTreeDispatcher = function(layerTreeModel)
{
    this._layerTreeModel = layerTreeModel;
}

WebInspector.LayerTreeDispatcher.prototype = {
    /**
     * @param {!Array.<!LayerTreeAgent.Layer>=} layers
     */
    layerTreeDidChange: function(layers)
    {
        this._layerTreeModel._layerTreeChanged(layers || null);
    },

    /**
     * @param {!LayerTreeAgent.LayerId} layerId
     * @param {!DOMAgent.Rect} clipRect
     */
    layerPainted: function(layerId, clipRect)
    {
        this._layerTreeModel._layerPainted(layerId, clipRect);
    }
}
