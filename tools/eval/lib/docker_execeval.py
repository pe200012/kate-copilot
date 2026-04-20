"""Module: docker_execeval

Manages the ExecEval daemon used by SAFIM execution-based evaluation.

Upstream reference (SAFIM README):
- https://github.com/gonglinyuan/safim
- ExecEval: https://github.com/ntunlp/ExecEval

The daemon exposes:
- GET  /api/all_runtimes
- POST /api/execute_code
"""

from __future__ import annotations

import contextlib
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

import requests


EXEC_EVAL_IMAGE = "exec-eval:1.0"


@dataclass(frozen=True)
class ExecEvalConfig:
    execeval_repo_dir: Path
    port: int = 5000
    num_workers: int = 2
    container_name: str = "kate_ai_execeval"
    startup_timeout_s: float = 90.0


def _docker(*args: str, cwd: Path | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(["docker", *args], cwd=cwd, check=True, stdout=subprocess.PIPE, text=True)


def ensure_image_built(cfg: ExecEvalConfig) -> None:
    try:
        _docker("image", "inspect", EXEC_EVAL_IMAGE)
        return
    except subprocess.CalledProcessError:
        pass

    _docker("build", ".", "-t", EXEC_EVAL_IMAGE, cwd=cfg.execeval_repo_dir)


def start_daemon(cfg: ExecEvalConfig) -> None:
    _docker(
        "run",
        "-d",
        "--rm",
        "-p",
        f"{cfg.port}:5000",
        "-e",
        f"NUM_WORKERS={cfg.num_workers}",
        "--name",
        cfg.container_name,
        EXEC_EVAL_IMAGE,
    )


def stop_daemon(cfg: ExecEvalConfig) -> None:
    subprocess.run(["docker", "stop", cfg.container_name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def wait_ready(cfg: ExecEvalConfig) -> None:
    deadline = time.time() + cfg.startup_timeout_s
    url = f"http://localhost:{cfg.port}/api/all_runtimes"

    while time.time() < deadline:
        try:
            r = requests.get(url, timeout=2.0)
            if r.status_code == 200:
                return
        except requests.RequestException:
            pass
        time.sleep(0.5)

    raise RuntimeError(f"ExecEval daemon did not become ready within {cfg.startup_timeout_s}s")


@contextlib.contextmanager
def execeval_daemon(cfg: ExecEvalConfig):
    ensure_image_built(cfg)
    start_daemon(cfg)
    try:
        wait_ready(cfg)
        yield
    finally:
        stop_daemon(cfg)
