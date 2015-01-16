#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import namedtuple
import coverage
import json
from mock import DEFAULT
from mock import MagicMock
import os
from os import path, sys
import shutil
import tempfile
import unittest

# Requires python-coverage and python-mock. Native python coverage
# version >= 3.7.1 should be installed to get the best speed.

TEST_WORKSPACE = path.join(tempfile.gettempdir(), "test-v8-run-benchmarks")

V8_JSON = {
  "path": ["."],
  "binary": "d7",
  "flags": ["--flag"],
  "main": "run.js",
  "run_count": 1,
  "results_regexp": "^%s: (.+)$",
  "benchmarks": [
    {"name": "Richards"},
    {"name": "DeltaBlue"},
  ]
}

V8_NESTED_SUITES_JSON = {
  "path": ["."],
  "flags": ["--flag"],
  "run_count": 1,
  "units": "score",
  "benchmarks": [
    {"name": "Richards",
     "path": ["richards"],
     "binary": "d7",
     "main": "run.js",
     "resources": ["file1.js", "file2.js"],
     "run_count": 2,
     "results_regexp": "^Richards: (.+)$"},
    {"name": "Sub",
     "path": ["sub"],
     "benchmarks": [
       {"name": "Leaf",
        "path": ["leaf"],
        "run_count_x64": 3,
        "units": "ms",
        "main": "run.js",
        "results_regexp": "^Simple: (.+) ms.$"},
     ]
    },
    {"name": "DeltaBlue",
     "path": ["delta_blue"],
     "main": "run.js",
     "flags": ["--flag2"],
     "results_regexp": "^DeltaBlue: (.+)$"},
    {"name": "ShouldntRun",
     "path": ["."],
     "archs": ["arm"],
     "main": "run.js"},
  ]
}

Output = namedtuple("Output", "stdout, stderr")

class BenchmarksTest(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    cls.base = path.dirname(path.dirname(path.abspath(__file__)))
    sys.path.append(cls.base)
    cls._cov = coverage.coverage(
        include=([os.path.join(cls.base, "run_benchmarks.py")]))
    cls._cov.start()
    import run_benchmarks
    from testrunner.local import commands
    global commands
    global run_benchmarks

  @classmethod
  def tearDownClass(cls):
    cls._cov.stop()
    print ""
    print cls._cov.report()

  def setUp(self):
    self.maxDiff = None
    if path.exists(TEST_WORKSPACE):
      shutil.rmtree(TEST_WORKSPACE)
    os.makedirs(TEST_WORKSPACE)

  def tearDown(self):
    if path.exists(TEST_WORKSPACE):
      shutil.rmtree(TEST_WORKSPACE)

  def _WriteTestInput(self, json_content):
    self._test_input = path.join(TEST_WORKSPACE, "test.json")
    with open(self._test_input, "w") as f:
      f.write(json.dumps(json_content))

  def _MockCommand(self, *args):
    # Fake output for each benchmark run.
    benchmark_outputs = [Output(stdout=arg, stderr=None) for arg in args[1]]
    def execute(*args, **kwargs):
      return benchmark_outputs.pop()
    commands.Execute = MagicMock(side_effect=execute)

    # Check that d8 is called from the correct cwd for each benchmark run.
    dirs = [path.join(TEST_WORKSPACE, arg) for arg in args[0]]
    def chdir(*args, **kwargs):
      self.assertEquals(dirs.pop(), args[0])
    os.chdir = MagicMock(side_effect=chdir)

  def _CallMain(self, *args):
    self._test_output = path.join(TEST_WORKSPACE, "results.json")
    all_args=[
      "--json-test-results",
      self._test_output,
      self._test_input,
    ]
    all_args += args
    return run_benchmarks.Main(all_args)

  def _LoadResults(self):
    with open(self._test_output) as f:
      return json.load(f)

  def _VerifyResults(self, suite, units, traces):
    self.assertEquals([
      {"units": units,
       "graphs": [suite, trace["name"]],
       "results": trace["results"]} for trace in traces],
        self._LoadResults()["traces"])

  def _VerifyErrors(self, errors):
    self.assertEquals(errors, self._LoadResults()["errors"])

  def _VerifyMock(self, binary, *args):
    arg = [path.join(path.dirname(self.base), binary)]
    arg += args
    commands.Execute.assert_called_with(arg, timeout=60)

  def _VerifyMockMultiple(self, *args):
    expected = []
    for arg in args:
      a = [path.join(path.dirname(self.base), arg[0])]
      a += arg[1:]
      expected.append(((a,), {"timeout": 60}))
    self.assertEquals(expected, commands.Execute.call_args_list)

  def testOneRun(self):
    self._WriteTestInput(V8_JSON)
    self._MockCommand(["."], ["x\nRichards: 1.234\nDeltaBlue: 10657567\ny\n"])
    self.assertEquals(0, self._CallMain())
    self._VerifyResults("test", "score", [
      {"name": "Richards", "results": ["1.234"]},
      {"name": "DeltaBlue", "results": ["10657567"]},
    ])
    self._VerifyErrors([])
    self._VerifyMock(path.join("out", "x64.release", "d7"), "--flag", "run.js")

  def testTwoRuns_Units_SuiteName(self):
    test_input = dict(V8_JSON)
    test_input["run_count"] = 2
    test_input["name"] = "v8"
    test_input["units"] = "ms"
    self._WriteTestInput(test_input)
    self._MockCommand([".", "."],
                      ["Richards: 100\nDeltaBlue: 200\n",
                       "Richards: 50\nDeltaBlue: 300\n"])
    self.assertEquals(0, self._CallMain())
    self._VerifyResults("v8", "ms", [
      {"name": "Richards", "results": ["50", "100"]},
      {"name": "DeltaBlue", "results": ["300", "200"]},
    ])
    self._VerifyErrors([])
    self._VerifyMock(path.join("out", "x64.release", "d7"), "--flag", "run.js")

  def testTwoRuns_SubRegexp(self):
    test_input = dict(V8_JSON)
    test_input["run_count"] = 2
    del test_input["results_regexp"]
    test_input["benchmarks"][0]["results_regexp"] = "^Richards: (.+)$"
    test_input["benchmarks"][1]["results_regexp"] = "^DeltaBlue: (.+)$"
    self._WriteTestInput(test_input)
    self._MockCommand([".", "."],
                      ["Richards: 100\nDeltaBlue: 200\n",
                       "Richards: 50\nDeltaBlue: 300\n"])
    self.assertEquals(0, self._CallMain())
    self._VerifyResults("test", "score", [
      {"name": "Richards", "results": ["50", "100"]},
      {"name": "DeltaBlue", "results": ["300", "200"]},
    ])
    self._VerifyErrors([])
    self._VerifyMock(path.join("out", "x64.release", "d7"), "--flag", "run.js")

  def testNestedSuite(self):
    self._WriteTestInput(V8_NESTED_SUITES_JSON)
    self._MockCommand(["delta_blue", "sub/leaf", "richards"],
                      ["DeltaBlue: 200\n",
                       "Simple: 1 ms.\n",
                       "Simple: 2 ms.\n",
                       "Simple: 3 ms.\n",
                       "Richards: 100\n",
                       "Richards: 50\n"])
    self.assertEquals(0, self._CallMain())
    self.assertEquals([
      {"units": "score",
       "graphs": ["test", "Richards"],
       "results": ["50", "100"]},
      {"units": "ms",
       "graphs": ["test", "Sub", "Leaf"],
       "results": ["3", "2", "1"]},
      {"units": "score",
       "graphs": ["test", "DeltaBlue"],
       "results": ["200"]},
      ], self._LoadResults()["traces"])
    self._VerifyErrors([])
    self._VerifyMockMultiple(
        (path.join("out", "x64.release", "d7"), "--flag", "file1.js",
         "file2.js", "run.js"),
        (path.join("out", "x64.release", "d7"), "--flag", "file1.js",
         "file2.js", "run.js"),
        (path.join("out", "x64.release", "d8"), "--flag", "run.js"),
        (path.join("out", "x64.release", "d8"), "--flag", "run.js"),
        (path.join("out", "x64.release", "d8"), "--flag", "run.js"),
        (path.join("out", "x64.release", "d8"), "--flag", "--flag2", "run.js"))

  def testBuildbot(self):
    self._WriteTestInput(V8_JSON)
    self._MockCommand(["."], ["Richards: 1.234\nDeltaBlue: 10657567\n"])
    self.assertEquals(0, self._CallMain("--buildbot"))
    self._VerifyResults("test", "score", [
      {"name": "Richards", "results": ["1.234"]},
      {"name": "DeltaBlue", "results": ["10657567"]},
    ])
    self._VerifyErrors([])
    self._VerifyMock(path.join("out", "Release", "d7"), "--flag", "run.js")

  def testRegexpNoMatch(self):
    self._WriteTestInput(V8_JSON)
    self._MockCommand(["."], ["x\nRichaards: 1.234\nDeltaBlue: 10657567\ny\n"])
    self.assertEquals(1, self._CallMain())
    self._VerifyResults("test", "score", [
      {"name": "Richards", "results": []},
      {"name": "DeltaBlue", "results": ["10657567"]},
    ])
    self._VerifyErrors(
        ["Regexp \"^Richards: (.+)$\" didn't match for benchmark Richards."])
    self._VerifyMock(path.join("out", "x64.release", "d7"), "--flag", "run.js")
