# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'verbose_libraries_build%': 0,
    'instrumented_libraries_jobs%': 1,
  },

  'ubuntu_release': '<!(lsb_release -cs)',

  'conditions': [
    ['asan==1', {
      'sanitizer_type': 'asan',
    }],
    ['msan==1', {
      'sanitizer_type': 'msan',
    }],
    ['tsan==1', {
      'sanitizer_type': 'tsan',
    }],
    ['use_goma==1', {
      'cc': '<(gomadir)/gomacc <!(cd <(DEPTH) && pwd -P)/<(make_clang_dir)/bin/clang',
      'cxx': '<(gomadir)/gomacc <!(cd <(DEPTH) && pwd -P)/<(make_clang_dir)/bin/clang++',
    }, {
      'cc': '<!(cd <(DEPTH) && pwd -P)/<(make_clang_dir)/bin/clang',
      'cxx': '<!(cd <(DEPTH) && pwd -P)/<(make_clang_dir)/bin/clang++',
    }],
  ],

  'target_defaults': {
    'build_method': 'destdir',
    'extra_configure_flags': [],
    'jobs': '<(instrumented_libraries_jobs)',
    'package_cflags': [
      '-O2',
      '-gline-tables-only',
      '-fPIC',
      '-w',
      '-U_FORITFY_SOURCE',
      '-fno-omit-frame-pointer'
    ],
    'package_ldflags': [
      '-Wl,-z,origin',
      # We set RPATH=XORIGIN when building the package and replace it with
      # $ORIGIN later. The reason is that this flag goes through configure/make
      # differently for different packages. Because of this, we can't escape the
      # $ character in a way that would work for every package.
      '-Wl,-R,XORIGIN/.'
    ],
    'patch': '',
    'run_before_build': '',
    'asan_blacklist': '',
    'msan_blacklist': '',
    'tsan_blacklist': '',

    'conditions': [
      ['asan==1', {
        'package_cflags': ['-fsanitize=address'],
        'package_ldflags': ['-fsanitize=address'],
      }],
      ['msan==1', {
        'package_cflags': [
          '-fsanitize=memory',
          '-fsanitize-memory-track-origins=<(msan_track_origins)'
        ],
        'package_ldflags': ['-fsanitize=memory'],
      }],
      ['tsan==1', {
        'package_cflags': ['-fsanitize=thread'],
        'package_ldflags': ['-fsanitize=thread'],
      }],
    ],
  },

  'targets': [
    {
      'target_name': 'instrumented_libraries',
      'type': 'none',
      'variables': {
        'prune_self_dependency': 1,
        # Don't add this target to the dependencies of targets with type=none.
        'link_dependency': 1,
      },
      'dependencies': [
        '<(_sanitizer_type)-freetype',
        '<(_sanitizer_type)-libcairo2',
        '<(_sanitizer_type)-libexpat1',
        '<(_sanitizer_type)-libffi6',
        '<(_sanitizer_type)-libgcrypt11',
        '<(_sanitizer_type)-libgpg-error0',
        '<(_sanitizer_type)-libnspr4',
        '<(_sanitizer_type)-libp11-kit0',
        '<(_sanitizer_type)-libpcre3',
        '<(_sanitizer_type)-libpng12-0',
        '<(_sanitizer_type)-libx11-6',
        '<(_sanitizer_type)-libxau6',
        '<(_sanitizer_type)-libxcb1',
        '<(_sanitizer_type)-libxcomposite1',
        '<(_sanitizer_type)-libxcursor1',
        '<(_sanitizer_type)-libxdamage1',
        '<(_sanitizer_type)-libxdmcp6',
        '<(_sanitizer_type)-libxext6',
        '<(_sanitizer_type)-libxfixes3',
        '<(_sanitizer_type)-libxi6',
        '<(_sanitizer_type)-libxinerama1',
        '<(_sanitizer_type)-libxrandr2',
        '<(_sanitizer_type)-libxrender1',
        '<(_sanitizer_type)-libxss1',
        '<(_sanitizer_type)-libxtst6',
        '<(_sanitizer_type)-zlib1g',
        '<(_sanitizer_type)-libglib2.0-0',
        '<(_sanitizer_type)-libdbus-1-3',
        '<(_sanitizer_type)-libdbus-glib-1-2',
        '<(_sanitizer_type)-nss',
        '<(_sanitizer_type)-libfontconfig1',
        '<(_sanitizer_type)-pulseaudio',
        '<(_sanitizer_type)-libasound2',
        '<(_sanitizer_type)-pango1.0',
        '<(_sanitizer_type)-libcap2',
        '<(_sanitizer_type)-udev',
        '<(_sanitizer_type)-libgnome-keyring0',
        '<(_sanitizer_type)-libgtk2.0-0',
        '<(_sanitizer_type)-libgdk-pixbuf2.0-0',
        '<(_sanitizer_type)-libpci3',
        '<(_sanitizer_type)-libdbusmenu-glib4',
        '<(_sanitizer_type)-overlay-scrollbar',
        '<(_sanitizer_type)-libgconf-2-4',
        '<(_sanitizer_type)-libappindicator1',
        '<(_sanitizer_type)-libdbusmenu',
        '<(_sanitizer_type)-atk1.0',
        '<(_sanitizer_type)-libunity9',
        '<(_sanitizer_type)-dee',
        '<(_sanitizer_type)-libpixman-1-0',
      ],
      'conditions': [
        ['"<(_ubuntu_release)"=="precise"', {
          'dependencies': [
            '<(_sanitizer_type)-libtasn1-3',
          ],
        }, {
          'dependencies': [
            '<(_sanitizer_type)-libtasn1-6',
          ],
        }],
        ['msan==1', {
          'dependencies': [
            '<(_sanitizer_type)-libcups2',
          ],
        }],
        ['tsan==1', {
          'dependencies!': [
            '<(_sanitizer_type)-libpng12-0',
          ],
        }],
      ],
      'actions': [
        {
          'action_name': 'fix_rpaths',
          'inputs': [
            'fix_rpaths.sh',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/instrumented_libraries/<(_sanitizer_type)/rpaths.fixed.txt',
          ],
          'action': [
            './fix_rpaths.sh',
            '<(PRODUCT_DIR)/instrumented_libraries/<(_sanitizer_type)'
          ],
        },
      ],
      'direct_dependent_settings': {
        'target_conditions': [
          ['_toolset=="target"', {
            'ldflags': [
              # Add RPATH to result binary to make it linking instrumented libraries ($ORIGIN means relative RPATH)
              '-Wl,-R,\$$ORIGIN/instrumented_libraries/<(_sanitizer_type)/lib/:\$$ORIGIN/instrumented_libraries/<(_sanitizer_type)/usr/lib/x86_64-linux-gnu/',
              '-Wl,-z,origin',
            ],
          }],
        ],
      },
    },
    {
      'package_name': 'freetype',
      'dependencies=': [],
      'run_before_build': 'scripts/freetype.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libcairo2',
      'dependencies=': [],
      'extra_configure_flags': ['--disable-gtk-doc'],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libdbus-1-3',
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libdbus-glib-1-2',
      'dependencies=': [],
      # Use system dbus-binding-tool. The just-built one is instrumented but
      # doesn't have the correct RPATH, and will crash.
      'extra_configure_flags': ['--with-dbus-binding-tool=dbus-binding-tool'],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libexpat1',
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libffi6',
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libfontconfig1',
      'dependencies=': [],
      'extra_configure_flags': [
        '--disable-docs',
        '--sysconfdir=/etc/',
        # From debian/rules.
        '--with-add-fonts=/usr/X11R6/lib/X11/fonts,/usr/local/share/fonts',
      ],
      'conditions': [
        ['"<(_ubuntu_release)"=="precise"', {
          'patch': 'patches/libfontconfig.precise.diff',
        }, {
          'patch': 'patches/libfontconfig.trusty.diff',
        }],
      ],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libgcrypt11',
      'dependencies=': [],
      'package_ldflags': ['-Wl,-z,muldefs'],
      'extra_configure_flags': [
        # From debian/rules.
        '--enable-noexecstack',
        '--enable-ld-version-script',
        '--enable-static',
        # http://crbug.com/344505
        '--disable-asm'
      ],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libglib2.0-0',
      'dependencies=': [],
      'extra_configure_flags': [
        '--disable-gtk-doc',
        '--disable-gtk-doc-html',
        '--disable-gtk-doc-pdf',
      ],
      'asan_blacklist': 'blacklists/asan/libglib2.0-0.txt',
      'msan_blacklist': 'blacklists/msan/libglib2.0-0.txt',
      'run_before_build': 'scripts/autogen.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libgpg-error0',
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libnspr4',
      'dependencies=': [],
      'extra_configure_flags': [
        '--enable-64bit',
        # TSan reports data races on debug variables.
        '--disable-debug',
      ],
      'run_before_build': 'scripts/libnspr4.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libp11-kit0',
      'dependencies=': [],
      # Required on Trusty due to autoconf version mismatch.
      'run_before_build': 'scripts/autoreconf.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libpcre3',
      'dependencies=': [],
      'extra_configure_flags': [
        '--enable-utf8',
        '--enable-unicode-properties',
      ],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libpixman-1-0',
      'dependencies=': [],
      'extra_configure_flags': [
        # From debian/rules.
        '--disable-gtk',
        '--disable-silent-rules',
      ],
      'patch': 'patches/libpixman-1-0.diff',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libpng12-0',
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libx11-6',
      'dependencies=': [],
      'extra_configure_flags': ['--disable-specs'],
      'msan_blacklist': 'blacklists/msan/libx11-6.txt',
      # Required on Trusty due to autoconf version mismatch.
      'run_before_build': 'scripts/autoreconf.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libxau6',
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libxcb1',
      'dependencies=': [],
      'extra_configure_flags': ['--disable-build-docs'],
      # Required on Trusty due to autoconf version mismatch.
      'run_before_build': 'scripts/autoreconf.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libxcomposite1',
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libxcursor1',
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libxdamage1',
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libxdmcp6',
      'dependencies=': [],
      'extra_configure_flags': ['--disable-docs'],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libxext6',
      'dependencies=': [],
      'extra_configure_flags': ['--disable-specs'],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libxfixes3',
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libxi6',
      'dependencies=': [],
      'extra_configure_flags': [
        '--disable-specs',
        '--disable-docs',
      ],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libxinerama1',
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libxrandr2',
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libxrender1',
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libxss1',
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libxtst6',
      'dependencies=': [],
      'extra_configure_flags': ['--disable-specs'],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'zlib1g',
      'dependencies=': [],
      'patch': 'patches/zlib1g.diff',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'nss',
      'dependencies=': [
        # TODO(earthdok): get rid of this dependency
        '<(_sanitizer_type)-libnspr4',
      ],
      'patch': 'patches/nss.diff',
      'build_method': 'custom_nss',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'pulseaudio',
      'dependencies=': [],
      'conditions': [
        ['"<(_ubuntu_release)"=="precise"', {
          'patch': 'patches/pulseaudio.precise.diff',
        }],
      ],
      'extra_configure_flags': [
          # From debian/rules.
          '--enable-x11',
          '--disable-hal-compat',
          # Disable some ARM-related code that fails compilation. No idea why
          # this even impacts x86-64 builds.
          '--disable-neon-opt'
      ],
      'run_before_build': 'scripts/pulseaudio.sh',
      'jobs': 1,
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libasound2',
      'dependencies=': [],
      'run_before_build': 'scripts/libasound2.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libcups2',
      'dependencies=': [],
      'patch': 'patches/libcups2.diff',
      'jobs': 1,
      'extra_configure_flags': [
        # All from debian/rules.
        '--localedir=/usr/share/cups/locale',
        '--enable-slp',
        '--enable-libpaper',
        '--enable-ssl',
        '--enable-gnutls',
        '--disable-openssl',
        '--enable-threads',
        '--enable-static',
        '--enable-debug',
        '--enable-dbus',
        '--with-dbusdir=/etc/dbus-1',
        '--enable-gssapi',
        '--enable-avahi',
        '--with-pdftops=/usr/bin/gs',
        '--disable-launchd',
        '--with-cups-group=lp',
        '--with-system-groups=lpadmin',
        '--with-printcap=/var/run/cups/printcap',
        '--with-log-file-perm=0640',
        '--with-local_protocols="CUPS dnssd"',
        '--with-remote_protocols="CUPS dnssd"',
        '--enable-libusb',
      ],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'pango1.0',
      'dependencies=': [],
      'extra_configure_flags': [
        # Avoid https://bugs.gentoo.org/show_bug.cgi?id=425620
        '--enable-introspection=no',
        # Pango is normally used with dynamically loaded modules. However,
        # ensuring pango is able to find instrumented versions of those modules
        # is a huge pain in the neck. Let's link them statically instead, and
        # hope for the best.
        '--with-included-modules=yes'
      ],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libcap2',
      'dependencies=': [],
      'build_method': 'custom_libcap',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'udev',
      'dependencies=': [],
      'extra_configure_flags': [
          # Without this flag there's a linking step that doesn't honor LDFLAGS
          # and fails.
          # TODO(earthdok): find a better fix.
          '--disable-gudev'
      ],
      # Required on Trusty due to autoconf version mismatch.
      'run_before_build': 'scripts/autoreconf.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libtasn1-3',
      'dependencies=': [],
      'extra_configure_flags': [
          # From debian/rules.
          '--enable-ld-version-script',
      ],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libtasn1-6',
      'dependencies=': [],
      'extra_configure_flags': [
          # From debian/rules.
          '--enable-ld-version-script',
      ],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libgnome-keyring0',
      'extra_configure_flags': [
          # Build static libs (from debian/rules).
          '--enable-static',
          '--enable-tests=no',
          # Make the build less problematic.
          '--disable-introspection',
      ],
      'package_ldflags': ['-Wl,--as-needed'],
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libgtk2.0-0',
      'package_cflags': ['-Wno-return-type'],
      'extra_configure_flags': [
          # From debian/rules.
          '--prefix=/usr',
          '--sysconfdir=/etc',
          '--enable-test-print-backend',
          '--enable-introspection=no',
          '--with-xinput=yes',
      ],
      'dependencies=': [],
      'conditions': [
        ['"<(_ubuntu_release)"=="precise"', {
          'patch': 'patches/libgtk2.0-0.precise.diff',
        }, {
          'patch': 'patches/libgtk2.0-0.trusty.diff',
        }],
      ],
      'run_before_build': 'scripts/libgtk2.0-0.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libgdk-pixbuf2.0-0',
      'extra_configure_flags': [
          # From debian/rules.
          '--with-libjasper',
          '--with-x11',
          # Make the build less problematic.
          '--disable-introspection',
      ],
      'dependencies=': [],
      'run_before_build': 'scripts/libgdk-pixbuf2.0-0.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libpci3',
      'dependencies=': [],
      'build_method': 'custom_libpci3',
      'jobs': 1,
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libdbusmenu-glib4',
      'extra_configure_flags': [
          # From debian/rules.
          '--disable-scrollkeeper',
          '--enable-gtk-doc',
          # --enable-introspection introduces a build step that attempts to run
          # a just-built binary and crashes. Vala requires introspection.
          # TODO(earthdok): find a better fix.
          '--disable-introspection',
          '--disable-vala',
      ],
      'dependencies=': [],
      'run_before_build': 'scripts/autogen.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'overlay-scrollbar',
      'extra_configure_flags': [
          '--with-gtk=2',
      ],
      'dependencies=': [],
      'run_before_build': 'scripts/autogen.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libgconf-2-4',
      'extra_configure_flags': [
          # From debian/rules. (Even though --with-gtk=3.0 doesn't make sense.)
          '--with-gtk=3.0',
          '--disable-orbit',
          # See above.
          '--disable-introspection',
      ],
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libappindicator1',
      'extra_configure_flags': [
          # See above.
          '--disable-introspection',
      ],
      'dependencies=': [],
      'jobs': 1,
      'run_before_build': 'scripts/autogen.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libdbusmenu',
      'extra_configure_flags': [
          # From debian/rules.
          '--disable-scrollkeeper',
          '--with-gtk=2',
          # See above.
          '--disable-introspection',
          '--disable-vala',
      ],
      'dependencies=': [],
      'run_before_build': 'scripts/autogen.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'atk1.0',
      'extra_configure_flags': [
          # See above.
          '--disable-introspection',
      ],
      'dependencies=': [],
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'libunity9',
      'dependencies=': [],
      'run_before_build': 'scripts/autogen.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
    {
      'package_name': 'dee',
      'extra_configure_flags': [
          # See above.
          '--disable-introspection',
      ],
      'dependencies=': [],
      'run_before_build': 'scripts/autogen.sh',
      'includes': ['standard_instrumented_package_target.gypi'],
    },
  ],
}
