// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for interacting with frames.
// Multiply-included message file, hence no include guard.

#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/frame_message_enums.h"
#include "content/common/frame_param.h"
#include "content/common/navigation_gesture.h"
#include "content/common/navigation_params.h"
#include "content/common/resource_request_body.h"
#include "content/public/common/color_suggestion.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/page_state.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/transition_element.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START FrameMsgStart

IPC_ENUM_TRAITS_MIN_MAX_VALUE(AccessibilityMode,
                              AccessibilityModeOff,
                              AccessibilityModeComplete)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(content::JavaScriptMessageType,
                              content::JAVASCRIPT_MESSAGE_TYPE_ALERT,
                              content::JAVASCRIPT_MESSAGE_TYPE_PROMPT)
IPC_ENUM_TRAITS_MAX_VALUE(FrameMsg_Navigate_Type::Value,
                          FrameMsg_Navigate_Type::NAVIGATE_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebContextMenuData::MediaType,
                          blink::WebContextMenuData::MediaTypeLast)
IPC_ENUM_TRAITS_MAX_VALUE(ui::MenuSourceType, ui::MENU_SOURCE_TYPE_LAST)

IPC_STRUCT_TRAITS_BEGIN(content::ColorSuggestion)
  IPC_STRUCT_TRAITS_MEMBER(color)
  IPC_STRUCT_TRAITS_MEMBER(label)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ContextMenuParams)
  IPC_STRUCT_TRAITS_MEMBER(media_type)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
  IPC_STRUCT_TRAITS_MEMBER(link_url)
  IPC_STRUCT_TRAITS_MEMBER(link_text)
  IPC_STRUCT_TRAITS_MEMBER(unfiltered_link_url)
  IPC_STRUCT_TRAITS_MEMBER(src_url)
  IPC_STRUCT_TRAITS_MEMBER(has_image_contents)
  IPC_STRUCT_TRAITS_MEMBER(page_url)
  IPC_STRUCT_TRAITS_MEMBER(keyword_url)
  IPC_STRUCT_TRAITS_MEMBER(frame_url)
  IPC_STRUCT_TRAITS_MEMBER(frame_page_state)
  IPC_STRUCT_TRAITS_MEMBER(media_flags)
  IPC_STRUCT_TRAITS_MEMBER(selection_text)
  IPC_STRUCT_TRAITS_MEMBER(suggested_filename)
  IPC_STRUCT_TRAITS_MEMBER(misspelled_word)
  IPC_STRUCT_TRAITS_MEMBER(misspelling_hash)
  IPC_STRUCT_TRAITS_MEMBER(dictionary_suggestions)
  IPC_STRUCT_TRAITS_MEMBER(spellcheck_enabled)
  IPC_STRUCT_TRAITS_MEMBER(is_editable)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_default)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_left_to_right)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_right_to_left)
  IPC_STRUCT_TRAITS_MEMBER(edit_flags)
  IPC_STRUCT_TRAITS_MEMBER(security_info)
  IPC_STRUCT_TRAITS_MEMBER(frame_charset)
  IPC_STRUCT_TRAITS_MEMBER(referrer_policy)
  IPC_STRUCT_TRAITS_MEMBER(custom_context)
  IPC_STRUCT_TRAITS_MEMBER(custom_items)
  IPC_STRUCT_TRAITS_MEMBER(source_type)
#if defined(OS_ANDROID)
  IPC_STRUCT_TRAITS_MEMBER(selection_start)
  IPC_STRUCT_TRAITS_MEMBER(selection_end)
#endif
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::CustomContextMenuContext)
  IPC_STRUCT_TRAITS_MEMBER(is_pepper_menu)
  IPC_STRUCT_TRAITS_MEMBER(request_id)
  IPC_STRUCT_TRAITS_MEMBER(render_widget_id)
  IPC_STRUCT_TRAITS_MEMBER(link_followed)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::TransitionElement)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(rect)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(FrameHostMsg_AddNavigationTransitionData_Params)
  IPC_STRUCT_MEMBER(int, render_frame_id)
  IPC_STRUCT_MEMBER(std::string, allowed_destination_host_pattern)
  IPC_STRUCT_MEMBER(std::string, selector)
  IPC_STRUCT_MEMBER(std::string, markup)
  IPC_STRUCT_MEMBER(std::vector<content::TransitionElement>, elements)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(FrameHostMsg_DidFailProvisionalLoadWithError_Params)
  // Error code as reported in the DidFailProvisionalLoad callback.
  IPC_STRUCT_MEMBER(int, error_code)
  // An error message generated from the error_code. This can be an empty
  // string if we were unable to find a meaningful description.
  IPC_STRUCT_MEMBER(base::string16, error_description)
  // The URL that the error is reported for.
  IPC_STRUCT_MEMBER(GURL, url)
  // True if the failure is the result of navigating to a POST again
  // and we're going to show the POST interstitial.
  IPC_STRUCT_MEMBER(bool, showing_repost_interstitial)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(content::FrameNavigateParams)
  IPC_STRUCT_TRAITS_MEMBER(page_id)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(base_url)
  IPC_STRUCT_TRAITS_MEMBER(referrer)
  IPC_STRUCT_TRAITS_MEMBER(transition)
  IPC_STRUCT_TRAITS_MEMBER(redirects)
  IPC_STRUCT_TRAITS_MEMBER(should_update_history)
  IPC_STRUCT_TRAITS_MEMBER(searchable_form_url)
  IPC_STRUCT_TRAITS_MEMBER(searchable_form_encoding)
  IPC_STRUCT_TRAITS_MEMBER(contents_mime_type)
  IPC_STRUCT_TRAITS_MEMBER(socket_address)
IPC_STRUCT_TRAITS_END()

// Parameters structure for FrameHostMsg_DidCommitProvisionalLoad, which has
// too many data parameters to be reasonably put in a predefined IPC message.
IPC_STRUCT_BEGIN_WITH_PARENT(FrameHostMsg_DidCommitProvisionalLoad_Params,
                             content::FrameNavigateParams)
  IPC_STRUCT_TRAITS_PARENT(content::FrameNavigateParams)

  // Information regarding the security of the connection (empty if the
  // connection was not secure).
  IPC_STRUCT_MEMBER(std::string, security_info)

  // The gesture that initiated this navigation.
  IPC_STRUCT_MEMBER(content::NavigationGesture, gesture)

  // True if this was a post request.
  IPC_STRUCT_MEMBER(bool, is_post)

  // The POST body identifier. -1 if it doesn't exist.
  IPC_STRUCT_MEMBER(int64, post_id)

  // Whether the frame navigation resulted in no change to the documents within
  // the page. For example, the navigation may have just resulted in scrolling
  // to a named anchor.
  IPC_STRUCT_MEMBER(bool, was_within_same_page)

  // The status code of the HTTP request.
  IPC_STRUCT_MEMBER(int, http_status_code)

  // This flag is used to warn if the renderer is displaying an error page,
  // so that we can set the appropriate page type.
  IPC_STRUCT_MEMBER(bool, url_is_unreachable)

  // True if the connection was proxied.  In this case, socket_address
  // will represent the address of the proxy, rather than the remote host.
  IPC_STRUCT_MEMBER(bool, was_fetched_via_proxy)

  // Serialized history item state to store in the navigation entry.
  IPC_STRUCT_MEMBER(content::PageState, page_state)

  // Original request's URL.
  IPC_STRUCT_MEMBER(GURL, original_request_url)

  // User agent override used to navigate.
  IPC_STRUCT_MEMBER(bool, is_overriding_user_agent)

  // Notifies the browser that for this navigation, the session history was
  // successfully cleared.
  IPC_STRUCT_MEMBER(bool, history_list_was_cleared)

  // The routing_id of the render view associated with the navigation.
  // We need to track the RenderViewHost routing_id because of downstream
  // dependencies (crbug.com/392171 DownloadRequestHandle, SaveFileManager,
  // ResourceDispatcherHostImpl, MediaStreamUIProxy,
  // SpeechRecognitionDispatcherHost and possibly others). They look up the view
  // based on the ID stored in the resource requests. Once those dependencies
  // are unwound or moved to RenderFrameHost (crbug.com/304341) we can move the
  // client to be based on the routing_id of the RenderFrameHost.
  IPC_STRUCT_MEMBER(int, render_view_routing_id)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(content::CommonNavigationParams)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(referrer)
  IPC_STRUCT_TRAITS_MEMBER(transition)
  IPC_STRUCT_TRAITS_MEMBER(navigation_type)
  IPC_STRUCT_TRAITS_MEMBER(allow_download)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::RequestNavigationParams)
  IPC_STRUCT_TRAITS_MEMBER(is_post)
  IPC_STRUCT_TRAITS_MEMBER(extra_headers)
  IPC_STRUCT_TRAITS_MEMBER(browser_initiated_post_data)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::CommitNavigationParams)
  IPC_STRUCT_TRAITS_MEMBER(page_state)
  IPC_STRUCT_TRAITS_MEMBER(is_overriding_user_agent)
  IPC_STRUCT_TRAITS_MEMBER(browser_navigation_start)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(FrameMsg_Navigate_Params)
  // TODO(clamy): investigate which parameters are also needed in PlzNavigate
  // and move them to the appropriate NavigationParams struct.

  // These structs contain parameters shared by other navigation IPCs.
  IPC_STRUCT_MEMBER(content::CommonNavigationParams, common_params)
  IPC_STRUCT_MEMBER(content::RequestNavigationParams, request_params)
  IPC_STRUCT_MEMBER(content::CommitNavigationParams, commit_params)

  // The page_id for this navigation, or -1 if it is a new navigation.  Back,
  // Forward, and Reload navigations should have a valid page_id.  If the load
  // succeeds, then this page_id will be reflected in the resultant
  // FrameHostMsg_DidCommitProvisionalLoad message.
  IPC_STRUCT_MEMBER(int32, page_id)

  // If page_id is -1, then pending_history_list_offset will also be -1.
  // Otherwise, it contains the offset into the history list corresponding to
  // the current navigation.
  IPC_STRUCT_MEMBER(int, pending_history_list_offset)

  // Informs the RenderView of where its current page contents reside in
  // session history and the total size of the session history list.
  IPC_STRUCT_MEMBER(int, current_history_list_offset)
  IPC_STRUCT_MEMBER(int, current_history_list_length)

  // Informs the RenderView the session history should be cleared. In that
  // case, the RenderView needs to notify the browser that the clearing was
  // succesful when the navigation commits.
  IPC_STRUCT_MEMBER(bool, should_clear_history_list)

  // Base URL for use in WebKit's SubstituteData.
  // Is only used with data: URLs.
  IPC_STRUCT_MEMBER(GURL, base_url_for_data_url)

  // History URL for use in WebKit's SubstituteData.
  // Is only used with data: URLs.
  IPC_STRUCT_MEMBER(GURL, history_url_for_data_url)

  // Any redirect URLs that occurred before |url|. Useful for cross-process
  // navigations; defaults to empty.
  IPC_STRUCT_MEMBER(std::vector<GURL>, redirects)

  // Informs the RenderView the pending navigation should replace the current
  // history entry when it commits. This is used for cross-process redirects so
  // the transferred navigation can recover the navigation state.
  IPC_STRUCT_MEMBER(bool, should_replace_current_entry)

  // The time the request was created. This is used by the old performance
  // infrastructure to set up DocumentState associated with the RenderView.
  // TODO(ppi): make it go away.
  IPC_STRUCT_MEMBER(base::Time, request_time)

  // The following two members identify a previous request that has been
  // created before this navigation is being transferred to a new render view.
  // This serves the purpose of recycling the old request.
  // Unless this refers to a transferred navigation, these values are -1 and -1.
  IPC_STRUCT_MEMBER(int, transferred_request_child_id)
  IPC_STRUCT_MEMBER(int, transferred_request_request_id)

  // Whether or not this url should be allowed to access local file://
  // resources.
  IPC_STRUCT_MEMBER(bool, can_load_local_resources)

  // If not empty, which frame to navigate.
  IPC_STRUCT_MEMBER(std::string, frame_to_navigate)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(FrameHostMsg_OpenURL_Params)
  IPC_STRUCT_MEMBER(GURL, url)
  IPC_STRUCT_MEMBER(content::Referrer, referrer)
  IPC_STRUCT_MEMBER(WindowOpenDisposition, disposition)
  IPC_STRUCT_MEMBER(bool, should_replace_current_entry)
  IPC_STRUCT_MEMBER(bool, user_gesture)
IPC_STRUCT_END()

// PlzNavigate
IPC_STRUCT_BEGIN(FrameHostMsg_BeginNavigation_Params)
  // TODO(clamy): See if it is possible to define a common struct between this
  // IPC and ResourceMsg_Request_Params.

  // The request method: GET, POST, etc.
  IPC_STRUCT_MEMBER(std::string, method)

  // Additional HTTP request headers.
  IPC_STRUCT_MEMBER(std::string, headers)

  // net::URLRequest load flags (net::LOAD_NORMAL) by default).
  IPC_STRUCT_MEMBER(int, load_flags)

  // Optional resource request body (may be null).
  IPC_STRUCT_MEMBER(scoped_refptr<content::ResourceRequestBody>,
                    request_body)

  // True if the request was user initiated.
  IPC_STRUCT_MEMBER(bool, has_user_gesture)
IPC_STRUCT_END()

#if defined(OS_MACOSX) || defined(OS_ANDROID)
// This message is used for supporting popup menus on Mac OS X and Android using
// native controls. See the FrameHostMsg_ShowPopup message.
IPC_STRUCT_BEGIN(FrameHostMsg_ShowPopup_Params)
  // Position on the screen.
  IPC_STRUCT_MEMBER(gfx::Rect, bounds)

  // The height of each item in the menu.
  IPC_STRUCT_MEMBER(int, item_height)

  // The size of the font to use for those items.
  IPC_STRUCT_MEMBER(double, item_font_size)

  // The currently selected (displayed) item in the menu.
  IPC_STRUCT_MEMBER(int, selected_item)

  // The entire list of items in the popup menu.
  IPC_STRUCT_MEMBER(std::vector<content::MenuItem>, popup_items)

  // Whether items should be right-aligned.
  IPC_STRUCT_MEMBER(bool, right_aligned)

  // Whether this is a multi-select popup.
  IPC_STRUCT_MEMBER(bool, allow_multiple_selection)
IPC_STRUCT_END()
#endif

// -----------------------------------------------------------------------------
// Messages sent from the browser to the renderer.

// Notifies the embedding frame that a new CompositorFrame is ready to be
// presented. When the frame finishes presenting, a matching
// FrameHostMsg_CompositorFrameSwappedACK should be sent back to the
// RenderViewHost that was produced the CompositorFrame.
//
// This is used in the ubercomp compositing path.
IPC_MESSAGE_ROUTED1(FrameMsg_CompositorFrameSwapped,
                    FrameMsg_CompositorFrameSwapped_Params /* params */)

// Notifies the embedding frame that the process rendering the child frame's
// contents has terminated.
IPC_MESSAGE_ROUTED0(FrameMsg_ChildFrameProcessGone)

// Sent in response to a FrameHostMsg_ContextMenu to let the renderer know that
// the menu has been closed.
IPC_MESSAGE_ROUTED1(FrameMsg_ContextMenuClosed,
                    content::CustomContextMenuContext /* custom_context */)

// Executes custom context menu action that was provided from Blink.
IPC_MESSAGE_ROUTED2(FrameMsg_CustomContextMenuAction,
                    content::CustomContextMenuContext /* custom_context */,
                    unsigned /* action */)

// Requests that the RenderFrame or RenderFrameProxy sets its opener to null.
IPC_MESSAGE_ROUTED0(FrameMsg_DisownOpener)

// Instructs the renderer to create a new RenderFrame object with |routing_id|.
// The new frame should be created as a child of the object identified by
// |parent_routing_id| or as top level if that is MSG_ROUTING_NONE.
// If a valid |proxy_routing_id| is provided, the new frame will be configured
// to replace the proxy on commit.
IPC_MESSAGE_CONTROL3(FrameMsg_NewFrame,
                     int /* routing_id */,
                     int /* parent_routing_id */,
                     int /* proxy_routing_id */)

// Instructs the renderer to create a new RenderFrameProxy object with
// |routing_id|. The new proxy should be created as a child of the object
// identified by |parent_routing_id| or as top level if that is
// MSG_ROUTING_NONE.
IPC_MESSAGE_CONTROL3(FrameMsg_NewFrameProxy,
                     int /* routing_id */,
                     int /* parent_routing_id */,
                     int /* render_view_routing_id */)

// Tells the renderer to perform the specified navigation, interrupting any
// existing navigation.
IPC_MESSAGE_ROUTED1(FrameMsg_Navigate, FrameMsg_Navigate_Params)

// Instructs the renderer to invoke the frame's beforeunload event handler.
// Expects the result to be returned via FrameHostMsg_BeforeUnload_ACK.
IPC_MESSAGE_ROUTED0(FrameMsg_BeforeUnload)

// Instructs the frame to swap out for a cross-site transition, including
// running the unload event handler and creating a RenderFrameProxy with the
// given |proxy_routing_id|. Expects a SwapOut_ACK message when finished.
IPC_MESSAGE_ROUTED1(FrameMsg_SwapOut,
                    int /* proxy_routing_id */)

// Instructs the frame to stop the load in progress, if any.
IPC_MESSAGE_ROUTED0(FrameMsg_Stop)

// Request for the renderer to insert CSS into the frame.
IPC_MESSAGE_ROUTED1(FrameMsg_CSSInsertRequest,
                    std::string  /* css */)

// Request for the renderer to execute JavaScript in the frame's context.
//
// javascript is the string containing the JavaScript to be executed in the
// target frame's context.
//
// If the third parameter is true the result is sent back to the browser using
// the message FrameHostMsg_JavaScriptExecuteResponse.
// FrameHostMsg_JavaScriptExecuteResponse is passed the ID parameter so that the
// host can uniquely identify the request.
IPC_MESSAGE_ROUTED3(FrameMsg_JavaScriptExecuteRequest,
                    base::string16,  /* javascript */
                    int,  /* ID */
                    bool  /* if true, a reply is requested */)

// ONLY FOR TESTS: Same as above but adds a fake UserGestureindicator around
// execution. (crbug.com/408426)
IPC_MESSAGE_ROUTED3(FrameMsg_JavaScriptExecuteRequestForTests,
                    base::string16,  /* javascript */
                    int,  /* ID */
                    bool  /* if true, a reply is requested */)

// Selects between the given start and end offsets in the currently focused
// editable field.
IPC_MESSAGE_ROUTED2(FrameMsg_SetEditableSelectionOffsets,
                    int /* start */,
                    int /* end */)

// Requests a navigation to the supplied markup, in an iframe with sandbox
// attributes.
IPC_MESSAGE_ROUTED1(FrameMsg_SetupTransitionView,
                    std::string /* markup */)

// Tells the renderer to hide the elements specified by the supplied CSS
// selector, and activates any exiting-transition stylesheets.
IPC_MESSAGE_ROUTED1(FrameMsg_BeginExitTransition,
                    std::string /* css_selector */)

// Tells the renderer to reload the frame, optionally ignoring the cache while
// doing so.
IPC_MESSAGE_ROUTED1(FrameMsg_Reload,
                    bool /* ignore_cache */)

// Notifies the color chooser client that the user selected a color.
IPC_MESSAGE_ROUTED2(FrameMsg_DidChooseColorResponse, unsigned, SkColor)

// Notifies the color chooser client that the color chooser has ended.
IPC_MESSAGE_ROUTED1(FrameMsg_DidEndColorChooser, unsigned)

// Notifies the corresponding RenderFrameProxy object to replace itself with the
// RenderFrame object it is associated with.
IPC_MESSAGE_ROUTED0(FrameMsg_DeleteProxy)

// Request the text surrounding the selection with a |max_length|. The response
// will be sent via FrameHostMsg_TextSurroundingSelectionResponse.
IPC_MESSAGE_ROUTED1(FrameMsg_TextSurroundingSelectionRequest,
                    size_t /* max_length */)

// Tells the renderer to insert a link to the specified stylesheet. This is
// needed to support navigation transitions.
IPC_MESSAGE_ROUTED1(FrameMsg_AddStyleSheetByURL, std::string)

// Change the accessibility mode in the renderer process.
IPC_MESSAGE_ROUTED1(FrameMsg_SetAccessibilityMode,
                    AccessibilityMode)

#if defined(OS_ANDROID)

// External popup menus.
IPC_MESSAGE_ROUTED2(FrameMsg_SelectPopupMenuItems,
                    bool /* user canceled the popup */,
                    std::vector<int> /* selected indices */)

#elif defined(OS_MACOSX)

// External popup menus.
IPC_MESSAGE_ROUTED1(FrameMsg_SelectPopupMenuItem,
                    int /* selected index, -1 means no selection */)

#endif

// PlzNavigate
// Tells the renderer that a navigation has been requested.
IPC_MESSAGE_ROUTED2(FrameMsg_RequestNavigation,
                    content::CommonNavigationParams, /* common_params */
                    content::RequestNavigationParams /* request_params */)

// PlzNavigate
// Tells the renderer that a navigation is ready to commit.  The renderer should
// request |stream_url| to get access to the stream containing the body of the
// response.
IPC_MESSAGE_ROUTED4(FrameMsg_CommitNavigation,
                    content::ResourceResponseHead, /* response */
                    GURL, /* stream_url */
                    content::CommonNavigationParams, /* common_params */
                    content::CommitNavigationParams /* commit_params */)

#if defined(ENABLE_PLUGINS)
// Notifies the renderer of updates to the Plugin Power Saver origin whitelist.
IPC_MESSAGE_ROUTED1(FrameMsg_UpdatePluginContentOriginWhitelist,
                    std::set<GURL> /* origin_whitelist */)
#endif  // defined(ENABLE_PLUGINS)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Blink and JavaScript error messages to log to the console
// or debugger UI.
IPC_MESSAGE_ROUTED4(FrameHostMsg_AddMessageToConsole,
                    int32, /* log level */
                    base::string16, /* msg */
                    int32, /* line number */
                    base::string16 /* source id */ )

// Sent by the renderer when a child frame is created in the renderer.
//
// Each of these messages will have a corresponding FrameHostMsg_Detach message
// sent when the frame is detached from the DOM.
IPC_SYNC_MESSAGE_CONTROL2_1(FrameHostMsg_CreateChildFrame,
                            int32 /* parent_routing_id */,
                            std::string /* frame_name */,
                            int32 /* new_routing_id */)

// Sent by the renderer to the parent RenderFrameHost when a child frame is
// detached from the DOM.
IPC_MESSAGE_ROUTED0(FrameHostMsg_Detach)

// Sent by the renderer when the frame becomes focused.
IPC_MESSAGE_ROUTED0(FrameHostMsg_FrameFocused)

// Sent when the renderer starts a provisional load for a frame.
// |is_transition_navigation| signals that the frame has defined transition
// elements which can be animated by the navigation destination to provide
// a transition effect during load.
IPC_MESSAGE_ROUTED2(FrameHostMsg_DidStartProvisionalLoadForFrame,
                    GURL /* url */,
                    bool /* is_transition_navigation */)

// Sent when the renderer fails a provisional load with an error.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidFailProvisionalLoadWithError,
                    FrameHostMsg_DidFailProvisionalLoadWithError_Params)

// Notifies the browser that a frame in the view has changed. This message
// has a lot of parameters and is packed/unpacked by functions defined in
// render_messages.h.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidCommitProvisionalLoad,
                    FrameHostMsg_DidCommitProvisionalLoad_Params)

// Notifies the browser that a document has been loaded.
IPC_MESSAGE_ROUTED0(FrameHostMsg_DidFinishDocumentLoad)

IPC_MESSAGE_ROUTED3(FrameHostMsg_DidFailLoadWithError,
                    GURL /* validated_url */,
                    int /* error_code */,
                    base::string16 /* error_description */)

// Sent when the renderer starts loading the page. |to_different_document| will
// be true unless the load is a fragment navigation, or triggered by
// history.pushState/replaceState.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidStartLoading,
                    bool /* to_different_document */)

// Sent when the renderer is done loading a page.
IPC_MESSAGE_ROUTED0(FrameHostMsg_DidStopLoading)

// Sent when the renderer changed the progress of a load.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidChangeLoadProgress,
                    double /* load_progress */)

// Requests that the given URL be opened in the specified manner.
IPC_MESSAGE_ROUTED1(FrameHostMsg_OpenURL, FrameHostMsg_OpenURL_Params)

// Notifies the browser that a frame finished loading.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidFinishLoad,
                    GURL /* validated_url */)

// Sent when after the onload handler has been invoked for the document
// in this frame. Sent for top-level frames.
IPC_MESSAGE_ROUTED0(FrameHostMsg_DocumentOnLoadCompleted)

// Notifies that the initial empty document of a view has been accessed.
// After this, it is no longer safe to show a pending navigation's URL without
// making a URL spoof possible.
IPC_MESSAGE_ROUTED0(FrameHostMsg_DidAccessInitialDocument)

// Sent when the frame sets its opener to null, disowning it for the lifetime of
// the window. Sent for top-level frames.
IPC_MESSAGE_ROUTED0(FrameHostMsg_DidDisownOpener)

// Notifies the browser that a page id was assigned.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidAssignPageId,
                    int32 /* page_id */)

// Changes the title for the page in the UI when the page is navigated or the
// title changes. Sent for top-level frames.
IPC_MESSAGE_ROUTED2(FrameHostMsg_UpdateTitle,
                    base::string16 /* title */,
                    blink::WebTextDirection /* title direction */)

// Change the encoding name of the page in UI when the page has detected
// proper encoding name. Sent for top-level frames.
IPC_MESSAGE_ROUTED1(FrameHostMsg_UpdateEncoding,
                    std::string /* new encoding name */)

// Following message is used to communicate the values received by the
// callback binding the JS to Cpp.
// An instance of browser that has an automation host listening to it can
// have a javascript send a native value (string, number, boolean) to the
// listener in Cpp. (DomAutomationController)
IPC_MESSAGE_ROUTED2(FrameHostMsg_DomOperationResponse,
                    std::string  /* json_string */,
                    int  /* automation_id */)

#if defined(ENABLE_PLUGINS)
// Sent to the browser when the renderer detects it is blocked on a pepper
// plugin message for too long. This is also sent when it becomes unhung
// (according to the value of is_hung). The browser can give the user the
// option of killing the plugin.
IPC_MESSAGE_ROUTED3(FrameHostMsg_PepperPluginHung,
                    int /* plugin_child_id */,
                    base::FilePath /* path */,
                    bool /* is_hung */)

// Sent by the renderer process to indicate that a plugin instance has crashed.
// Note: |plugin_pid| should not be trusted. The corresponding process has
// probably died. Moreover, the ID may have been reused by a new process. Any
// usage other than displaying it in a prompt to the user is very likely to be
// wrong.
IPC_MESSAGE_ROUTED2(FrameHostMsg_PluginCrashed,
                    base::FilePath /* plugin_path */,
                    base::ProcessId /* plugin_pid */)

// Return information about a plugin for the given URL and MIME
// type. If there is no matching plugin, |found| is false.
// |actual_mime_type| is the actual mime type supported by the
// found plugin.
IPC_SYNC_MESSAGE_CONTROL4_3(FrameHostMsg_GetPluginInfo,
                            int /* render_frame_id */,
                            GURL /* url */,
                            GURL /* page_url */,
                            std::string /* mime_type */,
                            bool /* found */,
                            content::WebPluginInfo /* plugin info */,
                            std::string /* actual_mime_type */)

// A renderer sends this to the browser process when it wants to temporarily
// whitelist an origin's plugin content as essential. This temporary whitelist
// is specific to a top level frame, and is cleared when the whitelisting
// RenderFrame is destroyed.
IPC_MESSAGE_ROUTED1(FrameHostMsg_PluginContentOriginAllowed,
                    GURL /* content_origin */)
#endif  // defined(ENABLE_PLUGINS)

// A renderer sends this to the browser process when it wants to
// create a plugin.  The browser will create the plugin process if
// necessary, and will return a handle to the channel on success.
// On error an empty string is returned.
IPC_SYNC_MESSAGE_CONTROL4_2(FrameHostMsg_OpenChannelToPlugin,
                            int /* render_frame_id */,
                            GURL /* url */,
                            GURL /* page_url */,
                            std::string /* mime_type */,
                            IPC::ChannelHandle /* channel_handle */,
                            content::WebPluginInfo /* info */)

// Acknowledge that we presented an ubercomp frame.
//
// See FrameMsg_CompositorFrameSwapped
IPC_MESSAGE_ROUTED1(FrameHostMsg_CompositorFrameSwappedACK,
                    FrameHostMsg_CompositorFrameSwappedACK_Params /* params */)

// Provides the result from handling BeforeUnload.  |proceed| matches the return
// value of the frame's beforeunload handler: true if the user decided to
// proceed with leaving the page.
IPC_MESSAGE_ROUTED3(FrameHostMsg_BeforeUnload_ACK,
                    bool /* proceed */,
                    base::TimeTicks /* before_unload_start_time */,
                    base::TimeTicks /* before_unload_end_time */)

// Indicates that the current frame has swapped out, after a SwapOut message.
IPC_MESSAGE_ROUTED0(FrameHostMsg_SwapOut_ACK)

IPC_MESSAGE_ROUTED1(FrameHostMsg_ReclaimCompositorResources,
                    FrameHostMsg_ReclaimCompositorResources_Params /* params */)

// Forwards an input event to a child.
// TODO(nick): Temporary bridge, revisit once the browser process can route
// input directly to subframes. http://crbug.com/339659
IPC_MESSAGE_ROUTED1(FrameHostMsg_ForwardInputEvent,
                    IPC::WebInputEventPointer /* event */)

// Used to tell the parent that the user right clicked on an area of the
// content area, and a context menu should be shown for it. The params
// object contains information about the node(s) that were selected when the
// user right clicked.
IPC_MESSAGE_ROUTED1(FrameHostMsg_ContextMenu, content::ContextMenuParams)

// Initial drawing parameters for a child frame that has been swapped out to
// another process.
IPC_MESSAGE_ROUTED2(FrameHostMsg_InitializeChildFrame,
                    gfx::Rect /* frame_rect */,
                    float /* scale_factor */)

// Response for FrameMsg_JavaScriptExecuteRequest, sent when a reply was
// requested. The ID is the parameter supplied to
// FrameMsg_JavaScriptExecuteRequest. The result has the value returned by the
// script as its only element, one of Null, Boolean, Integer, Real, Date, or
// String.
IPC_MESSAGE_ROUTED2(FrameHostMsg_JavaScriptExecuteResponse,
                    int  /* id */,
                    base::ListValue  /* result */)

// A request to run a JavaScript dialog.
IPC_SYNC_MESSAGE_ROUTED4_2(FrameHostMsg_RunJavaScriptMessage,
                           base::string16     /* in - alert message */,
                           base::string16     /* in - default prompt */,
                           GURL               /* in - originating page URL */,
                           content::JavaScriptMessageType /* in - type */,
                           bool               /* out - success */,
                           base::string16     /* out - user_input field */)

// Displays a dialog to confirm that the user wants to navigate away from the
// page. Replies true if yes, and false otherwise. The reply string is ignored,
// but is included so that we can use OnJavaScriptMessageBoxClosed.
IPC_SYNC_MESSAGE_ROUTED3_2(FrameHostMsg_RunBeforeUnloadConfirm,
                           GURL,           /* in - originating frame URL */
                           base::string16  /* in - alert message */,
                           bool            /* in - is a reload */,
                           bool            /* out - success */,
                           base::string16  /* out - This is ignored.*/)

// Asks the browser to open the color chooser.
IPC_MESSAGE_ROUTED3(FrameHostMsg_OpenColorChooser,
                    int /* id */,
                    SkColor /* color */,
                    std::vector<content::ColorSuggestion> /* suggestions */)

// Asks the browser to end the color chooser.
IPC_MESSAGE_ROUTED1(FrameHostMsg_EndColorChooser, int /* id */)

// Change the selected color in the color chooser.
IPC_MESSAGE_ROUTED2(FrameHostMsg_SetSelectedColorInColorChooser,
                    int /* id */,
                    SkColor /* color */)

// Notifies the browser that media has started/stopped playing.
IPC_MESSAGE_ROUTED4(FrameHostMsg_MediaPlayingNotification,
                    int64 /* player_cookie, distinguishes instances */,
                    bool /* has_video */,
                    bool /* has_audio */,
                    bool /* is_remote */)

IPC_MESSAGE_ROUTED1(FrameHostMsg_MediaPausedNotification,
                    int64 /* player_cookie, distinguishes instances */)

// Notify browser the theme color has been changed.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidChangeThemeColor,
                    SkColor /* theme_color */)

// Response for FrameMsg_TextSurroundingSelectionRequest, |startOffset| and
// |endOffset| are the offsets of the selection in the returned |content|.
IPC_MESSAGE_ROUTED3(FrameHostMsg_TextSurroundingSelectionResponse,
                    base::string16,  /* content */
                    size_t, /* startOffset */
                    size_t /* endOffset */)

// Notifies the browser that the renderer has a pending navigation transition.
// The string parameters are all UTF8.
IPC_MESSAGE_CONTROL1(FrameHostMsg_AddNavigationTransitionData,
                     FrameHostMsg_AddNavigationTransitionData_Params)

// PlzNavigate
// Tells the browser to perform a navigation.
IPC_MESSAGE_ROUTED2(FrameHostMsg_BeginNavigation,
                    FrameHostMsg_BeginNavigation_Params,
                    content::CommonNavigationParams)

// Sent once a paint happens after the first non empty layout. In other words
// after the frame has painted something.
IPC_MESSAGE_ROUTED0(FrameHostMsg_DidFirstVisuallyNonEmptyPaint)

#if defined(OS_MACOSX) || defined(OS_ANDROID)

// Message to show/hide a popup menu using native controls.
IPC_MESSAGE_ROUTED1(FrameHostMsg_ShowPopup,
                    FrameHostMsg_ShowPopup_Params)
IPC_MESSAGE_ROUTED0(FrameHostMsg_HidePopup)

#endif
