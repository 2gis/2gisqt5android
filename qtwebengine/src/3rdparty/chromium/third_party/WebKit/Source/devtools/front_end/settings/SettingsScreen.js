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

importScript("EditFileSystemDialog.js");

/**
 * @constructor
 * @param {!function()} onHide
 * @extends {WebInspector.HelpScreen}
 */
WebInspector.SettingsScreen = function(onHide)
{
    WebInspector.HelpScreen.call(this);
    this.element.id = "settings-screen";

    /** @type {function()} */
    this._onHide = onHide;

    this._tabbedPane = new WebInspector.TabbedPane();
    this._tabbedPane.element.classList.add("help-window-main");
    var settingsLabelElement = document.createElement("div");
    settingsLabelElement.className = "help-window-label";
    settingsLabelElement.createTextChild(WebInspector.UIString("Settings"));
    this._tabbedPane.element.insertBefore(settingsLabelElement, this._tabbedPane.element.firstChild);
    this._tabbedPane.element.appendChild(this._createCloseButton());
    this._tabbedPane.appendTab(WebInspector.SettingsScreen.Tabs.General, WebInspector.UIString("General"), new WebInspector.GenericSettingsTab());
    this._tabbedPane.appendTab(WebInspector.SettingsScreen.Tabs.Workspace, WebInspector.UIString("Workspace"), new WebInspector.WorkspaceSettingsTab());
    if (WebInspector.experimentsSettings.experimentsEnabled)
        this._tabbedPane.appendTab(WebInspector.SettingsScreen.Tabs.Experiments, WebInspector.UIString("Experiments"), new WebInspector.ExperimentsSettingsTab());
    this._tabbedPane.appendTab(WebInspector.SettingsScreen.Tabs.Shortcuts, WebInspector.UIString("Shortcuts"), WebInspector.shortcutsScreen.createShortcutsTabView());
    this._tabbedPane.shrinkableTabs = false;
    this._tabbedPane.verticalTabLayout = true;

    this._lastSelectedTabSetting = WebInspector.settings.createSetting("lastSelectedSettingsTab", WebInspector.SettingsScreen.Tabs.General);
    this.selectTab(this._lastSelectedTabSetting.get());
    this._tabbedPane.addEventListener(WebInspector.TabbedPane.EventTypes.TabSelected, this._tabSelected, this);
    this.element.addEventListener("keydown", this._keyDown.bind(this), false);
    this._developerModeCounter = 0;
}

/**
 * @param {number} min
 * @param {number} max
 * @param {string} text
 * @return {?string}
 */
WebInspector.SettingsScreen.integerValidator = function(min, max, text)
{
    var value = Number(text);
    if (isNaN(value))
        return WebInspector.UIString("Invalid number format");
    if (value < min || value > max)
        return WebInspector.UIString("Value is out of range [%d, %d]", min, max);
    return null;
}

WebInspector.SettingsScreen.Tabs = {
    General: "general",
    Overrides: "overrides",
    Workspace: "workspace",
    Experiments: "experiments",
    Shortcuts: "shortcuts"
}

WebInspector.SettingsScreen.prototype = {
    /**
     * @param {string} tabId
     */
    selectTab: function(tabId)
    {
        this._tabbedPane.selectTab(tabId);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _tabSelected: function(event)
    {
        this._lastSelectedTabSetting.set(this._tabbedPane.selectedTabId);
    },

    /**
     * @override
     */
    wasShown: function()
    {
        this._tabbedPane.show(this.element);
        WebInspector.HelpScreen.prototype.wasShown.call(this);
    },

    /**
     * @override
     * @return {boolean}
     */
    isClosingKey: function(keyCode)
    {
        return [
            WebInspector.KeyboardShortcut.Keys.Enter.code,
            WebInspector.KeyboardShortcut.Keys.Esc.code,
        ].indexOf(keyCode) >= 0;
    },

    /**
     * @override
     */
    willHide: function()
    {
        this._onHide();
        WebInspector.HelpScreen.prototype.willHide.call(this);
    },

    /**
     * @param {?Event} event
     */
    _keyDown: function(event)
    {
        var shiftKeyCode = 16;
        if (event.keyCode === shiftKeyCode && ++this._developerModeCounter > 5)
            this.element.classList.add("settings-developer-mode");
    },

    __proto__: WebInspector.HelpScreen.prototype
}

/**
 * @constructor
 * @extends {WebInspector.VBox}
 * @param {string} name
 * @param {string=} id
 */
WebInspector.SettingsTab = function(name, id)
{
    WebInspector.VBox.call(this);
    this.element.classList.add("settings-tab-container");
    if (id)
        this.element.id = id;
    var header = this.element.createChild("header");
    header.createChild("h3").appendChild(document.createTextNode(name));
    this.containerElement = this.element.createChild("div", "help-container-wrapper").createChild("div", "settings-tab help-content help-container");
}

WebInspector.SettingsTab.prototype = {
    /**
     *  @param {string=} name
     *  @return {!Element}
     */
    _appendSection: function(name)
    {
        var block = this.containerElement.createChild("div", "help-block");
        if (name)
            block.createChild("div", "help-section-title").textContent = name;
        return block;
    },

    _createSelectSetting: function(name, options, setting)
    {
        var p = document.createElement("p");
        var labelElement = p.createChild("label");
        labelElement.textContent = name;

        var select = p.createChild("select");
        var settingValue = setting.get();

        for (var i = 0; i < options.length; ++i) {
            var option = options[i];
            select.add(new Option(option[0], option[1]));
            if (settingValue === option[1])
                select.selectedIndex = i;
        }

        function changeListener(e)
        {
            // Don't use e.target.value to avoid conversion of the value to string.
            setting.set(options[select.selectedIndex][1]);
        }

        select.addEventListener("change", changeListener, false);
        return p;
    },

    __proto__: WebInspector.VBox.prototype
}

/**
 * @constructor
 * @extends {WebInspector.SettingsTab}
 */
WebInspector.GenericSettingsTab = function()
{
    WebInspector.SettingsTab.call(this, WebInspector.UIString("General"), "general-tab-content");

    this._populateSectionsFromExtensions();

    var restoreDefaults = this._appendSection().createChild("input", "settings-tab-text-button");
    restoreDefaults.type = "button";
    restoreDefaults.value = WebInspector.UIString("Restore defaults and reload");
    restoreDefaults.addEventListener("click", restoreAndReload, false);

    function restoreAndReload()
    {
        if (window.localStorage)
            window.localStorage.clear();
        WebInspector.reload();
    }
}

WebInspector.GenericSettingsTab.prototype = {
    _populateSectionsFromExtensions: function()
    {
        /** @const */
        var explicitSectionOrder = ["", "Appearance", "Elements", "Sources", "Profiler", "Console", "Extensions"];

        var allExtensions = WebInspector.moduleManager.extensions("ui-setting");

        /** @type {!StringMultimap.<!WebInspector.ModuleManager.Extension>} */
        var extensionsBySectionId = new StringMultimap();
        /** @type {!StringMultimap.<!WebInspector.ModuleManager.Extension>} */
        var childSettingExtensionsByParentName = new StringMultimap();

        allExtensions.forEach(function(extension) {
            var descriptor = extension.descriptor();
            var sectionName = descriptor["section"] || "";
            if (!sectionName && descriptor["parentSettingName"]) {
                childSettingExtensionsByParentName.put(descriptor["parentSettingName"], extension);
                return;
            }
            extensionsBySectionId.put(sectionName, extension);
        });

        var sectionIds = extensionsBySectionId.keys();
        var explicitlyOrderedSections = {};
        for (var i = 0; i < explicitSectionOrder.length; ++i) {
            explicitlyOrderedSections[explicitSectionOrder[i]] = true;
            var extensions = extensionsBySectionId.get(explicitSectionOrder[i]);
            if (!extensions.size())
                continue;
            this._addSectionWithExtensionProvidedSettings(explicitSectionOrder[i], extensions.values(), childSettingExtensionsByParentName);
        }
        for (var i = 0; i < sectionIds.length; ++i) {
            if (explicitlyOrderedSections[sectionIds[i]])
                continue;
            this._addSectionWithExtensionProvidedSettings(sectionIds[i], extensionsBySectionId.get(sectionIds[i]).values(), childSettingExtensionsByParentName);
        }
    },

    /**
     * @param {string} sectionName
     * @param {!Array.<!WebInspector.ModuleManager.Extension>} extensions
     * @param {!StringMultimap.<!WebInspector.ModuleManager.Extension>} childSettingExtensionsByParentName
     */
    _addSectionWithExtensionProvidedSettings: function(sectionName, extensions, childSettingExtensionsByParentName)
    {
        var uiSectionName = sectionName && WebInspector.UIString(sectionName);
        var sectionElement = this._appendSection(uiSectionName);
        extensions.forEach(processSetting.bind(this, null));

        /**
         * @param {?Element} parentFieldset
         * @param {!WebInspector.ModuleManager.Extension} extension
         * @this {WebInspector.GenericSettingsTab}
         */
        function processSetting(parentFieldset, extension)
        {
            var descriptor = extension.descriptor();
            var experimentName = descriptor["experiment"];
            if (experimentName && (!WebInspector.experimentsSettings[experimentName] || !WebInspector.experimentsSettings[experimentName].isEnabled()))
                return;

            var settingName = descriptor["settingName"];
            var setting = WebInspector.settings[settingName];
            var instance = extension.instance();
            var settingControl;
            if (instance && descriptor["settingType"] === "custom") {
                settingControl = instance.settingElement();
                if (!settingControl)
                    return;
            }
            if (!settingControl) {
                var uiTitle = WebInspector.UIString(descriptor["title"]);
                settingControl = createSettingControl.call(this, uiTitle, setting, descriptor, instance);
            }
            if (settingName) {
                var childSettings = childSettingExtensionsByParentName.get(settingName);
                if (childSettings.size()) {
                    var fieldSet = WebInspector.SettingsUI.createSettingFieldset(setting);
                    settingControl.appendChild(fieldSet);
                    childSettings.values().forEach(function(item) { processSetting.call(this, fieldSet, item); }, this);
                }
            }
            var containerElement = parentFieldset || sectionElement;
            containerElement.appendChild(settingControl);
        }

        /**
         * @param {string} uiTitle
         * @param {!WebInspector.Setting} setting
         * @param {!Object} descriptor
         * @param {?Object} instance
         * @return {!Element}
         * @this {WebInspector.GenericSettingsTab}
         */
        function createSettingControl(uiTitle, setting, descriptor, instance)
        {
            switch (descriptor["settingType"]) {
            case "checkbox":
                return WebInspector.SettingsUI.createSettingCheckbox(uiTitle, setting);
            case "select":
                var descriptorOptions = descriptor["options"]
                var options = new Array(descriptorOptions.length);
                for (var i = 0; i < options.length; ++i) {
                    // The third array item flags that the option name is "raw" (non-i18n-izable).
                    var optionName = descriptorOptions[i][2] ? descriptorOptions[i][0] : WebInspector.UIString(descriptorOptions[i][0]);
                    options[i] = [WebInspector.UIString(descriptorOptions[i][0]), descriptorOptions[i][1]];
                }
                return this._createSelectSetting(uiTitle, options, setting);
            default:
                throw "Invalid setting type: " + descriptor["settingType"];
            }
        }
    },

    /**
     * @param {?Element} p
     */
    _appendDrawerNote: function(p)
    {
        var noteElement = p.createChild("div", "help-field-note");
        noteElement.createTextChild("Hit ");
        noteElement.createChild("span", "help-key").textContent = "Esc";
        noteElement.createTextChild(WebInspector.UIString(" or click the"));
        noteElement.appendChild(new WebInspector.StatusBarButton(WebInspector.UIString("Drawer"), "console-status-bar-item").element);
        noteElement.createTextChild(WebInspector.UIString("toolbar item"));
    },

    __proto__: WebInspector.SettingsTab.prototype
}

/**
 * @constructor
 * @extends {WebInspector.SettingsTab}
 */
WebInspector.WorkspaceSettingsTab = function()
{
    WebInspector.SettingsTab.call(this, WebInspector.UIString("Workspace"), "workspace-tab-content");
    WebInspector.isolatedFileSystemManager.addEventListener(WebInspector.IsolatedFileSystemManager.Events.FileSystemAdded, this._fileSystemAdded, this);
    WebInspector.isolatedFileSystemManager.addEventListener(WebInspector.IsolatedFileSystemManager.Events.FileSystemRemoved, this._fileSystemRemoved, this);

    this._commonSection = this._appendSection(WebInspector.UIString("Common"));
    var folderExcludePatternInput = WebInspector.SettingsUI.createSettingInputField(WebInspector.UIString("Folder exclude pattern"), WebInspector.settings.workspaceFolderExcludePattern, false, 0, "270px", WebInspector.SettingsUI.regexValidator);
    this._commonSection.appendChild(folderExcludePatternInput);

    this._fileSystemsSection = this._appendSection(WebInspector.UIString("Folders"));
    this._fileSystemsListContainer = this._fileSystemsSection.createChild("p", "settings-list-container");

    this._addFileSystemRowElement = this._fileSystemsSection.createChild("div");
    var addFileSystemButton = this._addFileSystemRowElement.createChild("input", "settings-tab-text-button");
    addFileSystemButton.type = "button";
    addFileSystemButton.value = WebInspector.UIString("Add folder\u2026");
    addFileSystemButton.addEventListener("click", this._addFileSystemClicked.bind(this), false);

    this._editFileSystemButton = this._addFileSystemRowElement.createChild("input", "settings-tab-text-button");
    this._editFileSystemButton.type = "button";
    this._editFileSystemButton.value = WebInspector.UIString("Edit\u2026");
    this._editFileSystemButton.addEventListener("click", this._editFileSystemClicked.bind(this), false);
    this._updateEditFileSystemButtonState();

    this._reset();
}

WebInspector.WorkspaceSettingsTab.prototype = {
    wasShown: function()
    {
        WebInspector.SettingsTab.prototype.wasShown.call(this);
        this._reset();
    },

    _reset: function()
    {
        this._resetFileSystems();
    },

    _resetFileSystems: function()
    {
        this._fileSystemsListContainer.removeChildren();
        var fileSystemPaths = WebInspector.isolatedFileSystemManager.mapping().fileSystemPaths();
        delete this._fileSystemsList;

        if (!fileSystemPaths.length) {
            var noFileSystemsMessageElement = this._fileSystemsListContainer.createChild("div", "no-file-systems-message");
            noFileSystemsMessageElement.textContent = WebInspector.UIString("You have no file systems added.");
            return;
        }

        this._fileSystemsList = new WebInspector.SettingsList(["path"], this._renderFileSystem.bind(this));
        this._fileSystemsList.element.classList.add("file-systems-list");
        this._fileSystemsList.addEventListener(WebInspector.SettingsList.Events.Selected, this._fileSystemSelected.bind(this));
        this._fileSystemsList.addEventListener(WebInspector.SettingsList.Events.Removed, this._fileSystemRemovedfromList.bind(this));
        this._fileSystemsList.addEventListener(WebInspector.SettingsList.Events.DoubleClicked, this._fileSystemDoubleClicked.bind(this));
        this._fileSystemsListContainer.appendChild(this._fileSystemsList.element);
        for (var i = 0; i < fileSystemPaths.length; ++i)
            this._fileSystemsList.addItem(fileSystemPaths[i]);
        this._updateEditFileSystemButtonState();
    },

    _updateEditFileSystemButtonState: function()
    {
        this._editFileSystemButton.disabled = !this._selectedFileSystemPath();
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _fileSystemSelected: function(event)
    {
        this._updateEditFileSystemButtonState();
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _fileSystemDoubleClicked: function(event)
    {
        var id = /** @type{?string} */ (event.data);
        this._editFileSystem(id);
    },

    _editFileSystemClicked: function()
    {
        this._editFileSystem(this._selectedFileSystemPath());
    },

    /**
     * @param {?string} id
     */
    _editFileSystem: function(id)
    {
        WebInspector.EditFileSystemDialog.show(WebInspector.inspectorView.element, id);
    },

    /**
     * @param {function(?Event)} handler
     * @return {!Element}
     */
    _createRemoveButton: function(handler)
    {
        var removeButton = document.createElement("button");
        removeButton.classList.add("button");
        removeButton.classList.add("remove-item-button");
        removeButton.value = WebInspector.UIString("Remove");
        if (handler)
            removeButton.addEventListener("click", handler, false);
        else
            removeButton.disabled = true;
        return removeButton;
    },

    /**
     * @param {!Element} columnElement
     * @param {string} column
     * @param {?string} id
     */
    _renderFileSystem: function(columnElement, column, id)
    {
        if (!id)
            return "";
        var fileSystemPath = id;
        var textElement = columnElement.createChild("span", "list-column-text");
        var pathElement = textElement.createChild("span", "file-system-path");
        pathElement.title = fileSystemPath;

        const maxTotalPathLength = 55;
        const maxFolderNameLength = 30;

        var lastIndexOfSlash = fileSystemPath.lastIndexOf(WebInspector.isWin() ? "\\" : "/");
        var folderName = fileSystemPath.substr(lastIndexOfSlash + 1);
        var folderPath = fileSystemPath.substr(0, lastIndexOfSlash + 1);
        folderPath = folderPath.trimMiddle(maxTotalPathLength - Math.min(maxFolderNameLength, folderName.length));
        folderName = folderName.trimMiddle(maxFolderNameLength);

        var folderPathElement = pathElement.createChild("span");
        folderPathElement.textContent = folderPath;

        var nameElement = pathElement.createChild("span", "file-system-path-name");
        nameElement.textContent = folderName;
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _fileSystemRemovedfromList: function(event)
    {
        var id = /** @type{?string} */ (event.data);
        if (!id)
            return;
        WebInspector.isolatedFileSystemManager.removeFileSystem(id);
    },

    _addFileSystemClicked: function()
    {
        WebInspector.isolatedFileSystemManager.addFileSystem();
    },

    _fileSystemAdded: function(event)
    {
        var fileSystem = /** @type {!WebInspector.IsolatedFileSystem} */ (event.data);
        if (!this._fileSystemsList)
            this._reset();
        else
            this._fileSystemsList.addItem(fileSystem.path());
    },

    _fileSystemRemoved: function(event)
    {
        var fileSystem = /** @type {!WebInspector.IsolatedFileSystem} */ (event.data);
        var selectedFileSystemPath = this._selectedFileSystemPath();
        if (this._fileSystemsList.itemForId(fileSystem.path()))
            this._fileSystemsList.removeItem(fileSystem.path());
        if (!this._fileSystemsList.itemIds().length)
            this._reset();
        this._updateEditFileSystemButtonState();
    },

    _selectedFileSystemPath: function()
    {
        return this._fileSystemsList ? this._fileSystemsList.selectedId() : null;
    },

    __proto__: WebInspector.SettingsTab.prototype
}


/**
 * @constructor
 * @extends {WebInspector.SettingsTab}
 */
WebInspector.ExperimentsSettingsTab = function()
{
    WebInspector.SettingsTab.call(this, WebInspector.UIString("Experiments"), "experiments-tab-content");

    var experiments = WebInspector.experimentsSettings.experiments;
    if (experiments.length) {
        var experimentsSection = this._appendSection();
        experimentsSection.appendChild(this._createExperimentsWarningSubsection());
        for (var i = 0; i < experiments.length; ++i)
            experimentsSection.appendChild(this._createExperimentCheckbox(experiments[i]));
    }
}

WebInspector.ExperimentsSettingsTab.prototype = {
    /**
     * @return {!Element} element
     */
    _createExperimentsWarningSubsection: function()
    {
        var subsection = document.createElement("div");
        var warning = subsection.createChild("span", "settings-experiments-warning-subsection-warning");
        warning.textContent = WebInspector.UIString("WARNING:");
        subsection.appendChild(document.createTextNode(" "));
        var message = subsection.createChild("span", "settings-experiments-warning-subsection-message");
        message.textContent = WebInspector.UIString("These experiments could be dangerous and may require restart.");
        return subsection;
    },

    _createExperimentCheckbox: function(experiment)
    {
        var input = document.createElement("input");
        input.type = "checkbox";
        input.name = experiment.name;
        input.checked = experiment.isEnabled();
        function listener()
        {
            experiment.setEnabled(input.checked);
        }
        input.addEventListener("click", listener, false);

        var p = document.createElement("p");
        p.className = experiment.hidden && !experiment.isEnabled() ? "settings-experiment-hidden" : "";
        var label = p.createChild("label");
        label.appendChild(input);
        label.appendChild(document.createTextNode(WebInspector.UIString(experiment.title)));
        p.appendChild(label);
        return p;
    },

    __proto__: WebInspector.SettingsTab.prototype
}

/**
 * @constructor
 */
WebInspector.SettingsController = function()
{
    /** @type {?WebInspector.SettingsScreen} */
    this._settingsScreen;

    window.addEventListener("resize", this._resize.bind(this), true);
}

WebInspector.SettingsController.prototype =
{
    _onHideSettingsScreen: function()
    {
        delete this._settingsScreenVisible;
    },

    /**
     * @param {string=} tabId
     */
    showSettingsScreen: function(tabId)
    {
        if (!this._settingsScreen)
            this._settingsScreen = new WebInspector.SettingsScreen(this._onHideSettingsScreen.bind(this));

        if (tabId)
            this._settingsScreen.selectTab(tabId);

        this._settingsScreen.showModal();
        this._settingsScreenVisible = true;
    },

    _resize: function()
    {
        if (this._settingsScreen && this._settingsScreen.isShowing())
            this._settingsScreen.doResize();
    }
}

/**
 * @constructor
 * @implements {WebInspector.ActionDelegate}
 */
WebInspector.SettingsController.SettingsScreenActionDelegate = function() { }

WebInspector.SettingsController.SettingsScreenActionDelegate.prototype = {
    /**
     * @return {boolean}
     */
    handleAction: function()
    {
        WebInspector._settingsController.showSettingsScreen(WebInspector.SettingsScreen.Tabs.General);
        return true;
    }
}

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @param {!Array.<string>} columns
 * @param {function(!Element, string, ?string)} itemRenderer
 */
WebInspector.SettingsList = function(columns, itemRenderer)
{
    this.element = document.createElement("div");
    this.element.classList.add("settings-list");
    this.element.tabIndex = -1;
    this._itemRenderer = itemRenderer;
    this._listItems = {};
    this._ids = [];
    this._columns = columns;
}

WebInspector.SettingsList.Events = {
    Selected:  "Selected",
    Removed:  "Removed",
    DoubleClicked:  "DoubleClicked",
}

WebInspector.SettingsList.prototype = {
    /**
     * @param {?string} itemId
     * @param {?string=} beforeId
     * @return {!Element}
     */
    addItem: function(itemId, beforeId)
    {
        var listItem = document.createElement("div");
        listItem._id = itemId;
        listItem.classList.add("settings-list-item");
        if (typeof beforeId !== undefined)
            this.element.insertBefore(listItem, this._listItems[beforeId]);
        else
            this.element.appendChild(listItem);

        var listItemContents = listItem.createChild("div", "settings-list-item-contents");
        var listItemColumnsElement = listItemContents.createChild("div", "settings-list-item-columns");

        listItem.columnElements = {};
        for (var i = 0; i < this._columns.length; ++i) {
            var columnElement = listItemColumnsElement.createChild("div", "list-column");
            var columnId = this._columns[i];
            listItem.columnElements[columnId] = columnElement;
            this._itemRenderer(columnElement, columnId, itemId);
        }
        var removeItemButton = this._createRemoveButton(removeItemClicked.bind(this));
        listItemContents.addEventListener("click", this.selectItem.bind(this, itemId), false);
        listItemContents.addEventListener("dblclick", this._onDoubleClick.bind(this, itemId), false);
        listItemContents.appendChild(removeItemButton);

        this._listItems[itemId] = listItem;
        if (typeof beforeId !== undefined)
            this._ids.splice(this._ids.indexOf(beforeId), 0, itemId);
        else
            this._ids.push(itemId);

        /**
         * @param {?Event} event
         * @this {WebInspector.SettingsList}
         */
        function removeItemClicked(event)
        {
            removeItemButton.disabled = true;
            this.removeItem(itemId);
            this.dispatchEventToListeners(WebInspector.SettingsList.Events.Removed, itemId);
            event.consume();
        }

        return listItem;
    },

    /**
     * @param {?string} id
     */
    removeItem: function(id)
    {
        this._listItems[id].remove();
        delete this._listItems[id];
        this._ids.remove(id);
        if (id === this._selectedId) {
            delete this._selectedId;
            if (this._ids.length)
                this.selectItem(this._ids[0]);
        }
    },

    /**
     * @return {!Array.<?string>}
     */
    itemIds: function()
    {
        return this._ids.slice();
    },

    /**
     * @return {!Array.<string>}
     */
    columns: function()
    {
        return this._columns.slice();
    },

    /**
     * @return {?string}
     */
    selectedId: function()
    {
        return this._selectedId;
    },

    /**
     * @return {!Element}
     */
    selectedItem: function()
    {
        return this._selectedId ? this._listItems[this._selectedId] : null;
    },

    /**
     * @param {string} itemId
     * @return {!Element}
     */
    itemForId: function(itemId)
    {
        return this._listItems[itemId];
    },

    /**
     * @param {?string} id
     * @param {?Event=} event
     */
    _onDoubleClick: function(id, event)
    {
        this.dispatchEventToListeners(WebInspector.SettingsList.Events.DoubleClicked, id);
    },

    /**
     * @param {?string} id
     * @param {?Event=} event
     */
    selectItem: function(id, event)
    {
        if (typeof this._selectedId !== "undefined") {
            this._listItems[this._selectedId].classList.remove("selected");
        }

        this._selectedId = id;
        if (typeof this._selectedId !== "undefined") {
            this._listItems[this._selectedId].classList.add("selected");
        }
        this.dispatchEventToListeners(WebInspector.SettingsList.Events.Selected, id);
        if (event)
            event.consume();
    },

    /**
     * @param {function(?Event)} handler
     * @return {!Element}
     */
    _createRemoveButton: function(handler)
    {
        var removeButton = document.createElement("button");
        removeButton.classList.add("remove-item-button");
        removeButton.value = WebInspector.UIString("Remove");
        removeButton.addEventListener("click", handler, false);
        return removeButton;
    },

    __proto__: WebInspector.Object.prototype
}

/**
 * @constructor
 * @extends {WebInspector.SettingsList}
 * @param {!Array.<string>} columns
 * @param {function(string, string):string} valuesProvider
 * @param {function(?string, !Object)} validateHandler
 * @param {function(?string, !Object)} editHandler
 */
WebInspector.EditableSettingsList = function(columns, valuesProvider, validateHandler, editHandler)
{
    WebInspector.SettingsList.call(this, columns, this._renderColumn.bind(this));
    this._validateHandler = validateHandler;
    this._editHandler = editHandler;
    this._valuesProvider = valuesProvider;
    /** @type {!Object.<string, !HTMLInputElement>} */
    this._addInputElements = {};
    /** @type {!Object.<string, !Object.<string, !HTMLInputElement>>} */
    this._editInputElements = {};
    /** @type {!Object.<string, !Object.<string, !HTMLSpanElement>>} */
    this._textElements = {};

    this._addMappingItem = this.addItem(null);
    this._addMappingItem.classList.add("item-editing");
    this._addMappingItem.classList.add("add-list-item");
}

WebInspector.EditableSettingsList.prototype = {
    /**
     * @param {?string} itemId
     * @param {?string=} beforeId
     * @return {!Element}
     */
    addItem: function(itemId, beforeId)
    {
        var listItem = WebInspector.SettingsList.prototype.addItem.call(this, itemId, beforeId);
        listItem.classList.add("editable");
        return listItem;
    },

    /**
     * @param {!Element} columnElement
     * @param {string} columnId
     * @param {?string} itemId
     */
    _renderColumn: function(columnElement, columnId, itemId)
    {
        columnElement.classList.add("settings-list-column-" + columnId);
        var placeholder = (columnId === "url") ? WebInspector.UIString("URL prefix") : WebInspector.UIString("Folder path");
        if (itemId === null) {
            var inputElement = /** @type {!HTMLInputElement} */ (columnElement.createChild("input", "list-column-editor"));
            inputElement.placeholder = placeholder;
            inputElement.addEventListener("blur", this._onAddMappingInputBlur.bind(this), false);
            inputElement.addEventListener("input", this._validateEdit.bind(this, itemId), false);
            this._addInputElements[columnId] = inputElement;
            return;
        }
        var validItemId = itemId;

        if (!this._editInputElements[itemId])
            this._editInputElements[itemId] = {};
        if (!this._textElements[itemId])
            this._textElements[itemId] = {};

        var value = this._valuesProvider(itemId, columnId);

        var textElement = /** @type {!HTMLSpanElement} */ (columnElement.createChild("span", "list-column-text"));
        textElement.textContent = value;
        textElement.title = value;
        columnElement.addEventListener("click", rowClicked.bind(this), false);
        this._textElements[itemId][columnId] = textElement;

        var inputElement = /** @type {!HTMLInputElement} */ (columnElement.createChild("input", "list-column-editor"));
        inputElement.value = value;
        inputElement.addEventListener("blur", this._editMappingBlur.bind(this, itemId), false);
        inputElement.addEventListener("input", this._validateEdit.bind(this, itemId), false);
        columnElement.inputElement = inputElement;
        this._editInputElements[itemId][columnId] = inputElement;

        /**
         * @param {?Event} event
         * @this {WebInspector.EditableSettingsList}
         */
        function rowClicked(event)
        {
            if (itemId === this._editingId)
                return;
            event.consume();
            console.assert(!this._editingId);
            this._editingId = validItemId;
            var listItem = this.itemForId(validItemId);
            listItem.classList.add("item-editing");
            var inputElement = event.target.inputElement || this._editInputElements[validItemId][this.columns()[0]];
            inputElement.focus();
            inputElement.select();
        }
    },

    /**
     * @param {?string} itemId
     * @return {!Object}
     */
    _data: function(itemId)
    {
        var inputElements = this._inputElements(itemId);
        var data = {};
        var columns = this.columns();
        for (var i = 0; i < columns.length; ++i)
            data[columns[i]] = inputElements[columns[i]].value;
        return data;
    },

    /**
     * @param {?string} itemId
     * @return {?Object.<string, !HTMLInputElement>}
     */
    _inputElements: function(itemId)
    {
        if (!itemId)
            return this._addInputElements;
        return this._editInputElements[itemId] || null;
    },

    /**
     * @param {?string} itemId
     * @return {boolean}
     */
    _validateEdit: function(itemId)
    {
        var errorColumns = this._validateHandler(itemId, this._data(itemId));
        var hasChanges = this._hasChanges(itemId);
        var columns = this.columns();
        for (var i = 0; i < columns.length; ++i) {
            var columnId = columns[i];
            var inputElement = this._inputElements(itemId)[columnId];
            if (hasChanges && errorColumns.indexOf(columnId) !== -1)
                inputElement.classList.add("editable-item-error");
            else
                inputElement.classList.remove("editable-item-error");
        }
        return !errorColumns.length;
    },

    /**
     * @param {?string} itemId
     * @return {boolean}
     */
    _hasChanges: function(itemId)
    {
        var hasChanges = false;
        var columns = this.columns();
        for (var i = 0; i < columns.length; ++i) {
            var columnId = columns[i];
            var oldValue = itemId ? this._textElements[itemId][columnId].textContent : "";
            var newValue = this._inputElements(itemId)[columnId].value;
            if (oldValue !== newValue) {
                hasChanges = true;
                break;
            }
        }
        return hasChanges;
    },

    /**
     * @param {string} itemId
     * @param {?Event} event
     */
    _editMappingBlur: function(itemId, event)
    {
        var inputElements = Object.values(this._editInputElements[itemId]);
        if (inputElements.indexOf(event.relatedTarget) !== -1)
            return;

        var listItem = this.itemForId(itemId);
        listItem.classList.remove("item-editing");
        delete this._editingId;

        if (!this._hasChanges(itemId))
            return;

        if (!this._validateEdit(itemId)) {
            var columns = this.columns();
            for (var i = 0; i < columns.length; ++i) {
                var columnId = columns[i];
                var inputElement = this._editInputElements[itemId][columnId];
                inputElement.value = this._textElements[itemId][columnId].textContent;
                inputElement.classList.remove("editable-item-error");
            }
            return;
        }
        this._editHandler(itemId, this._data(itemId));
    },

    _onAddMappingInputBlur: function(event)
    {
        var inputElements = Object.values(this._addInputElements);
        if (inputElements.indexOf(event.relatedTarget) !== -1)
            return;

        if (!this._hasChanges(null))
            return;

        if (!this._validateEdit(null))
            return;

        this._editHandler(null, this._data(null));
        var columns = this.columns();
        for (var i = 0; i < columns.length; ++i) {
            var columnId = columns[i];
            var inputElement = this._addInputElements[columnId];
            inputElement.value = "";
        }
    },

    __proto__: WebInspector.SettingsList.prototype
}

WebInspector._settingsController = new WebInspector.SettingsController();
