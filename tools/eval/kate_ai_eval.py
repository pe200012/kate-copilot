"""kate_ai_eval.py

Offline prompt regression evaluation for Kate AI inline completion.

Supported benchmarks:
- HumanEval-Infilling (Python, functional correctness)
- SAFIM (multi-language, execution + syntax match)

The harness targets an OpenAI-compatible Chat Completions endpoint.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from lib.humaneval_infilling import HumanEvalRunConfig, run as run_humaneval
from lib.prompt_templates import TEMPLATE_FIM_V1, TEMPLATE_FIM_V2, TEMPLATE_FIM_V3
from lib.safim import SafimRunConfig, run as run_safim


def _add_common_args(p: argparse.ArgumentParser) -> None:
    p.add_argument(
        "--endpoint",
        required=True,
        help="OpenAI-compatible /v1/chat/completions endpoint (e.g. Ollama)",
    )
    p.add_argument("--model", required=True, help="Model id (e.g. codestral:latest)")
    p.add_argument("--api-key", default="", help="Bearer token (optional for Ollama)")

    p.add_argument(
        "--template",
        default=TEMPLATE_FIM_V3,
        choices=[TEMPLATE_FIM_V1, TEMPLATE_FIM_V2, TEMPLATE_FIM_V3],
        help="Prompt template id",
    )

    p.add_argument("--max-tokens", type=int, default=256)
    p.add_argument("--temperature", type=float, default=0.2)
    p.add_argument("--timeout-s", type=float, default=120.0)

    p.add_argument(
        "--deps-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "_deps",
        help="Directory for cloned benchmark dependencies",
    )
    p.add_argument(
        "--out-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "runs",
        help="Output directory for run artifacts",
    )


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="kate_ai_eval.py")
    sub = parser.add_subparsers(dest="command", required=True)

    h = sub.add_parser("humaneval", help="Run HumanEval-Infilling")
    _add_common_args(h)
    h.add_argument(
        "--benchmark",
        default="single-line",
        choices=["single-line", "multi-line", "random-span", "random-span-light", "test"],
    )
    h.add_argument("--samples-per-task", type=int, default=1)
    h.add_argument("--limit-tasks", type=int, default=0)

    h.add_argument(
        "--enable-unsafe-exec",
        action="store_true",
        help="Enable exec() in the upstream harness and run evaluation",
    )
    h.add_argument("--k", default="1")
    h.add_argument("--n-workers", type=int, default=4)
    h.add_argument("--eval-timeout-s", type=float, default=3.0)

    s = sub.add_parser("safim", help="Run SAFIM")
    _add_common_args(s)
    s.add_argument(
        "--task",
        required=True,
        choices=["block", "control", "api", "block_v2", "control_fixed"],
        help="SAFIM config name",
    )
    s.add_argument("--limit", type=int, default=0)
    s.add_argument("--offset", type=int, default=0)

    s.add_argument("--execeval-port", type=int, default=5000)
    s.add_argument("--execeval-workers", type=int, default=2)

    return parser


def main() -> int:
    parser = _build_parser()
    args = parser.parse_args()

    if args.command == "humaneval":
        cfg = HumanEvalRunConfig(
            deps_dir=args.deps_dir,
            out_dir=args.out_dir,
            endpoint=args.endpoint,
            model=args.model,
            api_key=args.api_key,
            template_id=args.template,
            benchmark_name=args.benchmark,
            max_tokens=args.max_tokens,
            temperature=args.temperature,
            timeout_s=args.timeout_s,
            samples_per_task=max(1, args.samples_per_task),
            limit_tasks=max(0, args.limit_tasks),
            enable_unsafe_exec=args.enable_unsafe_exec,
            k=args.k,
            n_workers=max(1, args.n_workers),
            eval_timeout_s=args.eval_timeout_s,
        )

        samples = run_humaneval(cfg)
        print(samples)
        return 0

    if args.command == "safim":
        cfg = SafimRunConfig(
            deps_dir=args.deps_dir,
            out_dir=args.out_dir,
            endpoint=args.endpoint,
            model=args.model,
            api_key=args.api_key,
            template_id=args.template,
            task=args.task,
            max_tokens=args.max_tokens,
            temperature=args.temperature,
            timeout_s=args.timeout_s,
            limit=max(0, args.limit),
            offset=max(0, args.offset),
            execeval_port=args.execeval_port,
            execeval_workers=max(1, args.execeval_workers),
        )

        outputs_path, results_path = run_safim(cfg)
        print(outputs_path)
        print(results_path)
        return 0

    raise RuntimeError("UnknownCommand")


if __name__ == "__main__":
    raise SystemExit(main())
