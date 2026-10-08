#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run one staging script's generate() and print its result as JSON.

SConscript calls this when SCons itself runs under Python 2.7 (the SDK's OneStep
commands do), because the staging scripts are Python 3 only.
usage: stage_run.py <stage_script.py> [generate arguments...]
"""
import json
import runpy
import sys

namespace = runpy.run_path(sys.argv[1])
sys.stdout.write(json.dumps(namespace['generate'](*sys.argv[2:])))
