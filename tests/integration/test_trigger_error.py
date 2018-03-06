# vim:ts=4:sw=4:et:
# Copyright 2012-present Facebook, Inc.
# Licensed under the Apache License, Version 2.0

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
# no unicode literals

import WatchmanTestCase
import pywatchman


@WatchmanTestCase.expand_matrix
class TestTriggerError(WatchmanTestCase.WatchmanTestCase):
    def assertTriggerRegError(self, err, *args):
        with self.assertRaises(pywatchman.WatchmanError) as ctx:
            self.watchmanCommand(*args)
        self.assertRegex(str(ctx.exception), err)

    def test_bad_args(self):
        root = self.mkdtemp()
        self.watchmanCommand('watch', root)

        self.assertTriggerRegError(
            'not enough arguments',
            'trigger', root)

        self.assertTriggerRegError(
            'no command was specified',
            'trigger', root, 'oink')

        self.assertTriggerRegError(
            'no command was specified',
            'trigger', root, 'oink', '--')

        self.assertTriggerRegError(
            'failed to parse query: rule @ position 4 is not a string value',
            'trigger', root, 'oink', '--', 123)

        self.assertTriggerRegError(
            'invalid or missing name',
            'trigger', root, 123)

        self.assertTriggerRegError(
            'invalid or missing name',
            'trigger', root, [])

        self.assertTriggerRegError(
            'invalid or missing name',
            'trigger', root, {'name': 123})

        self.assertTriggerRegError(
            'invalid command array',
            'trigger', root, {'name': 'oink'})

        self.assertTriggerRegError(
            'invalid command array',
            'trigger', root, {
                'name': 'oink',
                'command': []})

        self.assertTriggerRegError(
            'invalid stdin value lemon',
            'trigger', root, {
                'name': 'oink',
                'command': ['cat'],
                'stdin': 'lemon'})

        self.assertTriggerRegError(
            'invalid value for stdin',
            'trigger', root, {
                'name': 'oink',
                'command': ['cat'],
                'stdin': 13})

        self.assertTriggerRegError(
            'max_files_stdin must be >= 0',
            'trigger', root, {
                'name': 'oink',
                'command': ['cat'],
                'max_files_stdin': -1})

        self.assertTriggerRegError(
            'stdout: must be prefixed with either > or >>, got out',
            'trigger', root, {
                'name': 'oink',
                'command': ['cat'],
                'stdout': 'out'})

        self.assertTriggerRegError(
            'stderr: must be prefixed with either > or >>, got out',
            'trigger', root, {
                'name': 'oink',
                'command': ['cat'],
                'stderr': 'out'})
