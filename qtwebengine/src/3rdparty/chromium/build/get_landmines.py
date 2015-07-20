#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This file emits the list of reasons why a particular build needs to be clobbered
(or a list of 'landmines').
"""

import sys

import landmine_utils


builder = landmine_utils.builder
distributor = landmine_utils.distributor
gyp_defines = landmine_utils.gyp_defines
gyp_msvs_version = landmine_utils.gyp_msvs_version
platform = landmine_utils.platform


def print_landmines():
  """
  ALL LANDMINES ARE EMITTED FROM HERE.
  """
  if (distributor() == 'goma' and platform() == 'win32' and
      builder() == 'ninja'):
    print 'Need to clobber winja goma due to backend cwd cache fix.'
  if platform() == 'android':
    print 'Clobber: To delete newly generated mojo class files.'
    print 'Clobber to ensure that recipe tests do not break (issue 680923002).'
  if platform() == 'win' and builder() == 'ninja':
    print 'Compile on cc_unittests fails due to symbols removed in r185063.'
  if platform() == 'linux' and builder() == 'ninja':
    print 'Builders switching from make to ninja will clobber on this.'
  if platform() == 'mac':
    print 'Switching from bundle to unbundled dylib (issue 14743002).'
  if platform() in ('win', 'mac'):
    print ('Improper dependency for create_nmf.py broke in r240802, '
           'fixed in r240860.')
  if (platform() == 'win' and builder() == 'ninja' and
      gyp_msvs_version() == '2012' and
      gyp_defines().get('target_arch') == 'x64' and
      gyp_defines().get('dcheck_always_on') == '1'):
    print "Switched win x64 trybots from VS2010 to VS2012."
  if (platform() == 'win' and builder() == 'ninja' and
      gyp_msvs_version().startswith('2013')):
    print "Switched win from VS2010 to VS2013."
    print "Update to VS2013 Update 2."
  print 'Need to clobber everything due to an IDL change in r154579 (blink)'
  print 'Need to clobber everything due to gen file moves in r175513 (Blink)'
  if (platform() != 'ios'):
    print 'Clobber to get rid of obselete test plugin after r248358'
    print 'Clobber to rebuild GN files for V8'
  print 'Need to clobber everything due to build_nexe change in nacl r13424'
  print '[chromium-dev] PSA: clobber build needed for IDR_INSPECTOR_* compil...'
  print 'blink_resources.grd changed: crbug.com/400860'
  print 'ninja dependency cycle: crbug.com/408192'
  if platform() == 'android':
    print 'Delete stale generated .java files yet again. crbug.com/349592'
    print 'Clobber to delete incompatible object binary format with NDK r10c'
  print 'Clobber to fix missing NaCl gyp dependencies (crbug.com/427427).'


def main():
  print_landmines()
  return 0


if __name__ == '__main__':
  sys.exit(main())
