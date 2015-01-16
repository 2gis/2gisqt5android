#
# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

{
    'includes': [
      'devtools.gypi',
    ],
    'targets': [
        {
            'target_name': 'devtools_frontend_resources',
            'type': 'none',
            'dependencies': [
                'devtools_html',
                'supported_css_properties',
                'frontend_protocol_sources',
                'build_audits_module',
                'build_core_module',
                'build_console_module',
                'build_devices_module',
                'build_elements_module',
                'build_extensions_module',
                'build_layers_module',
                'build_network_module',
                'build_profiler_module',
                'build_resources_module',
                'build_search_module',
                'build_settings_module',
                'build_source_frame_module',
                'build_sources_module',
                'build_timeline_module',
                'build_heap_snapshot_worker_module',
                'build_script_formatter_worker_module',
                'build_temp_storage_shared_worker_module',
            ],
            'conditions': [
                ['debug_devtools==0', {
                    'dependencies': [
                        'concatenated_devtools_css',
                    ],
                }],
            ],
            'copies': [
                {
                    'destination': '<(PRODUCT_DIR)/resources/inspector/Images',
                    'files': [
                        '<@(devtools_image_files)',
                    ],
                },
            ],
        },
        {
            'target_name': 'devtools_html',
            'type': 'none',
            'sources': ['<(PRODUCT_DIR)/resources/inspector/devtools.html'],
            'actions': [{
                'action_name': 'devtools_html',
                'script_name': 'scripts/generate_devtools_html.py',
                'input_page': 'front_end/inspector.html',
                'inputs': [
                    '<@(_script_name)',
                    '<@(_input_page)',
                ],
                'outputs': ['<(PRODUCT_DIR)/resources/inspector/devtools.html'],
                'action': ['python', '<@(_script_name)', '<@(_input_page)', '<@(_outputs)', '<@(debug_devtools)'],
            }],
        },
        {
            'target_name': 'devtools_extension_api',
            'type': 'none',
            'actions': [{
                'action_name': 'devtools_html',
                'script_name': 'scripts/generate_devtools_extension_api.py',
                'inputs': [
                    '<@(_script_name)',
                    '<@(devtools_extension_api_files)',
                ],
                'outputs': ['<(PRODUCT_DIR)/resources/inspector/devtools_extension_api.js'],
                'action': ['python', '<@(_script_name)', '<@(_outputs)', '<@(devtools_extension_api_files)'],
            }],
        },
        {
            'target_name': 'generate_devtools_grd',
            'type': 'none',
            'dependencies': [
                'devtools_html',
                'devtools_extension_api',
                'devtools_frontend_resources',
            ],
            'conditions': [
                ['debug_devtools==0', {
                    'actions': [{
                        'action_name': 'generate_devtools_grd',
                        'script_name': 'scripts/generate_devtools_grd.py',
                        'relative_path_dirs': [
                            '<(PRODUCT_DIR)/resources/inspector',
                            'front_end'
                        ],
                        'input_pages': [
                            '<(PRODUCT_DIR)/resources/inspector/devtools.html',
                            '<(PRODUCT_DIR)/resources/inspector/main/Main.js',
                            '<(PRODUCT_DIR)/resources/inspector/search/AdvancedSearchView.js',
                            '<(PRODUCT_DIR)/resources/inspector/console/ConsolePanel.js',
                            '<(PRODUCT_DIR)/resources/inspector/elements/ElementsPanel.js',
                            '<(PRODUCT_DIR)/resources/inspector/extensions/ExtensionServer.js',
                            '<(PRODUCT_DIR)/resources/inspector/resources/ResourcesPanel.js',
                            '<(PRODUCT_DIR)/resources/inspector/network/NetworkPanel.js',
                            '<(PRODUCT_DIR)/resources/inspector/settings/SettingsScreen.js',
                            '<(PRODUCT_DIR)/resources/inspector/source_frame/SourceFrame.js',
                            '<(PRODUCT_DIR)/resources/inspector/sources/SourcesPanel.js',
                            '<(PRODUCT_DIR)/resources/inspector/timeline/TimelinePanel.js',
                            '<(PRODUCT_DIR)/resources/inspector/profiler/ProfilesPanel.js',
                            '<(PRODUCT_DIR)/resources/inspector/audits/AuditsPanel.js',
                            '<(PRODUCT_DIR)/resources/inspector/layers/LayersPanel.js',
                            '<(PRODUCT_DIR)/resources/inspector/profiler/heap_snapshot_worker/HeapSnapshotWorker.js',
                            '<(PRODUCT_DIR)/resources/inspector/script_formatter_worker/ScriptFormatterWorker.js',
                            '<(PRODUCT_DIR)/resources/inspector/temp_storage_shared_worker/TempStorageSharedWorker.js',
                            '<(PRODUCT_DIR)/resources/inspector/devices/DevicesView.js',
                            '<(PRODUCT_DIR)/resources/inspector/inspector.css',
                            '<(PRODUCT_DIR)/resources/inspector/devtools_extension_api.js',
                            '<@(devtools_standalone_files)',
                            '<@(devtools_cm_css_files)',
                        ],
                        'images': [
                            '<@(devtools_image_files)',
                        ],
                        'inputs': [
                            '<@(_script_name)',
                            '<@(_input_pages)',
                            '<@(_images)',
                        ],
                        'images_path': [
                            'front_end/Images',
                        ],
                        'outputs': ['<(SHARED_INTERMEDIATE_DIR)/devtools/devtools_resources.grd'],
                        'action': ['python', '<@(_script_name)', '<@(_input_pages)', '--relative_path_dirs', '<@(_relative_path_dirs)', '--images', '<@(_images_path)', '--output', '<@(_outputs)'],
                    }],
                },
                {
                    # If we're not concatenating devtools files, we want to
                    # run after the original files have been copied to
                    # <(PRODUCT_DIR)/resources/inspector.
                    'dependencies': ['devtools_frontend_resources'],
                    'actions': [{
                        'action_name': 'generate_devtools_grd',
                        'script_name': 'scripts/generate_devtools_grd.py',
                        'relative_path_dirs': [
                            'front_end',
                            '<(PRODUCT_DIR)/resources/inspector',
                            '<(SHARED_INTERMEDIATE_DIR)/blink',
                        ],
                        'input_pages': [
                            '<@(all_devtools_files)',
                            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorBackendCommands.js',
                            '<(SHARED_INTERMEDIATE_DIR)/blink/SupportedCSSProperties.js',
                            '<(PRODUCT_DIR)/resources/inspector/devtools.html',
                        ],
                        'images': [
                            '<@(devtools_image_files)',
                        ],
                        'inputs': [
                            '<@(_script_name)',
                            '<@(_input_pages)',
                            '<@(_images)',
                        ],
                        'images_path': [
                            'front_end/Images',
                        ],
                        # Note that other files are put under /devtools directory, together with declared devtools_resources.grd
                        'outputs': ['<(SHARED_INTERMEDIATE_DIR)/devtools/devtools_resources.grd'],
                        'action': ['python', '<@(_script_name)', '<@(_input_pages)', '--relative_path_dirs', '<@(_relative_path_dirs)', '--images', '<@(_images_path)', '--output', '<@(_outputs)'],
                    }],
                }],
            ],
        },
        {
          'target_name': 'frontend_protocol_sources',
          'type': 'none',
          'actions': [
            {
              'action_name': 'generateInspectorProtocolFrontendSources',
              'inputs': [
                # The python script in action below.
                'scripts/CodeGeneratorFrontend.py',
                # Input file for the script.
                'protocol.json',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorBackendCommands.js',
              ],
              'action': [
                'python',
                'scripts/CodeGeneratorFrontend.py',
                'protocol.json',
                '--output_js_dir', '<(SHARED_INTERMEDIATE_DIR)/blink',
              ],
              'message': 'Generating Inspector protocol frontend sources from protocol.json',
            },
          ]
        },
        {
          'target_name': 'supported_css_properties',
          'type': 'none',
          'actions': [
            {
              'action_name': 'generateSupportedCSSProperties',
              'inputs': [
                # The python script in action below.
                'scripts/generate_supported_css.py',
                # Input files for the script.
                '../core/css/CSSPropertyNames.in',
                '../core/css/SVGCSSPropertyNames.in',
                '../core/css/CSSShorthands.in',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/blink/SupportedCSSProperties.js',
              ],
              'action': [
                'python',
                '<@(_inputs)',
                '<@(_outputs)',
              ],
              'message': 'Generating supported CSS properties for front end',
            },
          ]
        },
        # Frontend modules.
        {
            'target_name': 'build_core_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'dependencies': [
                        'devtools_html',
                        'supported_css_properties',
                        'frontend_protocol_sources',
                        'concatenated_module_descriptors',
                    ],
                    'actions': [{
                        'action_name': 'build_core_module',
                        'script_name': 'scripts/concatenate_js_files.py',
                        'input_page': 'front_end/inspector.html',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(_input_page)',
                            '<@(devtools_core_js_files)',
                            '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorBackendCommands.js',
                            '<(SHARED_INTERMEDIATE_DIR)/blink/SupportedCSSProperties.js',
                            '<(SHARED_INTERMEDIATE_DIR)/blink/common/modules.js',
                        ],
                        'search_path': [
                            '<(SHARED_INTERMEDIATE_DIR)/blink',
                            'front_end',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/main/Main.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_page)', '<@(_search_path)', '<@(_outputs)'],
                    }]
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector',
                            'files': [
                                '<@(devtools_core_base_js_files)',
                                '<(SHARED_INTERMEDIATE_DIR)/blink/InspectorBackendCommands.js',
                                '<(SHARED_INTERMEDIATE_DIR)/blink/SupportedCSSProperties.js',
                            ],
                        },
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/common',
                            'files': [
                                '<@(devtools_common_js_files)',
                            ],
                        },
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/components',
                            'files': [
                                '<@(devtools_components_js_files)',
                            ],
                        },
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/sdk',
                            'files': [
                                '<@(devtools_sdk_js_files)',
                            ],
                        },
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/ui',
                            'files': [
                                '<@(devtools_ui_js_files)',
                            ],
                        },
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/main',
                            'files': [
                                '<@(devtools_main_js_files)',
                                'front_end/main/module.json',
                            ],
                        },
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_console_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_console_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/console/ConsolePanel.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_console_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/console/ConsolePanel.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/console',
                            'files': [
                                '<@(devtools_console_js_files)',
                                'front_end/console/module.json',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_search_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_search_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/search/AdvancedSearchView.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_search_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/search/AdvancedSearchView.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/search',
                            'files': [
                                '<@(devtools_search_js_files)',
                                'front_end/search/module.json',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_devices_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_devices_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/devices/DevicesView.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_devices_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/devices/DevicesView.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/devices',
                            'files': [
                                '<@(devtools_devices_js_files)',
                                'front_end/devices/module.json',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_elements_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_elements_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/elements/ElementsPanel.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_elements_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/elements/ElementsPanel.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/elements',
                            'files': [
                                '<@(devtools_elements_js_files)',
                                'front_end/elements/module.json',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_resources_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_resources_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/resources/ResourcesPanel.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_resources_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/resources/ResourcesPanel.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/resources',
                            'files': [
                                '<@(devtools_resources_js_files)',
                                'front_end/resources/module.json',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_network_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_network_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/network/NetworkPanel.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_network_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/network/NetworkPanel.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/network',
                            'files': [
                                '<@(devtools_network_js_files)',
                                'front_end/network/module.json',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_extensions_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_extensions_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/extensions/ExtensionServer.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_extensions_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/extensions/ExtensionServer.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/extensions',
                            'files': [
                                '<@(devtools_extensions_js_files)',
                                'front_end/extensions/module.json',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_source_frame_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_source_frame_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/source_frame/SourceFrame.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_source_frame_js_files)',
                            '<@(devtools_cm_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/source_frame/SourceFrame.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/cm',
                            'files': [
                                '<@(devtools_cm_css_files)',
                            ],
                        }
                    ],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/source_frame',
                            'files': [
                                '<@(devtools_source_frame_js_files)',
                                'front_end/source_frame/module.json',
                            ],
                        },
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/cm',
                            'files': [
                                '<@(devtools_cm_js_files)',
                                '<@(devtools_cm_css_files)',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_sources_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_sources_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/sources/SourcesPanel.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_sources_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/sources/SourcesPanel.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/sources',
                            'files': [
                                '<@(devtools_sources_js_files)',
                                'front_end/sources/module.json',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_timeline_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_timeline_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/timeline/TimelinePanel.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_timeline_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/timeline/TimelinePanel.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/timeline',
                            'files': [
                                '<@(devtools_timeline_js_files)',
                                'front_end/timeline/module.json',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_profiler_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_profiler_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/profiler/ProfilesPanel.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_profiler_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/profiler/ProfilesPanel.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/profiler',
                            'files': [
                                '<@(devtools_profiler_js_files)',
                                'front_end/profiler/module.json',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_audits_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_audits_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/audits/AuditsPanel.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_audits_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/audits/AuditsPanel.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/audits',
                            'files': [
                                '<@(devtools_audits_js_files)',
                                'front_end/audits/module.json',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_settings_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_settings_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/settings/SettingsScreen.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_settings_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/settings/SettingsScreen.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/settings',
                            'files': [
                                '<@(devtools_settings_js_files)',
                                'front_end/settings/module.json',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_heap_snapshot_worker_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_heap_snapshot_worker_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/profiler/heap_snapshot_worker/HeapSnapshotWorker.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(_input_file)',
                            '<@(devtools_heap_snapshot_worker_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/profiler/heap_snapshot_worker/HeapSnapshotWorker.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/profiler/heap_snapshot_worker',
                            'files': [
                                '<@(devtools_heap_snapshot_worker_js_files)',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_script_formatter_worker_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_script_formatter_worker_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/script_formatter_worker/ScriptFormatterWorker.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(_input_file)',
                            '<@(devtools_uglify_files)'
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/script_formatter_worker/ScriptFormatterWorker.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/script_formatter_worker',
                            'files': [
                                '<@(devtools_script_formatter_worker_js_files)',
                            ],
                        },
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/UglifyJS',
                            'files': [
                                '<@(devtools_uglify_files)',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_temp_storage_shared_worker_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_temp_storage_shared_worker_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/temp_storage_shared_worker/TempStorageSharedWorker.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_temp_storage_shared_worker_js_files)'
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/temp_storage_shared_worker/TempStorageSharedWorker.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/temp_storage_shared_worker',
                            'files': [
                                '<@(devtools_temp_storage_shared_worker_js_files)',
                            ],
                        }
                    ]
                }]
            ]
        },
        {
            'target_name': 'build_layers_module',
            'type': 'none',
            'conditions': [
                ['debug_devtools==0', { # Release
                    'actions': [{
                        'action_name': 'build_layers_module',
                        'script_name': 'scripts/inline_js_imports.py',
                        'input_file': 'front_end/layers/LayersPanel.js',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(devtools_layers_js_files)',
                        ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/layers/LayersPanel.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)'],
                    }],
                },
                { # Debug
                    'copies': [
                        {
                            'destination': '<(PRODUCT_DIR)/resources/inspector/layers',
                            'files': [
                                '<@(devtools_layers_js_files)',
                                'front_end/layers/module.json',
                            ],
                        }
                    ]
                }]
            ]
        },
    ], # targets
    'conditions': [
        ['debug_devtools==0', {
            'targets': [
                {
                    'target_name': 'concatenated_devtools_css',
                    'type': 'none',
                    'dependencies': [
                        'devtools_html'
                    ],
                    'actions': [{
                        'action_name': 'concatenate_devtools_css',
                        'script_name': 'scripts/concatenate_css_files.py',
                        'input_page': 'front_end/inspector.html',
                        'inputs': [
                            '<@(_script_name)',
                            '<@(_input_page)',
                            '<@(all_devtools_files)',
                        ],
                        'search_path': [ 'front_end' ],
                        'outputs': ['<(PRODUCT_DIR)/resources/inspector/inspector.css'],
                        'action': ['python', '<@(_script_name)', '<@(_input_page)', '<@(_search_path)', '<@(_outputs)'],
                    }],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/resources/inspector',
                        'files': [
                            '<@(devtools_standalone_files)',
                        ],
                    }],
                },
                {
                    'target_name': 'concatenated_module_descriptors',
                    'type': 'none',
                    'actions': [{
                        'action_name': 'concatenated_module_descriptors',
                        'script_name': 'scripts/concatenate_module_descriptors.py',
                        'input_file': ['front_end/common/modules.js'],
                        'inputs': [
                            '<@(_script_name)',
                            '<@(_input_file)',
                            '<@(devtools_module_json_files)',
                        ],
                        'outputs': ['<(SHARED_INTERMEDIATE_DIR)/blink/common/modules.js'],
                        'action': ['python', '<@(_script_name)', '<@(_input_file)', '<@(_outputs)', '<@(devtools_module_json_files)'],
                    }],
                },
            ],
        }],
    ], # conditions
}
