// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @return {number} Width of a scrollbar in pixels
 */
function getScrollbarWidth() {
  var div = document.createElement('div');
  div.style.visibility = 'hidden';
  div.style.overflow = 'scroll';
  div.style.width = '50px';
  div.style.height = '50px';
  div.style.position = 'absolute';
  document.body.appendChild(div);
  var result = div.offsetWidth - div.clientWidth;
  div.parentNode.removeChild(div);
  return result;
}

/**
 * The minimum number of pixels to offset the toolbar by from the bottom and
 * right side of the screen.
 */
PDFViewer.MIN_TOOLBAR_OFFSET = 15;

/**
 * Creates a new PDFViewer. There should only be one of these objects per
 * document.
 * @param {Object} streamDetails The stream object which points to the data
 *     contained in the PDF.
 */
function PDFViewer(streamDetails) {
  this.streamDetails = streamDetails;
  this.loaded = false;

  // The sizer element is placed behind the plugin element to cause scrollbars
  // to be displayed in the window. It is sized according to the document size
  // of the pdf and zoom level.
  this.sizer_ = $('sizer');
  this.toolbar_ = $('toolbar');
  this.pageIndicator_ = $('page-indicator');
  this.progressBar_ = $('progress-bar');
  this.passwordScreen_ = $('password-screen');
  this.passwordScreen_.addEventListener('password-submitted',
                                        this.onPasswordSubmitted_.bind(this));
  this.errorScreen_ = $('error-screen');

  // Create the viewport.
  this.viewport_ = new Viewport(window,
                                this.sizer_,
                                this.viewportChanged_.bind(this),
                                this.beforeZoom_.bind(this),
                                this.afterZoom_.bind(this),
                                getScrollbarWidth());

  // Create the plugin object dynamically so we can set its src. The plugin
  // element is sized to fill the entire window and is set to be fixed
  // positioning, acting as a viewport. The plugin renders into this viewport
  // according to the scroll position of the window.
  this.plugin_ = document.createElement('object');
  // NOTE: The plugin's 'id' field must be set to 'plugin' since
  // chrome/renderer/printing/print_web_view_helper.cc actually references it.
  this.plugin_.id = 'plugin';
  this.plugin_.type = 'application/x-google-chrome-pdf';
  this.plugin_.addEventListener('message', this.handlePluginMessage_.bind(this),
                                false);

  // Handle scripting messages from outside the extension that wish to interact
  // with it. We also send a message indicating that extension has loaded and
  // is ready to receive messages.
  window.addEventListener('message', this.handleScriptingMessage_.bind(this),
                          false);
  this.sendScriptingMessage_({type: 'readyToReceive'});

  this.plugin_.setAttribute('src', this.streamDetails.originalUrl);
  this.plugin_.setAttribute('stream-url', this.streamDetails.streamUrl);
  var headers = '';
  for (var header in this.streamDetails.responseHeaders) {
    headers += header + ': ' +
        this.streamDetails.responseHeaders[header] + '\n';
  }
  this.plugin_.setAttribute('headers', headers);

  if (window.top == window)
    this.plugin_.setAttribute('full-frame', '');
  document.body.appendChild(this.plugin_);

  // TODO(raymes): Remove this spurious message once crbug.com/388606 is fixed.
  // This is a hack to initialize pepper sync scripting and avoid re-entrancy.
  this.plugin_.postMessage({
    type: 'viewport',
    zoom: 1,
    xOffset: 0,
    yOffset: 0
  });

  // Setup the button event listeners.
  $('fit-to-width-button').addEventListener('click',
      this.viewport_.fitToWidth.bind(this.viewport_));
  $('fit-to-page-button').addEventListener('click',
      this.viewport_.fitToPage.bind(this.viewport_));
  $('zoom-in-button').addEventListener('click',
      this.viewport_.zoomIn.bind(this.viewport_));
  $('zoom-out-button').addEventListener('click',
      this.viewport_.zoomOut.bind(this.viewport_));
  $('save-button-link').href = this.streamDetails.originalUrl;
  $('print-button').addEventListener('click', this.print_.bind(this));

  // Setup the keyboard event listener.
  document.onkeydown = this.handleKeyEvent_.bind(this);

  // Set up the zoom API.
  if (chrome.tabs) {
    chrome.tabs.setZoomSettings({mode: 'manual', scope: 'per-tab'},
                                this.afterZoom_.bind(this));
    chrome.tabs.onZoomChange.addListener(function(zoomChangeInfo) {
      // If the zoom level is close enough to the current zoom level, don't
      // change it. This avoids us getting into an infinite loop of zoom changes
      // due to floating point error.
      var MIN_ZOOM_DELTA = 0.01;
      var zoomDelta = Math.abs(this.viewport_.zoom -
                               zoomChangeInfo.newZoomFactor);
      // We should not change zoom level when we are responsible for initiating
      // the zoom. onZoomChange() is called before setZoomComplete() callback
      // when we initiate the zoom.
      if ((zoomDelta > MIN_ZOOM_DELTA) && !this.setZoomInProgress_)
        this.viewport_.setZoom(zoomChangeInfo.newZoomFactor);
    }.bind(this));
  }

  // Parse open pdf parameters.
  var paramsParser = new OpenPDFParamsParser(this.streamDetails.originalUrl);
  this.urlParams_ = paramsParser.urlParams;
}

PDFViewer.prototype = {
  /**
   * @private
   * Handle key events. These may come from the user directly or via the
   * scripting API.
   * @param {KeyboardEvent} e the event to handle.
   */
  handleKeyEvent_: function(e) {
    var position = this.viewport_.position;
    // Certain scroll events may be sent from outside of the extension.
    var fromScriptingAPI = e.type == 'scriptingKeypress';

    var pageUpHandler = function() {
      // Go to the previous page if we are fit-to-page.
      if (this.viewport_.fittingType == Viewport.FittingType.FIT_TO_PAGE) {
        this.viewport_.goToPage(this.viewport_.getMostVisiblePage() - 1);
        // Since we do the movement of the page.
        e.preventDefault();
      } else if (fromScriptingAPI) {
        position.y -= this.viewport.size.height;
        this.viewport.position = position;
      }
    }.bind(this);
    var pageDownHandler = function() {
      // Go to the next page if we are fit-to-page.
      if (this.viewport_.fittingType == Viewport.FittingType.FIT_TO_PAGE) {
        this.viewport_.goToPage(this.viewport_.getMostVisiblePage() + 1);
        // Since we do the movement of the page.
        e.preventDefault();
      } else if (fromScriptingAPI) {
        position.y += this.viewport.size.height;
        this.viewport.position = position;
      }
    }.bind(this);

    switch (e.keyCode) {
      case 32:  // Space key.
        if (e.shiftKey)
          pageUpHandler();
        else
          pageDownHandler();
        return;
      case 33:  // Page up key.
        pageUpHandler();
        return;
      case 34:  // Page down key.
        pageDownHandler();
        return;
      case 37:  // Left arrow key.
        // Go to the previous page if there are no horizontal scrollbars.
        if (!this.viewport_.documentHasScrollbars().x) {
          this.viewport_.goToPage(this.viewport_.getMostVisiblePage() - 1);
          // Since we do the movement of the page.
          e.preventDefault();
        } else if (fromScriptingAPI) {
          position.x -= Viewport.SCROLL_INCREMENT;
          this.viewport.position = position;
        }
        return;
      case 38:  // Up arrow key.
        if (fromScriptingAPI) {
          position.y -= Viewport.SCROLL_INCREMENT;
          this.viewport.position = position;
        }
        return;
      case 39:  // Right arrow key.
        // Go to the next page if there are no horizontal scrollbars.
        if (!this.viewport_.documentHasScrollbars().x) {
          this.viewport_.goToPage(this.viewport_.getMostVisiblePage() + 1);
          // Since we do the movement of the page.
          e.preventDefault();
        } else if (fromScriptingAPI) {
          position.x += Viewport.SCROLL_INCREMENT;
          this.viewport.position = position;
        }
        return;
      case 40:  // Down arrow key.
        if (fromScriptingAPI) {
          position.y += Viewport.SCROLL_INCREMENT;
          this.viewport.position = position;
        }
        return;
      case 65:  // a key.
        if (e.ctrlKey || e.metaKey) {
          this.plugin_.postMessage({
            type: 'selectAll',
          });
        }
        return;
      case 83:  // s key.
        if (e.ctrlKey || e.metaKey) {
          // Simulate a click on the button so that the <a download ...>
          // attribute is used.
          $('save-button-link').click();
          // Since we do the saving of the page.
          e.preventDefault();
        }
        return;
      case 80:  // p key.
        if (e.ctrlKey || e.metaKey) {
          this.print_();
          // Since we do the printing of the page.
          e.preventDefault();
        }
        return;
      case 219:  // left bracket.
        if (e.ctrlKey) {
          this.plugin_.postMessage({
            type: 'rotateCounterclockwise',
          });
        }
        return;
      case 221:  // right bracket.
        if (e.ctrlKey) {
          this.plugin_.postMessage({
            type: 'rotateClockwise',
          });
        }
        return;
    }
  },

  /**
   * @private
   * Notify the plugin to print.
   */
  print_: function() {
    this.plugin_.postMessage({
      type: 'print',
    });
  },

  /**
   * @private
   * Handle open pdf parameters. This function updates the viewport as per
   * the parameters mentioned in the url while opening pdf. The order is
   * important as later actions can override the effects of previous actions.
   */
  handleURLParams_: function() {
    if (this.urlParams_.page)
      this.viewport_.goToPage(this.urlParams_.page);
    if (this.urlParams_.position) {
      // Make sure we don't cancel effect of page parameter.
      this.viewport_.position = {
        x: this.viewport_.position.x + this.urlParams_.position.x,
        y: this.viewport_.position.y + this.urlParams_.position.y
      };
    }
    if (this.urlParams_.zoom)
      this.viewport_.setZoom(this.urlParams_.zoom);
  },

  /**
   * @private
   * Update the loading progress of the document in response to a progress
   * message being received from the plugin.
   * @param {number} progress the progress as a percentage.
   */
  updateProgress_: function(progress) {
    this.progressBar_.progress = progress;
    if (progress == -1) {
      // Document load failed.
      this.errorScreen_.style.visibility = 'visible';
      this.sizer_.style.display = 'none';
      this.toolbar_.style.visibility = 'hidden';
      if (this.passwordScreen_.active) {
        this.passwordScreen_.deny();
        this.passwordScreen_.active = false;
      }
    } else if (progress == 100) {
      // Document load complete.
      if (this.lastViewportPosition_)
        this.viewport_.position = this.lastViewportPosition_;
      this.handleURLParams_();
      this.loaded = true;
      var loadEvent = new Event('pdfload');
      window.dispatchEvent(loadEvent);
      this.sendScriptingMessage_({
        type: 'documentLoaded'
      });
    }
  },

  /**
   * @private
   * An event handler for handling password-submitted events. These are fired
   * when an event is entered into the password screen.
   * @param {Object} event a password-submitted event.
   */
  onPasswordSubmitted_: function(event) {
    this.plugin_.postMessage({
      type: 'getPasswordComplete',
      password: event.detail.password
    });
  },

  /**
   * @private
   * An event handler for handling message events received from the plugin.
   * @param {MessageObject} message a message event.
   */
  handlePluginMessage_: function(message) {
    switch (message.data.type.toString()) {
      case 'documentDimensions':
        this.documentDimensions_ = message.data;
        this.viewport_.setDocumentDimensions(this.documentDimensions_);
        this.toolbar_.style.visibility = 'visible';
        // If we received the document dimensions, the password was good so we
        // can dismiss the password screen.
        if (this.passwordScreen_.active)
          this.passwordScreen_.accept();

        this.pageIndicator_.initialFadeIn();
        this.toolbar_.initialFadeIn();
        break;
      case 'email':
        var href = 'mailto:' + message.data.to + '?cc=' + message.data.cc +
            '&bcc=' + message.data.bcc + '&subject=' + message.data.subject +
            '&body=' + message.data.body;
        var w = window.open(href, '_blank', 'width=1,height=1');
        if (w)
          w.close();
        break;
      case 'getAccessibilityJSONReply':
        this.sendScriptingMessage_(message.data);
        break;
      case 'getPassword':
        // If the password screen isn't up, put it up. Otherwise we're
        // responding to an incorrect password so deny it.
        if (!this.passwordScreen_.active)
          this.passwordScreen_.active = true;
        else
          this.passwordScreen_.deny();
        break;
      case 'goToPage':
        this.viewport_.goToPage(message.data.page);
        break;
      case 'loadProgress':
        this.updateProgress_(message.data.progress);
        break;
      case 'navigate':
        if (message.data.newTab)
          window.open(message.data.url);
        else
          window.location.href = message.data.url;
        break;
      case 'setScrollPosition':
        var position = this.viewport_.position;
        if (message.data.x != undefined)
          position.x = message.data.x;
        if (message.data.y != undefined)
          position.y = message.data.y;
        this.viewport_.position = position;
        break;
      case 'setTranslatedStrings':
        this.passwordScreen_.text = message.data.getPasswordString;
        this.progressBar_.text = message.data.loadingString;
        this.errorScreen_.text = message.data.loadFailedString;
        break;
      case 'cancelStreamUrl':
        chrome.streamsPrivate.abort(this.streamDetails.streamUrl);
        break;
    }
  },

  /**
   * @private
   * A callback that's called before the zoom changes. Notify the plugin to stop
   * reacting to scroll events while zoom is taking place to avoid flickering.
   */
  beforeZoom_: function() {
    this.plugin_.postMessage({
      type: 'stopScrolling'
    });
  },

  /**
   * @private
   * A callback that's called after the zoom changes. Notify the plugin of the
   * zoom change and to continue reacting to scroll events.
   */
  afterZoom_: function() {
    var position = this.viewport_.position;
    var zoom = this.viewport_.zoom;
    if (chrome.tabs && !this.setZoomInProgress_) {
      this.setZoomInProgress_ = true;
      chrome.tabs.setZoom(zoom, this.setZoomComplete_.bind(this, zoom));
    }
    this.plugin_.postMessage({
      type: 'viewport',
      zoom: zoom,
      xOffset: position.x,
      yOffset: position.y
    });
  },

  /**
   * @private
   * A callback that's called after chrome.tabs.setZoom is complete. This will
   * call chrome.tabs.setZoom again if the zoom level has changed since it was
   * last called.
   * @param {number} lastZoom the zoom level that chrome.tabs.setZoom was called
   *     with.
   */
  setZoomComplete_: function(lastZoom) {
    var zoom = this.viewport_.zoom;
    if (zoom != lastZoom)
      chrome.tabs.setZoom(zoom, this.setZoomComplete_.bind(this, zoom));
    else
      this.setZoomInProgress_ = false;
  },

  /**
   * @private
   * A callback that's called after the viewport changes.
   */
  viewportChanged_: function() {
    if (!this.documentDimensions_)
      return;

    // Update the buttons selected.
    $('fit-to-page-button').classList.remove('polymer-selected');
    $('fit-to-width-button').classList.remove('polymer-selected');
    if (this.viewport_.fittingType == Viewport.FittingType.FIT_TO_PAGE) {
      $('fit-to-page-button').classList.add('polymer-selected');
    } else if (this.viewport_.fittingType ==
               Viewport.FittingType.FIT_TO_WIDTH) {
      $('fit-to-width-button').classList.add('polymer-selected');
    }

    var hasScrollbars = this.viewport_.documentHasScrollbars();
    var scrollbarWidth = this.viewport_.scrollbarWidth;
    // Offset the toolbar position so that it doesn't move if scrollbars appear.
    var toolbarRight = Math.max(PDFViewer.MIN_TOOLBAR_OFFSET, scrollbarWidth);
    var toolbarBottom = Math.max(PDFViewer.MIN_TOOLBAR_OFFSET, scrollbarWidth);
    if (hasScrollbars.vertical)
      toolbarRight -= scrollbarWidth;
    if (hasScrollbars.horizontal)
      toolbarBottom -= scrollbarWidth;
    this.toolbar_.style.right = toolbarRight + 'px';
    this.toolbar_.style.bottom = toolbarBottom + 'px';

    // Update the page indicator.
    var visiblePage = this.viewport_.getMostVisiblePage();
    this.pageIndicator_.index = visiblePage;
    if (this.documentDimensions_.pageDimensions.length > 1 &&
        hasScrollbars.vertical) {
      this.pageIndicator_.style.visibility = 'visible';
    } else {
      this.pageIndicator_.style.visibility = 'hidden';
    }

    var visiblePageDimensions = this.viewport_.getPageScreenRect(visiblePage);
    var size = this.viewport_.size;
    this.sendScriptingMessage_({
      type: 'viewport',
      pageX: visiblePageDimensions.x,
      pageY: visiblePageDimensions.y,
      pageWidth: visiblePageDimensions.width,
      viewportWidth: size.width,
      viewportHeight: size.height,
    });
  },

  /**
   * @private
   * Handle a scripting message from outside the extension (typically sent by
   * PDFScriptingAPI in a page containing the extension) to interact with the
   * plugin.
   * @param {MessageObject} message the message to handle.
   */
  handleScriptingMessage_: function(message) {
    switch (message.data.type.toString()) {
      case 'getAccessibilityJSON':
      case 'loadPreviewPage':
        this.plugin_.postMessage(message.data);
        break;
      case 'resetPrintPreviewMode':
        if (!this.inPrintPreviewMode_) {
          this.inPrintPreviewMode_ = true;
          this.viewport_.fitToPage();
        }

        // Stash the scroll location so that it can be restored when the new
        // document is loaded.
        this.lastViewportPosition_ = this.viewport_.position;

        // TODO(raymes): Disable these properly in the plugin.
        var printButton = $('print-button');
        if (printButton)
          printButton.parentNode.removeChild(printButton);
        var saveButton = $('save-button');
        if (saveButton)
          saveButton.parentNode.removeChild(saveButton);

        this.pageIndicator_.pageLabels = message.data.pageNumbers;

        this.plugin_.postMessage({
          type: 'resetPrintPreviewMode',
          url: message.data.url,
          grayscale: message.data.grayscale,
          // If the PDF isn't modifiable we send 0 as the page count so that no
          // blank placeholder pages get appended to the PDF.
          pageCount: (message.data.modifiable ?
                      message.data.pageNumbers.length : 0)
        });
        break;
      case 'sendKeyEvent':
        var e = document.createEvent('Event');
        e.initEvent('scriptingKeypress');
        e.keyCode = message.data.keyCode;
        this.handleKeyEvent_(e);
        break;
    }

  },

  /**
   * @private
   * Send a scripting message outside the extension (typically to
   * PDFScriptingAPI in a page containing the extension).
   * @param {Object} message the message to send.
   */
  sendScriptingMessage_: function(message) {
    window.parent.postMessage(message, '*');
  },

  /**
   * @type {Viewport} the viewport of the PDF viewer.
   */
  get viewport() {
    return this.viewport_;
  }
};
