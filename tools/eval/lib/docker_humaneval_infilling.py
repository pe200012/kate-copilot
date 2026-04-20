"""Module: docker_humaneval_infilling

Runs HumanEval-Infilling evaluation inside a Docker container.

The upstream harness executes model-generated code during functional correctness
checks. Container execution keeps the host environment isolated.

Image build context:
- tools/eval/docker/humaneval-infilling/Dockerfile

Upstream repo mounted read-only:
- tools/eval/_deps/human-eval-infilling

Run artifacts mounted read-write:
- tools/eval/runs/...
"""

from __future__ import annotations

import os
import subprocess
from dataclasses import dataclass
from pathlib import Path


DEFAULT_IMAGE = "kate-ai-humaneval-infilling-eval:1.0"


@dataclass(frozen=True)
class DockerEvalLimits:
    cpus: str = "2"
    memory: str = "4g"
    pids_limit: int = 512


def _docker(*args: str, cwd: Path | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(["docker", *args], cwd=cwd, check=True)


def ensure_image_built(project_root: Path, image: str = DEFAULT_IMAGE) -> None:
    try:
        subprocess.run(
            ["docker", "image", "inspect", image],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return
    except subprocess.CalledProcessError:
        pass

    docker_dir = project_root / "tools" / "eval" / "docker" / "humaneval-infilling"
    if not (docker_dir / "Dockerfile").exists():
        raise FileNotFoundError(str(docker_dir / "Dockerfile"))

    _docker("build", "-t", image, str(docker_dir))


def run_evaluation(
    *,
    project_root: Path,
    image: str,
    repo_dir: Path,
    run_dir: Path,
    benchmark_name: str,
    samples_filename: str,
    k: str,
    n_workers: int,
    timeout_s: float,
    limits: DockerEvalLimits = DockerEvalLimits(),
) -> None:
    ensure_image_built(project_root, image=image)

    uid = os.getuid() if hasattr(os, "getuid") else 0
    gid = os.getgid() if hasattr(os, "getgid") else 0

    repo_dir = repo_dir.resolve()
    run_dir = run_dir.resolve()

    container_repo = "/workspace/human-eval-infilling"
    container_run = "/workspace/run"
    container_samples = f"{container_run}/{samples_filename}"

    cmd = [
        "docker",
        "run",
        "--rm",
        "--network",
        "none",
        "--cap-drop",
        "ALL",
        "--security-opt",
        "no-new-privileges",
        "--pids-limit",
        str(limits.pids_limit),
        "--cpus",
        str(limits.cpus),
        "--memory",
        str(limits.memory),
        "--tmpfs",
        "/tmp:rw,nosuid,nodev,size=1024m",
        "--user",
        f"{uid}:{gid}",
        "-v",
        f"{repo_dir}:{container_repo}:ro",
        "-v",
        f"{run_dir}:{container_run}:rw",
        "-e",
        f"PYTHONPATH={container_repo}",
        "-e",
        "PYTHONDONTWRITEBYTECODE=1",
        "-w",
        container_run,
        image,
        "python",
        "-m",
        "human_eval_infilling.evaluate_functional_correctness",
        f"--benchmark_name={benchmark_name}",
        container_samples,
        f"--k=\"{k}\"",
        f"--n_workers={n_workers}",
        f"--timeout={timeout_s}",
    ]

    subprocess.run(cmd, check=True)
