#!/usr/bin/env python3
"""Tests for tools/common/file_traversal.py."""

from __future__ import annotations

from pathlib import Path

from common.file_traversal import (
    DEFAULT_SKIP_DIRS,
    find_repo_root,
    should_skip_path,
)


class TestFindRepoRoot:
    def test_find_repo_root(self) -> None:
        root = find_repo_root(Path(__file__))
        assert (root / "src" / "qgc_version.h.in").is_file()

    def test_source_snapshot_in_parent_repository(self, tmp_path: Path) -> None:
        (tmp_path / ".git").mkdir()
        source = tmp_path / "QGroundControl"
        (source / "src").mkdir(parents=True)
        (source / "tools").mkdir()
        (source / "src" / "qgc_version.h.in").touch()
        (source / "tools" / "pyproject.toml").touch()
        assert find_repo_root(source / "tools") == source


class TestShouldSkipPath:
    def test_skip_build(self) -> None:
        assert should_skip_path(Path("/project/build/file.cpp"))
        assert should_skip_path(Path("/project/src/build/nested.h"))

    def test_skip_libs(self) -> None:
        assert should_skip_path(Path("/project/libs/external/file.h"))

    def test_skip_cache(self) -> None:
        assert should_skip_path(Path("/project/.cache/file.cpp"))
        assert should_skip_path(Path("/project/.ccache/file.cpp"))

    def test_no_skip_src(self) -> None:
        assert not should_skip_path(Path("/project/src/file.cpp"))
        assert not should_skip_path(Path("/project/src/Vehicle/Vehicle.h"))


class TestDefaultSkipDirs:
    def test_contents(self) -> None:
        for entry in ("build", "libs", "node_modules", ".git", ".ccache"):
            assert entry in DEFAULT_SKIP_DIRS
