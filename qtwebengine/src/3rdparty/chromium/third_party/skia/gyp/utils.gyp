# Gyp for utils.
{
  'targets': [
    {
      'target_name': 'utils',
      'product_name': 'skia_utils',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [
        'core.gyp:*',
      ],
      'includes': [
        'utils.gypi',
      ],
      'include_dirs': [
        '../include/effects',
        '../include/images',
        '../include/pathops',
        '../include/pipe',
        '../include/utils',
        '../include/utils/mac',
        '../include/utils/unix',
        '../include/utils/win',
        '../include/xml',
        '../src/core',
        '../src/utils',
      ],
      'sources': [
        'utils.gypi', # Makes the gypi appear in IDEs (but does not modify the build).
      ],
      'sources!': [
          '../src/utils/SDL/SkOSWindow_SDL.cpp',
      ],
      'conditions': [
        [ 'skia_os == "mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AGL.framework',
            ],
          },
        }],
        [ 'skia_os in ["mac", "ios"]', {
          'direct_dependent_settings': {
            'include_dirs': [
              '../include/utils/mac',
            ],
          },
          'sources!': [
            '../src/utils/SkThreadUtils_pthread_other.cpp',
          ],
        },{ #else if 'skia_os != "mac"'
          'include_dirs!': [
            '../include/utils/mac',
          ],
          'sources!': [
            '../include/utils/mac/SkCGUtils.h',
            '../src/utils/mac/SkCreateCGImageRef.cpp',
            '../src/utils/SkThreadUtils_pthread_mach.cpp',
          ],
        }],
        [ 'skia_os in ["linux", "freebsd", "openbsd", "solaris", "chromeos"]', {
          'sources!': [
            '../src/utils/SkThreadUtils_pthread_other.cpp',
          ],
        },{ #else if 'skia_os not in ["linux", "freebsd", "openbsd", "solaris", "chromeos"]'
          'include_dirs!': [
            '../include/utils/unix',
          ],
          'sources!': [
            '../src/utils/SkThreadUtils_pthread_linux.cpp',
          ],
        }],
        [ 'skia_os == "win"', {
          'direct_dependent_settings': {
            'include_dirs': [
              '../include/utils/win',
            ],
          },
          'sources!': [
            '../src/utils/SkThreadUtils_pthread.cpp',
            '../src/utils/SkThreadUtils_pthread.h',
            '../src/utils/SkThreadUtils_pthread_other.cpp',
          ],
        },{ #else if 'skia_os != "win"'
          'include_dirs!': [
            '../include/utils/win',
          ],
          'sources/': [ ['exclude', '_win.(h|cpp)$'],],
          'sources!': [
            '../include/utils/win/SkAutoCoInitialize.h',
            '../include/utils/win/SkHRESULT.h',
            '../include/utils/win/SkIStream.h',
            '../include/utils/win/SkTScopedComPtr.h',
            '../src/utils/win/SkAutoCoInitialize.cpp',
            '../src/utils/win/SkDWrite.h',
            '../src/utils/win/SkDWrite.cpp',
            '../src/utils/win/SkDWriteFontFileStream.cpp',
            '../src/utils/win/SkDWriteFontFileStream.h',
            '../src/utils/win/SkDWriteGeometrySink.cpp',
            '../src/utils/win/SkDWriteGeometrySink.h',
            '../src/utils/win/SkHRESULT.cpp',
            '../src/utils/win/SkIStream.cpp',
          ],
        }],
        [ 'skia_os == "nacl"', {
          'sources': [
            '../src/utils/SkThreadUtils_pthread_other.cpp',
          ],
          'sources!': [
            '../src/utils/SkThreadUtils_pthread_linux.cpp',
          ],
        }],
        ['skia_run_pdfviewer_in_gm', {
          'defines': [
            'SK_BUILD_NATIVE_PDF_RENDERER',
          ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../include/utils',
          '../src/utils',
        ],
      },
    },
  ],
}
