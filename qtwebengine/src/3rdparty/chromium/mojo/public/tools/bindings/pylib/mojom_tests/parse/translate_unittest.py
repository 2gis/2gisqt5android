# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import imp
import os.path
import sys
import unittest

def _GetDirAbove(dirname):
  """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
  path = os.path.abspath(__file__)
  while True:
    path, tail = os.path.split(path)
    assert tail
    if tail == dirname:
      return path

try:
  imp.find_module("mojom")
except ImportError:
  sys.path.append(os.path.join(_GetDirAbove("pylib"), "pylib"))
import mojom.parse.translate as translate


class TranslateTest(unittest.TestCase):
  """Tests |parser.Parse()|."""

  def testSimpleArray(self):
    """Tests a simple int32[]."""
    # pylint: disable=W0212
    self.assertEquals(translate._MapKind("int32[]"), "a:i32")

  def testAssociativeArray(self):
    """Tests a simple uint8{string}."""
    # pylint: disable=W0212
    self.assertEquals(translate._MapKind("uint8{string}"), "m[s][u8]")

  def testLeftToRightAssociativeArray(self):
    """Makes sure that parsing is done from right to left on the internal kinds
       in the presence of an associative array."""
    # pylint: disable=W0212
    self.assertEquals(translate._MapKind("uint8[]{string}"), "m[s][a:u8]")

if __name__ == "__main__":
  unittest.main()
