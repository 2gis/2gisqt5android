# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from ui import spy_project
from tvcm import module_test_case


def load_tests(_, _2, _3):
  project = spy_project.SpyProject()
  suite = module_test_case.DiscoverTestsInModule(
      project,
      project.spy_path)
  assert suite.countTestCases() > 0, 'Expected to find at least one test.'
  return suite
