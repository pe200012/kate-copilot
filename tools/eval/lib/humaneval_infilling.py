"""Module: humaneval_infilling

Runs HumanEval-Infilling generation + evaluation.

Upstream harness:
- https://github.com/openai/human-eval-infilling

This module:
- clones the upstream repo into tools/eval/_deps
- generates samples.jsonl by calling an OpenAI-compatible endpoint (Ollama)
- enables unsafe execution in the upstream harness when explicitly requested
- runs the upstream evaluation entrypoint to produce pass@k and *_results.jsonl
"""

from __future__ import annotations

import json
import sys
from dataclasses import dataclass
from pathlib import Path

from tqdm import tqdm

from .docker_humaneval_infilling import DEFAULT_IMAGE, run_evaluation
from .git_deps import git_clone
from .path_utils import slugify_segment
from .jsonl import read_jsonl
from .ollama_openai_client import (
    ChatCompletionParams,
    TextCompletionParams,
    chat_completion,
    text_completion,
)
from .prompt_templates import BuiltPrompt, PromptContext, build as build_prompt, sanitize_completion


HUMAN_EVAL_INFILLING_REPO = "https://github.com/openai/human-eval-infilling"


@dataclass(frozen=True)
class HumanEvalRunConfig:
    deps_dir: Path
    out_dir: Path

    endpoint: str
    model: str
    api_key: str

    template_id: str
    benchmark_name: str

    max_tokens: int = 256
    temperature: float = 0.2
    timeout_s: float = 120.0

    samples_per_task: int = 1
    limit_tasks: int = 0

    enable_unsafe_exec: bool = False
    k: str = "1"
    n_workers: int = 4
    eval_timeout_s: float = 3.0


def _cursor_from_prefix(prefix: str) -> tuple[int, int]:
    lines = prefix.splitlines(keepends=True)
    if not lines:
        return (1, 1)

    line1 = prefix.count("\n") + 1
    last_nl = prefix.rfind("\n")
    col1 = len(prefix) - (last_nl + 1) + 1
    return (line1, col1)


def _repo_dir(cfg: HumanEvalRunConfig) -> Path:
    return cfg.deps_dir / "human-eval-infilling"


def ensure_repo(cfg: HumanEvalRunConfig) -> Path:
    repo_dir = _repo_dir(cfg)
    git_clone(HUMAN_EVAL_INFILLING_REPO, repo_dir, depth=1)
    return repo_dir


def enable_unsafe_exec(repo_dir: Path) -> None:
    """Prepare upstream harness for execution-based evaluation inside Docker."""

    execution_py = repo_dir / "human_eval_infilling" / "execution.py"
    exec_text = execution_py.read_text(encoding="utf-8")

    exec_needle = "#                     exec(check_program, exec_globals)"
    exec_repl = "                    exec(check_program, exec_globals)"

    if exec_needle in exec_text:
        execution_py.write_text(exec_text.replace(exec_needle, exec_repl), encoding="utf-8")

    evaluation_py = repo_dir / "human_eval_infilling" / "evaluation.py"
    eval_text = evaluation_py.read_text(encoding="utf-8")

    assert_line = "        assert len(completion_id) == len(problems), \"Some problems are not attempted.\""
    if assert_line in eval_text:
        patched = eval_text.replace(
            assert_line,
            "        # subset evaluation enabled: sample file may cover a subset of tasks",
        )
        evaluation_py.write_text(patched, encoding="utf-8")


def _load_problems(repo_dir: Path, benchmark_name: str) -> dict:
    sys.path.insert(0, str(repo_dir))
    try:
        from human_eval_infilling.data import read_problems  # type: ignore

        return read_problems(benchmark_name=benchmark_name)
    finally:
        sys.path.pop(0)


def _existing_samples_count(samples_path: Path) -> dict[str, int]:
    if not samples_path.exists():
        return {}

    counts: dict[str, int] = {}
    for row in read_jsonl(samples_path):
        tid = str(row.get("task_id", ""))
        if not tid:
            continue
        counts[tid] = counts.get(tid, 0) + 1
    return counts


def generate_samples(cfg: HumanEvalRunConfig) -> Path:
    repo_dir = ensure_repo(cfg)

    problems = _load_problems(repo_dir, cfg.benchmark_name)

    run_dir = (
        cfg.out_dir
        / "humaneval_infilling"
        / cfg.benchmark_name
        / slugify_segment(cfg.model)
        / cfg.template_id
    )
    samples_path = run_dir / "samples.jsonl"

    existing = _existing_samples_count(samples_path)

    task_ids = list(problems.keys())
    if cfg.limit_tasks > 0:
        task_ids = task_ids[: cfg.limit_tasks]

    total_to_generate = 0
    for tid in task_ids:
        total_to_generate += max(0, cfg.samples_per_task - existing.get(tid, 0))

    if total_to_generate == 0:
        return samples_path

    samples_path.parent.mkdir(parents=True, exist_ok=True)

    with samples_path.open("a", encoding="utf-8") as f:
        for tid in tqdm(task_ids, desc="HumanEval-Infilling generate"):
            problem = problems[tid]
            prefix = problem["prompt"]
            suffix = problem["suffix"]

            line1, col1 = _cursor_from_prefix(prefix)
            ctx = PromptContext(
                file_path=f"humaneval_infilling/{tid}.py",
                language="Python",
                cursor_line1=line1,
                cursor_column1=col1,
                prefix=prefix,
                suffix=suffix,
            )
            built: BuiltPrompt = build_prompt(cfg.template_id, ctx)

            endpoint_norm = cfg.endpoint.rstrip("/")
            use_text_completion = endpoint_norm.endswith("/v1/completions")

            need = max(0, cfg.samples_per_task - existing.get(tid, 0))
            for _ in range(need):
                if use_text_completion:
                    raw = text_completion(
                        TextCompletionParams(
                            endpoint=cfg.endpoint,
                            model=cfg.model,
                            prompt=prefix,
                            suffix=suffix,
                            api_key=cfg.api_key,
                            max_tokens=cfg.max_tokens,
                            temperature=cfg.temperature,
                            stop=[],
                            timeout_s=cfg.timeout_s,
                        )
                    )
                else:
                    raw = chat_completion(
                        ChatCompletionParams(
                            endpoint=cfg.endpoint,
                            model=cfg.model,
                            system_prompt=built.system_prompt,
                            user_prompt=built.user_prompt,
                            api_key=cfg.api_key,
                            max_tokens=cfg.max_tokens,
                            temperature=cfg.temperature,
                            stop=built.stop_sequences,
                            timeout_s=cfg.timeout_s,
                        )
                    )

                completion = sanitize_completion(raw)

                if cfg.benchmark_name == "single-line":
                    if completion:
                        completion = completion.splitlines(keepends=True)[0]
                        if not completion.endswith("\n"):
                            completion += "\n"

                f.write(json.dumps({"task_id": tid, "completion": completion}, ensure_ascii=False) + "\n")

                existing[tid] = existing.get(tid, 0) + 1

    return samples_path


def evaluate(cfg: HumanEvalRunConfig, samples_path: Path) -> None:
    repo_dir = ensure_repo(cfg)

    enable_unsafe_exec(repo_dir)

    project_root = Path(__file__).resolve().parents[3]

    run_evaluation(
        project_root=project_root,
        image=DEFAULT_IMAGE,
        repo_dir=repo_dir,
        run_dir=samples_path.parent,
        benchmark_name=cfg.benchmark_name,
        samples_filename=samples_path.name,
        k=cfg.k,
        n_workers=cfg.n_workers,
        timeout_s=cfg.eval_timeout_s,
    )


def run(cfg: HumanEvalRunConfig) -> Path:
    samples = generate_samples(cfg)

    if cfg.enable_unsafe_exec:
        evaluate(cfg, samples)

    return samples
