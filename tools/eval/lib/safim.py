"""Module: safim

Runs SAFIM generation + evaluation against an OpenAI-compatible endpoint.

Upstream benchmark:
- https://github.com/gonglinyuan/safim
- HF dataset: https://huggingface.co/datasets/gonglinyuan/safim

Execution-based evaluation uses ExecEval:
- https://github.com/ntunlp/ExecEval

This module mirrors `gonglinyuan/safim/evaluate.py` semantics while keeping the
implementation independent from tree-sitter build steps.
"""

from __future__ import annotations

import ast
import contextlib
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import datasets
import requests
from tqdm import tqdm

from .docker_execeval import ExecEvalConfig, execeval_daemon
from .git_deps import git_clone
from .jsonl import append_jsonl, read_jsonl
from .ollama_openai_client import (
    ChatCompletionParams,
    TextCompletionParams,
    chat_completion,
    text_completion,
)
from .path_utils import slugify_segment
from .prompt_templates import BuiltPrompt, PromptContext, build as build_prompt, sanitize_completion


SAFIM_DATASET = "gonglinyuan/safim"
EXEC_EVAL_REPO = "https://github.com/ntunlp/ExecEval"


LANG_TO_RUNTIME = {
    "cpp": "GNU C++17",
    "csharp": "Mono C#",
    "java": "Java 17",
    "python": "PyPy 3",
}


@dataclass(frozen=True)
class SafimRunConfig:
    deps_dir: Path
    out_dir: Path

    endpoint: str
    model: str
    api_key: str
    template_id: str

    task: str

    max_tokens: int = 256
    temperature: float = 0.2
    timeout_s: float = 120.0

    limit: int = 0
    offset: int = 0

    execeval_port: int = 5000
    execeval_workers: int = 2


def _repo_dir(cfg: SafimRunConfig) -> Path:
    return cfg.deps_dir / "ExecEval"


def ensure_execeval_repo(cfg: SafimRunConfig) -> Path:
    repo_dir = _repo_dir(cfg)
    git_clone(EXEC_EVAL_REPO, repo_dir, depth=1)
    return repo_dir


def _cursor_from_prefix(prefix: str) -> tuple[int, int]:
    line1 = prefix.count("\n") + 1
    last_nl = prefix.rfind("\n")
    col1 = len(prefix) - (last_nl + 1) + 1
    return (line1, col1)


def _language_label(lang: str) -> str:
    return {
        "python": "Python",
        "cpp": "C++",
        "java": "Java",
        "csharp": "C#",
    }.get(lang, lang)


def _split_eval_prompt(eval_prompt: str) -> tuple[str, str]:
    marker = "{{completion}}"
    if marker not in eval_prompt:
        raise ValueError("MissingCompletionMarker")

    prefix, suffix = eval_prompt.split(marker, 1)
    return prefix, suffix


def _load_dataset(task: str) -> list[dict[str, Any]]:
    ds = datasets.load_dataset(SAFIM_DATASET, task, split="test")
    out: list[dict[str, Any]] = []
    for row in ds:
        row = dict(row)
        row["unit_tests"] = json.loads(row.get("unit_tests") or "[]")
        out.append(row)
    return out


def _existing_task_ids(outputs_path: Path) -> set[str]:
    if not outputs_path.exists():
        return set()
    return {str(r.get("task_id")) for r in read_jsonl(outputs_path) if r.get("task_id")}


def generate_outputs(cfg: SafimRunConfig) -> Path:
    run_dir = cfg.out_dir / "safim" / cfg.task / slugify_segment(cfg.model) / cfg.template_id
    outputs_path = run_dir / "outputs.jsonl"

    existing = _existing_task_ids(outputs_path)

    samples = _load_dataset(cfg.task)
    if cfg.offset > 0:
        samples = samples[cfg.offset :]
    if cfg.limit > 0:
        samples = samples[: cfg.limit]

    endpoint_norm = cfg.endpoint.rstrip("/")
    use_text_completion = endpoint_norm.endswith("/v1/completions")

    rows = []
    for sample in tqdm(samples, desc=f"SAFIM generate {cfg.task}"):
        tid = str(sample["task_id"])
        if tid in existing:
            continue

        prefix, suffix = _split_eval_prompt(sample["eval_prompt"])
        line1, col1 = _cursor_from_prefix(prefix)

        ctx = PromptContext(
            file_path=f"safim/{cfg.task}/{tid}",
            language=_language_label(sample.get("lang", "")),
            cursor_line1=line1,
            cursor_column1=col1,
            prefix=prefix,
            suffix=suffix,
        )
        built: BuiltPrompt = build_prompt(cfg.template_id, ctx)

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

        rows.append({"task_id": tid, "completion": completion})

        if len(rows) >= 50:
            append_jsonl(outputs_path, rows)
            for r in rows:
                existing.add(str(r["task_id"]))
            rows.clear()

    if rows:
        append_jsonl(outputs_path, rows)

    return outputs_path


def _syntax_match(code1: str, code2: str, lang: str) -> bool:
    code1 = "".join(code1.split())
    code2 = "".join(code2.split())

    if lang == "python":
        try:
            tree1 = ast.parse(code1, mode="eval")
            tree2 = ast.parse(code2, mode="eval")

            n1 = tree1.body
            n2 = tree2.body

            if isinstance(n1, ast.Call) and isinstance(n2, ast.Call):
                p1 = ([ast.dump(a) for a in n1.args], {kw.arg: ast.dump(kw.value) for kw in n1.keywords})
                p2 = ([ast.dump(a) for a in n2.args], {kw.arg: ast.dump(kw.value) for kw in n2.keywords})
                return p1 == p2

            return ast.dump(tree1) == ast.dump(tree2)
        except SyntaxError:
            return code1 == code2

    return code1 == code2


def _python_syntax_ok(full_code: str) -> bool:
    try:
        ast.parse(full_code)
        return True
    except SyntaxError:
        return False


def _run_execeval(port: int, lang: str, source_code: str, unit_tests: list[dict]) -> tuple[str, bool]:
    runtime = LANG_TO_RUNTIME.get(lang)
    if not runtime:
        raise ValueError(f"UnknownLang: {lang}")

    url = f"http://localhost:{port}/api/execute_code"
    body = {
        "language": runtime,
        "source_code": source_code,
        "unittests": unit_tests,
        "block_network": True,
        "stop_on_first_fail": True,
    }

    r = requests.post(url, json=body, timeout=120.0)
    r.raise_for_status()
    data = r.json()

    if not isinstance(data, dict) or "data" not in data:
        return "COMPILATION_ERROR", False

    rows = data["data"]
    if not (isinstance(rows, list) and rows and isinstance(rows[0], dict)):
        return "COMPILATION_ERROR", False

    passed = all(o.get("exec_outcome") == "PASSED" for o in rows)
    return (json.dumps(rows, ensure_ascii=False), passed)


def evaluate(cfg: SafimRunConfig, outputs_path: Path) -> Path:
    run_dir = cfg.out_dir / "safim" / cfg.task / slugify_segment(cfg.model) / cfg.template_id
    results_path = run_dir / "results.jsonl"

    completions = {str(r["task_id"]): r for r in read_jsonl(outputs_path)}

    samples = _load_dataset(cfg.task)
    if cfg.offset > 0:
        samples = samples[cfg.offset :]
    if cfg.limit > 0:
        samples = samples[: cfg.limit]

    pass_cnt = 0
    total = 0
    results = []

    needs_execeval = any(bool(s.get("unit_tests")) for s in samples)

    daemon_ctx = contextlib.nullcontext()
    if needs_execeval:
        execeval_repo = ensure_execeval_repo(cfg)
        daemon_cfg = ExecEvalConfig(
            execeval_repo_dir=execeval_repo,
            port=cfg.execeval_port,
            num_workers=cfg.execeval_workers,
            container_name=f"kate_ai_execeval_{cfg.execeval_port}",
        )
        daemon_ctx = execeval_daemon(daemon_cfg)

    with daemon_ctx:
        for sample in tqdm(samples, desc=f"SAFIM eval {cfg.task}"):
            tid = str(sample["task_id"])
            total += 1

            comp = completions.get(tid)
            if not comp:
                results.append({"task_id": tid, "result": "EMPTY", "passed": False, "check_result": 0})
                continue

            completion = str(comp.get("completion") or "")
            lang = str(sample.get("lang") or "")
            unit_tests = sample.get("unit_tests") or []
            ground_truth = str(sample.get("ground_truth") or "")

            result = "WRONG_ANSWER"
            passed = False

            if unit_tests:
                if completion == ground_truth:
                    result = "PASSED"
                    passed = True
                else:
                    code = str(sample["eval_prompt"]).replace("{{completion}}", completion)
                    detail, passed = _run_execeval(cfg.execeval_port, lang, code, unit_tests)
                    result = "PASSED" if passed else detail
            else:
                if _syntax_match(completion, ground_truth, lang):
                    result = "EXACT_MATCH"
                    passed = True

            if not completion.strip() and not passed:
                result = "EMPTY"

            if lang == "python" and unit_tests and not passed:
                full_code = str(sample["eval_prompt"]).replace("{{completion}}", completion)
                if not _python_syntax_ok(full_code):
                    result = "COMPILATION_ERROR"

            pass_cnt += int(passed)
            results.append({"task_id": tid, "result": result, "passed": passed, "check_result": 0})

            if len(results) >= 200:
                append_jsonl(results_path, results)
                results.clear()

    if results:
        append_jsonl(results_path, results)

    score = pass_cnt / max(1, total) * 100.0
    print(f"Pass {pass_cnt} / Total {total}")
    print(f"Pass@1: {score:.04f}%")

    return results_path


def run(cfg: SafimRunConfig) -> tuple[Path, Path]:
    outputs_path = generate_outputs(cfg)
    results_path = evaluate(cfg, outputs_path)
    return outputs_path, results_path
