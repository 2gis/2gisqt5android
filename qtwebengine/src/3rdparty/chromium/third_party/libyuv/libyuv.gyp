# Copyright 2011 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    'libyuv.gypi',
  ],
  'variables': {
    'use_system_libjpeg%': 0,
    'build_neon': 0,
    'conditions': [
       [ '(target_arch == "armv7" or target_arch == "armv7s" or (target_arch == "arm" and arm_version >= 7)) and target_subarch != 64 and (arm_neon == 1 or arm_neon_optional == 1)', {
         'build_neon': 1,
       }],
    ],
  },
  'conditions': [
    [ 'build_neon != 0', {
      'targets': [
        # The NEON-specific components.
        {
          'target_name': 'libyuv_neon',
          'type': 'static_library',
          'standalone_static_library': 1,
          # TODO(noahric): This should remove whatever mfpu is set, not
          # just vfpv3-d16.
          'cflags!': [
            '-mfpu=vfp',
            '-mfpu=vfpv3',
            '-mfpu=vfpv3-d16',
          ],
          'cflags': [
            '-mfpu=neon',
          ],
          'include_dirs': [
            'include',
            '.',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'include',
              '.',
            ],
          },
          'sources': [
            # sources.
            'source/compare_neon.cc',
            'source/rotate_neon.cc',
            'source/row_neon.cc',
            'source/scale_neon.cc',
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'libyuv',
      # Change type to 'shared_library' to build .so or .dll files.
      'type': 'static_library',
      # Allows libyuv.a redistributable library without external dependencies.
      'standalone_static_library': 1,
      'conditions': [
        [ 'OS == "ios" and target_subarch == 64', {
          'defines': [
            'LIBYUV_DISABLE_NEON'
          ],
        }],
        # TODO(fbarchard): Use gyp define to enable jpeg.
        [ 'OS != "ios"', {
          'defines': [
            'HAVE_JPEG'
          ],
          'conditions': [
            # Caveat system jpeg support may not support motion jpeg
            [ 'use_system_libjpeg == 1', {
              'dependencies': [
                 '<(DEPTH)/third_party/libjpeg/libjpeg.gyp:libjpeg',
              ],
            }, {
              'dependencies': [
                 '<(DEPTH)/third_party/libjpeg_turbo/libjpeg.gyp:libjpeg',
              ],
            }],
            [ 'use_system_libjpeg == 1', {
              'link_settings': {
                'libraries': [
                  '-ljpeg',
                ],
              }
            }],
          ],
        }],
        [ 'build_neon != 0', {
          'dependencies': [
            'libyuv_neon',
          ],
          'defines': [
            'LIBYUV_NEON',
          ]
        }],
      ],
      'defines': [
        # Enable the following 3 macros to turn off assembly for specified CPU.
        # 'LIBYUV_DISABLE_X86',
        # 'LIBYUV_DISABLE_NEON',
        # 'LIBYUV_DISABLE_MIPS',
        # Enable the following macro to build libyuv as a shared library (dll).
        # 'LIBYUV_USING_SHARED_LIBRARY',
      ],
      'include_dirs': [
        'include',
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
          '.',
        ],
      },
      'sources': [
        '<@(libyuv_sources)',
      ],
    },
  ], # targets.
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
