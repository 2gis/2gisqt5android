# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'type': 'shared_library',
  'android_unmangled_name': 1,
  'dependencies': [
    'android_webview_common',
  ],
  'conditions': [
    [ 'android_webview_build==1', {
      # When building inside the android tree we also need to depend on all
      # the java sources generated from templates which will be needed by
      # android_webview_java in android_webview/java_library_common.mk.
      'dependencies': [
        '../base/base.gyp:base_java_application_state',
        '../base/base.gyp:base_java_library_load_from_apk_status_codes',
        '../base/base.gyp:base_java_memory_pressure_level',
        '../content/content.gyp:content_gamepad_mapping',
        '../content/content.gyp:gesture_event_type_java',
        '../content/content.gyp:popup_item_type_java',
        '../content/content.gyp:result_codes_java',
        '../content/content.gyp:screen_orientation_values_java',
        '../content/content.gyp:selection_event_type_java',
        '../content/content.gyp:speech_recognition_error_java',
        '../content/content.gyp:top_controls_state_java',
        '../media/media.gyp:media_android_imageformat',
        '../net/net.gyp:cert_verify_status_android_java',
        '../net/net.gyp:certificate_mime_types_java',
        '../net/net.gyp:net_errors_java',
        '../net/net.gyp:private_key_types_java',
        '../ui/android/ui_android.gyp:bitmap_format_java',
        '../ui/android/ui_android.gyp:page_transition_types_java',
        '../ui/android/ui_android.gyp:window_open_disposition_java',
      ],
      # Enable feedback-directed optimisation for the library when building in
      # android.
      'aosp_build_settings': {
        'LOCAL_FDO_SUPPORT': 'true',
      },
    }],
  ],
  'sources': [
    'lib/main/webview_entry_point.cc',
  ],
}

