# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/invalidation
      'target_name': 'invalidation',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_prefs',
        '../google_apis/google_apis.gyp:google_apis',
        '../jingle/jingle.gyp:notifier',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        # TODO(akalin): Remove this (http://crbug.com/133352).
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_proto_cpp',
        'gcm_driver',
        'keyed_service_core',
        'pref_registry',
        'signin_core_browser',
      ],
      'export_dependent_settings': [
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'invalidation/ack_handle.cc',
        'invalidation/ack_handle.h',
        'invalidation/ack_handler.cc',
        'invalidation/ack_handler.h',
        'invalidation/invalidation.cc',
        'invalidation/invalidation.h',
        'invalidation/invalidation_handler.cc',
        'invalidation/invalidation_handler.h',
        'invalidation/invalidation_logger.cc',
        'invalidation/invalidation_logger.h',
        'invalidation/invalidation_logger_observer.h',
        'invalidation/invalidation_prefs.cc',
        'invalidation/invalidation_prefs.h',
        'invalidation/invalidation_service.h',
        'invalidation/invalidation_service_util.cc',
        'invalidation/invalidation_service_util.h',
        'invalidation/invalidation_state_tracker.cc',
        'invalidation/invalidation_state_tracker.h',
        'invalidation/invalidation_switches.cc',
        'invalidation/invalidation_switches.h',
        'invalidation/invalidation_util.cc',
        'invalidation/invalidation_util.h',
        'invalidation/invalidator.cc',
        'invalidation/invalidator.h',
        'invalidation/invalidator_registrar.cc',
        'invalidation/invalidator_registrar.h',
        'invalidation/invalidator_state.cc',
        'invalidation/invalidator_state.h',
        'invalidation/invalidator_storage.cc',
        'invalidation/invalidator_storage.h',
        'invalidation/mock_ack_handler.cc',
        'invalidation/mock_ack_handler.h',
        'invalidation/object_id_invalidation_map.cc',
        'invalidation/object_id_invalidation_map.h',
        'invalidation/profile_invalidation_provider.cc',
        'invalidation/profile_invalidation_provider.h',
        'invalidation/single_object_invalidation_set.cc',
        'invalidation/single_object_invalidation_set.h',
        'invalidation/unacked_invalidation_set.cc',
        'invalidation/unacked_invalidation_set.h',
      ],
      'conditions': [
          ['OS != "android"', {
            'sources': [
              # Note: sources list duplicated in GN build.
              'invalidation/gcm_invalidation_bridge.cc',
              'invalidation/gcm_invalidation_bridge.h',
              'invalidation/gcm_network_channel.cc',
              'invalidation/gcm_network_channel.h',
              'invalidation/gcm_network_channel_delegate.h',
              'invalidation/invalidation_notifier.cc',
              'invalidation/invalidation_notifier.h',
              'invalidation/non_blocking_invalidator.cc',
              'invalidation/non_blocking_invalidator.h',
              'invalidation/notifier_reason_util.cc',
              'invalidation/notifier_reason_util.h',
              'invalidation/p2p_invalidator.cc',
              'invalidation/p2p_invalidator.h',
              'invalidation/push_client_channel.cc',
              'invalidation/push_client_channel.h',
              'invalidation/registration_manager.cc',
              'invalidation/registration_manager.h',
              'invalidation/state_writer.h',
              'invalidation/sync_invalidation_listener.cc',
              'invalidation/sync_invalidation_listener.h',
              'invalidation/sync_system_resources.cc',
              'invalidation/sync_system_resources.h',
              'invalidation/ticl_invalidation_service.cc',
              'invalidation/ticl_invalidation_service.h',
              'invalidation/ticl_settings_provider.cc',
              'invalidation/ticl_settings_provider.h',
            ],
          }],
        ['OS == "android"', {
          'dependencies': [
            'invalidation_jni_headers',
          ],
          'sources': [
            'invalidation/android/component_jni_registrar.cc',
            'invalidation/android/component_jni_registrar.h',
            'invalidation/invalidation_service_android.cc',
            'invalidation/invalidation_service_android.h',
          ],
        }],
      ],
    },

    {
      # GN version: //components/invalidation:test_support
      'target_name': 'invalidation_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../jingle/jingle.gyp:notifier',
        '../jingle/jingle.gyp:notifier_test_util',
        '../net/net.gyp:net',
        '../testing/gmock.gyp:gmock',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_proto_cpp',
        'gcm_driver_test_support',
        'keyed_service_core',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'invalidation/fake_invalidation_handler.cc',
        'invalidation/fake_invalidation_handler.h',
        'invalidation/fake_invalidation_state_tracker.cc',
        'invalidation/fake_invalidation_state_tracker.h',
        'invalidation/fake_invalidator.cc',
        'invalidation/fake_invalidator.h',
        'invalidation/invalidation_service_test_template.cc',
        'invalidation/invalidation_service_test_template.h',
        'invalidation/invalidation_test_util.cc',
        'invalidation/invalidation_test_util.h',
        'invalidation/invalidator_test_template.cc',
        'invalidation/invalidator_test_template.h',
        'invalidation/object_id_invalidation_map_test_util.cc',
        'invalidation/object_id_invalidation_map_test_util.h',
        'invalidation/unacked_invalidation_set_test_util.cc',
        'invalidation/unacked_invalidation_set_test_util.h',
      ],
      'conditions': [
          ['OS != "android"', {
            'sources': [
              # Note: sources list duplicated in GN build.
              'invalidation/p2p_invalidation_service.cc',
              'invalidation/p2p_invalidation_service.h',
            ],
          }],
          ['OS == "android"', {
            'dependencies': [
              'invalidation_jni_headers',
            ],
          }],
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'invalidation_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
            '../sync/sync.gyp:sync_java',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_javalib',
          ],
          'variables': {
            'java_in_dir': 'invalidation/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'invalidation_javatests',
          'type': 'none',
          'dependencies': [
            'invalidation_java',
            '../base/base.gyp:base_java_test_support',
          ],
          'variables': {
            'java_in_dir': 'invalidation/android/javatests',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'invalidation_jni_headers',
          'type': 'none',
          'sources': [
            'invalidation/android/java/src/org/chromium/components/invalidation/InvalidationService.java',
          ],
          'variables': {
            'jni_gen_package': 'components/invalidation',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
      ],
     },
    ],
  ],
}
