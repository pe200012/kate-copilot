"""Module: git_deps

Helpers to fetch external benchmark repositories.

The eval harness keeps third-party code in `tools/eval/_deps/` so that the main
plugin build stays independent from Python tooling.
"""

from __future__ import annotations

import subprocess
from pathlib import Path


def git_clone(url: str, dest: Path, *, depth: int = 1) -> None:
    if dest.exists():
        return

    dest.parent.mkdir(parents=True, exist_ok=True)

    cmd = ["git", "clone", url, str(dest)]
    if depth > 0:
        cmd = ["git", "clone", "--depth", str(depth), url, str(dest)]

    subprocess.run(cmd, check=True)


def git_head_commit(repo_dir: Path) -> str:
    cp = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=repo_dir,
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    )
    return cp.stdout.strip()
