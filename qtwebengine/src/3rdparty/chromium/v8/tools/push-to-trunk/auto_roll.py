#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys
import urllib

from common_includes import *
import chromium_roll

CONFIG = {
  PERSISTFILE_BASENAME: "/tmp/v8-auto-roll-tempfile",
}

CR_DEPS_URL = 'http://src.chromium.org/svn/trunk/src/DEPS'

class CheckActiveRoll(Step):
  MESSAGE = "Check active roll."

  @staticmethod
  def ContainsChromiumRoll(changes):
    for change in changes:
      if change["subject"].startswith("Update V8 to"):
        return True
    return False

  def RunStep(self):
    params = {
      "closed": 3,
      "owner": self._options.author,
      "limit": 30,
      "format": "json",
    }
    params = urllib.urlencode(params)
    search_url = "https://codereview.chromium.org/search"
    result = self.ReadURL(search_url, params, wait_plan=[5, 20])
    if self.ContainsChromiumRoll(json.loads(result)["results"]):
      print "Stop due to existing Chromium roll."
      return True


class DetectLastPush(Step):
  MESSAGE = "Detect commit ID of the last push to trunk."

  def RunStep(self):
    push_hash = self.FindLastTrunkPush(include_patches=True)
    self["last_push"] = self.GitSVNFindSVNRev(push_hash)


class DetectLastRoll(Step):
  MESSAGE = "Detect commit ID of the last Chromium roll."

  def RunStep(self):
    # Interpret the DEPS file to retrieve the v8 revision.
    Var = lambda var: '%s'
    exec(self.ReadURL(CR_DEPS_URL))
    last_roll = vars['v8_revision']
    if last_roll >= self["last_push"]:
      print("There is no newer v8 revision than the one in Chromium (%s)."
            % last_roll)
      return True


class RollChromium(Step):
  MESSAGE = "Roll V8 into Chromium."

  def RunStep(self):
    if self._options.roll:
      args = [
        "--author", self._options.author,
        "--reviewer", self._options.reviewer,
        "--chromium", self._options.chromium,
        "--force",
      ]
      if self._options.sheriff:
        args.extend([
            "--sheriff", "--googlers-mapping", self._options.googlers_mapping])
      R = chromium_roll.ChromiumRoll
      self._side_effect_handler.Call(
          R(chromium_roll.CONFIG, self._side_effect_handler).Run,
          args)


class AutoRoll(ScriptsBase):
  def _PrepareOptions(self, parser):
    parser.add_argument("-c", "--chromium", required=True,
                        help=("The path to your Chromium src/ "
                              "directory to automate the V8 roll."))
    parser.add_argument("--roll",
                        help="Make Chromium roll. Dry run if unspecified.",
                        default=False, action="store_true")

  def _ProcessOptions(self, options):  # pragma: no cover
    if not options.reviewer:
      print "A reviewer (-r) is required."
      return False
    if not options.author:
      print "An author (-a) is required."
      return False
    return True

  def _Steps(self):
    return [
      CheckActiveRoll,
      DetectLastPush,
      DetectLastRoll,
      RollChromium,
    ]


if __name__ == "__main__":  # pragma: no cover
  sys.exit(AutoRoll(CONFIG).Run())
