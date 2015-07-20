# Gyp file for opts projects
{
  'targets': [
    # Due to an unfortunate intersection of lameness between gcc and gyp,
    # we have to build the *_SSE2.cpp files in a separate target.  The
    # gcc lameness is that, in order to compile SSE2 intrinsics code, it
    # must be passed the -msse2 flag.  However, with this flag, it may
    # emit SSE2 instructions even for scalar code, such as the CPUID
    # test used to test for the presence of SSE2.  So that, and all other
    # code must be compiled *without* -msse2.  The gyp lameness is that it
    # does not allow file-specific CFLAGS, so we must create this extra
    # target for those files to be compiled with -msse2.
    #
    # This is actually only a problem on 32-bit Linux (all Intel Macs have
    # SSE2, Linux x86_64 has SSE2 by definition, and MSC will happily emit
    # SSE2 from instrinsics, while generating plain ol' 386 for everything
    # else).  However, to keep the .gyp file simple and avoid platform-specific
    # build breakage, we do this on all platforms.

    # For about the same reason, we need to compile the ARM opts files
    # separately as well.
    {
      'target_name': 'opts',
      'product_name': 'skia_opts',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [
        'core.gyp:*',
        'effects.gyp:*'
      ],
      'include_dirs': [
        '../src/core',
        '../src/opts',
        '../src/utils',
      ],
      'conditions': [
        [ 'skia_arch_type == "x86" and skia_os != "ios"', {
          'conditions': [
            [ 'skia_os in ["linux", "freebsd", "openbsd", "solaris", "nacl", "chromeos", "android"]', {
              'cflags': [
                '-msse2',
              ],
            }],
          ],
          'include_dirs': [
            '../include/utils',
          ],
          'dependencies': [
            'opts_ssse3',
            'opts_sse4',
          ],
          'sources': [
            '../src/opts/opts_check_x86.cpp',
            '../src/opts/SkBitmapProcState_opts_SSE2.cpp',
            '../src/opts/SkBitmapFilter_opts_SSE2.cpp',
            '../src/opts/SkBlitRow_opts_SSE2.cpp',
            '../src/opts/SkBlitRect_opts_SSE2.cpp',
            '../src/opts/SkBlurImage_opts_SSE2.cpp',
            '../src/opts/SkMorphology_opts_SSE2.cpp',
            '../src/opts/SkTextureCompression_opts_none.cpp',
            '../src/opts/SkUtils_opts_SSE2.cpp',
            '../src/opts/SkXfermode_opts_SSE2.cpp',
          ],
        }],
        [ 'skia_arch_type == "arm" and arm_version >= 7', {
          # The assembly uses the frame pointer register (r7 in Thumb/r11 in
          # ARM), the compiler doesn't like that.
          'cflags!': [
            '-fno-omit-frame-pointer',
            '-mapcs-frame',
            '-mapcs',
          ],
          'cflags': [
            '-fomit-frame-pointer',
          ],
          'variables': {
            'arm_neon_optional%': '<(arm_neon_optional>',
          },
          'sources': [
            '../src/opts/memset.arm.S',
            '../src/opts/SkBitmapProcState_opts_arm.cpp',
            '../src/opts/SkBlitMask_opts_arm.cpp',
            '../src/opts/SkBlitRow_opts_arm.cpp',
            '../src/opts/SkBlurImage_opts_arm.cpp',
            '../src/opts/SkMorphology_opts_arm.cpp',
            '../src/opts/SkTextureCompression_opts_arm.cpp',
            '../src/opts/SkUtils_opts_arm.cpp',
            '../src/opts/SkXfermode_opts_arm.cpp',
          ],
          'conditions': [
            [ 'arm_neon == 1 or arm_neon_optional == 1', {
              'dependencies': [
                'opts_neon',
              ]
            }],
            [ 'skia_os == "ios"', {
              'sources!': [
                # these fail to compile under xcode for ios
                '../src/opts/memset.arm.S',
                '../src/opts/SkBitmapProcState_opts_arm.cpp',
                '../src/opts/SkBlitRow_opts_arm.cpp',
              ],
            }],
          ],
        }],
        [ 'skia_arch_type == "mips"', {
          'sources': [
            '../src/opts/SkBlitMask_opts_none.cpp',
            '../src/opts/SkBlurImage_opts_none.cpp',
            '../src/opts/SkMorphology_opts_none.cpp',
            '../src/opts/SkUtils_opts_none.cpp',
            '../src/opts/SkTextureCompression_opts_none.cpp',
            '../src/opts/SkXfermode_opts_none.cpp',
          ],
          'conditions': [
            [ '(mips_arch_variant == "mips32r2") \
                and (mips_dsp == 1 or mips_dsp == 2)', {
              'sources': [
                '../src/opts/SkBitmapProcState_opts_mips_dsp.cpp',
                '../src/opts/SkBlitRow_opts_mips_dsp.cpp',
              ],
            }, {
              'sources': [
                '../src/opts/SkBitmapProcState_opts_none.cpp',
                '../src/opts/SkBlitRow_opts_none.cpp',
              ],
            }],
          ],
        }],
        [ '(skia_arch_type == "arm" and arm_version < 7) \
            or (skia_os == "ios") \
            or (skia_os == "android" and skia_arch_type not in ["x86", "arm", "mips", "arm64"])', {
          'sources': [
            '../src/opts/SkBitmapProcState_opts_none.cpp',
            '../src/opts/SkBlitMask_opts_none.cpp',
            '../src/opts/SkBlitRow_opts_none.cpp',
            '../src/opts/SkBlurImage_opts_none.cpp',
            '../src/opts/SkMorphology_opts_none.cpp',
            '../src/opts/SkUtils_opts_none.cpp',
            '../src/opts/SkTextureCompression_opts_none.cpp',
            '../src/opts/SkXfermode_opts_none.cpp',
          ],
        }],
        [ 'skia_android_framework', {
          'cflags!': [
            '-msse2',
            '-mfpu=neon',
            '-fomit-frame-pointer',
          ]
        }],
        [ 'skia_arch_type == "arm64"', {
          'sources': [
            '../src/opts/SkBitmapProcState_arm_neon.cpp',
            '../src/opts/SkBitmapProcState_matrixProcs_neon.cpp',
            '../src/opts/SkBitmapProcState_opts_arm.cpp',
            '../src/opts/SkBlitMask_opts_arm.cpp',
            '../src/opts/SkBlitMask_opts_arm_neon.cpp',
            '../src/opts/SkBlitRow_opts_arm.cpp',
            '../src/opts/SkBlitRow_opts_arm_neon.cpp',
            '../src/opts/SkBlurImage_opts_arm.cpp',
            '../src/opts/SkBlurImage_opts_neon.cpp',
            '../src/opts/SkMorphology_opts_arm.cpp',
            '../src/opts/SkMorphology_opts_neon.cpp',
            '../src/opts/SkTextureCompression_opts_none.cpp',
            '../src/opts/SkUtils_opts_none.cpp',
            '../src/opts/SkXfermode_opts_arm.cpp',
            '../src/opts/SkXfermode_opts_arm_neon.cpp',
          ],
        }],
      ],
    },
    # For the same lame reasons as what is done for skia_opts, we have to
    # create another target specifically for SSSE3 code as we would not want
    # to compile the SSE2 code with -mssse3 which would potentially allow
    # gcc to generate SSSE3 code.
    {
      'target_name': 'opts_ssse3',
      'product_name': 'skia_opts_ssse3',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [
        'core.gyp:*',
        'effects.gyp:*'
      ],
      'include_dirs': [
        '../src/core',
        '../src/utils',
      ],
      'sources': [
        '../src/opts/SkBitmapProcState_opts_SSSE3.cpp',
      ],
      'conditions': [
        [ 'skia_os == "win"', {
            'defines' : [ 'SK_CPU_SSE_LEVEL=31' ],
        }],
        # (Mac has -mssse3 globally.)
        [ 'skia_os in ["linux", "freebsd", "openbsd", "solaris", "nacl", "chromeos", "android"] \
           and not skia_android_framework', {
          'cflags': [
            '-mssse3',
          ],
        }],
      ],
    },
    # For the same lame reasons as what is done for skia_opts, we also have to
    # create another target specifically for SSE4 code as we would not want
    # to compile the SSE2 code with -msse4 which would potentially allow
    # gcc to generate SSE4 code.
    {
      'target_name': 'opts_sse4',
      'product_name': 'skia_opts_sse4',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [
        'core.gyp:*',
        'effects.gyp:*'
      ],
      'include_dirs': [
        '../src/core',
        '../src/utils',
      ],
      'sources': [
        '../src/opts/SkBlurImage_opts_SSE4.cpp',
      ],
      'conditions': [
        [ 'skia_arch_width == 64', {
          'sources': [
            '../src/opts/SkBlitRow_opts_SSE4_x64_asm.S',
          ],
        }],
        [ 'skia_arch_width == 32', {
          'sources': [
            '../src/opts/SkBlitRow_opts_SSE4_asm.S',
          ],
        }],
        [ 'skia_os == "win"', {
            'defines' : [ 'SK_CPU_SSE_LEVEL=41' ],
        }],
        [ 'skia_os in ["linux", "freebsd", "openbsd", "solaris", "nacl", "chromeos", "android"] \
           and not skia_android_framework', {
          'cflags': [
            '-msse4.1',
          ],
        }],
        [ 'skia_os == "mac"', {
          'xcode_settings': {
            'OTHER_CPLUSPLUSFLAGS!': [
              '-mssse3',
            ],
            'OTHER_CPLUSPLUSFLAGS': [
              '-msse4.1',
            ],
          },
        }],
      ],
    },
    # NEON code must be compiled with -mfpu=neon which also affects scalar
    # code. To support dynamic NEON code paths, we need to build all
    # NEON-specific sources in a separate static library. The situation
    # is very similar to the SSSE3 and SSE4 one.
    {
      'target_name': 'opts_neon',
      'product_name': 'skia_opts_neon',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [
        'core.gyp:*',
        'effects.gyp:*'
      ],
      'include_dirs': [
        '../src/core',
        '../src/opts',
        '../src/utils',
      ],
      'cflags!': [
        '-fno-omit-frame-pointer',
        '-mfpu=vfp',  # remove them all, just in case.
        '-mfpu=vfpv3',
        '-mfpu=vfpv3-d16',
      ],
      'conditions': [
        [ 'not skia_android_framework', {
          'cflags': [
            '-mfpu=neon',
            '-fomit-frame-pointer',
          ],
        }],
      ],
      'ldflags': [
        '-march=armv7-a',
        '-Wl,--fix-cortex-a8',
      ],
      'sources': [
        '../src/opts/memset16_neon.S',
        '../src/opts/memset32_neon.S',
        '../src/opts/SkBitmapProcState_arm_neon.cpp',
        '../src/opts/SkBitmapProcState_matrixProcs_neon.cpp',
        '../src/opts/SkBitmapProcState_matrix_neon.h',
        '../src/opts/SkBlitMask_opts_arm_neon.cpp',
        '../src/opts/SkBlitRow_opts_arm_neon.cpp',
        '../src/opts/SkBlurImage_opts_neon.cpp',
        '../src/opts/SkMorphology_opts_neon.cpp',
        '../src/opts/SkTextureCompression_opts_neon.cpp',
        '../src/opts/SkXfermode_opts_arm_neon.cpp',
      ],
    },
  ],
}
