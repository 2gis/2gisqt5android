// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../uber/uber_utils.js">
<include src="extension_code.js">
<include src="extension_commands_overlay.js">
<include src="extension_error_overlay.js">
<include src="extension_focus_manager.js">
<include src="extension_list.js">
<include src="pack_extension_overlay.js">
<include src="extension_loader.js">
<include src="extension_options_overlay.js">

<if expr="chromeos">
<include src="chromeos/kiosk_apps.js">
</if>

/**
 * The type of the extension data object. The definition is based on
 * chrome/browser/ui/webui/extensions/extension_settings_handler.cc:
 *     ExtensionSettingsHandler::HandleRequestExtensionsData()
 * @typedef {{developerMode: boolean,
 *            extensions: Array,
 *            incognitoAvailable: boolean,
 *            loadUnpackedDisabled: boolean,
 *            profileIsSupervised: boolean,
 *            promoteAppsDevTools: boolean}}
 */
var ExtensionDataResponse;

// Used for observing function of the backend datasource for this page by
// tests.
var webuiResponded = false;

cr.define('extensions', function() {
  var ExtensionsList = options.ExtensionsList;

  // Implements the DragWrapper handler interface.
  var dragWrapperHandler = {
    /** @override */
    shouldAcceptDrag: function(e) {
      // We can't access filenames during the 'dragenter' event, so we have to
      // wait until 'drop' to decide whether to do something with the file or
      // not.
      // See: http://www.w3.org/TR/2011/WD-html5-20110113/dnd.html#concept-dnd-p
      return (e.dataTransfer.types &&
              e.dataTransfer.types.indexOf('Files') > -1);
    },
    /** @override */
    doDragEnter: function() {
      chrome.send('startDrag');
      ExtensionSettings.showOverlay(null);
      ExtensionSettings.showOverlay($('drop-target-overlay'));
    },
    /** @override */
    doDragLeave: function() {
      this.hideDropTargetOverlay_();
      chrome.send('stopDrag');
    },
    /** @override */
    doDragOver: function(e) {
      e.preventDefault();
    },
    /** @override */
    doDrop: function(e) {
      this.hideDropTargetOverlay_();
      if (e.dataTransfer.files.length != 1)
        return;

      var toSend = null;
      // Files lack a check if they're a directory, but we can find out through
      // its item entry.
      for (var i = 0; i < e.dataTransfer.items.length; ++i) {
        if (e.dataTransfer.items[i].kind == 'file' &&
            e.dataTransfer.items[i].webkitGetAsEntry().isDirectory) {
          toSend = 'installDroppedDirectory';
          break;
        }
      }
      // Only process files that look like extensions. Other files should
      // navigate the browser normally.
      if (!toSend &&
          /\.(crx|user\.js|zip)$/i.test(e.dataTransfer.files[0].name)) {
        toSend = 'installDroppedFile';
      }

      if (toSend) {
        e.preventDefault();
        chrome.send(toSend);
      }
    },

    /**
     * Hide the current overlay if it is the drop target overlay.
     * @private
     */
    hideDropTargetOverlay_: function() {
      var currentOverlay = ExtensionSettings.getCurrentOverlay();
      if (currentOverlay && currentOverlay.id === 'drop-target-overlay')
        ExtensionSettings.showOverlay(null);
    }
  };

  /**
   * ExtensionSettings class
   * @class
   */
  function ExtensionSettings() {}

  cr.addSingletonGetter(ExtensionSettings);

  ExtensionSettings.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Whether or not to try to display the Apps Developer Tools promotion.
     * @type {boolean}
     * @private
     */
    displayPromo_: false,

    /**
     * Perform initial setup.
     */
    initialize: function() {
      uber.onContentFrameLoaded();
      cr.ui.FocusOutlineManager.forDocument(document);
      measureCheckboxStrings();

      // Set the title.
      uber.setTitle(loadTimeData.getString('extensionSettings'));

      // This will request the data to show on the page and will get a response
      // back in returnExtensionsData.
      chrome.send('extensionSettingsRequestExtensionsData');

      var extensionLoader = extensions.ExtensionLoader.getInstance();

      $('toggle-dev-on').addEventListener('change',
          this.handleToggleDevMode_.bind(this));
      $('dev-controls').addEventListener('webkitTransitionEnd',
          this.handleDevControlsTransitionEnd_.bind(this));

      // Set up the three dev mode buttons (load unpacked, pack and update).
      $('load-unpacked').addEventListener('click', function(e) {
          extensionLoader.loadUnpacked();
      });
      $('pack-extension').addEventListener('click',
          this.handlePackExtension_.bind(this));
      $('update-extensions-now').addEventListener('click',
          this.handleUpdateExtensionNow_.bind(this));

      // Set up the close dialog for the apps developer tools promo.
      $('apps-developer-tools-promo').querySelector('.close-button').
          addEventListener('click', function(e) {
        this.displayPromo_ = false;
        this.updatePromoVisibility_();
        chrome.send('extensionSettingsDismissADTPromo');
      }.bind(this));

      if (!loadTimeData.getBoolean('offStoreInstallEnabled')) {
        this.dragWrapper_ = new cr.ui.DragWrapper(document.documentElement,
                                                  dragWrapperHandler);
      }

      extensions.PackExtensionOverlay.getInstance().initializePage();

      // Hook up the configure commands link to the overlay.
      var link = document.querySelector('.extension-commands-config');
      link.addEventListener('click',
          this.handleExtensionCommandsConfig_.bind(this));

      // Initialize the Commands overlay.
      extensions.ExtensionCommandsOverlay.getInstance().initializePage();

      extensions.ExtensionErrorOverlay.getInstance().initializePage(
          extensions.ExtensionSettings.showOverlay);

      extensions.ExtensionOptionsOverlay.getInstance().initializePage(
          extensions.ExtensionSettings.showOverlay);

      // Initialize the kiosk overlay.
      if (cr.isChromeOS) {
        var kioskOverlay = extensions.KioskAppsOverlay.getInstance();
        kioskOverlay.initialize();

        $('add-kiosk-app').addEventListener('click', function() {
          ExtensionSettings.showOverlay($('kiosk-apps-page'));
          kioskOverlay.didShowPage();
        });

        extensions.KioskDisableBailoutConfirm.getInstance().initialize();
      }

      cr.ui.overlay.setupOverlay($('drop-target-overlay'));
      cr.ui.overlay.globalInitialization();

      extensions.ExtensionFocusManager.getInstance().initialize();

      var path = document.location.pathname;
      if (path.length > 1) {
        // Skip starting slash and remove trailing slash (if any).
        var overlayName = path.slice(1).replace(/\/$/, '');
        if (overlayName == 'configureCommands')
          this.showExtensionCommandsConfigUi_();
      }
    },

    /**
     * Updates the Chrome Apps and Extensions Developer Tools promotion's
     * visibility.
     * @private
     */
    updatePromoVisibility_: function() {
      var extensionSettings = $('extension-settings');
      var visible = extensionSettings.classList.contains('dev-mode') &&
                    this.displayPromo_;

      var adtPromo = $('apps-developer-tools-promo');
      var controls = adtPromo.querySelectorAll('a, button');
      Array.prototype.forEach.call(controls, function(control) {
        control[visible ? 'removeAttribute' : 'setAttribute']('tabindex', '-1');
      });

      adtPromo.setAttribute('aria-hidden', !visible);
      extensionSettings.classList.toggle('adt-promo', visible);
    },

    /**
     * Handles the Pack Extension button.
     * @param {Event} e Change event.
     * @private
     */
    handlePackExtension_: function(e) {
      ExtensionSettings.showOverlay($('pack-extension-overlay'));
      chrome.send('metricsHandler:recordAction', ['Options_PackExtension']);
    },

    /**
     * Shows the Extension Commands configuration UI.
     * @param {Event} e Change event.
     * @private
     */
    showExtensionCommandsConfigUi_: function(e) {
      ExtensionSettings.showOverlay($('extension-commands-overlay'));
      chrome.send('metricsHandler:recordAction',
                  ['Options_ExtensionCommands']);
    },

    /**
     * Handles the Configure (Extension) Commands link.
     * @param {Event} e Change event.
     * @private
     */
    handleExtensionCommandsConfig_: function(e) {
      this.showExtensionCommandsConfigUi_();
    },

    /**
     * Handles the Update Extension Now button.
     * @param {Event} e Change event.
     * @private
     */
    handleUpdateExtensionNow_: function(e) {
      chrome.send('extensionSettingsAutoupdate');
    },

    /**
     * Handles the Toggle Dev Mode button.
     * @param {Event} e Change event.
     * @private
     */
    handleToggleDevMode_: function(e) {
      if ($('toggle-dev-on').checked) {
        $('dev-controls').hidden = false;
        window.setTimeout(function() {
          $('extension-settings').classList.add('dev-mode');
        }, 0);
      } else {
        $('extension-settings').classList.remove('dev-mode');
      }
      window.setTimeout(this.updatePromoVisibility_.bind(this), 0);

      chrome.send('extensionSettingsToggleDeveloperMode');
    },

    /**
     * Called when a transition has ended for #dev-controls.
     * @param {Event} e webkitTransitionEnd event.
     * @private
     */
    handleDevControlsTransitionEnd_: function(e) {
      if (e.propertyName == 'height' &&
          !$('extension-settings').classList.contains('dev-mode')) {
        $('dev-controls').hidden = true;
      }
    },
  };

  /**
   * Called by the dom_ui_ to re-populate the page with data representing
   * the current state of installed extensions.
   * @param {ExtensionDataResponse} extensionsData
   */
  ExtensionSettings.returnExtensionsData = function(extensionsData) {
    // We can get called many times in short order, thus we need to
    // be careful to remove the 'finished loading' timeout.
    if (this.loadingTimeout_)
      window.clearTimeout(this.loadingTimeout_);
    document.documentElement.classList.add('loading');
    this.loadingTimeout_ = window.setTimeout(function() {
      document.documentElement.classList.remove('loading');
    }, 0);

    webuiResponded = true;

    if (extensionsData.extensions.length > 0) {
      // Enforce order specified in the data or (if equal) then sort by
      // extension name (case-insensitive) followed by their ID (in the case
      // where extensions have the same name).
      extensionsData.extensions.sort(function(a, b) {
        function compare(x, y) {
          return x < y ? -1 : (x > y ? 1 : 0);
        }
        return compare(a.order, b.order) ||
               compare(a.name.toLowerCase(), b.name.toLowerCase()) ||
               compare(a.id, b.id);
      });
    }

    var pageDiv = $('extension-settings');
    var marginTop = 0;
    if (extensionsData.profileIsSupervised) {
      pageDiv.classList.add('profile-is-supervised');
    } else {
      pageDiv.classList.remove('profile-is-supervised');
    }
    if (extensionsData.profileIsSupervised) {
      pageDiv.classList.add('showing-banner');
      $('toggle-dev-on').disabled = true;
      marginTop += 45;
    } else {
      pageDiv.classList.remove('showing-banner');
      $('toggle-dev-on').disabled = false;
    }

    pageDiv.style.marginTop = marginTop + 'px';

    if (extensionsData.developerMode) {
      pageDiv.classList.add('dev-mode');
      $('toggle-dev-on').checked = true;
      $('dev-controls').hidden = false;
    } else {
      pageDiv.classList.remove('dev-mode');
      $('toggle-dev-on').checked = false;
    }

    ExtensionSettings.getInstance().displayPromo_ =
        extensionsData.promoteAppsDevTools;
    ExtensionSettings.getInstance().updatePromoVisibility_();

    $('load-unpacked').disabled = extensionsData.loadUnpackedDisabled;

    ExtensionsList.prototype.data_ = extensionsData;
    var extensionList = $('extension-settings-list');
    ExtensionsList.decorate(extensionList);
  };

  // Indicate that warning |message| has occured for pack of |crx_path| and
  // |pem_path| files.  Ask if user wants override the warning.  Send
  // |overrideFlags| to repeated 'pack' call to accomplish the override.
  ExtensionSettings.askToOverrideWarning =
      function(message, crx_path, pem_path, overrideFlags) {
    var closeAlert = function() {
      ExtensionSettings.showOverlay(null);
    };

    alertOverlay.setValues(
        loadTimeData.getString('packExtensionWarningTitle'),
        message,
        loadTimeData.getString('packExtensionProceedAnyway'),
        loadTimeData.getString('cancel'),
        function() {
          chrome.send('pack', [crx_path, pem_path, overrideFlags]);
          closeAlert();
        },
        closeAlert);
    ExtensionSettings.showOverlay($('alertOverlay'));
  };

  /**
   * Returns the current overlay or null if one does not exist.
   * @return {Element} The overlay element.
   */
  ExtensionSettings.getCurrentOverlay = function() {
    return document.querySelector('#overlay .page.showing');
  };

  /**
   * Sets the given overlay to show. This hides whatever overlay is currently
   * showing, if any.
   * @param {HTMLElement} node The overlay page to show. If falsey, all overlays
   *     are hidden.
   */
  ExtensionSettings.showOverlay = function(node) {
    var pageDiv = $('extension-settings');
    if (node) {
      pageDiv.style.width = window.getComputedStyle(pageDiv).width;
      document.body.classList.add('no-scroll');
    } else {
      document.body.classList.remove('no-scroll');
      pageDiv.style.width = '';
    }

    var currentlyShowingOverlay = ExtensionSettings.getCurrentOverlay();
    if (currentlyShowingOverlay)
      currentlyShowingOverlay.classList.remove('showing');

    if (node)
      node.classList.add('showing');

    var pages = document.querySelectorAll('.page');
    for (var i = 0; i < pages.length; i++) {
      pages[i].setAttribute('aria-hidden', node ? 'true' : 'false');
    }

    $('overlay').hidden = !node;
    uber.invokeMethodOnParent(node ? 'beginInterceptingEvents' :
                                     'stopInterceptingEvents');
  };

  /**
   * Utility function to find the width of various UI strings and synchronize
   * the width of relevant spans. This is crucial for making sure the
   * Enable/Enabled checkboxes align, as well as the Developer Mode checkbox.
   */
  function measureCheckboxStrings() {
    var trashWidth = 30;
    var measuringDiv = $('font-measuring-div');
    measuringDiv.textContent =
        loadTimeData.getString('extensionSettingsEnabled');
    measuringDiv.className = 'enabled-text';
    var pxWidth = measuringDiv.clientWidth + trashWidth;
    measuringDiv.textContent =
        loadTimeData.getString('extensionSettingsEnable');
    measuringDiv.className = 'enable-text';
    pxWidth = Math.max(measuringDiv.clientWidth + trashWidth, pxWidth);
    measuringDiv.textContent =
        loadTimeData.getString('extensionSettingsDeveloperMode');
    measuringDiv.className = '';
    pxWidth = Math.max(measuringDiv.clientWidth, pxWidth);

    var style = document.createElement('style');
    style.type = 'text/css';
    style.textContent =
        '.enable-checkbox-text {' +
        '  min-width: ' + (pxWidth - trashWidth) + 'px;' +
        '}' +
        '#dev-toggle span {' +
        '  min-width: ' + pxWidth + 'px;' +
        '}';
    document.querySelector('head').appendChild(style);
  };

  // Export
  return {
    ExtensionSettings: ExtensionSettings
  };
});

window.addEventListener('load', function(e) {
  extensions.ExtensionSettings.getInstance().initialize();
});
