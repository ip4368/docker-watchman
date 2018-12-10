# vim:ts=4:sw=4:et:
# Copyright 2016-present Facebook, Inc.
# Licensed under the Apache License, Version 2.0

# no unicode literals
from __future__ import absolute_import, division, print_function

import os

import pywatchman
import WatchmanEdenTestCase


def populate(repo):
    repo.write_file(".watchmanconfig", '{"ignore_dirs":[".buckd"]}')
    repo.write_file("hello", "hola\n")
    repo.write_file("adir/file", "foo!\n")
    repo.write_file("bdir/test.sh", "#!/bin/bash\necho test\n", mode=0o755)
    repo.write_file("bdir/noexec.sh", "#!/bin/bash\necho test\n")
    repo.write_file("b*ir/star", "star")
    repo.write_file("b\\*ir/foo", "foo")
    repo.symlink("slink", "hello")
    repo.commit("initial commit.")


class TestEdenPathGenerator(WatchmanEdenTestCase.WatchmanEdenTestCase):
    def test_defer_mtime(self):
        root = self.makeEdenMount(populate)
        res = self.watchmanCommand("watch", root)
        self.assertEqual("eden", res["watcher"])

        # Ensure that the mtime field is loaded for rendering.
        # The expression doesn't use timestamps but the field list does.
        res = self.watchmanCommand(
            "query", root, {"glob": ["bdir/noexec.sh"], "fields": ["name", "mtime"]}
        )
        print(res)
        self.assertEqual(res["files"][0]["name"], "bdir/noexec.sh")
        self.assertGreater(res["files"][0]["mtime"], 0)

    def test_eden_readlink(self):
        root = self.makeEdenMount(populate)
        res = self.watchmanCommand("watch", root)
        self.assertEqual("eden", res["watcher"])
        res = self.watchmanCommand(
            "query",
            root,
            {"expression": ["type", "l"], "fields": ["name", "symlink_target"]},
        )
        print(res)
        self.assertEqual(res["files"][0], {"name": "slink", "symlink_target": "hello"})

    def test_eden_watch(self):
        root = self.makeEdenMount(populate)

        # make sure this exists; we should not observe it in any of the results
        # that we get back from watchman because it is listed in the ignore_dirs
        # config section.
        os.mkdir(os.path.join(root, ".buckd"))

        res = self.watchmanCommand("watch", root)
        self.assertEqual("eden", res["watcher"])
        self.assertFileList(
            root,
            [
                ".eden",
                ".eden/root",
                ".eden/socket",
                ".eden/client",
                ".watchmanconfig",
                "adir",
                "adir/file",
                "bdir",
                "bdir/noexec.sh",
                "bdir/test.sh",
                "b*ir",
                "b*ir/star",
                "b\\*ir",
                "b\\*ir/foo",
                "hello",
                "slink",
            ],
        )

        res = self.watchmanCommand(
            "query", root, {"expression": ["type", "f"], "fields": ["name"]}
        )
        self.assertFileListsEqual(
            res["files"],
            [
                ".watchmanconfig",
                "adir/file",
                "bdir/noexec.sh",
                "bdir/test.sh",
                "b*ir/star",
                "b\\*ir/foo",
                "hello",
            ],
        )

        res = self.watchmanCommand(
            "query", root, {"expression": ["type", "l"], "fields": ["name"]}
        )
        self.assertFileListsEqual(
            res["files"], [".eden/root", ".eden/socket", ".eden/client", "slink"]
        )

        res = self.watchmanCommand(
            "query",
            root,
            {"expression": ["type", "f"], "relative_root": "bdir", "fields": ["name"]},
        )
        self.assertFileListsEqual(res["files"], ["noexec.sh", "test.sh"])

        res = self.watchmanCommand(
            "query",
            root,
            {"expression": ["type", "f"], "fields": ["name"], "glob": ["*.sh"]},
        )
        self.assertFileListsEqual([], res["files"])

        res = self.watchmanCommand(
            "query",
            root,
            {
                "expression": ["type", "f"],
                "fields": ["name"],
                "relative_root": "bdir",
                "glob": ["*.sh"],
            },
        )
        self.assertFileListsEqual(res["files"], ["noexec.sh", "test.sh"])

        res = self.watchmanCommand(
            "query",
            root,
            {"expression": ["type", "f"], "fields": ["name"], "glob": ["**/*.sh"]},
        )
        self.assertFileListsEqual(res["files"], ["bdir/noexec.sh", "bdir/test.sh"])

        # glob_includedotfiles should be False by default.
        res = self.watchmanCommand(
            "query", root, {"fields": ["name"], "glob": ["**/root"]}
        )
        self.assertFileListsEqual(res["files"], [])

        # Verify glob_includedotfiles=True is honored in Eden.
        res = self.watchmanCommand(
            "query",
            root,
            {"fields": ["name"], "glob": ["**/root"], "glob_includedotfiles": True},
        )
        self.assertFileListsEqual(res["files"], [".eden/root"])

        res = self.watchmanCommand("query", root, {"path": [""], "fields": ["name"]})
        self.assertFileListsEqual(
            res["files"],
            [
                ".eden",
                ".eden/root",
                ".eden/socket",
                ".eden/client",
                ".watchmanconfig",
                "adir",
                "adir/file",
                "b*ir",
                "b*ir/star",
                "bdir",
                "bdir/noexec.sh",
                "bdir/test.sh",
                "b\\*ir",
                "b\\*ir/foo",
                "hello",
                "slink",
            ],
        )

        res = self.watchmanCommand(
            "query", root, {"path": [{"path": "bdir", "depth": 0}], "fields": ["name"]}
        )
        self.assertFileListsEqual(res["files"], ["bdir/noexec.sh", "bdir/test.sh"])

        with self.assertRaises(pywatchman.CommandError) as ctx:
            self.watchmanCommand(
                "query",
                root,
                {"path": [{"path": "bdir", "depth": 1}], "fields": ["name"]},
            )
        self.assertIn("only supports depth", str(ctx.exception))

        res = self.watchmanCommand(
            "query", root, {"path": [""], "relative_root": "bdir", "fields": ["name"]}
        )
        self.assertFileListsEqual(res["files"], ["noexec.sh", "test.sh"])

        # Don't wildcard match a name with a * in it
        res = self.watchmanCommand(
            "query", root, {"path": [{"path": "b*ir", "depth": 0}], "fields": ["name"]}
        )
        self.assertFileListsEqual(res["files"], ["b*ir/star"])

        # Check that the globbing stuff does the right thing
        # with a backslash literal here.  Unfortunately, watchman
        # has a slight blindsport with such a path; we're normalizing
        # backslash to a forward slash in the name of portability...
        res = self.watchmanCommand(
            "query",
            root,
            {"path": [{"path": "b\\*ir", "depth": 0}], "fields": ["name"]},
        )
        # ... so the path that gets encoded in the query is
        # "b/*ir" and that gets expanded to "b/\*ir/*" when this
        # is mapped to a glob and passed to eden.  This same
        # path query won't yield the correct set of matches
        # in the non-eden case either.
        self.assertFileListsEqual(res["files"], [])

        res = self.watchmanCommand(
            "query",
            root,
            {"expression": ["type", "f"], "fields": ["name"], "suffix": ["sh", "js"]},
        )
        self.assertFileListsEqual(res["files"], ["bdir/noexec.sh", "bdir/test.sh"])

        res = self.watchmanCommand(
            "query",
            root,
            {"expression": ["type", "f"], "fields": ["name"], "suffix": ["s*"]},
        )
