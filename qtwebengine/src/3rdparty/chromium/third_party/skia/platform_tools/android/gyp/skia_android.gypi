{
  'targets': [
    {
      'target_name': 'CopySampleAppDeps',
      'type': 'none',
      'dependencies': [
        'skia_lib.gyp:skia_lib',
        'SampleApp.gyp:SampleApp',
      ],
      'variables': {
        'conditions': [
          [ 'skia_arch_type == "x86"', {
            'android_arch%': "x86",
          }, {
            'conditions': [
              [ 'arm_version == 7', {
                'android_arch%': "armeabi-v7a",
              }, {
               'android_arch%': "armeabi",
              }],
            ],
          }],
        ],
      },
      'copies': [
        # Copy all shared libraries into the Android app's libs folder.  Note
        # that this copy requires us to build SkiaAndroidApp after those
        # libraries, so that they exist by the time it occurs.  If there are no
        # libraries to copy, this will cause an error in Make, but the app will
        # still build.
        {
          'destination': '<(PRODUCT_DIR)/android/libs/<(android_arch)',
          'conditions': [
            [ 'skia_shared_lib', {
              'files': [
                '<(SHARED_LIB_DIR)/libSampleApp.so',
                '<(SHARED_LIB_DIR)/libskia_android.so',
              ]}, {
              'files': [
                '<(SHARED_LIB_DIR)/libSampleApp.so',
             ]}
           ],
          ],
        },
      ],
    },
    {
      'target_name': 'skia_launcher',
      'type': 'executable',
      'sources': [
        '../launcher/skia_launcher.cpp',
      ],
    },
    {
      'target_name': 'SampleApp_APK',
      'type': 'none',
      'dependencies': [
        'CopySampleAppDeps',
      ],
      'variables': {
         'ANDROID_SDK_ROOT': '<!(echo $ANDROID_SDK_ROOT)',
         # the ninja generator treats PRODUCT_DIR as a relative path to the
         # gyp directory but android ant build wants a path relative to the
         # build.xml file so we do that adjustment here.
         'ANDROID_OUT': '../../<(PRODUCT_DIR)/android'
       },
      'actions': [
        {
          'action_name': 'SkiaAndroid_apk',
          'inputs': [
            '<(android_base)/app/AndroidManifest.xml',
            '<(android_base)/app/build.xml',
            '<(android_base)/app/project.properties',
            '<(android_base)/app/jni/com_skia_SkiaSampleRenderer.h',
            '<(android_base)/app/jni/com_skia_SkiaSampleRenderer.cpp',
            '<(android_base)/app/src/com/skia/SkiaSampleActivity.java',
            '<(android_base)/app/src/com/skia/SkiaSampleRenderer.java',
            '<(android_base)/app/src/com/skia/SkiaSampleView.java',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/../android/bin/SkiaAndroid.apk',
          ],
          'action': [
            'ant',
            '-quiet',
            '-f',
            '<(android_base)/app/build.xml',
            '-Dout.dir=<(ANDROID_OUT)/bin',
            '-Dgen.absolute.dir=<(ANDROID_OUT)/gen',
            '-Dnative.libs.absolute.dir=<(ANDROID_OUT)/libs',
            '-Dout.final.file=<(ANDROID_OUT)/bin/SkiaAndroid.apk',
            '-Dsdk.dir=<(ANDROID_SDK_ROOT)',
            'debug',
          ],
        },
      ],
    },
  ],
}
