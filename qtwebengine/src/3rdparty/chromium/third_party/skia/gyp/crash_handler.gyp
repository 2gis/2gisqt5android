{
  'targets': [{
      'target_name': 'CrashHandler',
          'type': 'static_library',
          'sources': [ '../tools/CrashHandler.cpp' ],
          'dependencies': [ 'skia_lib.gyp:skia_lib' ],
          'direct_dependent_settings': {
              'include_dirs': [ '../tools' ],
          },
          'all_dependent_settings': {
              'msvs_settings': {
                  'VCLinkerTool': {
                      'AdditionalDependencies': [ 'Dbghelp.lib' ],
                  }
              },
          }
  }]
}
