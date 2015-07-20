# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This turns on e.g. the filename-based detection of which
    # platforms to include source files on (e.g. files ending in
    # _mac.h or _mac.cc are only compiled on MacOSX).
    'chromium_code': 1,
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/components',
   },
  'conditions': [
    ['android_webview_build == 0', {
      'targets': [
        {
          # GN version: //components/components_unittests
          'target_name': 'components_unittests',
          'type': '<(gtest_target_type)',
          'sources': [
            # Note: sources list duplicated in GN build. In the GN build,
            # each component has its own unit tests target defined in its
            # directory that are then linked into the final content_unittests.
            'auto_login_parser/auto_login_parser_unittest.cc',
            'autofill/content/browser/content_autofill_driver_unittest.cc',
            'autofill/content/browser/request_autocomplete_manager_unittest.cc',
            'autofill/content/browser/wallet/full_wallet_unittest.cc',
            'autofill/content/browser/wallet/instrument_unittest.cc',
            'autofill/content/browser/wallet/wallet_address_unittest.cc',
            'autofill/content/browser/wallet/wallet_client_unittest.cc',
            'autofill/content/browser/wallet/wallet_items_unittest.cc',
            'autofill/content/browser/wallet/wallet_service_url_unittest.cc',
            'autofill/content/browser/wallet/wallet_signin_helper_unittest.cc',
            'autofill/core/browser/address_field_unittest.cc',
            'autofill/core/browser/address_i18n_unittest.cc',
            'autofill/core/browser/address_unittest.cc',
            'autofill/core/browser/autocomplete_history_manager_unittest.cc',
            'autofill/core/browser/autofill_country_unittest.cc',
            'autofill/core/browser/autofill_data_model_unittest.cc',
            'autofill/core/browser/autofill_download_manager_unittest.cc',
            'autofill/core/browser/autofill_external_delegate_unittest.cc',
            'autofill/core/browser/autofill_field_unittest.cc',
            'autofill/core/browser/autofill_ie_toolbar_import_win_unittest.cc',
            'autofill/core/browser/autofill_manager_unittest.cc',
            'autofill/core/browser/autofill_merge_unittest.cc',
            'autofill/core/browser/autofill_metrics_unittest.cc',
            'autofill/core/browser/autofill_profile_unittest.cc',
            'autofill/core/browser/autofill_regexes_unittest.cc',
            'autofill/core/browser/autofill_type_unittest.cc',
            'autofill/core/browser/autofill_xml_parser_unittest.cc',
            'autofill/core/browser/contact_info_unittest.cc',
            'autofill/core/browser/credit_card_field_unittest.cc',
            'autofill/core/browser/credit_card_unittest.cc',
            'autofill/core/browser/form_field_unittest.cc',
            'autofill/core/browser/form_structure_unittest.cc',
            'autofill/core/browser/name_field_unittest.cc',
            'autofill/core/browser/password_generator_unittest.cc',
            'autofill/core/browser/personal_data_manager_unittest.cc',
            'autofill/core/browser/phone_field_unittest.cc',
            'autofill/core/browser/phone_number_i18n_unittest.cc',
            'autofill/core/browser/phone_number_unittest.cc',
            'autofill/core/browser/validation_unittest.cc',
            'autofill/core/browser/webdata/autofill_profile_syncable_service_unittest.cc',
            'autofill/core/browser/webdata/autofill_table_unittest.cc',
            'autofill/core/browser/webdata/web_data_service_unittest.cc',
            'autofill/core/common/form_data_unittest.cc',
            'autofill/core/common/form_field_data_unittest.cc',
            'autofill/core/common/password_form_fill_data_unittest.cc',
            'autofill/core/common/save_password_progress_logger_unittest.cc',
            'bookmarks/browser/bookmark_codec_unittest.cc',
            'bookmarks/browser/bookmark_expanded_state_tracker_unittest.cc',
            'bookmarks/browser/bookmark_index_unittest.cc',
            'bookmarks/browser/bookmark_model_unittest.cc',
            'bookmarks/browser/bookmark_utils_unittest.cc',
            'crash/app/crash_keys_win_unittest.cc',
            'captive_portal/captive_portal_detector_unittest.cc',
            'cloud_devices/common/cloud_devices_urls_unittest.cc',
            'cloud_devices/common/printer_description_unittest.cc',
            'component_updater/test/component_patcher_unittest.cc',
            'component_updater/test/component_updater_ping_manager_unittest.cc',
            'component_updater/test/crx_downloader_unittest.cc',
            'component_updater/test/request_sender_unittest.cc',
            'component_updater/test/update_checker_unittest.cc',
            'component_updater/test/update_response_unittest.cc',
            'content_settings/core/browser/content_settings_mock_provider.cc',
            'content_settings/core/browser/content_settings_mock_provider.h',
            'content_settings/core/browser/content_settings_provider_unittest.cc',
            'content_settings/core/browser/content_settings_rule_unittest.cc',
            'content_settings/core/browser/content_settings_utils_unittest.cc',
            'content_settings/core/common/content_settings_pattern_parser_unittest.cc',
            'content_settings/core/common/content_settings_pattern_unittest.cc',
            'crx_file/id_util_unittest.cc',
            'data_reduction_proxy/core/browser/data_reduction_proxy_auth_request_handler_unittest.cc',
            'data_reduction_proxy/core/browser/data_reduction_proxy_config_service_unittest.cc',
            'data_reduction_proxy/core/browser/data_reduction_proxy_interceptor_unittest.cc',
            'data_reduction_proxy/core/browser/data_reduction_proxy_metrics_unittest.cc',
            'data_reduction_proxy/core/browser/data_reduction_proxy_prefs_unittest.cc',
            'data_reduction_proxy/core/browser/data_reduction_proxy_protocol_unittest.cc',
            'data_reduction_proxy/core/browser/data_reduction_proxy_settings_unittest.cc',
            'data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs_unittest.cc',
            'data_reduction_proxy/core/browser/data_reduction_proxy_tamper_detection_unittest.cc',
            'data_reduction_proxy/core/browser/data_reduction_proxy_usage_stats_unittest.cc',
            'data_reduction_proxy/core/common/data_reduction_proxy_headers_unittest.cc',
            'data_reduction_proxy/core/common/data_reduction_proxy_params_unittest.cc',
            'dom_distiller/core/article_entry_unittest.cc',
            'dom_distiller/core/distilled_content_store_unittest.cc',
            'dom_distiller/core/distilled_page_prefs_unittests.cc',
            'dom_distiller/core/distiller_unittest.cc',
            'dom_distiller/core/distiller_url_fetcher_unittest.cc',
            'dom_distiller/core/dom_distiller_model_unittest.cc',
            'dom_distiller/core/dom_distiller_service_unittest.cc',
            'dom_distiller/core/dom_distiller_store_unittest.cc',
            'dom_distiller/core/task_tracker_unittest.cc',
            'dom_distiller/core/url_utils_unittest.cc',
            'dom_distiller/core/viewer_unittest.cc',
            'domain_reliability/config_unittest.cc',
            'domain_reliability/context_unittest.cc',
            'domain_reliability/dispatcher_unittest.cc',
            'domain_reliability/monitor_unittest.cc',
            'domain_reliability/scheduler_unittest.cc',
            'domain_reliability/test_util.cc',
            'domain_reliability/test_util.h',
            'domain_reliability/uploader_unittest.cc',
            'domain_reliability/util_unittest.cc',
            # Note: GN tests converted to here, need to do the rest.
            'enhanced_bookmarks/enhanced_bookmark_model_unittest.cc',
            'enhanced_bookmarks/enhanced_bookmark_utils_unittest.cc',
            'enhanced_bookmarks/image_store_ios_unittest.mm',
            'enhanced_bookmarks/image_store_unittest.cc',
            'enhanced_bookmarks/item_position_unittest.cc',
            'feedback/feedback_common_unittest.cc',
            'feedback/feedback_data_unittest.cc',
            'feedback/feedback_uploader_unittest.cc',
            'gcm_driver/gcm_account_mapper_unittest.cc',
            'gcm_driver/gcm_channel_status_request_unittest.cc',
            'gcm_driver/gcm_client_impl_unittest.cc',
            'gcm_driver/gcm_delayed_task_controller_unittest.cc',
            'gcm_driver/gcm_driver_desktop_unittest.cc',
            'gcm_driver/gcm_stats_recorder_impl_unittest.cc',
            'google/core/browser/google_url_tracker_unittest.cc',
            'google/core/browser/google_util_unittest.cc',
            'history/core/android/android_history_types_unittest.cc',
            'history/core/browser/history_types_unittest.cc',
            'history/core/browser/url_database_unittest.cc',
            'history/core/common/thumbnail_score_unittest.cc',
            'invalidation/invalidation_logger_unittest.cc',
            'json_schema/json_schema_validator_unittest.cc',
            'json_schema/json_schema_validator_unittest_base.cc',
            'json_schema/json_schema_validator_unittest_base.h',
            'keyed_service/content/browser_context_dependency_manager_unittest.cc',
            'keyed_service/core/dependency_graph_unittest.cc',
            'language_usage_metrics/language_usage_metrics_unittest.cc',
            'leveldb_proto/proto_database_impl_unittest.cc',
            'login/screens/screen_context_unittest.cc',
            'metrics/compression_utils_unittest.cc',
            'metrics/daily_event_unittest.cc',
            'metrics/machine_id_provider_win_unittest.cc',
            'metrics/metrics_hashes_unittest.cc',
            'metrics/metrics_log_manager_unittest.cc',
            'metrics/metrics_log_unittest.cc',
            'metrics/metrics_reporting_scheduler_unittest.cc',
            'metrics/metrics_service_unittest.cc',
            'metrics/metrics_state_manager_unittest.cc',
            'metrics/persisted_logs_unittest.cc',
            'metrics/profiler/profiler_metrics_provider_unittest.cc',
            'navigation_interception/intercept_navigation_resource_throttle_unittest.cc',
            'network_time/network_time_tracker_unittest.cc',
            'omaha_query_params/omaha_query_params_unittest.cc',
            'omnibox/answers_cache_unittest.cc',
            'omnibox/base_search_provider_unittest.cc',
            'omnibox/autocomplete_input_unittest.cc',
            'omnibox/autocomplete_match_unittest.cc',
            'omnibox/autocomplete_result_unittest.cc',
            'omnibox/keyword_provider_unittest.cc',
            'omnibox/omnibox_field_trial_unittest.cc',
            'omnibox/suggestion_answer_unittest.cc',
            'os_crypt/ie7_password_win_unittest.cc',
            'os_crypt/keychain_password_mac_unittest.mm',
            'os_crypt/os_crypt_unittest.cc',
            'ownership/owner_key_util_impl_unittest.cc',
            'password_manager/core/browser/browser_save_password_progress_logger_unittest.cc',
            'password_manager/core/browser/log_router_unittest.cc',
            'password_manager/core/browser/login_database_unittest.cc',
            'password_manager/core/browser/password_autofill_manager_unittest.cc',
            'password_manager/core/browser/password_form_manager_unittest.cc',
            'password_manager/core/browser/password_generation_manager_unittest.cc',
            'password_manager/core/browser/password_manager_unittest.cc',
            'password_manager/core/browser/password_store_default_unittest.cc',
            'password_manager/core/browser/password_store_unittest.cc',
            'password_manager/core/browser/password_syncable_service_unittest.cc',
            'password_manager/core/browser/psl_matching_helper_unittest.cc',
            'precache/content/precache_manager_unittest.cc',
            'precache/core/precache_database_unittest.cc',
            'precache/core/precache_fetcher_unittest.cc',
            'precache/core/precache_url_table_unittest.cc',
            'query_parser/query_parser_unittest.cc',
            'query_parser/snippet_unittest.cc',
            'rappor/bloom_filter_unittest.cc',
            'rappor/byte_vector_utils_unittest.cc',
            'rappor/log_uploader_unittest.cc',
            'rappor/rappor_metric_unittest.cc',
            'rappor/rappor_service_unittest.cc',
            'search/search_android_unittest.cc',
            'search/search_unittest.cc',
            'search_engines/default_search_manager_unittest.cc',
            'search_engines/default_search_policy_handler_unittest.cc',
            'search_engines/search_host_to_urls_map_unittest.cc',
            'search_engines/template_url_prepopulate_data_unittest.cc',
            'search_engines/template_url_service_util_unittest.cc',
            'search_engines/template_url_unittest.cc',
            'search_provider_logos/logo_cache_unittest.cc',
            'search_provider_logos/logo_tracker_unittest.cc',
            'sessions/content/content_serialized_navigation_builder_unittest.cc',
            'sessions/content/content_serialized_navigation_driver_unittest.cc',
            'sessions/ios/ios_serialized_navigation_builder_unittest.cc',
            'sessions/ios/ios_serialized_navigation_driver_unittest.cc',
            'sessions/serialized_navigation_entry_unittest.cc',
            'signin/core/browser/account_tracker_service_unittest.cc',
            'signin/core/browser/mutable_profile_oauth2_token_service_unittest.cc',
            'signin/core/browser/signin_error_controller_unittest.cc',
            'signin/core/browser/webdata/token_service_table_unittest.cc',
            'signin/ios/browser/profile_oauth2_token_service_ios_unittest.mm',
            'storage_monitor/image_capture_device_manager_unittest.mm',
            'storage_monitor/media_storage_util_unittest.cc',
            'storage_monitor/media_transfer_protocol_device_observer_linux_unittest.cc',
            'storage_monitor/storage_info_unittest.cc',
            'storage_monitor/storage_monitor_chromeos_unittest.cc',
            'storage_monitor/storage_monitor_linux_unittest.cc',
            'storage_monitor/storage_monitor_mac_unittest.mm',
            'storage_monitor/storage_monitor_unittest.cc',
            'storage_monitor/storage_monitor_win_unittest.cc',
            'suggestions/blacklist_store_unittest.cc',
            'suggestions/image_manager_unittest.cc',
            'suggestions/suggestions_service_unittest.cc',
            'suggestions/suggestions_store_unittest.cc',
            'sync_driver/non_ui_data_type_controller_unittest.cc',
            'sync_driver/data_type_manager_impl_unittest.cc',
            'sync_driver/device_info_data_type_controller_unittest.cc',
            'sync_driver/device_info_sync_service_unittest.cc',
            'sync_driver/generic_change_processor_unittest.cc',
            'sync_driver/model_association_manager_unittest.cc',
            'sync_driver/non_blocking_data_type_controller_unittest.cc',
            'sync_driver/shared_change_processor_unittest.cc',
            'sync_driver/sync_prefs_unittest.cc',
            'sync_driver/system_encryptor_unittest.cc',
            'sync_driver/ui_data_type_controller_unittest.cc',
            'test/run_all_unittests.cc',
            'translate/core/browser/language_state_unittest.cc',
            'translate/core/browser/translate_browser_metrics_unittest.cc',
            'translate/core/browser/translate_prefs_unittest.cc',
            'translate/core/browser/translate_script_unittest.cc',
            'translate/core/common/translate_metrics_unittest.cc',
            'translate/core/common/translate_util_unittest.cc',
            'translate/core/language_detection/language_detection_util_unittest.cc',
            'url_matcher/regex_set_matcher_unittest.cc',
            'url_matcher/string_pattern_unittest.cc',
            'url_matcher/substring_set_matcher_unittest.cc',
            'url_matcher/url_matcher_factory_unittest.cc',
            'url_matcher/url_matcher_unittest.cc',
            'url_fixer/url_fixer_unittest.cc',
            'variations/active_field_trials_unittest.cc',
            'variations/caching_permuted_entropy_provider_unittest.cc',
            'variations/entropy_provider_unittest.cc',
            'variations/metrics_util_unittest.cc',
            'variations/net/variations_http_header_provider_unittest.cc',
            'variations/study_filtering_unittest.cc',
            'variations/variations_associated_data_unittest.cc',
            'variations/variations_seed_processor_unittest.cc',
            'variations/variations_seed_simulator_unittest.cc',
            'visitedlink/test/visitedlink_unittest.cc',
            'web_cache/browser/web_cache_manager_unittest.cc',
            'web_modal/web_contents_modal_dialog_manager_unittest.cc',
            'webdata/common/web_database_migration_unittest.cc',
          ],
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:base_prefs_test_support',
            '../base/base.gyp:test_support_base',
            # TODO(blundell): Eliminate this dependency by having
            # components_unittests have its own pakfile. crbug.com/348563
            '../chrome/chrome_resources.gyp:packed_extra_resources',
            # TODO(blundell): Eliminate the need for this dependency in code
            # that iOS shares. crbug.com/325243
            '../content/content_shell_and_tests.gyp:test_support_content',
            '../sql/sql.gyp:test_support_sql',
            '../sync/sync.gyp:sync',
            '../sync/sync.gyp:test_support_sync_api',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_test_support',

            'components_resources.gyp:components_resources',

            # Dependencies of auto_login_parser
            'components.gyp:auto_login_parser',

            # Dependencies of autofill
            'components.gyp:autofill_core_browser',
            'components.gyp:autofill_core_common',
            'components.gyp:autofill_core_test_support',
            'components_strings.gyp:components_strings',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_util',

            # Dependencies of bookmarks
            'components.gyp:bookmarks_browser',
            'components.gyp:bookmarks_test_support',

            # Dependencies of captive_portal
            'components.gyp:captive_portal_test_support',
            '../net/net.gyp:net_test_support',

            # Dependencies of cloud_devices
            'components.gyp:cloud_devices_common',

            # Dependencies of component_updater
            'components.gyp:component_updater',
            'components.gyp:component_updater_test_support',
            '../third_party/libxml/libxml.gyp:libxml',

            # Dependencies of content_settings
            'components.gyp:content_settings_core_browser',
            'components.gyp:content_settings_core_common',
            'components.gyp:content_settings_core_test_support',

            # Dependencies of crash
            'components.gyp:crash_test_support',

            # Dependencies of crx_file
            'components.gyp:crx_file',

            # Dependencies of data_reduction_proxy
            'components.gyp:data_reduction_proxy_core_browser',
            'components.gyp:data_reduction_proxy_core_common',
            'components.gyp:data_reduction_proxy_test_support',

            # Dependencies of dom_distiller
            'components.gyp:distilled_page_proto',
            'components.gyp:dom_distiller_core',
            'components.gyp:dom_distiller_test_support',

            # Dependencies of domain_reliability
            'components.gyp:domain_reliability',

            # Dependencies of enhanced_bookmarks
            'components.gyp:enhanced_bookmarks',
            'components.gyp:enhanced_bookmarks_test_support',

            # Dependencies of feedback
            'components.gyp:feedback_component',

            # Dependencies of gcm
            'components.gyp:gcm_driver',
            'components.gyp:gcm_driver_test_support',

            # Dependencies of google
            'components.gyp:google_core_browser',

            # Dependencies of history
            'components.gyp:history_core_browser',
            'components.gyp:history_core_common',

            # Dependencies of infobar
            'components.gyp:infobars_test_support',

            # Dependencies of invalidation
            'components.gyp:invalidation',
            'components.gyp:invalidation_test_support',
            '../jingle/jingle.gyp:notifier_test_util',
            '../third_party/libjingle/libjingle.gyp:libjingle',

            # Dependencies of json_schema
            'components.gyp:json_schema',

            # Dependencies of keyed_service
            'components.gyp:keyed_service_core',

            # Dependencies of language_usage_metrics
            'components.gyp:language_usage_metrics',

            # Dependencies of leveldb_proto
            '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
            'components.gyp:leveldb_proto',
            'components.gyp:leveldb_proto_test_support',

            # Dependencies of login
            'components.gyp:login',

            # Dependencies of metrics
            'components.gyp:metrics',
            'components.gyp:metrics_gpu',
            'components.gyp:metrics_net',
            'components.gyp:metrics_profiler',
            'components.gyp:metrics_test_support',

            # Dependencies of network_time
            'components.gyp:network_time',

            # Dependencies of omaha_query_params
            'components.gyp:omaha_query_params',

            # Dependencies of omnibox
            'components.gyp:omnibox',
            'components.gyp:omnibox_test_support',

            # Dependencies of os_crypt
            'components.gyp:os_crypt',

            # Dependencies of ownership
            'components.gyp:ownership',

            # Dependencies of password_manager
            'components.gyp:password_manager_core_browser',
            'components.gyp:password_manager_core_browser_test_support',

            # Dependencies of precache/core
            'components.gyp:password_manager_core_browser',
            'components.gyp:precache_core',

            # Dependencies of pref_registry
            'components.gyp:pref_registry_test_support',

            # Dependencies of query_parser
            'components.gyp:query_parser',

            # Dependencies of rappor
            'components.gyp:rappor',

            # Dependencies of search
            'components.gyp:search',

            # Dependencies of search_engines
            'components.gyp:search_engines',
            'components.gyp:search_engines_test_support',

            # Dependencies of search_provider_logos
            'components.gyp:search_provider_logos',

            # Dependencies of sessions
            '../third_party/protobuf/protobuf.gyp:protobuf_lite',
            'components.gyp:sessions_test_support',

            # Dependencies of signin
            'components.gyp:signin_core_browser',
            'components.gyp:signin_core_browser_test_support',
            '../google_apis/google_apis.gyp:google_apis_test_support',

            # Dependencies of suggestions
            'components.gyp:suggestions',

            # Dependencies of sync_driver
            'components.gyp:sync_driver_test_support',

            # Dependencies of translate.
            'components.gyp:translate_core_browser',
            'components.gyp:translate_core_common',
            'components.gyp:translate_core_language_detection',

            # Dependencies of url_fixer
            'components.gyp:url_fixer',
            '../url/url.gyp:url_lib',

            # Dependencies of variations
            'components.gyp:variations',
            'components.gyp:variations_http_provider',
          ],
          'conditions': [
            ['toolkit_views == 1', {
              'sources': [
                'bookmarks/browser/bookmark_node_data_unittest.cc',
                'constrained_window/constrained_window_views_unittest.cc',
              ],
              'dependencies': [
                '<(DEPTH)/ui/views/views.gyp:views_test_support',
                'components.gyp:constrained_window',
              ]
            }],
            ['OS=="win" and component!="shared_library" and win_use_allocator_shim==1', {
              'dependencies': [
                '<(DEPTH)/base/allocator/allocator.gyp:allocator',
              ],
            }],
            ['OS != "ios"', {
              'sources': [
                'autofill/content/renderer/renderer_save_password_progress_logger_unittest.cc',
                'dom_distiller/content/dom_distiller_viewer_source_unittest.cc',
                'dom_distiller/content/web_contents_main_frame_observer_unittest.cc',
                'error_page/renderer/net_error_helper_core_unittest.cc',
                'metrics/gpu/gpu_metrics_provider_unittest.cc',
                'password_manager/content/browser/content_credential_manager_dispatcher_unittest.cc',
                'power/origin_power_map_unittest.cc',
              ],
              'dependencies': [
                # Dependencies of autofill
                'components.gyp:autofill_content_browser',
                'components.gyp:autofill_content_renderer',
                'components.gyp:autofill_content_test_support',

                # Dependencies of dom_distiller
                'components.gyp:dom_distiller_content',

                # Dependencies of error_page
                'components.gyp:error_page_renderer',

                # Dependencies of
                # intercept_navigation_resource_throttle_unittest.cc
                '../skia/skia.gyp:skia',
                'components.gyp:navigation_interception',

                # Dependencies of keyed_service
                'components.gyp:keyed_service_content',

                # Dependencies of password_manager
                'components.gyp:password_manager_content_browser',
                'components.gyp:password_manager_content_common',

                # Dependencies of precache/content
                'components.gyp:precache_content',

                # Dependencies of power
                'components.gyp:power',

                # Dependencies of sessions
                'components.gyp:sessions_content',

                # Dependencies of storage monitor
                'components.gyp:storage_monitor',
                'components.gyp:storage_monitor_test_support',

                # Dependencies of url_matcher.
                'components.gyp:url_matcher',

                # Dependencies of visitedlink
                'components.gyp:visitedlink_browser',
                'components.gyp:visitedlink_renderer',

                # Dependencies of web_cache
                'components.gyp:web_cache_browser',

                # Dependencies of web_modal
                'components.gyp:web_modal',
                'components.gyp:web_modal_test_support',
              ],
            }, { # 'OS == "ios"'
              'includes': ['../chrome/chrome_ios_bundle_resources.gypi'],
              'sources/': [
                ['exclude', '\\.cc$'],
                ['exclude', '\\.mm$'],
                ['include', '^test/run_all_unittests\\.cc$'],
                ['include', '^auto_login_parser/'],
                ['include', '^autofill/core/'],
                ['include', '^bookmarks/'],
                ['include', '^component_updater/'],
                ['include', '^crash/'],
                ['include', '^content_settings/'],
                ['include', '^data_reduction_proxy/'],
                ['include', '^dom_distiller/'],
                ['include', '^enhanced_bookmarks/'],
                ['include', '^gcm_driver/'],
                ['include', '^google/'],
                ['include', '^history/'],
                ['include', '^invalidation/'],
                ['include', '^json_schema/'],
                ['include', '^keyed_service/core/'],
                ['include', '^language_usage_metrics/'],
                ['include', '^leveldb_proto/'],
                ['include', '^metrics/'],
                ['include', '^network_time/'],
                ['include', '^password_manager/'],
                ['include', '^precache/core/'],
                ['include', '^query_parser/'],
                ['include', '^search/'],
                ['include', '^search_engines/'],
                ['include', '^search_provider_logos/'],
                ['include', '^sessions/ios/'],
                ['include', '^sessions/serialized_navigation_entry_unittest\\.cc$'],
                ['exclude', '^signin/core/browser/mutable_profile_oauth2_token_service_unittest\\.cc$'],
                ['include', '^suggestions/'],
                ['include', '^sync_driver/'],
                ['include', '^translate/'],
                ['include', '^url_fixer/'],
                ['include', '^variations/'],
              ],
              'dependencies': [
                # Dependencies of sessions
                'components.gyp:sessions_ios',

                # Dependencies of signin
                'components.gyp:signin_ios_browser',
                '../ios/ios_tests.gyp:test_support_ios',
              ],
              'actions': [
                {
                  'action_name': 'copy_test_data',
                  'variables': {
                    'test_data_files': [
                      'test/data',
                    ],
                    'test_data_prefix': 'components',
                  },
                  'includes': [ '../build/copy_test_data_ios.gypi' ],
                },
              ],
              'conditions': [
                ['configuration_policy==1', {
                  'sources/': [
                    ['include', '^policy/'],
                  ],
                }],
              ],
            }],
            ['disable_nacl==0', {
              'includes': [
                'nacl/nacl_defines.gypi',
              ],
              'defines': [
                '<@(nacl_defines)',
              ],
              'sources': [
                'nacl/browser/nacl_file_host_unittest.cc',
                'nacl/browser/nacl_process_host_unittest.cc',
                'nacl/browser/nacl_validation_cache_unittest.cc',
                'nacl/browser/pnacl_host_unittest.cc',
                'nacl/browser/pnacl_translation_cache_unittest.cc',
                'nacl/browser/test_nacl_browser_delegate.cc',
                'nacl/zygote/nacl_fork_delegate_linux_unittest.cc',
              ],
              'dependencies': [
                'nacl.gyp:nacl_browser',
                'nacl.gyp:nacl_common',
              ],
            }],
            ['OS == "mac"', {
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/AddressBook.framework',
                  '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
                  '$(SDKROOT)/System/Library/Frameworks/ImageCaptureCore.framework',
                ],
              },
              'sources!': [
                'password_manager/core/browser/password_store_default_unittest.cc',
              ],
            }],
            ['OS == "android"', {
              'sources': [
                'invalidation/invalidation_service_android_unittest.cc',
              ],
              'sources!': [
                'gcm_driver/gcm_account_mapper_unittest.cc',
                'gcm_driver/gcm_channel_status_request_unittest.cc',
                'gcm_driver/gcm_client_impl_unittest.cc',
                'gcm_driver/gcm_delayed_task_controller_unittest.cc',
                'gcm_driver/gcm_driver_desktop_unittest.cc',
                'gcm_driver/gcm_stats_recorder_impl_unittest.cc',
                'feedback/feedback_common_unittest.cc',
                'feedback/feedback_data_unittest.cc',
                'feedback/feedback_uploader_unittest.cc',
                'signin/core/browser/mutable_profile_oauth2_token_service_unittest.cc',
                'storage_monitor/media_storage_util_unittest.cc',
                'storage_monitor/storage_info_unittest.cc',
                'storage_monitor/storage_monitor_unittest.cc',
                'web_modal/web_contents_modal_dialog_manager_unittest.cc',
              ],
              'dependencies': [
                '../testing/android/native_test.gyp:native_test_native_code',
                'components.gyp:history_core_android',
              ],
              'dependencies!': [
                'components.gyp:feedback_component',
                'components.gyp:storage_monitor',
                'components.gyp:storage_monitor_test_support',
                'components.gyp:web_modal',
                'components.gyp:web_modal_test_support',
              ],
            }],
            ['OS != "android"', {
              'sources': [
                'invalidation/fake_invalidator_unittest.cc',
                'invalidation/gcm_network_channel_unittest.cc',
                'invalidation/invalidation_notifier_unittest.cc',
                'invalidation/invalidator_registrar_unittest.cc',
                'invalidation/non_blocking_invalidator_unittest.cc',
                'invalidation/object_id_invalidation_map_unittest.cc',
                'invalidation/p2p_invalidator_unittest.cc',
                'invalidation/push_client_channel_unittest.cc',
                'invalidation/registration_manager_unittest.cc',
                'invalidation/single_object_invalidation_set_unittest.cc',
                'invalidation/sync_invalidation_listener_unittest.cc',
                'invalidation/sync_system_resources_unittest.cc',
                'invalidation/ticl_invalidation_service_unittest.cc',
                'invalidation/unacked_invalidation_set_unittest.cc',
              ],
            }],
            ['OS != "ios" and OS != "android"', {
              'sources': [
                'copresence/handlers/audio/audio_directive_handler_unittest.cc',
                'copresence/handlers/audio/audio_directive_list_unittest.cc',
                'copresence/handlers/directive_handler_unittest.cc',
                'copresence/handlers/gcm_handler_unittest.cc',
                'copresence/mediums/audio/audio_manager_unittest.cc',
                'copresence/mediums/audio/audio_player_unittest.cc',
                'copresence/mediums/audio/audio_recorder_unittest.cc',
                'copresence/rpc/http_post_unittest.cc',
                'copresence/rpc/rpc_handler_unittest.cc',
                'copresence/timed_map_unittest.cc',
                'proximity_auth/base64url_unittest.cc',
                'proximity_auth/bluetooth_connection_unittest.cc',
                'proximity_auth/bluetooth_connection_finder_unittest.cc',
                'proximity_auth/client_unittest.cc',
                'proximity_auth/connection_unittest.cc',
                'proximity_auth/cryptauth/cryptauth_api_call_flow_unittest.cc',
                'proximity_auth/proximity_auth_system_unittest.cc',
                'proximity_auth/remote_status_update_unittest.cc',
                'proximity_auth/wire_message_unittest.cc',
              ],
              'dependencies': [
                # Dependencies for copresence.
                'components.gyp:copresence',
                'components.gyp:copresence_test_support',

                # Dependencies of proxmity_auth
                'components.gyp:proximity_auth',
                'components.gyp:cryptauth',
                '../device/bluetooth/bluetooth.gyp:device_bluetooth_mocks',
              ],
            }],
            ['chromeos==1', {
              'sources': [
                'pairing/message_buffer_unittest.cc',
                'timers/alarm_timer_unittest.cc',
              ],
              'sources!': [
                'storage_monitor/storage_monitor_linux_unittest.cc',
              ],
              'dependencies': [
                'components.gyp:pairing',
                '../chromeos/chromeos.gyp:chromeos_test_support',
              ],
            }],
            ['OS=="linux"', {
              'sources': [
                'metrics/serialization/serialization_utils_unittest.cc',
              ],
              'dependencies': [
                'components.gyp:metrics_serialization',
                '../dbus/dbus.gyp:dbus',
                '../device/media_transfer_protocol/media_transfer_protocol.gyp:device_media_transfer_protocol',
              ],
            }],
            ['OS=="linux" and use_udev==0', {
              'dependencies!': [
                'components.gyp:storage_monitor',
                'components.gyp:storage_monitor_test_support',
              ],
              'sources/': [
                ['exclude', '^storage_monitor/'],
              ],
            }],
            ['OS=="win" and win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
            ['OS=="linux" and component=="shared_library" and use_allocator!="none"', {
            'dependencies': [
                '<(DEPTH)/base/allocator/allocator.gyp:allocator',
            ],
            'link_settings': {
                'ldflags': ['-rdynamic'],
            },
            }],
            ['configuration_policy==1', {
              'dependencies': [
                'components.gyp:policy_component',
                'components.gyp:policy_component_test_support',
                'components.gyp:policy_test_support',
              ],
              'sources': [
                'policy/core/browser/autofill_policy_handler_unittest.cc',
                'policy/core/browser/browser_policy_connector_unittest.cc',
                'policy/core/browser/configuration_policy_handler_unittest.cc',
                'policy/core/browser/configuration_policy_pref_store_unittest.cc',
                'policy/core/browser/managed_bookmarks_tracker_unittest.cc',
                'policy/core/browser/url_blacklist_policy_handler_unittest.cc',
                'policy/core/common/async_policy_provider_unittest.cc',
                'policy/core/common/cloud/cloud_policy_client_unittest.cc',
                'policy/core/common/cloud/cloud_policy_core_unittest.cc',
                'policy/core/common/cloud/cloud_policy_manager_unittest.cc',
                'policy/core/common/cloud/cloud_policy_refresh_scheduler_unittest.cc',
                'policy/core/common/cloud/cloud_policy_service_unittest.cc',
                'policy/core/common/cloud/cloud_policy_validator_unittest.cc',
                'policy/core/common/cloud/component_cloud_policy_service_unittest.cc',
                'policy/core/common/cloud/component_cloud_policy_store_unittest.cc',
                'policy/core/common/cloud/component_cloud_policy_updater_unittest.cc',
                'policy/core/common/cloud/device_management_service_unittest.cc',
                'policy/core/common/cloud/external_policy_data_fetcher_unittest.cc',
                'policy/core/common/cloud/external_policy_data_updater_unittest.cc',
                'policy/core/common/cloud/policy_header_io_helper_unittest.cc',
                'policy/core/common/cloud/policy_header_service_unittest.cc',
                'policy/core/common/cloud/resource_cache_unittest.cc',
                'policy/core/common/cloud/user_cloud_policy_manager_unittest.cc',
                'policy/core/common/cloud/user_cloud_policy_store_unittest.cc',
                'policy/core/common/cloud/user_info_fetcher_unittest.cc',
                'policy/core/common/config_dir_policy_loader_unittest.cc',
                'policy/core/common/forwarding_policy_provider_unittest.cc',
                'policy/core/common/generate_policy_source_unittest.cc',
                'policy/core/common/policy_bundle_unittest.cc',
                'policy/core/common/policy_loader_ios_unittest.mm',
                'policy/core/common/policy_loader_mac_unittest.cc',
                'policy/core/common/policy_loader_win_unittest.cc',
                'policy/core/common/policy_map_unittest.cc',
                'policy/core/common/policy_provider_android_unittest.cc',
                'policy/core/common/policy_service_impl_unittest.cc',
                'policy/core/common/policy_statistics_collector_unittest.cc',
                'policy/core/common/preg_parser_win_unittest.cc',
                'policy/core/common/registry_dict_win_unittest.cc',
                'policy/core/common/schema_map_unittest.cc',
                'policy/core/common/schema_registry_unittest.cc',
                'policy/core/common/schema_unittest.cc',
                'search_engines/default_search_policy_handler_unittest.cc',
              ],
              'conditions': [
                ['OS=="android"', {
                  'sources/': [
                    ['exclude', '^policy/core/common/async_policy_provider_unittest\\.cc'],
                  ],
                }],
                ['OS=="android" or OS=="ios"', {
                  # Note: 'sources!' is processed before any 'sources/', so the
                  # ['include', '^policy/'] on iOS above will include all of the
                  # policy source files again. Using 'source/' here too will get
                  # these files excluded as expected.
                  'sources/': [
                    ['exclude', '^policy/core/common/cloud/component_cloud_policy_service_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/component_cloud_policy_store_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/component_cloud_policy_updater_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/external_policy_data_fetcher_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/external_policy_data_updater_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/resource_cache_unittest\\.cc'],
                    ['exclude', '^policy/core/common/config_dir_policy_loader_unittest\\.cc'],
                  ],
                }],
                ['chromeos==1', {
                  'sources': [
                    'policy/core/common/proxy_policy_provider_unittest.cc',
                  ],
                  'sources!': [
                    'policy/core/common/cloud/user_cloud_policy_manager_unittest.cc',
                    'policy/core/common/cloud/user_cloud_policy_store_unittest.cc',
                  ],
                }],
                ['OS=="ios" or OS=="mac"', {
                  'sources': [
                    'policy/core/common/mac_util_unittest.cc',
                  ],
                }],
              ],
            }],
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
      ],
    }],
    ['OS != "ios" and android_webview_build == 0', {
      'targets': [
        {
          'target_name': 'components_perftests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_perf',
            '../content/content_shell_and_tests.gyp:test_support_content',
            '../testing/gtest.gyp:gtest',
            'components.gyp:visitedlink_browser',
          ],
         'include_dirs': [
           '..',
         ],
         'sources': [
           'visitedlink/test/visitedlink_perftest.cc',
         ],
         'conditions': [
           ['OS == "android"', {
             'dependencies': [
               '../testing/android/native_test.gyp:native_test_native_code',
             ],
           }],
           ['OS=="win" and component!="shared_library" and win_use_allocator_shim==1', {
             'dependencies': [
               '<(DEPTH)/base/allocator/allocator.gyp:allocator',
             ],
           }],
         ],
         # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
         'msvs_disabled_warnings': [ 4267, ],
        },
      ],
      'conditions': [
        ['OS == "android"', {
          'targets': [
            {
              'target_name': 'components_unittests_apk',
              'type': 'none',
              'dependencies': [
                'components_unittests',
                'components.gyp:invalidation_java',
              ],
              'variables': {
                'test_suite_name': 'components_unittests',
              },
              'includes': [ '../build/apk_test.gypi' ],
            },
          ],
        }],
      ],
    }],
    ['OS!="ios"', {
      'targets': [
        {
          'target_name': 'components_browsertests',
          'type': '<(gtest_target_type)',
          'defines!': ['CONTENT_IMPLEMENTATION'],
          'dependencies': [
            'components.gyp:autofill_content_browser',
            'components.gyp:password_manager_content_renderer',
            'components.gyp:pref_registry_test_support',
            'components_resources.gyp:components_resources',
            '../content/content.gyp:content_common',
            '../content/content.gyp:content_gpu',
            '../content/content.gyp:content_plugin',
            '../content/content.gyp:content_renderer',
            '../content/content_shell_and_tests.gyp:content_browser_test_support',
            '../content/content_shell_and_tests.gyp:content_shell_lib',
            '../content/content_shell_and_tests.gyp:content_shell_pak',
            '../content/content_shell_and_tests.gyp:test_support_content',
            '../skia/skia.gyp:skia',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',

            # Dependencies of dom_distiller
            'components.gyp:dom_distiller_content',
            'components.gyp:dom_distiller_core',
            'components_strings.gyp:components_strings',
          ],
          'include_dirs': [
            '..',
          ],
          'defines': [
            'HAS_OUT_OF_PROC_TEST_RUNNER',
          ],
          'sources': [
            'autofill/content/browser/risk/fingerprint_browsertest.cc',
            'dom_distiller/content/distiller_page_web_contents_browsertest.cc',
            'password_manager/content/renderer/credential_manager_client_browsertest.cc',
          ],
          'actions': [
            {
              'action_name': 'repack_components_pack',
              'variables': {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/components/components_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/components/strings/components_strings_en-US.pak',
                ],
                'pak_output': '<(PRODUCT_DIR)/components_resources.pak',
              },
              'includes': [ '../build/repack_action.gypi' ],
            },
          ],
          'conditions': [
            ['OS == "android"', {
              'sources!': [
                'autofill/content/browser/risk/fingerprint_browsertest.cc',
              ],
            }],
            ['OS == "linux"', {
              'sources': [
                  # content_extractor is a standalone content extraction tool built as
                  # a MANUAL component_browsertest.
                  'dom_distiller/standalone/content_extractor.cc',
                ],
            }],
            ['OS=="win"', {
              'resource_include_dirs': [
                '<(SHARED_INTERMEDIATE_DIR)/content/app/resources',
                '<(SHARED_INTERMEDIATE_DIR)/webkit',
              ],
              'sources': [
                '../content/shell/app/resource.h',
                '../content/shell/app/shell.rc',
                # TODO:  It would be nice to have these pulled in
                # automatically from direct_dependent_settings in
                # their various targets (net.gyp:net_resources, etc.),
                # but that causes errors in other targets when
                # resulting .res files get referenced multiple times.
                '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/content/app/strings/content_strings_en-US.rc',
                '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
              ],
              'dependencies': [
                '<(DEPTH)/content/app/resources/content_resources.gyp:content_resources',
                '<(DEPTH)/content/app/strings/content_strings.gyp:content_strings',
                '<(DEPTH)/net/net.gyp:net_resources',
                '<(DEPTH)/third_party/WebKit/public/blink_resources.gyp:blink_resources',
                '<(DEPTH)/third_party/iaccessible2/iaccessible2.gyp:iaccessible2',
                '<(DEPTH)/third_party/isimpledom/isimpledom.gyp:isimpledom',
              ],
              'configurations': {
                'Debug_Base': {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                    },
                  },
                },
              },
              # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
              'msvs_disabled_warnings': [ 4267, ],
            }],
            ['OS=="win" and win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
